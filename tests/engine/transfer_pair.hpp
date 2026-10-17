#pragma once

// Two TftpTransfers joined by a link in memory, on a clock the test owns: the client and the
// server of one file transfer, with the loss, duplication and reordering a seed decides. No
// platform is involved; this is the state machine alone.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <vector>

#include "adl/arinc615a/tftp_transfer.hpp"
#include "fake_world.hpp"

namespace adl::test {

using namespace adl::arinc615a;

class MemorySink final : public BlockSink {
public:
    IoResult<void> write(std::span<const std::byte> data) noexcept override {
        if (fail_after_bytes && bytes.size() + data.size() > *fail_after_bytes) {
            return std::unexpected(IoFailure{IoError::no_space, 0});
        }
        bytes.insert(bytes.end(), data.begin(), data.end());
        ++writes;
        return {};
    }
    std::vector<std::byte> bytes;
    std::size_t writes = 0;
    std::optional<std::size_t> fail_after_bytes;
};

class MemorySource final : public BlockSource {
public:
    explicit MemorySource(std::vector<std::byte> content = {}) : bytes(std::move(content)) {}
    std::uint64_t size() const noexcept override { return bytes.size(); }
    IoResult<std::size_t> read(std::uint64_t offset, std::span<std::byte> out) noexcept override {
        if (fail_reads) {
            return std::unexpected(IoFailure{IoError::io_error, 0});
        }
        if (offset >= bytes.size()) {
            return std::size_t{0};
        }
        const std::size_t count =
            std::min<std::size_t>(out.size(), bytes.size() - static_cast<std::size_t>(offset));
        std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(offset), count, out.begin());
        return count;
    }
    std::vector<std::byte> bytes;
    bool fail_reads = false;
};

[[nodiscard]] inline std::vector<std::byte> pattern(std::size_t size, std::uint64_t seed = 1) {
    adl::fake::Random random(seed);
    std::vector<std::byte> bytes(size);
    for (auto& byte : bytes) {
        byte = static_cast<std::byte>(random.next() & 0xFF);
    }
    return bytes;
}

struct LinkProfile {
    Duration latency = std::chrono::milliseconds(1);
    Duration jitter{};
    std::uint32_t loss_per_million = 0;
    std::uint32_t duplicate_per_million = 0;
};

// A datagram as the link carried it, for assertions.
struct Seen {
    Instant at;
    bool from_client = false;
    std::vector<std::byte> bytes;
};

class TransferPair {
public:
    static constexpr Endpoint client_endpoint{Address{{10, 0, 0, 1}}, 2000};
    static constexpr Endpoint server_control{Address{{10, 0, 0, 2}}, 59};
    static constexpr Endpoint server_tid{Address{{10, 0, 0, 2}}, 3000};

    explicit TransferPair(std::size_t buffer_bytes = 1024 + 4, std::uint64_t seed = 1)
        : client_tx_(std::max<std::size_t>(buffer_bytes, TftpTransfer::min_buffer_bytes)),
          server_tx_(std::max<std::size_t>(buffer_bytes, TftpTransfer::min_buffer_bytes)),
          client_(client_tx_),
          server_(server_tx_),
          random_(seed) {}

    TftpTransfer& client() { return client_; }
    TftpTransfer& server() { return server_; }
    LinkProfile& profile() { return profile_; }
    const std::vector<Seen>& seen() const { return seen_; }
    Instant now() const { return now_; }
    bool server_started() const { return server_created_; }

    // What the server does with the request that arrives at its control port.
    enum class ServerMode { read, write, ignore };
    ServerMode server_mode = ServerMode::ignore;
    BlockSink* server_sink = nullptr;
    BlockSource* server_source = nullptr;
    TransferSettings server_settings;
    // Instead of a transfer, the server answers a request with this datagram (from its TID).
    std::vector<std::byte> canned_reply;

    // Called for every datagram sent, in order, with its index; true drops it.
    std::function<bool(bool from_client, std::span<const std::byte> bytes, std::size_t index)> drop;

    // The client's outgoing datagram (take it from the transfer and put it on the link).
    void flush_client() { flush(client_, true); }

    // Runs until both transfers are over or `limit` has passed. True if they ended.
    bool run(Duration limit = std::chrono::minutes(30)) {
        flush(client_, true);
        flush(server_, false);
        const Instant deadline = now_ + limit;
        for (;;) {
            const bool over = !client_.active() && !server_.active();
            if (over && in_flight_.empty()) {
                return true;
            }
            Instant next = earlier_of(client_.next_wake(), server_.next_wake());
            std::size_t pick = in_flight_.size();
            for (std::size_t i = 0; i < in_flight_.size(); ++i) {
                if (pick == in_flight_.size() || in_flight_[i].at < in_flight_[pick].at ||
                    (in_flight_[i].at == in_flight_[pick].at &&
                     in_flight_[i].sequence < in_flight_[pick].sequence)) {
                    pick = i;
                }
            }
            if (pick != in_flight_.size() && in_flight_[pick].at <= next) {
                next = in_flight_[pick].at;
            } else {
                pick = in_flight_.size();
            }
            if (next == never || next > deadline) {
                return over;
            }
            now_ = next;
            if (pick != in_flight_.size()) {
                const InFlight packet = std::move(in_flight_[pick]);
                in_flight_.erase(in_flight_.begin() + static_cast<std::ptrdiff_t>(pick));
                deliver(packet);
            } else {
                client_.on_timer(now_);
                server_.on_timer(now_);
                flush(client_, true);
                flush(server_, false);
            }
        }
    }

private:
    struct InFlight {
        Instant at;
        std::uint64_t sequence = 0;
        bool to_server = false;
        Endpoint from;
        Endpoint to;
        std::vector<std::byte> bytes;
    };

    void flush(TftpTransfer& transfer, bool from_client) {
        while (const auto out = transfer.take_outgoing()) {
            send(from_client, from_client ? client_endpoint : server_tid, out->to, out->bytes);
        }
    }

    void send(bool from_client, const Endpoint& from, const Endpoint& to,
              std::span<const std::byte> bytes) {
        seen_.push_back(Seen{now_, from_client, {bytes.begin(), bytes.end()}});
        if (drop && drop(from_client, bytes, seen_.size() - 1)) {
            return;
        }
        if (random_.chance(profile_.loss_per_million)) {
            return;
        }
        const int copies = random_.chance(profile_.duplicate_per_million) ? 2 : 1;
        for (int copy = 0; copy < copies; ++copy) {
            Duration delay = profile_.latency;
            if (profile_.jitter > Duration::zero()) {
                delay += Duration(static_cast<std::int64_t>(
                    random_.below(static_cast<std::uint64_t>(profile_.jitter.count()) + 1)));
            }
            in_flight_.push_back(InFlight{now_ + delay, sequence_++, from_client, from, to,
                                          {bytes.begin(), bytes.end()}});
        }
    }

    void deliver(const InFlight& packet) {
        if (packet.to_server) {
            if (packet.to.port == 59) {
                if (!server_created_) {
                    server_created_ = true;
                    const auto decoded = decode_packet(packet.bytes);
                    const auto* request = decoded ? std::get_if<TftpRequest>(&*decoded) : nullptr;
                    if (!canned_reply.empty()) {
                        send(false, server_tid, client_endpoint, canned_reply);
                    } else if (request != nullptr && server_mode == ServerMode::read &&
                               server_source != nullptr) {
                        server_.accept_read(now_, packet.from, *request, *server_source,
                                            server_settings);
                    } else if (request != nullptr && server_mode == ServerMode::write &&
                               server_sink != nullptr) {
                        server_.accept_write(now_, packet.from, *request, *server_sink,
                                             server_settings);
                    }
                } else {
                    server_.on_repeated_request(now_);
                }
            } else {
                server_.on_datagram(now_, packet.from, packet.bytes);
            }
            flush(server_, false);
        } else {
            client_.on_datagram(now_, packet.from, packet.bytes);
            flush(client_, true);
        }
    }

    std::vector<std::byte> client_tx_;
    std::vector<std::byte> server_tx_;
    TftpTransfer client_;
    TftpTransfer server_;
    adl::fake::Random random_;
    LinkProfile profile_;
    Instant now_{};
    std::uint64_t sequence_ = 0;
    std::vector<InFlight> in_flight_;
    std::vector<Seen> seen_;
    bool server_created_ = false;
};

}  // namespace adl::test

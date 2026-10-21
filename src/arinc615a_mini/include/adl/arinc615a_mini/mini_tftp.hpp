#pragma once

// A small TFTP endpoint, written for the simulated target (tests/engine) and used by the mock
// target (src/mock_target): lock-step blocks, a two second time-out and one retry, the options
// the loader uses (blksize, tsize, checksum_<n>), and WAIT and ABORT. It is deliberately plain,
// so that the engine is checked against a second, independent reading of RFC 1350 and section
// 5.3.2 and not against itself. It speaks in the engine's Endpoint but uses only the codec for
// the packets.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "adl/arinc615a/endpoint.hpp"
#include "adl/arinc615a/status_code.hpp"
#include "adl/arinc615a/tftp.hpp"
#include "adl/arinc615a/time.hpp"

namespace adl::arinc615a_mini {

using Bytes = std::vector<std::byte>;

// What a MiniTransfer needs of the thing it runs in.
class MiniHost {
public:
    virtual ~MiniHost() = default;
    virtual void send(std::uint16_t from_port, const adl::arinc615a::Endpoint& to,
                      std::span<const std::byte> payload) = 0;
    virtual void after(adl::arinc615a::Duration delay, std::function<void()> action) = 0;
};

struct MiniResult {
    enum class Kind { running, ok, timed_out, error, waited, aborted };
    Kind kind = Kind::running;
    std::uint16_t wait_seconds = 0;
    adl::arinc615a::StatusCode abort_status = adl::arinc615a::StatusCode::aborted_by_data_loader;
    adl::arinc615a::TftpErrorCode error_code = adl::arinc615a::TftpErrorCode::not_defined;
    std::string error_text;
};

class MiniTransfer {
public:
    enum class Role { client_write, client_read, server_read, server_write };

    struct Request {
        std::string file_name;
        Bytes content;                                           // client_write: the file to send
        std::size_t block_size = 0;                              // 0: ask for none
        std::optional<adl::arinc615a::ChecksumOption> checksum;  // sent with a write
    };

    MiniTransfer(MiniHost& host, std::uint16_t port, Role role)
        : host_(host), port_(port), role_(role) {}

    // A client sends its request to `server`.
    void start_client(const adl::arinc615a::Endpoint& server, Request request);

    // A server answers a request from `client`. For a read, `content` is the file; a write
    // collects what comes. The server takes any blksize up to `block_size_cap`.
    void serve(const adl::arinc615a::Endpoint& client, const adl::arinc615a::TftpRequest& request,
               Bytes content, std::size_t block_size_cap = 1024, bool echo_port = false);

    void on_datagram(const adl::arinc615a::Endpoint& from, std::span<const std::byte> payload);
    void on_timeout(std::uint64_t generation);

    [[nodiscard]] std::uint16_t port() const noexcept { return port_; }
    [[nodiscard]] Role role() const noexcept { return role_; }
    [[nodiscard]] const MiniResult& result() const noexcept { return result_; }
    [[nodiscard]] bool running() const noexcept {
        return result_.kind == MiniResult::Kind::running;
    }
    [[nodiscard]] const Bytes& received() const noexcept { return received_; }
    [[nodiscard]] const std::string& file_name() const noexcept { return name_; }
    [[nodiscard]] std::size_t block_size() const noexcept { return block_size_; }
    [[nodiscard]] bool checksum_acknowledged() const noexcept { return checksum_acknowledged_; }
    [[nodiscard]] std::optional<std::uint64_t> announced_size() const noexcept {
        return announced_;
    }
    [[nodiscard]] std::uint64_t datagrams_sent() const noexcept { return sent_; }

    // Called once, when the transfer ends (successfully or not).
    std::function<void(MiniTransfer&)> on_done;

private:
    void transmit(Bytes packet);
    void send_data();
    void send_ack(std::uint16_t block);
    void finish(MiniResult::Kind kind);
    void accept_error(const adl::arinc615a::TftpError& error);
    void receive(const adl::arinc615a::TftpData& data);

    MiniHost& host_;
    std::uint16_t port_;
    Role role_;
    adl::arinc615a::Endpoint peer_;
    bool peer_known_ = false;
    std::string name_;

    Bytes content_;  // to send
    Bytes received_;
    std::size_t block_size_ = 512;
    std::size_t asked_block_size_ = 0;
    std::uint64_t offset_ = 0;
    std::uint16_t block_ = 0;  // sender: the block in flight; receiver: the last acknowledged
    bool last_sent_ = false;
    bool awaiting_first_ = false;  // a client before the first reply; a server before block 1
    std::optional<std::uint64_t> announced_;
    bool checksum_asked_ = false;
    bool checksum_acknowledged_ = false;

    Bytes last_packet_;
    unsigned tries_ = 0;
    std::uint64_t generation_ = 0;
    std::uint64_t sent_ = 0;
    MiniResult result_;
    bool done_called_ = false;
};

[[nodiscard]] Bytes mini_encode(const adl::arinc615a::TftpPacket& packet);

}  // namespace adl::arinc615a_mini

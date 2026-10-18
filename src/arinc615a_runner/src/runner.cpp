#include "adl/arinc615a/runner.hpp"

#include <algorithm>
#include <array>
#include <type_traits>
#include <vector>

#include <adl/profiling.hpp>

namespace adl::arinc615a {
namespace {

// The engine's types are the platform's, so that converting is copying.
static_assert(std::is_same_v<Duration, adl::platform::Duration>);
static_assert(sizeof(Address) == sizeof(adl::platform::Ipv4Address));
static_assert(sizeof(Endpoint) == sizeof(adl::platform::Endpoint));

using adl::platform::Owned;
using adl::platform::UdpSocket;

[[nodiscard]] adl::platform::Endpoint to_platform(const Endpoint& endpoint) noexcept {
    return adl::platform::Endpoint{adl::platform::Ipv4Address{endpoint.address.octets},
                                   endpoint.port};
}

[[nodiscard]] Endpoint from_platform(const adl::platform::Endpoint& endpoint) noexcept {
    return Endpoint{Address{endpoint.address.octets}, endpoint.port};
}

[[nodiscard]] adl::platform::Duration to_platform(Duration d) noexcept {
    return adl::platform::Duration{d};
}

// Carries out what the loader asked for after a call. A datagram that cannot be sent is a lost
// datagram, which is what UDP promises anyway and what the engine's time-outs are for.
class Sockets {
public:
    Sockets(adl::platform::Network& network, Owned<UdpSocket> control) noexcept
        : network_(network), control_(std::move(control)) {}

    void execute(std::span<const Command> commands) noexcept {
        for (const Command& command : commands) {
            switch (command.kind) {
                case Command::Kind::send_from_control:
                    static_cast<void>(control_->send_to(to_platform(command.to), command.bytes));
                    break;
                case Command::Kind::send_from_slot: {
                    Owned<UdpSocket>& socket = slots_[command.slot];
                    if (!socket) {
                        adl::platform::UdpOptions options;  // any address, any free port
                        if (auto opened = network_.open_udp(options)) {
                            socket = std::move(*opened);
                        } else {
                            break;
                        }
                    }
                    static_cast<void>(socket->send_to(to_platform(command.to), command.bytes));
                    break;
                }
                case Command::Kind::release_slot:
                    slots_[command.slot].reset();
                    break;
            }
        }
    }

    // The socket of an owner now (control is `max_slots`), or null if it has been closed since
    // the sockets were gathered: executing a command can close one.
    UdpSocket* find(std::size_t owner) noexcept {
        return owner == DataLoader::max_slots ? control_.get() : slots_[owner].get();
    }

    // The sockets to wait on, control first, and which slot each is (control is `max_slots`).
    std::size_t gather(std::array<UdpSocket*, DataLoader::max_slots + 1>& sockets,
                       std::array<std::size_t, DataLoader::max_slots + 1>& owners) noexcept {
        std::size_t count = 0;
        sockets[count] = control_.get();
        owners[count++] = DataLoader::max_slots;
        for (std::size_t i = 0; i < DataLoader::max_slots; ++i) {
            if (slots_[i]) {
                sockets[count] = slots_[i].get();
                owners[count++] = i;
            }
        }
        return count;
    }

private:
    adl::platform::Network& network_;
    Owned<UdpSocket> control_;
    std::array<Owned<UdpSocket>, DataLoader::max_slots> slots_;
};

}  // namespace

Instant to_instant(adl::platform::Instant at) noexcept {
    return Instant{at.since_origin};
}

std::expected<Outcome, RunError> Runner::run(DataLoader& loader, const Operation& operation,
                                             const RunOptions& options) noexcept {
    const adl::profiling::Zone zone{"615a: run operation"};
    auto& network = host_.network();
    auto& clock = host_.clock();

    adl::platform::UdpOptions control_options;
    control_options.local =
        adl::platform::Endpoint{adl::platform::Ipv4Address::any(), loader.control_port()};
    auto control = network.open_udp(control_options);
    if (!control) {
        return std::unexpected(
            RunError{RunError::Stage::control_socket, Error::invalid_value, control.error().code});
    }
    Sockets sockets(network, std::move(*control));

    if (const auto started = loader.start(to_instant(clock.now()), operation); !started) {
        return std::unexpected(
            RunError{RunError::Stage::start, started.error(), adl::platform::ErrorCode::other});
    }
    sockets.execute(loader.commands());

    std::vector<std::byte> buffer(loader.datagram_buffer_bytes());
    std::array<UdpSocket*, DataLoader::max_slots + 1> waiting{};
    std::array<std::size_t, DataLoader::max_slots + 1> owners{};
    std::array<bool, DataLoader::max_slots + 1> ready{};

    while (!loader.idle()) {
        Instant now = to_instant(clock.now());
        Duration timeout = Duration::max();
        if (const Instant wake = loader.next_wake(); wake != never) {
            timeout = wake > now ? wake - now : Duration::zero();
        }
        if (options.controller != nullptr) {
            timeout = std::min(timeout, options.poll_interval);
        }

        const std::size_t count = sockets.gather(waiting, owners);
        const auto waited =
            network.wait_readable(std::span<UdpSocket* const>{waiting}.first(count),
                                  to_platform(timeout), std::span<bool>{ready}.first(count));
        if (!waited) {
            if (waited.error().code == adl::platform::ErrorCode::interrupted) {
                continue;
            }
            return std::unexpected(
                RunError{RunError::Stage::wait, Error::invalid_value, waited.error().code});
        }
        now = to_instant(clock.now());

        if (*waited != 0) {
            for (std::size_t i = 0; i < count; ++i) {
                if (!ready[i]) {
                    continue;
                }
                // An earlier datagram of this round may have closed (or reopened) this socket.
                UdpSocket* const socket = sockets.find(owners[i]);
                if (socket == nullptr) {
                    continue;
                }
                const auto datagram = socket->receive_from(buffer, adl::platform::Duration::zero());
                if (!datagram || datagram->truncated) {
                    continue;  // a datagram larger than any block is not ours to read
                }
                const std::span<const std::byte> bytes{buffer.data(), datagram->size};
                const Endpoint from = from_platform(datagram->source);
                if (owners[i] == DataLoader::max_slots) {
                    loader.on_control_datagram(now, from, bytes);
                } else {
                    loader.on_transfer_datagram(now, owners[i], from, bytes);
                }
                sockets.execute(loader.commands());
            }
        }
        if (options.controller != nullptr) {
            options.controller->on_poll(loader, now);
            sockets.execute(loader.commands());
        }
        loader.on_timer(now);
        sockets.execute(loader.commands());
    }
    return loader.outcome();
}

adl::platform::Result<std::size_t> Runner::find(const FindConfig& config, std::span<FindTarget> out,
                                                std::uint16_t port) noexcept {
    const adl::profiling::Zone zone{"615a: find"};
    auto& network = host_.network();
    auto& clock = host_.clock();

    adl::platform::UdpOptions options;
    options.local = adl::platform::Endpoint{adl::platform::Ipv4Address::any(), port};
    options.allow_broadcast = true;
    options.reuse_address = true;  // another FIND on this machine may be listening too
    auto opened = network.open_udp(options);
    if (!opened) {
        return std::unexpected(opened.error());
    }
    UdpSocket& socket = **opened;

    FindClient client(config);
    client.start(to_instant(clock.now()));
    for (const Address& destination : client.destinations()) {
        static_cast<void>(
            socket.send_to(to_platform(Endpoint{destination, find_port}), client.request()));
    }

    std::vector<std::byte> buffer(find_max_answer_size + 16);
    std::array<UdpSocket*, 1> waiting{&socket};
    std::array<bool, 1> ready{};
    while (!client.done()) {
        Instant now = to_instant(clock.now());
        const Instant wake = client.next_wake();
        const Duration timeout = wake > now ? wake - now : Duration::zero();
        const auto waited =
            network.wait_readable(waiting, to_platform(timeout), std::span<bool>{ready});
        if (!waited) {
            if (waited.error().code == adl::platform::ErrorCode::interrupted) {
                continue;
            }
            return std::unexpected(waited.error());
        }
        now = to_instant(clock.now());
        if (*waited != 0) {
            if (const auto datagram = socket.receive_from(buffer, adl::platform::Duration::zero());
                datagram && !datagram->truncated) {
                client.on_datagram(now, from_platform(datagram->source),
                                   std::span<const std::byte>{buffer.data(), datagram->size});
            }
        }
        client.on_timer(now);
    }
    const std::size_t count = std::min(out.size(), client.targets().size());
    std::copy_n(client.targets().begin(), count, out.begin());
    return count;
}

}  // namespace adl::arinc615a

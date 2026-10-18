#include "adl/mock/link.hpp"

#include <algorithm>
#include <array>

namespace adl::mock {

namespace {

using adl::arinc615a::Address;
using adl::arinc615a::Duration;
using adl::arinc615a::Endpoint;
using adl::arinc615a::Instant;

[[nodiscard]] adl::platform::Endpoint to_platform(const Endpoint& endpoint) noexcept {
    return adl::platform::Endpoint{adl::platform::Ipv4Address{endpoint.address.octets},
                                   endpoint.port};
}

[[nodiscard]] Endpoint from_platform(const adl::platform::Endpoint& endpoint) noexcept {
    return Endpoint{Address{endpoint.address.octets}, endpoint.port};
}

[[nodiscard]] Instant from_platform_instant(adl::platform::Instant at) noexcept {
    return Instant{at.since_origin};
}

}  // namespace

void RealTargetLink::send(std::uint16_t from_port, const Endpoint& to,
                          std::span<const std::byte> payload) {
    auto& socket = sockets_[from_port];
    if (!socket) {
        // Any address, the port the transfer speaks from. A send is also what opens the
        // socket, so a port that has never sent has nothing listening on it (a datagram for
        // it cannot have arrived, since the loader answers what it was sent).
        adl::platform::UdpOptions options;
        options.local = adl::platform::Endpoint{adl::platform::Ipv4Address::any(), from_port};
        auto opened = host_->network().open_udp(options);
        if (!opened) {
            return;  // a lost datagram, as UDP promises
        }
        socket = std::move(*opened);
    }
    static_cast<void>(socket->send_to(to_platform(to), payload));
}

void RealTargetLink::after(Duration delay, std::function<void()> action) {
    timers_.emplace(from_platform_instant(host_->clock().now()) + delay, std::move(action));
}

bool RealTargetLink::bind(std::uint16_t port, std::string& error) {
    if (sockets_.contains(port)) {
        return true;
    }
    adl::platform::UdpOptions options;
    options.local = adl::platform::Endpoint{adl::platform::Ipv4Address::any(), port};
    options.reuse_address = true;  // several targets (or a loader) may share this machine
    options.receive_buffer_bytes = 256 * 1024;
    auto opened = host_->network().open_udp(options);
    if (!opened) {
        error = "port " + std::to_string(port) + ": " +
                std::string{adl::platform::to_string(opened.error().code)};
        return false;
    }
    sockets_[port] = std::move(*opened);
    return true;
}

std::vector<std::uint16_t> RealTargetLink::ports() const {
    std::vector<std::uint16_t> result;
    result.reserve(sockets_.size());
    for (const auto& [port, socket] : sockets_) {
        static_cast<void>(socket);
        result.push_back(port);
    }
    return result;
}

std::int32_t RealTargetLink::wait(std::uint32_t timeout_ms) {
    auto& clock = host_->clock();
    const Instant entered = from_platform_instant(clock.now());
    fire_due(entered);

    Instant deadline = entered + std::chrono::milliseconds(timeout_ms);
    if (!timers_.empty()) {
        deadline = std::min(deadline, timers_.begin()->first);
    }

    // One wait takes at most max_wait_sockets; with more ports open (a long session) the
    // engine comes round again soon and the rest are drained then.
    std::array<adl::platform::UdpSocket*, adl::platform::max_wait_sockets> waiting{};
    std::array<bool, adl::platform::max_wait_sockets> ready{};
    std::size_t count = 0;
    for (const auto& [port, socket] : sockets_) {
        static_cast<void>(port);
        if (count == waiting.size()) {
            deadline = std::min(deadline, entered + std::chrono::milliseconds(50));
            break;
        }
        waiting[count++] = socket.get();
    }

    std::int32_t ready_count = 0;
    if (count == 0) {
        clock.sleep_for(deadline - from_platform_instant(clock.now()));
    } else {
        const Duration timeout = deadline - from_platform_instant(clock.now());
        const auto waited = host_->network().wait_readable(
            std::span<adl::platform::UdpSocket* const>{waiting}.first(count), timeout,
            std::span<bool>{ready}.first(count));
        if (!waited) {
            fire_due(from_platform_instant(clock.now()));
            if (waited.error().code == adl::platform::ErrorCode::interrupted) {
                return 0;
            }
            return -1;
        }
        ready_count = static_cast<std::int32_t>(*waited);
    }
    fire_due(from_platform_instant(clock.now()));
    return ready_count;
}

bool RealTargetLink::receive(std::uint16_t port, Endpoint& from, std::span<std::byte> out,
                             std::size_t& size) {
    const auto found = sockets_.find(port);
    if (found == sockets_.end()) {
        return false;
    }
    const auto datagram = found->second->receive_from(out, adl::platform::Duration::zero());
    if (!datagram || datagram->truncated) {
        return false;  // nothing (or one too large for any block, which is not ours to read)
    }
    from = from_platform(datagram->source);
    size = datagram->size;
    return true;
}

Instant RealTargetLink::now() const noexcept {
    return Instant{host_->clock().now().since_origin};
}

void RealTargetLink::fire_due(Instant now) {
    for (auto it = timers_.begin(); it != timers_.end() && it->first <= now;) {
        std::function<void()> action = std::move(it->second);
        it = timers_.erase(it);
        action();
    }
}

}  // namespace adl::mock

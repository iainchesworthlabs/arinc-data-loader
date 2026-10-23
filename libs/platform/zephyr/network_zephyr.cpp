// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/platform/network.hpp"
#include "detail/nothrow.hpp"
#include "zephyr/zephyr_os.hpp"

#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>

#include <array>
#include <bit>
#include <cerrno>
#include <climits>
#include <cstring>
#include <memory>

namespace adl::platform::zephyr
{
namespace
{

using zephyr_os::fail_errno;

constexpr std::size_t max_udp_payload = 65507;

// Network byte order without htons, which is a macro in Zephyr.
[[nodiscard]] constexpr std::uint16_t to_network(std::uint16_t host) noexcept
{
    return std::endian::native == std::endian::little ? std::byteswap(host) : host;
}
[[nodiscard]] constexpr std::uint16_t from_network(std::uint16_t net) noexcept
{
    return to_network(net);
}

[[nodiscard]] sockaddr_in to_sockaddr(const Endpoint& endpoint) noexcept
{
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = to_network(endpoint.port);
    std::memcpy(&address.sin_addr, endpoint.address.octets.data(), 4);
    return address;
}

[[nodiscard]] Endpoint to_endpoint(const sockaddr_in& address) noexcept
{
    Endpoint endpoint;
    std::memcpy(endpoint.address.octets.data(), &address.sin_addr, 4);
    endpoint.port = from_network(address.sin_port);
    return endpoint;
}

// Milliseconds for zsock_poll: rounded up so that a wait never ends early, capped
// at what an int holds, -1 for `forever`.
[[nodiscard]] int to_poll_milliseconds(Duration timeout) noexcept
{
    if (timeout == forever)
    {
        return -1;
    }
    if (timeout <= Duration::zero())
    {
        return 0;
    }
    constexpr std::int64_t nanoseconds_per_millisecond = 1'000'000;
    std::int64_t ms = timeout.count() / nanoseconds_per_millisecond;
    if (timeout.count() % nanoseconds_per_millisecond != 0)
    {
        ++ms;
    }
    return ms > INT_MAX ? INT_MAX : static_cast<int>(ms);
}

class ZephyrUdpSocket final : public UdpSocket
{
public:
    ZephyrUdpSocket(int fd, Endpoint local, bool allow_broadcast, Clock& clock) noexcept
        : fd_(fd), local_(local), allow_broadcast_(allow_broadcast), clock_(clock)
    {
    }
    ~ZephyrUdpSocket() override
    {
        (void)zsock_close(fd_);
    }

    [[nodiscard]] int fd() const noexcept
    {
        return fd_;
    }

    Endpoint local_endpoint() const noexcept override
    {
        return local_;
    }

    Result<void> send_to(const Endpoint& destination, std::span<const std::byte> data) noexcept override
    {
        if (data.size() > max_udp_payload)
        {
            return fail(ErrorCode::message_too_large);
        }
        // Whether a stack refuses a broadcast from a socket that did not ask is up
        // to the stack; the interface says it is refused, so it is, here.
        if (destination.address.is_broadcast() && !allow_broadcast_)
        {
            return fail(ErrorCode::permission_denied);
        }
        const sockaddr_in address = to_sockaddr(destination);
        for (int attempt = 0; attempt < 2; ++attempt)
        {
            const ssize_t sent = zsock_sendto(fd_, data.data(), data.size(), 0,
                                              reinterpret_cast<const sockaddr*>(&address), sizeof address);
            if (sent >= 0)
            {
                return {};
            }
            const int error = errno;
            if (error == EINTR)
            {
                --attempt;
                continue;
            }
            if (error == EAGAIN || error == EWOULDBLOCK || error == ENOBUFS)
            {
                // No buffer for it at the moment: wait a moment for one, once.
                zsock_pollfd waiting{fd_, ZSOCK_POLLOUT, 0};
                (void)zsock_poll(&waiting, 1, 100);
                continue;
            }
            return fail_errno(error);
        }
        return fail(ErrorCode::resource_exhausted);
    }

    Result<Datagram> receive_from(std::span<std::byte> buffer, Duration timeout) noexcept override
    {
        const Instant deadline = clock_.now() + timeout;
        for (;;)
        {
            // The call does not block, so a readiness report that turns out to be
            // nothing cannot hang the caller past its time-out. MSG_TRUNC asks for
            // the real length of a datagram that did not fit, which is how a cut
            // one is told from one that was just the size of the buffer.
            sockaddr_in from{};
            socklen_t from_length = sizeof from;
            const ssize_t got = zsock_recvfrom(fd_, buffer.data(), buffer.size(), ZSOCK_MSG_DONTWAIT | ZSOCK_MSG_TRUNC,
                                               reinterpret_cast<sockaddr*>(&from), &from_length);
            if (got >= 0)
            {
                Datagram datagram;
                const auto real = static_cast<std::size_t>(got);
                datagram.size = real < buffer.size() ? real : buffer.size();
                datagram.truncated = real > buffer.size();
                datagram.source = to_endpoint(from);
                return datagram;
            }
            const int error = errno;
            if (error != EAGAIN && error != EWOULDBLOCK && error != EINTR && error != ECONNREFUSED)
            {
                return fail_errno(error);
            }
            const Duration remaining = deadline - clock_.now();
            if (remaining <= Duration::zero() && timeout != forever)
            {
                return fail(ErrorCode::timeout);
            }
            zsock_pollfd waiting{fd_, ZSOCK_POLLIN, 0};
            (void)zsock_poll(&waiting, 1, to_poll_milliseconds(timeout == forever ? forever : remaining));
        }
    }

private:
    int fd_;
    Endpoint local_;
    bool allow_broadcast_;
    Clock& clock_;
};

class ZephyrNetwork final : public Network
{
public:
    explicit ZephyrNetwork(Clock& clock) noexcept : clock_(clock) {}

    Result<Owned<UdpSocket>> open_udp(const UdpOptions& options) noexcept override
    {
        const int fd = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (fd < 0)
        {
            return fail_errno(errno);
        }
        const int on = 1;
        if (options.reuse_address)
        {
            if (zsock_setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on) != 0)
            {
                const int error = errno;
                (void)zsock_close(fd);
                return fail_errno(error);
            }
            // Where the stack has SO_REUSEPORT it is wanted as well, as on the BSDs.
            (void)zsock_setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof on);
        }
        if (options.allow_broadcast)
        {
            // A stack that has no such option still sends a broadcast; send_to has
            // already refused it for a socket that did not ask.
            (void)zsock_setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &on, sizeof on);
        }
        if (options.receive_buffer_bytes != 0)
        {
            const int size = options.receive_buffer_bytes > static_cast<std::size_t>(INT_MAX)
                                 ? INT_MAX
                                 : static_cast<int>(options.receive_buffer_bytes);
            (void)zsock_setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof size);
        }
        const sockaddr_in address = to_sockaddr(options.local);
        if (zsock_bind(fd, reinterpret_cast<const sockaddr*>(&address), sizeof address) != 0)
        {
            const int error = errno;
            (void)zsock_close(fd);
            // An address that is not an interface's: Zephyr answers ENOENT, where
            // the sockets API of every other system says EADDRNOTAVAIL.
            if (error == ENOENT)
            {
                return fail(ErrorCode::address_unavailable, error);
            }
            return fail_errno(error);
        }
        sockaddr_in bound{};
        socklen_t length = sizeof bound;
        if (zsock_getsockname(fd, reinterpret_cast<sockaddr*>(&bound), &length) != 0)
        {
            const int error = errno;
            (void)zsock_close(fd);
            return fail_errno(error);
        }
        Endpoint local = options.local;
        local.port = from_network(bound.sin_port);

        auto socket = make_owned_nothrow<UdpSocket, ZephyrUdpSocket>(fd, local, options.allow_broadcast, clock_);
        if (!socket)
        {
            (void)zsock_close(fd);
            return fail(ErrorCode::resource_exhausted);
        }
        return socket;
    }

    Result<std::size_t> wait_readable(std::span<UdpSocket* const> sockets, Duration timeout,
                                      std::span<bool> ready) noexcept override
    {
        if (sockets.size() > max_wait_sockets || ready.size() != sockets.size())
        {
            return fail(ErrorCode::invalid_argument);
        }
        const Instant deadline = clock_.now() + timeout;
        std::array<zsock_pollfd, max_wait_sockets> set{};
        for (std::size_t i = 0; i < sockets.size(); ++i)
        {
            set[i] = zsock_pollfd{static_cast<ZephyrUdpSocket*>(sockets[i])->fd(), ZSOCK_POLLIN, 0};
        }
        for (;;)
        {
            const Duration remaining = timeout == forever ? forever : deadline - clock_.now();
            const int result =
                zsock_poll(set.data(), static_cast<int>(sockets.size()), to_poll_milliseconds(remaining));
            if (result < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                return fail_errno(errno);
            }
            std::size_t count = 0;
            for (std::size_t i = 0; i < sockets.size(); ++i)
            {
                ready[i] = (set[i].revents & (ZSOCK_POLLIN | ZSOCK_POLLERR | ZSOCK_POLLHUP)) != 0;
                count += ready[i] ? 1U : 0U;
            }
            if (count != 0 || (timeout != forever && deadline - clock_.now() <= Duration::zero()))
            {
                return count;
            }
        }
    }

    std::size_t max_datagram_bytes() const noexcept override
    {
        return max_udp_payload;
    }

private:
    Clock& clock_;
};

} // namespace

std::unique_ptr<Network> make_network(Clock& clock) noexcept
{
    return detail::make_unique_nothrow<ZephyrNetwork>(clock);
}

} // namespace adl::platform::zephyr

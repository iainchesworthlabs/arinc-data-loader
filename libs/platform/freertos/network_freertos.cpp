#include "adl/platform/network.hpp"
#include "detail/nothrow.hpp"
#include "freertos/freertos_os.hpp"

#include <lwip/netif.h>
#include <lwip/sockets.h>
#include <lwip/tcpip.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cerrno>
#include <climits>
#include <cstring>
#include <memory>

namespace adl::platform::freertos
{
namespace
{

constexpr std::size_t max_udp_payload = 65507;

// lwIP's socket calls fail with -1 and the system's errno values (the port's cc.h says
// which errno.h), so the usual mapping applies.
[[nodiscard]] ErrorCode map_errno(int error) noexcept
{
    if (error == EAGAIN || error == EWOULDBLOCK)
    {
        return ErrorCode::resource_exhausted;
    }
    switch (error)
    {
    case EADDRINUSE:
        return ErrorCode::address_in_use;
    case EADDRNOTAVAIL:
        return ErrorCode::address_unavailable;
    case EINVAL:
    case EBADF:
    case EFAULT:
        return ErrorCode::invalid_argument;
    case ENOMEM:
    case EMFILE:
    case ENFILE:
    case ENOBUFS:
        return ErrorCode::resource_exhausted;
    case ENETUNREACH:
    case EHOSTUNREACH:
    case ENETDOWN:
        return ErrorCode::unreachable;
    case EMSGSIZE:
        return ErrorCode::message_too_large;
    case EACCES:
    case EPERM:
        return ErrorCode::permission_denied;
    case EINTR:
        return ErrorCode::interrupted;
    case ETIMEDOUT:
        return ErrorCode::timeout;
    case ECONNRESET:
    case EPIPE:
        return ErrorCode::closed;
    case ENOSYS:
    case EOPNOTSUPP:
    case ENOPROTOOPT:
        return ErrorCode::unsupported;
    default:
        return ErrorCode::other;
    }
}

[[nodiscard]] std::unexpected<Error> fail_errno(int error) noexcept
{
    return fail(map_errno(error), error);
}

// Network byte order without htons, which lwIP makes a macro.
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
    address.sin_len = sizeof address;
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

// Milliseconds for lwip_poll: rounded up so that a wait never ends early, capped at what an
// int holds, -1 for `forever`.
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

// lwIP binds a socket to any address it is given, even one that no interface has. The
// interface says that is an error, so the address is looked for among the interfaces'.
[[nodiscard]] bool is_local_address(const Ipv4Address& address) noexcept
{
    if (address.is_any())
    {
        return true;
    }
    ip4_addr_t wanted{};
    std::memcpy(&wanted, address.octets.data(), 4);
    bool found = false;
    struct netif* interface = nullptr;
    LOCK_TCPIP_CORE();
    NETIF_FOREACH(interface)
    {
        if (netif_is_up(interface) && ip4_addr_eq(netif_ip4_addr(interface), &wanted))
        {
            found = true;
            break;
        }
    }
    UNLOCK_TCPIP_CORE();
    return found;
}

class FreeRtosUdpSocket final : public UdpSocket
{
public:
    FreeRtosUdpSocket(int fd, Endpoint local, bool allow_broadcast, Clock& clock) noexcept
        : fd_(fd), local_(local), allow_broadcast_(allow_broadcast), clock_(clock)
    {
    }
    ~FreeRtosUdpSocket() override
    {
        (void)lwip_close(fd_);
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
        // Whether a stack refuses a broadcast from a socket that did not ask is up to the
        // stack and its configuration; the interface says it is refused, so it is, here.
        if (destination.address.is_broadcast() && !allow_broadcast_)
        {
            return fail(ErrorCode::permission_denied);
        }
        const sockaddr_in address = to_sockaddr(destination);
        for (int attempt = 0; attempt < 2; ++attempt)
        {
            const ssize_t sent = lwip_sendto(fd_, data.data(), data.size(), 0,
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
            if (error == EAGAIN || error == EWOULDBLOCK || error == ENOMEM || error == ENOBUFS)
            {
                // No buffer for it at the moment: wait a moment for one, once.
                clock_.sleep_for(std::chrono::milliseconds(20));
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
            // The call does not block, so a readiness report that turns out to be nothing
            // cannot hang the caller past its time-out. recvmsg reports MSG_TRUNC for a
            // datagram that did not fit, which is how a cut one is told from one that was
            // just the size of the buffer.
            sockaddr_in from{};
            iovec chunk{buffer.data(), buffer.size()};
            msghdr message{};
            message.msg_name = &from;
            message.msg_namelen = sizeof from;
            message.msg_iov = &chunk;
            message.msg_iovlen = 1;
            const ssize_t got = lwip_recvmsg(fd_, &message, MSG_DONTWAIT);
            if (got >= 0)
            {
                Datagram datagram;
                // For a datagram that did not fit, lwIP returns the datagram's length and not
                // the bytes it kept.
                datagram.size = std::min(static_cast<std::size_t>(got), buffer.size());
                datagram.source = to_endpoint(from);
                datagram.truncated = (message.msg_flags & MSG_TRUNC) != 0;
                return datagram;
            }
            const int error = errno;
            if (error != EAGAIN && error != EWOULDBLOCK && error != EINTR)
            {
                return fail_errno(error);
            }
            const Duration remaining = deadline - clock_.now();
            if (remaining <= Duration::zero() && timeout != forever)
            {
                return fail(ErrorCode::timeout);
            }
            pollfd waiting{fd_, POLLIN, 0};
            (void)lwip_poll(&waiting, 1, to_poll_milliseconds(timeout == forever ? forever : remaining));
        }
    }

private:
    int fd_;
    Endpoint local_;
    bool allow_broadcast_;
    Clock& clock_;
};

class FreeRtosNetwork final : public Network
{
public:
    explicit FreeRtosNetwork(Clock& clock) noexcept : clock_(clock) {}

    Result<Owned<UdpSocket>> open_udp(const UdpOptions& options) noexcept override
    {
        if (!is_local_address(options.local.address))
        {
            return fail(ErrorCode::address_unavailable);
        }
        const int fd = lwip_socket(AF_INET, SOCK_DGRAM, 0);
        if (fd < 0)
        {
            return fail_errno(errno);
        }
        if (const int flags = lwip_fcntl(fd, F_GETFL, 0); flags < 0 || lwip_fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        {
            const int error = errno;
            (void)lwip_close(fd);
            return fail_errno(error);
        }
        const int on = 1;
        if (options.reuse_address)
        {
            if (lwip_setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on) != 0)
            {
                const int error = errno;
                (void)lwip_close(fd);
                return fail_errno(error);
            }
        }
        if (options.allow_broadcast)
        {
            (void)lwip_setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &on, sizeof on);
        }
        if (options.receive_buffer_bytes != 0)
        {
            const int size = options.receive_buffer_bytes > static_cast<std::size_t>(INT_MAX)
                                 ? INT_MAX
                                 : static_cast<int>(options.receive_buffer_bytes);
            (void)lwip_setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof size);
        }
        const sockaddr_in address = to_sockaddr(options.local);
        if (lwip_bind(fd, reinterpret_cast<const sockaddr*>(&address), sizeof address) != 0)
        {
            const int error = errno;
            (void)lwip_close(fd);
            return fail_errno(error);
        }
        sockaddr_in bound{};
        socklen_t length = sizeof bound;
        if (lwip_getsockname(fd, reinterpret_cast<sockaddr*>(&bound), &length) != 0)
        {
            const int error = errno;
            (void)lwip_close(fd);
            return fail_errno(error);
        }
        Endpoint local = options.local;
        local.port = from_network(bound.sin_port);

        auto socket = make_owned_nothrow<UdpSocket, FreeRtosUdpSocket>(fd, local, options.allow_broadcast, clock_);
        if (!socket)
        {
            (void)lwip_close(fd);
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
        std::array<pollfd, max_wait_sockets> set{};
        for (std::size_t i = 0; i < sockets.size(); ++i)
        {
            set[i] = pollfd{static_cast<FreeRtosUdpSocket*>(sockets[i])->fd(), POLLIN, 0};
        }
        for (;;)
        {
            const Duration remaining = timeout == forever ? forever : deadline - clock_.now();
            const int result =
                lwip_poll(set.data(), static_cast<nfds_t>(sockets.size()), to_poll_milliseconds(remaining));
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
                ready[i] = (set[i].revents & (POLLIN | POLLERR | POLLHUP)) != 0;
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
    return detail::make_unique_nothrow<FreeRtosNetwork>(clock);
}

} // namespace adl::platform::freertos

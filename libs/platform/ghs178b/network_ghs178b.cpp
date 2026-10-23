// INTEGRITY-178B (GHS). UNBUILT AND UNTESTED: this file has never been
// compiled against the INTEGRITY-178B headers; see docs/platform-ghs.md.
//
// UDP over ipflite: socket/bind/sendto/recvfrom/select/socket_close, with
// the socket set non-blocking once (ipflite_ioctl FIONBIO) so that
// receive_from can wait on the one descriptor with select() and a deadline.
// The deadline is read from the primary clock, the same timer driver the
// host's Clock reads.
//
// Assumptions named: socket descriptors index the calling task's own
// ipflite client table, so a socket must be used by the task that opened it
// (and that task must have initialised ipflite - see host_ghs178b.cpp);
// allow_broadcast and reuse_address are accepted but cannot be asked of the
// stack, which declares no SO_BROADCAST and no SO_REUSEADDR, so the bind
// below is the whole story; recvfrom has no MSG_TRUNC (any nonzero flags
// argument is refused), so a datagram larger than the buffer is
// indistinguishable from one that exactly filled it and `truncated` is
// always false; and the real ceiling on a datagram is the ipflite buffer
// configuration, which this file cannot see.

#include "adl/platform/network.hpp"
#include "adl/platform/time.hpp"
#include "detail/nothrow.hpp"
#include "ghs/ghs_native.hpp"
#include "ghs178b/ghs178b_services.hpp"

#include <ipflite/ipflite.h>
#include <sys/time.h>

#include <INTEGRITY.h>
#include <errno.h>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstring>
#include <memory>

namespace adl::platform::ghs178b
{
namespace
{

// 65535 bytes of UDP_MTU minus the 20-byte IP header and the 8-byte UDP
// header: the protocol's own limit. The real ceiling is the ipflite buffer
// configuration (the default send queue is UDP_MTU - 8), which the product
// sets in ipflite_clients[] and the __ghsbegin_mr_rwe_ipflite_* regions.
constexpr std::size_t max_udp_payload = 65507;

// Ports travel as big-endian integers inside sockaddr_in. The tree's
// sys/endian.h declares htons()/ntohs() (bcvtbe16(), the identity on
// big-endian PowerPC); the same result from <bit>, the way
// posix/network_posix.cpp writes it, so no macro spelling enters this file.
[[nodiscard]] constexpr std::uint16_t to_network(std::uint16_t host) noexcept
{
    return std::endian::native == std::endian::little ? std::byteswap(host) : host;
}
[[nodiscard]] constexpr std::uint16_t from_network(std::uint16_t network) noexcept
{
    return to_network(network);
}

[[nodiscard]] sockaddr_in to_sockaddr(const Endpoint& endpoint) noexcept
{
    sockaddr_in address{};
    address.sin_len = sizeof address;
    address.sin_family = AF_INET;
    address.sin_port = to_network(endpoint.port);
    // The four octets are the four address bytes; a plain copy keeps the
    // network order whatever the host's byte order is.
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

// struct timeval for `timeout`, which is positive and finite (a caller that
// wants forever passes NULL to select instead): millisecond granularity,
// rounded up so a wait never ends early, capped so tv_sec cannot overflow
// the 32-bit long of this tree's sys/time.h. The reverse conversion is not
// needed: ipflite's select does not report the time a wait had left.
[[nodiscard]] timeval to_timeval(Duration timeout) noexcept
{
    constexpr std::int64_t milliseconds_per_second = 1000;
    constexpr std::int64_t max_tv_sec = 0x7fffffff; // LONG_MAX on this target
    const std::int64_t nanos = timeout.count();
    const std::int64_t milliseconds = nanos / 1'000'000 + (nanos % 1'000'000 != 0 ? 1 : 0);
    const std::int64_t seconds = std::min(milliseconds / milliseconds_per_second, max_tv_sec);
    timeval value{};
    value.tv_sec = static_cast<long>(seconds);
    if (seconds < max_tv_sec)
    {
        value.tv_usec = static_cast<long>((milliseconds % milliseconds_per_second) * 1000);
    }
    return value;
}

[[nodiscard]] bool read_primary_time(Time& time) noexcept
{
    return PrimaryClock != NULLClock && GetClockTime(PrimaryClock, &time) == Success;
}

// One select() on one descriptor: -1 (errno set), 0 (time-out), 1 (ready).
[[nodiscard]] int wait_for_one(int fd, timeval* timeout) noexcept
{
    fd_set set;
    FD_ZERO(&set);
    FD_SET(fd, &set);
    // The third fd_set is the error set (ipflite names the parameter
    // "empty_set"); it stays NULL, as does the write set.
    return select(fd + 1, &set, NULL, NULL, timeout);
}

class Ghs178bUdpSocket final : public UdpSocket
{
public:
    Ghs178bUdpSocket(int fd, Endpoint local) noexcept : fd_(fd), local_(local) {}

    // socket_close is ipflite's spelling of close for these sockets.
    ~Ghs178bUdpSocket() override
    {
        (void)socket_close(fd_);
    }

    [[nodiscard]] int fd() const noexcept
    {
        return fd_;
    }

    Endpoint local_endpoint() const noexcept override
    {
        sockaddr_in bound{};
        socklen_t length = sizeof bound;
        if (getsockname(fd_, reinterpret_cast<sockaddr*>(&bound), &length) == 0)
        {
            return to_endpoint(bound);
        }
        // The calling task never initialised ipflite (see the file comment):
        // the value captured at open, which this very call produced.
        return local_;
    }

    Result<void> send_to(const Endpoint& destination, std::span<const std::byte> data) noexcept override
    {
        if (data.size() > max_udp_payload)
        {
            return fail(ErrorCode::message_too_large);
        }
        const sockaddr_in address = to_sockaddr(destination);
        for (;;)
        {
            // flags 0 and tolen sizeof(struct sockaddr_in): the server
            // refuses anything else with EINVAL.
            const ssize_t sent =
                sendto(fd_, data.data(), data.size(), 0, reinterpret_cast<const sockaddr*>(&address), sizeof address);
            if (sent >= 0)
            {
                return {};
            }
            const int error = errno;
            if (error == EINTR)
            {
                continue;
            }
            // EAGAIN (the send queue is full) maps to resource_exhausted.
            return ghs::fail_errno(error);
        }
    }

    Result<Datagram> receive_from(std::span<std::byte> buffer, Duration timeout) noexcept override
    {
        Instant deadline{};
        bool have_deadline = false;
        if (timeout != forever)
        {
            Time now{};
            if (read_primary_time(now))
            {
                deadline = Instant{ghs::to_duration(now)} + timeout;
                have_deadline = true;
            }
        }
        for (;;)
        {
            sockaddr_in from{};
            socklen_t from_length = sizeof from;
            // recvfrom copies min(buffer, datagram) and has no MSG_TRUNC (a
            // nonzero flags argument is refused), so a datagram larger than
            // the buffer is indistinguishable from one that exactly filled
            // it: `truncated` is always false, and an oversized TFTP block
            // is caught by the protocol's own size checks instead.
            const ssize_t got =
                recvfrom(fd_, buffer.data(), buffer.size(), 0, reinterpret_cast<sockaddr*>(&from), &from_length);
            if (got >= 0)
            {
                Datagram datagram;
                datagram.size = static_cast<std::size_t>(got);
                datagram.source = to_endpoint(from);
                datagram.truncated = false;
                return datagram;
            }
            const int error = errno;
            if (error != EAGAIN && error != EWOULDBLOCK && error != EINTR)
            {
                return ghs::fail_errno(error);
            }
            if (timeout == forever)
            {
                const int ready = wait_for_one(fd_, NULL);
                if (ready < 0 && errno != EINTR)
                {
                    return ghs::fail_errno(errno);
                }
                continue; // ready, or the wait was interrupted: try again
            }
            if (have_deadline)
            {
                Time now{};
                Instant current = deadline; // an unreadable clock counts as expired
                if (read_primary_time(now))
                {
                    current = Instant{ghs::to_duration(now)};
                }
                if (current >= deadline)
                {
                    return fail(ErrorCode::timeout);
                }
                timeval wait = to_timeval(deadline - current);
                const int ready = wait_for_one(fd_, &wait);
                if (ready < 0)
                {
                    if (errno != EINTR)
                    {
                        return ghs::fail_errno(errno);
                    }
                }
                else if (ready == 0)
                {
                    return fail(ErrorCode::timeout);
                }
                continue; // ready, or the wait ended early: re-measure and loop
            }
            // No readable clock: one select() of the full time-out is the
            // whole wait, and anything short of a ready socket ends it.
            timeval wait = to_timeval(timeout);
            const int ready = wait_for_one(fd_, &wait);
            if (ready < 0 && errno != EINTR)
            {
                return ghs::fail_errno(errno);
            }
            if (ready <= 0)
            {
                return fail(ErrorCode::timeout);
            }
        }
    }

private:
    int fd_;
    Endpoint local_;
};

class Ghs178bNetwork final : public Network
{
public:
    Result<Owned<UdpSocket>> open_udp(const UdpOptions& options) noexcept override
    {
        // An Endpoint is a 16-bit port and four octets - exactly what
        // sockaddr_in holds - so nothing here can overflow, and there is
        // nothing to validate. ipflite wants protocol 0, and AF_INET +
        // SOCK_DGRAM is all it has.
        const int fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd < 0)
        {
            return ghs::fail_errno(errno);
        }
        if (options.receive_buffer_bytes != 0)
        {
            // The default receive queue is UDP_MTU - 8; a caller that sizes
            // it differently asks for SO_RCVBUF, the one size option the
            // header declares. A refusal is ignored: the option is a request,
            // not a promise, and the socket works with the default.
            const int bytes = static_cast<int>(options.receive_buffer_bytes);
            (void)setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &bytes, sizeof bytes);
        }
        // ipflite.h declares no SO_BROADCAST and no SO_REUSEADDR, so
        // allow_broadcast and reuse_address are accepted but cannot be asked
        // of the stack: a broadcast leaves by the bound interface as the
        // stack sees fit, and a second bind of one endpoint fails or not by
        // the stack's own rule. The bind below is the whole story.
        const sockaddr_in address = to_sockaddr(options.local);
        if (bind(fd, reinterpret_cast<const sockaddr*>(&address), sizeof address) != 0)
        {
            const int error = errno;
            (void)socket_close(fd);
            return ghs::fail_errno(error);
        }
        // Non-blocking, once, right after the bind: the receive loop above
        // depends on recvfrom answering EWOULDBLOCK instead of blocking.
        // FIONBIO accepts a 1-, 2- or 4-byte argument, read by its size.
        int one = 1;
        if (ipflite_ioctl(fd, FIONBIO, &one, sizeof one) != 0)
        {
            const int error = errno;
            (void)socket_close(fd);
            return ghs::fail_errno(error);
        }
        sockaddr_in bound{};
        socklen_t length = sizeof bound;
        if (getsockname(fd, reinterpret_cast<sockaddr*>(&bound), &length) != 0)
        {
            const int error = errno;
            (void)socket_close(fd);
            return ghs::fail_errno(error);
        }
        Endpoint local = options.local;
        local.port = from_network(bound.sin_port);
        auto socket = make_owned_nothrow<UdpSocket, Ghs178bUdpSocket>(fd, local);
        if (!socket)
        {
            (void)socket_close(fd);
            return fail(ErrorCode::resource_exhausted);
        }
        return socket;
    }

    Result<std::size_t> wait_readable(std::span<UdpSocket* const> sockets, Duration timeout,
                                      std::span<bool> ready) noexcept override
    {
        if (sockets.empty() || sockets.size() > max_wait_sockets || ready.size() != sockets.size())
        {
            return fail(ErrorCode::invalid_argument);
        }
        fd_set set;
        FD_ZERO(&set);
        int highest = 0;
        for (UdpSocket* const socket : sockets)
        {
            // The interface promises these sockets were opened by this
            // Network, so the descriptor is one of its own.
            const int fd = static_cast<Ghs178bUdpSocket*>(socket)->fd();
            FD_SET(fd, &set);
            if (fd > highest)
            {
                highest = fd;
            }
        }
        timeval wait{};
        timeval* waiting = nullptr;
        if (timeout != forever)
        {
            wait = to_timeval(timeout);
            waiting = &wait;
        }
        const int result = select(highest + 1, &set, NULL, NULL, waiting);
        if (result < 0)
        {
            return ghs::fail_errno(errno); // EINTR maps to interrupted
        }
        std::size_t count = 0;
        for (std::size_t i = 0; i < sockets.size(); ++i)
        {
            const int fd = static_cast<Ghs178bUdpSocket*>(sockets[i])->fd();
            ready[i] = FD_ISSET(fd, &set) != 0;
            count += ready[i] ? 1U : 0U;
        }
        return count;
    }

    std::size_t max_datagram_bytes() const noexcept override
    {
        return max_udp_payload;
    }
};

} // namespace

std::unique_ptr<Network> make_network() noexcept
{
    return detail::make_unique_nothrow<Ghs178bNetwork>();
}

} // namespace adl::platform::ghs178b

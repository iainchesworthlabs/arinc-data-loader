#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include <array>
#include <bit>
#include <cerrno>
#include <climits>
#include <cstring>
#include <memory>

#include "adl/platform/network.hpp"
#include "detail/nothrow.hpp"
#include "posix/posix_os.hpp"

namespace adl::platform::posix {
namespace {

using posix_os::fail_errno;

constexpr std::size_t max_udp_payload = 65507;

// Network byte order without htons, whose macro form some C libraries write with
// C-style casts that the warning set refuses.
[[nodiscard]] constexpr std::uint16_t to_network(std::uint16_t host) noexcept {
    return std::endian::native == std::endian::little ? std::byteswap(host) : host;
}
[[nodiscard]] constexpr std::uint16_t from_network(std::uint16_t net) noexcept {
    return to_network(net);
}

[[nodiscard]] sockaddr_in to_sockaddr(const Endpoint& endpoint) noexcept {
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = to_network(endpoint.port);
    std::memcpy(&address.sin_addr, endpoint.address.octets.data(), 4);
    return address;
}

[[nodiscard]] Endpoint to_endpoint(const sockaddr_in& address) noexcept {
    Endpoint endpoint;
    std::memcpy(endpoint.address.octets.data(), &address.sin_addr, 4);
    endpoint.port = from_network(address.sin_port);
    return endpoint;
}

class PosixUdpSocket final : public UdpSocket {
public:
    PosixUdpSocket(int fd, Endpoint local, Clock& clock) noexcept
        : fd_(fd), local_(local), clock_(clock) {}
    ~PosixUdpSocket() override { ::close(fd_); }

    [[nodiscard]] int fd() const noexcept { return fd_; }

    Endpoint local_endpoint() const noexcept override { return local_; }

    Result<void> send_to(const Endpoint& destination,
                         std::span<const std::byte> data) noexcept override {
        if (data.size() > max_udp_payload) {
            return fail(ErrorCode::message_too_large);
        }
        const sockaddr_in address = to_sockaddr(destination);
        for (int attempt = 0; attempt < 2; ++attempt) {
            const ssize_t sent =
                ::sendto(fd_, data.data(), data.size(), 0,
                         reinterpret_cast<const sockaddr*>(&address), sizeof address);
            if (sent >= 0) {
                return {};
            }
            if (errno == EINTR) {
                --attempt;
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // The send buffer is full: wait a moment for room, once.
                pollfd waiting{fd_, POLLOUT, 0};
                ::poll(&waiting, 1, 100);
                continue;
            }
            return fail_errno(errno);
        }
        return fail(ErrorCode::resource_exhausted);
    }

    Result<Datagram> receive_from(std::span<std::byte> buffer, Duration timeout) noexcept override {
        const Instant deadline = clock_.now() + timeout;
        for (;;) {
            // The socket does not block, so a readiness report that turns out to
            // be nothing (Linux drops a datagram with a bad checksum after poll
            // has said readable) cannot hang the caller past its time-out.
            sockaddr_in from{};
            iovec chunk{buffer.data(), buffer.size()};
            msghdr message{};
            message.msg_name = &from;
            message.msg_namelen = sizeof from;
            message.msg_iov = &chunk;
            message.msg_iovlen = 1;
            const ssize_t got = ::recvmsg(fd_, &message, 0);
            if (got >= 0) {
                Datagram datagram;
                datagram.size = static_cast<std::size_t>(got);
                datagram.source = to_endpoint(from);
                datagram.truncated = (message.msg_flags & MSG_TRUNC) != 0;
                return datagram;
            }
            const int error = errno;
            if (error != EAGAIN && error != EWOULDBLOCK && error != EINTR &&
                error != ECONNREFUSED) {
                return fail_errno(error);
            }
            const Duration remaining = deadline - clock_.now();
            if (remaining <= Duration::zero() && timeout != forever) {
                return fail(ErrorCode::timeout);
            }
            pollfd waiting{fd_, POLLIN, 0};
            ::poll(&waiting, 1,
                   posix_os::to_poll_milliseconds(timeout == forever ? forever : remaining));
        }
    }

private:
    int fd_;
    Endpoint local_;
    Clock& clock_;
};

class PosixNetwork final : public Network {
public:
    explicit PosixNetwork(Clock& clock) noexcept : clock_(clock) {}

    Result<Owned<UdpSocket>> open_udp(const UdpOptions& options) noexcept override {
        const int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (fd < 0) {
            return fail_errno(errno);
        }
        ::fcntl(fd, F_SETFD, FD_CLOEXEC);
        const int flags = ::fcntl(fd, F_GETFL, 0);
        if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
            const int error = errno;
            ::close(fd);
            return fail_errno(error);
        }
        const int on = 1;
        if (options.reuse_address) {
            // BSD systems (macOS) need SO_REUSEPORT as well to let two UDP sockets bind
            // one endpoint; Linux accepts either.
            ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
            ::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof on);
        }
        // macOS limits a UDP datagram to its send buffer, which starts at 9216 bytes
        // (net.inet.udp.maxdgram), and TFTP blocks go to 65464. Room for the largest
        // datagram costs nothing on the other systems.
        const int send_buffer = static_cast<int>(max_udp_payload) * 2;
        ::setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &send_buffer, sizeof send_buffer);
        if (options.allow_broadcast &&
            ::setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &on, sizeof on) != 0) {
            const int error = errno;
            ::close(fd);
            return fail_errno(error);
        }
        if (options.receive_buffer_bytes != 0) {
            const int size = options.receive_buffer_bytes > static_cast<std::size_t>(INT_MAX)
                                 ? INT_MAX
                                 : static_cast<int>(options.receive_buffer_bytes);
            ::setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof size);
        }
        const sockaddr_in address = to_sockaddr(options.local);
        if (::bind(fd, reinterpret_cast<const sockaddr*>(&address), sizeof address) != 0) {
            const int error = errno;
            ::close(fd);
            return fail_errno(error);
        }
        sockaddr_in bound{};
        socklen_t length = sizeof bound;
        if (::getsockname(fd, reinterpret_cast<sockaddr*>(&bound), &length) != 0) {
            const int error = errno;
            ::close(fd);
            return fail_errno(error);
        }
        Endpoint local = options.local;
        local.port = from_network(bound.sin_port);

        auto socket = make_owned_nothrow<UdpSocket, PosixUdpSocket>(fd, local, clock_);
        if (!socket) {
            ::close(fd);
            return fail(ErrorCode::resource_exhausted);
        }
        return socket;
    }

    Result<std::size_t> wait_readable(std::span<UdpSocket* const> sockets, Duration timeout,
                                      std::span<bool> ready) noexcept override {
        if (sockets.size() > max_wait_sockets || ready.size() != sockets.size()) {
            return fail(ErrorCode::invalid_argument);
        }
        const Instant deadline = clock_.now() + timeout;
        std::array<pollfd, max_wait_sockets> set{};
        for (std::size_t i = 0; i < sockets.size(); ++i) {
            set[i] = pollfd{static_cast<PosixUdpSocket*>(sockets[i])->fd(), POLLIN, 0};
        }
        for (;;) {
            const Duration remaining = timeout == forever ? forever : deadline - clock_.now();
            const int result = ::poll(set.data(), static_cast<nfds_t>(sockets.size()),
                                      posix_os::to_poll_milliseconds(remaining));
            if (result < 0) {
                if (errno == EINTR) {
                    continue;
                }
                return fail_errno(errno);
            }
            std::size_t count = 0;
            for (std::size_t i = 0; i < sockets.size(); ++i) {
                ready[i] = (set[i].revents & (POLLIN | POLLERR | POLLHUP)) != 0;
                count += ready[i] ? 1U : 0U;
            }
            if (count != 0 || (timeout != forever && deadline - clock_.now() <= Duration::zero())) {
                return count;
            }
        }
    }

    std::size_t max_datagram_bytes() const noexcept override { return max_udp_payload; }

private:
    Clock& clock_;
};

}  // namespace

std::unique_ptr<Network> make_network(Clock& clock) noexcept {
    return detail::make_unique_nothrow<PosixNetwork>(clock);
}

}  // namespace adl::platform::posix

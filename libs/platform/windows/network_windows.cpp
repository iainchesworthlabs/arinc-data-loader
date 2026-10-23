// SPDX-License-Identifier: GPL-3.0-or-later
// The Windows SDK headers depend on their order (winsock2.h before windows.h, both before mswsock.h), so
// clang-format leaves them as they are.
// clang-format off
#include <winsock2.h>
#include <ws2tcpip.h>

#include <windows.h>

#include <mswsock.h>

#include <array>
#include <bit>
#include <cstring>
#include <memory>

#include "adl/platform/network.hpp"
#include "detail/nothrow.hpp"
#include "windows/windows_services.hpp"
// clang-format on

namespace adl::platform::windows
{
namespace
{

constexpr std::size_t max_udp_payload = 65507;

[[nodiscard]] constexpr std::uint16_t to_network(std::uint16_t host) noexcept
{
    return std::endian::native == std::endian::little ? std::byteswap(host) : host;
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
    endpoint.port = to_network(address.sin_port);
    return endpoint;
}

// Winsock is started once per Network and stopped with it.
class WindowsUdpSocket final : public UdpSocket
{
public:
    WindowsUdpSocket(SOCKET handle, Endpoint local, Clock& clock) noexcept
        : handle_(handle), local_(local), clock_(clock)
    {
    }
    ~WindowsUdpSocket() override
    {
        ::closesocket(handle_);
    }
    WindowsUdpSocket(const WindowsUdpSocket&) = delete;
    WindowsUdpSocket& operator=(const WindowsUdpSocket&) = delete;
    WindowsUdpSocket(WindowsUdpSocket&&) = delete;
    WindowsUdpSocket& operator=(WindowsUdpSocket&&) = delete;

    [[nodiscard]] SOCKET handle() const noexcept
    {
        return handle_;
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
        const sockaddr_in address = to_sockaddr(destination);
        for (int attempt = 0; attempt < 2; ++attempt)
        {
            const int sent =
                ::sendto(handle_, reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()), 0,
                         reinterpret_cast<const sockaddr*>(&address), sizeof address);
            if (sent != SOCKET_ERROR)
            {
                return {};
            }
            const int error = ::WSAGetLastError();
            if (error == WSAEWOULDBLOCK)
            {
                // The send buffer is full: wait a moment for room, once.
                WSAPOLLFD waiting{handle_, POLLWRNORM, 0};
                ::WSAPoll(&waiting, 1, 100);
                continue;
            }
            return fail_winsock(error);
        }
        return fail(ErrorCode::resource_exhausted);
    }

    Result<Datagram> receive_from(std::span<std::byte> buffer, Duration timeout) noexcept override
    {
        const Instant deadline = clock_.now() + timeout;
        for (;;)
        {
            sockaddr_in from{};
            int from_length = sizeof from;
            const int got = ::recvfrom(handle_, reinterpret_cast<char*>(buffer.data()),
                                       static_cast<int>(buffer.size() > 0x7fffffff ? 0x7fffffff : buffer.size()), 0,
                                       reinterpret_cast<sockaddr*>(&from), &from_length);
            if (got != SOCKET_ERROR)
            {
                Datagram datagram;
                datagram.size = static_cast<std::size_t>(got);
                datagram.source = to_endpoint(from);
                return datagram;
            }
            const int error = ::WSAGetLastError();
            if (error == WSAEMSGSIZE)
            {
                // The datagram was bigger than the buffer: Windows keeps what fits.
                Datagram datagram;
                datagram.size = buffer.size();
                datagram.source = to_endpoint(from);
                datagram.truncated = true;
                return datagram;
            }
            // WSAECONNRESET: an earlier send reached a closed port and the ICMP answer
            // is being reported on this socket. It is not a datagram; go on waiting.
            if (error != WSAEWOULDBLOCK && error != WSAECONNRESET && error != WSAEINTR)
            {
                return fail_winsock(error);
            }
            const Duration remaining = deadline - clock_.now();
            if (remaining <= Duration::zero() && timeout != forever)
            {
                return fail(ErrorCode::timeout);
            }
            WSAPOLLFD waiting{handle_, POLLRDNORM, 0};
            ::WSAPoll(&waiting, 1,
                      static_cast<INT>(timeout == forever ? INFINITE : to_milliseconds(remaining, 0x7fffffff)));
        }
    }

private:
    SOCKET handle_;
    Endpoint local_;
    Clock& clock_;
};

class WindowsNetwork final : public Network
{
public:
    explicit WindowsNetwork(Clock& clock) noexcept : clock_(clock) {}
    ~WindowsNetwork() override
    {
        if (started_)
        {
            ::WSACleanup();
        }
    }
    WindowsNetwork(const WindowsNetwork&) = delete;
    WindowsNetwork& operator=(const WindowsNetwork&) = delete;
    WindowsNetwork(WindowsNetwork&&) = delete;
    WindowsNetwork& operator=(WindowsNetwork&&) = delete;

    [[nodiscard]] Result<void> start() noexcept
    {
        WSADATA data{};
        if (const int error = ::WSAStartup(MAKEWORD(2, 2), &data); error != 0)
        {
            return fail_winsock(error);
        }
        started_ = true;
        return {};
    }

    Result<Owned<UdpSocket>> open_udp(const UdpOptions& options) noexcept override
    {
        const SOCKET handle = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (handle == INVALID_SOCKET)
        {
            return fail_winsock(::WSAGetLastError());
        }
        const auto close_and_fail = [handle](int error)
        {
            ::closesocket(handle);
            return fail_winsock(error);
        };
        u_long nonblocking = 1;
        // FIONBIO's _IOW expansion casts the size of u_long to a signed long;
        // the warning is the SDK macro's, not this call's.
        // NOLINTNEXTLINE(clang-diagnostic-sign-conversion)
        if (::ioctlsocket(handle, FIONBIO, &nonblocking) != 0)
        {
            return close_and_fail(::WSAGetLastError());
        }
        // Without this, a send to a port nobody listens on makes a later receive on the
        // same socket fail with WSAECONNRESET: ICMP "port unreachable" is not an error
        // of a datagram socket on any other platform.
        DWORD no_reset = FALSE;
        DWORD returned = 0;
        ::WSAIoctl(handle, SIO_UDP_CONNRESET, &no_reset, sizeof no_reset, nullptr, 0, &returned, nullptr, nullptr);

        const BOOL on = TRUE;
        if (options.reuse_address)
        {
            ::setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&on), sizeof on);
        }
        else
        {
            // Windows lets a second socket take over a port unless the first asks to be
            // exclusive; the other platforms do not, so ask.
            ::setsockopt(handle, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&on), sizeof on);
        }
        const int send_buffer = static_cast<int>(max_udp_payload) * 2;
        ::setsockopt(handle, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&send_buffer), sizeof send_buffer);
        if (options.allow_broadcast &&
            ::setsockopt(handle, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&on), sizeof on) != 0)
        {
            return close_and_fail(::WSAGetLastError());
        }
        if (options.receive_buffer_bytes != 0)
        {
            const int size = options.receive_buffer_bytes > 0x7fffffffu
                                 ? 0x7fffffff
                                 : static_cast<int>(options.receive_buffer_bytes);
            ::setsockopt(handle, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&size), sizeof size);
        }
        const sockaddr_in address = to_sockaddr(options.local);
        if (::bind(handle, reinterpret_cast<const sockaddr*>(&address), sizeof address) != 0)
        {
            return close_and_fail(::WSAGetLastError());
        }
        sockaddr_in bound{};
        int length = sizeof bound;
        if (::getsockname(handle, reinterpret_cast<sockaddr*>(&bound), &length) != 0)
        {
            return close_and_fail(::WSAGetLastError());
        }
        Endpoint local = options.local;
        local.port = to_network(bound.sin_port);

        auto socket = make_owned_nothrow<UdpSocket, WindowsUdpSocket>(handle, local, clock_);
        if (!socket)
        {
            ::closesocket(handle);
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
        if (sockets.empty())
        {
            clock_.sleep_for(timeout == forever ? Duration::zero() : timeout);
            return std::size_t{0};
        }
        const Instant deadline = clock_.now() + timeout;
        std::array<WSAPOLLFD, max_wait_sockets> set{};
        for (std::size_t i = 0; i < sockets.size(); ++i)
        {
            set[i] = WSAPOLLFD{static_cast<WindowsUdpSocket*>(sockets[i])->handle(), POLLRDNORM, 0};
        }
        for (;;)
        {
            const Duration remaining = timeout == forever ? forever : deadline - clock_.now();
            const INT ms = static_cast<INT>(remaining == forever ? INFINITE : to_milliseconds(remaining, 0x7fffffff));
            const int result = ::WSAPoll(set.data(), static_cast<ULONG>(sockets.size()), ms);
            if (result == SOCKET_ERROR)
            {
                return fail_winsock(::WSAGetLastError());
            }
            std::size_t count = 0;
            for (std::size_t i = 0; i < sockets.size(); ++i)
            {
                ready[i] = (set[i].revents & (POLLRDNORM | POLLERR | POLLHUP)) != 0;
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
    bool started_ = false;
};

} // namespace

Result<std::unique_ptr<Network>> make_network(Clock& clock) noexcept
{
    auto network = detail::make_unique_nothrow<WindowsNetwork>(clock);
    if (!network)
    {
        return fail(ErrorCode::resource_exhausted);
    }
    if (auto started = network->start(); !started)
    {
        return std::unexpected(started.error());
    }
    return std::unique_ptr<Network>(std::move(network));
}

} // namespace adl::platform::windows

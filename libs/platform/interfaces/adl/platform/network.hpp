// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Networking: IPv4 UDP datagrams, which is all 615A uses. TFTP (RFC 1350) runs
// on UDP, with the loader as client for the target's initialisation files and
// as server for the files the target asks it for (port 59, or the port a
// "port" option names), and FIND (Attachment 3) is a broadcast request and
// unicast answers on port 1001.
//
// What that needs from a platform, and so all this interface offers:
//   - a socket bound to a chosen local port (59, 1001, an ephemeral one for each
//     transfer, one of 49152 and up for a "port" option);
//   - send to an endpoint, including the limited broadcast address;
//   - receive with a time limit, telling who sent it;
//   - wait on several sockets at once, so that one thread runs the control
//     port and every transfer in flight.
// It does not offer TCP, IPv6, multicast group membership, interface
// enumeration or name resolution. AFDX (Appendix E) has no broadcast, so a
// FIND over it is a series of unicast sends, which this already does.

#include "adl/platform/error.hpp"
#include "adl/platform/owned.hpp"
#include "adl/platform/time.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

namespace adl::platform
{

struct Ipv4Address
{
    std::array<std::uint8_t, 4> octets{};

    [[nodiscard]] static constexpr Ipv4Address any() noexcept
    {
        return {};
    }
    [[nodiscard]] static constexpr Ipv4Address loopback() noexcept
    {
        return Ipv4Address{{127, 0, 0, 1}};
    }
    // The limited broadcast address, 255.255.255.255.
    [[nodiscard]] static constexpr Ipv4Address broadcast() noexcept
    {
        return Ipv4Address{{255, 255, 255, 255}};
    }

    // "a.b.c.d", each part decimal 0 to 255 with no sign, blank or leading
    // zero beyond a single "0" (so "010" is refused rather than read as octal).
    [[nodiscard]] static constexpr std::optional<Ipv4Address> parse(std::string_view text) noexcept
    {
        Ipv4Address result;
        std::size_t index = 0;
        std::size_t position = 0;
        while (index < 4)
        {
            std::size_t digits = 0;
            unsigned value = 0;
            while (position < text.size() && text[position] >= '0' && text[position] <= '9')
            {
                value = value * 10 + static_cast<unsigned>(text[position] - '0');
                ++digits;
                ++position;
                if (digits > 3 || value > 255)
                {
                    return std::nullopt;
                }
            }
            if (digits == 0 || (digits > 1 && text[position - digits] == '0'))
            {
                return std::nullopt;
            }
            result.octets[index++] = static_cast<std::uint8_t>(value);
            if (index < 4)
            {
                if (position >= text.size() || text[position] != '.')
                {
                    return std::nullopt;
                }
                ++position;
            }
        }
        if (position != text.size())
        {
            return std::nullopt;
        }
        return result;
    }

    friend constexpr bool operator==(const Ipv4Address&, const Ipv4Address&) noexcept = default;
    [[nodiscard]] constexpr bool is_any() const noexcept
    {
        return *this == any();
    }
    [[nodiscard]] constexpr bool is_broadcast() const noexcept
    {
        return *this == broadcast();
    }
};

// Writes "a.b.c.d" and a terminating NUL into `out` (16 bytes always suffice)
// and returns the length without the NUL.
constexpr std::size_t format(Ipv4Address address, std::span<char, 16> out) noexcept
{
    std::size_t length = 0;
    for (std::size_t i = 0; i < 4; ++i)
    {
        const unsigned value = address.octets[i];
        if (value >= 100)
        {
            out[length++] = static_cast<char>('0' + value / 100);
        }
        if (value >= 10)
        {
            out[length++] = static_cast<char>('0' + (value / 10) % 10);
        }
        out[length++] = static_cast<char>('0' + value % 10);
        if (i < 3)
        {
            out[length++] = '.';
        }
    }
    out[length] = '\0';
    return length;
}

struct Endpoint
{
    Ipv4Address address;
    std::uint16_t port = 0;

    friend constexpr bool operator==(const Endpoint&, const Endpoint&) noexcept = default;
};

struct UdpOptions
{
    // Port 0 asks for an unused one (read it back with local_endpoint()). An
    // address of 0.0.0.0 listens on every interface; a specific one chooses the
    // interface, which also chooses the one a broadcast leaves by.
    Endpoint local{};
    // Lets the socket send to a broadcast address.
    bool allow_broadcast = false;
    // Lets a socket bind a port another is using. Without it, a second bind of
    // the same endpoint fails with `address_in_use` on every platform, and no
    // other process can take the port from under this one.
    bool reuse_address = false;
    // 0 keeps the platform's default. A datagram that arrives while this much
    // is already queued is dropped, as UDP does.
    std::size_t receive_buffer_bytes = 0;
};

struct Datagram
{
    std::size_t size = 0; // bytes written to the buffer
    Endpoint source;
    // The datagram was larger than the buffer: `size` bytes were kept and the
    // rest discarded. For TFTP that is a block bigger than the negotiated size.
    bool truncated = false;
};

// The most sockets one wait_readable call takes. Fixed so that a platform can
// keep its poll set on the stack: the loader needs the control port, the FIND
// port and one socket per transfer in flight, a handful in all.
inline constexpr std::size_t max_wait_sockets = 64;

class UdpSocket
{
public:
    virtual ~UdpSocket() = default;

    // The address and port the socket is bound to. The address is the one asked
    // for (0.0.0.0 stays 0.0.0.0); the port is the real one.
    [[nodiscard]] virtual Endpoint local_endpoint() const noexcept = 0;

    // The whole datagram or an error; UDP has no partial send.
    // `message_too_large` above Network::max_datagram_bytes(); `unreachable`
    // where the platform learns at once that there is no route.
    [[nodiscard]] virtual Result<void> send_to(const Endpoint& destination,
                                               std::span<const std::byte> data) noexcept = 0;

    // Waits up to `timeout` for a datagram (`Duration::zero()` polls). `timeout`
    // is the result when none came. An ICMP "port unreachable" from an earlier
    // send is never reported here as an error on any platform: it is not a
    // datagram and the next wait goes on as if it had not happened.
    [[nodiscard]] virtual Result<Datagram> receive_from(std::span<std::byte> buffer, Duration timeout) noexcept = 0;
};

class Network
{
public:
    virtual ~Network() = default;

    [[nodiscard]] virtual Result<Owned<UdpSocket>> open_udp(const UdpOptions& options) noexcept = 0;

    // Waits until at least one of `sockets` has a datagram to receive, or
    // `timeout`. On return `ready[i]` says whether `sockets[i]` has one (the
    // spans are the same length), and the result is how many are ready: 0 is a
    // time-out, not an error. The sockets must have been opened by this Network,
    // and there are at most max_wait_sockets of them (`invalid_argument` if not).
    [[nodiscard]] virtual Result<std::size_t> wait_readable(std::span<UdpSocket* const> sockets, Duration timeout,
                                                            std::span<bool> ready) noexcept = 0;

    // The largest payload send_to accepts: 65507 bytes for IPv4 UDP. (A TFTP
    // block of the RFC 2348 maximum, 65464 bytes, plus its 4 byte header fits.)
    [[nodiscard]] virtual std::size_t max_datagram_bytes() const noexcept = 0;
};

} // namespace adl::platform

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Who a datagram is from or for: an IPv4 address and a UDP port. The engine works in these
// and the runner converts them to and from its platform's own type, which has the same
// shape.

#include <array>
#include <cstdint>

namespace adl::arinc615a
{

struct Address
{
    std::array<std::uint8_t, 4> octets{};

    friend constexpr bool operator==(const Address&, const Address&) noexcept = default;

    [[nodiscard]] static constexpr Address broadcast() noexcept
    {
        return Address{{255, 255, 255, 255}};
    }
    [[nodiscard]] static constexpr Address loopback() noexcept
    {
        return Address{{127, 0, 0, 1}};
    }
    // Anywhere in 127.0.0.0/8: an address of this machine that no packet leaves it for.
    [[nodiscard]] constexpr bool is_loopback() const noexcept
    {
        return octets[0] == 127;
    }
};

struct Endpoint
{
    Address address;
    std::uint16_t port = 0;

    friend constexpr bool operator==(const Endpoint&, const Endpoint&) noexcept = default;
};

} // namespace adl::arinc615a

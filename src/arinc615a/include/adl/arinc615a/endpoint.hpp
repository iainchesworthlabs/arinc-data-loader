#pragma once

// Who a datagram is from or for: an IPv4 address and a UDP port. The engine works in these
// and the runner converts them to and from its platform's own type, which has the same
// shape.

#include <array>
#include <cstdint>

namespace adl::arinc615a {

struct Address {
    std::array<std::uint8_t, 4> octets{};

    friend constexpr bool operator==(const Address&, const Address&) noexcept = default;

    [[nodiscard]] static constexpr Address broadcast() noexcept {
        return Address{{255, 255, 255, 255}};
    }
};

struct Endpoint {
    Address address;
    std::uint16_t port = 0;

    friend constexpr bool operator==(const Endpoint&, const Endpoint&) noexcept = default;
};

}  // namespace adl::arinc615a

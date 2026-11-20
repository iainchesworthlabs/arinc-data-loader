// SPDX-License-Identifier: GPL-3.0-or-later
// The mock target's link over the real platform's sockets, on the loopback interface: what it does when a
// socket it needs cannot be opened.

#include "adl/arinc615a/endpoint.hpp"
#include "adl/mock/link.hpp"
#include "adl/platform/host.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace
{

// Nothing else in the tests uses these (the loopback tests keep to the 30000s, adl_mock's to 31950 to 31990).
constexpr std::uint16_t busy_port = 31991;
constexpr std::uint16_t free_port = 31993;

[[nodiscard]] std::unique_ptr<adl::platform::Host> new_host()
{
    auto made = adl::platform::make_host();
    REQUIRE(made.has_value());
    return std::move(*made);
}

} // namespace

TEST_CASE("a send from a port that is in use is a lost datagram and leaves nothing to wait on", "[mock][link]")
{
    // Another program holds the port, without sharing it.
    auto other = new_host();
    adl::platform::UdpOptions held;
    held.local = adl::platform::Endpoint{adl::platform::Ipv4Address::loopback(), busy_port};
    auto holder = other->network().open_udp(held);
    REQUIRE(holder.has_value());

    adl::mock::RealTargetLink link(new_host());
    link.set_local_address(adl::arinc615a::Address::loopback());
    const std::array<std::byte, 4> payload{};
    const adl::arinc615a::Endpoint to{adl::arinc615a::Address::loopback(), 9};
    link.send(busy_port, to, payload);

    // Nothing is kept for the port, so the link neither waits on it nor reads from it.
    CHECK(link.ports().empty());
    CHECK(link.wait(10) == 0);
    adl::arinc615a::Endpoint from;
    std::array<std::byte, 16> buffer{};
    std::size_t size = 0;
    CHECK_FALSE(link.receive(busy_port, from, buffer, size));
}

TEST_CASE("a send from a port that is free opens it and keeps it", "[mock][link]")
{
    adl::mock::RealTargetLink link(new_host());
    link.set_local_address(adl::arinc615a::Address::loopback());
    const std::array<std::byte, 4> payload{};
    const adl::arinc615a::Endpoint to{adl::arinc615a::Address::loopback(), 9};
    link.send(free_port, to, payload);
    link.send(free_port, to, payload);
    CHECK(link.ports() == std::vector<std::uint16_t>{free_port});
}

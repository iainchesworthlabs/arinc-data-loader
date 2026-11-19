// SPDX-License-Identifier: GPL-3.0-or-later
// A target's address as the operator types it, and the name its protocol files go by.

#include "adl/loader_core/address.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using namespace adl::loader_core;

TEST_CASE("an address is four decimal numbers of 0 to 255", "[loader_core][address]")
{
    const auto octets = parse_ipv4("10.0.0.2");
    REQUIRE(octets.has_value());
    CHECK(*octets == Ipv4Octets{10, 0, 0, 2});
    CHECK(parse_ipv4("255.255.255.255") == Ipv4Octets{255, 255, 255, 255});
    CHECK(parse_ipv4("0.0.0.0") == Ipv4Octets{0, 0, 0, 0});
    // A leading zero is part of the number, not the mark of an octal one.
    CHECK(parse_ipv4("010.001.000.002") == Ipv4Octets{10, 1, 0, 2});
}

TEST_CASE("anything else is not an address", "[loader_core][address]")
{
    const std::vector<std::string> bad{"",
                                       "10.0.0",
                                       "10.0.0.2.3",
                                       "10.0.0.",
                                       ".0.0.2",
                                       "10..0.2",
                                       "256.0.0.1",
                                       "1000.0.0.1",
                                       "0010.0.0.1",
                                       "-1.0.0.1",
                                       "+1.0.0.1",
                                       "0x0A.0.0.1",
                                       "a.b.c.d",
                                       " 10.0.0.2",
                                       "10.0.0.2 ",
                                       "10.0.0.2\n",
                                       "10.0.0.\xD9\xA2", // an Arabic-Indic digit
                                       std::string{"10.0.0.2\0", 9},
                                       "::1"};
    for (const std::string& text : bad)
    {
        INFO("text: " << text);
        CHECK_FALSE(parse_ipv4(text).has_value());
    }
}

TEST_CASE("an address is written the way it is read", "[loader_core][address]")
{
    for (const char* text : {"10.0.0.2", "192.168.1.255", "0.0.0.0", "255.255.255.255"})
    {
        const auto octets = parse_ipv4(text);
        REQUIRE(octets.has_value());
        CHECK(format_ipv4(*octets) == text);
    }
    CHECK(format_ipv4(*parse_ipv4("010.001.000.002")) == "10.1.0.2");
}

TEST_CASE("a target's THW_ID_POS is its hardware identifier and its position", "[loader_core][address]")
{
    CHECK(thw_id_pos_of("HNPFMS", "L") == "HNPFMS_L");
    CHECK(thw_id_pos_of("HNPFMS", "") == "HNPFMS");
}

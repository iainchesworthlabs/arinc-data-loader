#include <catch2/catch_test_macros.hpp>
#include <string>

#include "adl/platform/error.hpp"
#include "adl/platform/network.hpp"
#include "adl/platform/time.hpp"

using namespace adl::platform;
using namespace std::chrono_literals;

TEST_CASE("an error is compared by its code, and keeps the system's number for the log",
          "[platform][error]") {
    const Error a{ErrorCode::not_found, 2};
    const Error b{ErrorCode::not_found, 3};
    const Error c{ErrorCode::timeout, 2};
    CHECK(a == b);
    CHECK_FALSE(a == c);
    CHECK(a.native == 2);

    Result<int> good = 5;
    Result<int> bad = fail(ErrorCode::no_space, 28);
    CHECK(good.value() == 5);
    REQUIRE_FALSE(bad.has_value());
    CHECK(bad.error().code == ErrorCode::no_space);
    CHECK(bad.error().native == 28);
}

TEST_CASE("every error code has a name", "[platform][error]") {
    for (int i = 0; i <= static_cast<int>(ErrorCode::other); ++i) {
        const std::string name = to_string(static_cast<ErrorCode>(i));
        CHECK_FALSE(name.empty());
        CHECK(name != "unknown");
    }
}

TEST_CASE("instants add and subtract durations without wrapping", "[platform][time]") {
    const Instant origin{};
    CHECK((origin + 5s).since_origin == 5s);
    CHECK(((origin + 5s) - origin) == 5s);
    CHECK(origin < origin + 1ns);
    CHECK((origin + 2s) - (origin + 7s) == -5s);

    SECTION("forever is a deadline that never arrives") {
        CHECK((origin + forever).since_origin == Duration::max());
        const Instant late{Duration::max() - 1s};
        CHECK((late + 10s).since_origin == Duration::max());
        CHECK((late + forever).since_origin == Duration::max());
    }

    SECTION("a long negative step stops at the earliest instant") {
        const Instant early{Duration::min() + 1s};
        CHECK((early + (-10s)).since_origin == Duration::min());
    }
}

TEST_CASE("the protocol's figures fit a duration", "[platform][time]") {
    // The largest WAIT (5.3.2.3.4) is 65535 seconds; the DLP time-out is 13.
    CHECK(std::chrono::seconds(65535) < forever);
    CHECK(Duration(13s) == 13'000'000'000ns);
}

TEST_CASE("an IPv4 address is read from dotted decimal and written back", "[platform][network]") {
    SECTION("good addresses") {
        const auto a = Ipv4Address::parse("192.168.1.20");
        REQUIRE(a.has_value());
        CHECK(a->octets == std::array<std::uint8_t, 4>{192, 168, 1, 20});
        CHECK(Ipv4Address::parse("0.0.0.0") == Ipv4Address::any());
        CHECK(Ipv4Address::parse("127.0.0.1") == Ipv4Address::loopback());
        CHECK(Ipv4Address::parse("255.255.255.255") == Ipv4Address::broadcast());
        CHECK(Ipv4Address::broadcast().is_broadcast());
        CHECK(Ipv4Address::any().is_any());
    }

    SECTION("bad addresses are refused") {
        for (const char* text : {"", "1.2.3", "1.2.3.4.5", "256.1.1.1", "1.2.3.256", "1..2.3",
                                 "a.b.c.d", "1.2.3.4 ", " 1.2.3.4", "01.2.3.4", "1.2.3.04",
                                 "1.2.3.-4", "1.2.3.4.", ".1.2.3", "1234.1.1.1", "1.2.3.+4"}) {
            INFO(text);
            CHECK_FALSE(Ipv4Address::parse(text).has_value());
        }
    }

    SECTION("formatting is the inverse of parsing") {
        for (const char* text :
             {"0.0.0.0", "1.2.3.4", "10.20.30.40", "100.200.255.9", "255.255.255.255"}) {
            const auto a = Ipv4Address::parse(text);
            REQUIRE(a.has_value());
            std::array<char, 16> out{};
            const std::size_t length = format(*a, out);
            CHECK(std::string(out.data(), length) == text);
            CHECK(out[length] == '\0');
        }
    }
}

TEST_CASE("an endpoint is an address and a port", "[platform][network]") {
    const Endpoint a{Ipv4Address::loopback(), 59};
    const Endpoint b{Ipv4Address::loopback(), 59};
    const Endpoint c{Ipv4Address::loopback(), 1001};
    CHECK(a == b);
    CHECK_FALSE(a == c);
    // Enough for the control port, FIND and several transfers in flight.
    CHECK(max_wait_sockets >= 8);
}

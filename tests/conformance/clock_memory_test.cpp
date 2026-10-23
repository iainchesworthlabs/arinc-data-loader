// SPDX-License-Identifier: GPL-3.0-or-later
// Conformance: the clock and the memory provider.

#include "host_under_test.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <vector>

using namespace adl::platform;
using namespace adl::test;
using namespace std::chrono_literals;

TEST_CASE("a clock never goes backwards", "[platform][conformance][clock]")
{
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    Clock& clock = under.host().clock();

    Instant previous = clock.now();
    for (int i = 0; i < 2000; ++i)
    {
        const Instant next = clock.now();
        REQUIRE(next >= previous);
        previous = next;
    }
}

TEST_CASE("sleeping takes at least the time asked and not wildly more", "[platform][conformance][clock]")
{
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    Clock& clock = under.host().clock();

    for (const Duration asked : {Duration(1ms), Duration(20ms), Duration(55ms)})
    {
        CAPTURE(asked.count());
        const Instant before = clock.now();
        clock.sleep_for(asked);
        const Duration took = clock.now() - before;
        CHECK(took >= asked);
        // A desktop may be late; a generous bound still catches a sleep that
        // ignores its argument or rounds a millisecond up to a second.
        CHECK(took < asked + 750ms);
    }
}

TEST_CASE("sleeping for nothing, or for a negative time, returns at once", "[platform][conformance][clock]")
{
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    Clock& clock = under.host().clock();

    const Instant before = clock.now();
    clock.sleep_for(Duration::zero());
    clock.sleep_for(-5s);
    clock.sleep_until(before - 1s);
    CHECK(clock.now() - before < 250ms);
}

TEST_CASE("sleep_until waits for the instant", "[platform][conformance][clock]")
{
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    Clock& clock = under.host().clock();

    const Instant target = clock.now() + 30ms;
    clock.sleep_until(target);
    CHECK(clock.now() >= target);
}

TEST_CASE("the wall clock gives a plausible date or says it cannot", "[platform][conformance][clock]")
{
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);

    const auto seconds = under.host().clock().wall_time_seconds();
    if (seconds.has_value())
    {
        // After 2020-09-13 and before 2100.
        CHECK(*seconds > 1'600'000'000);
        CHECK(*seconds < 4'102'444'800);
    }
    else
    {
        CHECK(seconds.error().code == ErrorCode::unsupported);
    }
}

TEST_CASE("memory is acquired zero-filled, aligned, writable and released", "[platform][conformance][memory]")
{
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    MemoryProvider& memory = under.host().memory();

    for (const std::size_t alignment : {std::size_t{1}, std::size_t{16}, std::size_t{64}, std::size_t{4096}})
    {
        for (const std::size_t bytes : {std::size_t{1}, std::size_t{100}, std::size_t{70'000}})
        {
            CAPTURE(alignment, bytes);
            auto region = memory.acquire(bytes, alignment, MemoryUse::general);
            REQUIRE(region.has_value());
            REQUIRE(region->size() == bytes);
            CHECK(reinterpret_cast<std::uintptr_t>(region->data()) % alignment == 0);
            CHECK(std::all_of(region->bytes().begin(), region->bytes().end(),
                              [](std::byte b) { return b == std::byte{0}; }));
            std::fill(region->bytes().begin(), region->bytes().end(), std::byte{0xA5});
            CHECK(region->bytes().back() == std::byte{0xA5});
        }
    }
}

TEST_CASE("memory regions are independent and can be moved", "[platform][conformance][memory]")
{
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    MemoryProvider& memory = under.host().memory();

    auto a = memory.acquire(256, 16, MemoryUse::general);
    auto b = memory.acquire(256, 16, MemoryUse::general);
    REQUIRE(a.has_value());
    REQUIRE(b.has_value());
    CHECK(a->data() != b->data());
    std::fill(a->bytes().begin(), a->bytes().end(), std::byte{1});
    CHECK(b->bytes().front() == std::byte{0});

    MemoryRegion moved = std::move(*a);
    CHECK(moved.size() == 256);
    CHECK(a->size() == 0);
    moved.reset();
    CHECK(moved.size() == 0);
}

TEST_CASE("memory that cannot be given is refused with a reason", "[platform][conformance][memory]")
{
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    MemoryProvider& memory = under.host().memory();

    SECTION("nothing")
    {
        auto region = memory.acquire(0, 16, MemoryUse::general);
        REQUIRE_FALSE(region.has_value());
        CHECK(region.error().code == ErrorCode::invalid_argument);
    }
    SECTION("an alignment that is not a power of two")
    {
        auto region = memory.acquire(64, 24, MemoryUse::general);
        REQUIRE_FALSE(region.has_value());
        CHECK(region.error().code == ErrorCode::invalid_argument);
    }
}

TEST_CASE("memory beyond what there is gives resource_exhausted", "[platform][conformance][memory]")
{
    // Only the fake is asked: under the sanitizers a real allocator aborts the process on
    // an impossible request instead of returning null, as the sanitizer builds are meant
    // to. The real providers map the system's refusal (ENOMEM, ERROR_NOT_ENOUGH_MEMORY) to
    // the same code.
    HostUnderTest under(HostKind::fake);
    under.fake_system().host().fake_memory().set_budget(1 << 20);
    auto region = under.host().memory().acquire(2 << 20, 16, MemoryUse::general);
    REQUIRE_FALSE(region.has_value());
    CHECK(region.error().code == ErrorCode::resource_exhausted);
}

TEST_CASE("resident memory is granted, or refused with a reason the caller can act on",
          "[platform][conformance][memory]")
{
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);

    auto region = under.host().memory().acquire(16 * 1024, 4096, MemoryUse::resident);
    if (region.has_value())
    {
        CHECK(region->size() == 16 * 1024);
        std::fill(region->bytes().begin(), region->bytes().end(), std::byte{7});
    }
    else
    {
        const ErrorCode code = region.error().code;
        CHECK((code == ErrorCode::resource_exhausted || code == ErrorCode::permission_denied ||
               code == ErrorCode::unsupported));
    }
}

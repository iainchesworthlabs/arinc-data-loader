#include "adl/platform/memory.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <set>
#include <vector>

using namespace adl::platform;

namespace
{

struct Pair
{
    std::uint32_t a = 7;
    std::uint64_t b = 9;
};

[[nodiscard]] bool aligned(const void* p, std::size_t alignment)
{
    return reinterpret_cast<std::uintptr_t>(p) % alignment == 0;
}

} // namespace

TEST_CASE("alignment arithmetic rounds up to a power of two", "[platform][memory]")
{
    STATIC_REQUIRE(align_up(0, 8) == 0);
    STATIC_REQUIRE(align_up(1, 8) == 8);
    STATIC_REQUIRE(align_up(8, 8) == 8);
    STATIC_REQUIRE(align_up(9, 8) == 16);
    STATIC_REQUIRE(is_power_of_two(64));
    STATIC_REQUIRE_FALSE(is_power_of_two(0));
    STATIC_REQUIRE_FALSE(is_power_of_two(24));
    // A value that cannot be rounded up without wrapping gives 0, which no block is.
    CHECK(align_up(static_cast<std::size_t>(-1), 16) == 0);
}

TEST_CASE("an arena hands out aligned, zeroed room until it is full", "[platform][memory]")
{
    StaticStorage<256> storage;
    Arena arena(storage.span());
    REQUIRE(arena.capacity() == 256);

    SECTION("allocations are aligned and do not overlap")
    {
        auto* a = static_cast<std::byte*>(arena.allocate_bytes(3, 1));
        auto* b = static_cast<std::byte*>(arena.allocate_bytes(8, 8));
        auto* c = static_cast<std::byte*>(arena.allocate_bytes(16, 16));
        REQUIRE(a != nullptr);
        REQUIRE(b != nullptr);
        REQUIRE(c != nullptr);
        CHECK(aligned(b, 8));
        CHECK(aligned(c, 16));
        CHECK(b >= a + 3);
        CHECK(c >= b + 8);
        CHECK(arena.used() >= 3 + 8 + 16);
    }

    SECTION("arrays and objects are constructed in place")
    {
        auto numbers = arena.allocate_array<std::uint32_t>(10);
        REQUIRE(numbers.size() == 10);
        for (const auto n : numbers)
        {
            CHECK(n == 0);
        }
        auto* pair = arena.create<Pair>();
        REQUIRE(pair != nullptr);
        CHECK(pair->a == 7);
        CHECK(pair->b == 9);
        CHECK(aligned(pair, alignof(Pair)));
    }

    SECTION("running out of room is a null result, never a write past the end")
    {
        CHECK(arena.allocate_bytes(257, 1) == nullptr);
        CHECK(arena.allocate_array<std::uint64_t>(33).empty());
        CHECK(arena.allocate_bytes(256, 1) != nullptr);
        CHECK(arena.allocate_bytes(1, 1) == nullptr);
        CHECK(arena.remaining() == 0);
        // A request for nothing still succeeds.
        CHECK(arena.allocate_bytes(0, 1) != nullptr);
    }

    SECTION("a bad alignment is refused")
    {
        CHECK(arena.allocate_bytes(8, 0) == nullptr);
        CHECK(arena.allocate_bytes(8, 3) == nullptr);
    }

    SECTION("a size that would wrap is refused")
    {
        CHECK(arena.allocate_array<std::uint64_t>(static_cast<std::size_t>(-1) / 4).empty());
    }

    SECTION("rewinding to a mark gives back what came after it")
    {
        REQUIRE(arena.allocate_bytes(100, 1) != nullptr);
        const auto mark = arena.mark();
        REQUIRE(arena.allocate_bytes(100, 1) != nullptr);
        CHECK(arena.used() == 200);
        arena.rewind(mark);
        CHECK(arena.used() == 100);
        CHECK(arena.high_water() == 200);
        arena.reset();
        CHECK(arena.used() == 0);
        CHECK(arena.high_water() == 200);
    }
}

TEST_CASE("an empty arena allocates nothing", "[platform][memory]")
{
    Arena arena;
    CHECK(arena.capacity() == 0);
    CHECK(arena.allocate_bytes(1, 1) == nullptr);
    CHECK(arena.allocate_bytes(0, 1) == nullptr);
}

TEST_CASE("a block pool hands out each block once and takes them back in any order", "[platform][memory]")
{
    constexpr std::size_t block = 100;
    constexpr std::size_t count = 6;
    StaticStorage<BlockPool::storage_required(block, count)> storage;
    BlockPool pool(storage.span(), block);
    REQUIRE(pool.capacity() == count);
    REQUIRE(pool.available() == count);
    REQUIRE(pool.block_bytes() == block);

    std::vector<std::span<std::byte>> taken;
    std::set<const std::byte*> distinct;
    for (std::size_t i = 0; i < count; ++i)
    {
        auto b = pool.acquire();
        REQUIRE(b.size() == block);
        CHECK(aligned(b.data(), alignof(std::max_align_t)));
        distinct.insert(b.data());
        taken.push_back(b);
    }
    CHECK(distinct.size() == count);
    CHECK(pool.available() == 0);
    CHECK(pool.acquire().empty());

    SECTION("blocks do not overlap")
    {
        for (std::size_t i = 0; i < taken.size(); ++i)
        {
            std::fill(taken[i].begin(), taken[i].end(), static_cast<std::byte>(i + 1));
        }
        for (std::size_t i = 0; i < taken.size(); ++i)
        {
            for (const auto byte : taken[i])
            {
                CHECK(byte == static_cast<std::byte>(i + 1));
            }
        }
    }

    SECTION("a released block can be acquired again")
    {
        REQUIRE(pool.release(taken[3]));
        CHECK(pool.available() == 1);
        auto again = pool.acquire();
        CHECK(again.data() == taken[3].data());
        CHECK(pool.available() == 0);
    }

    SECTION("something that is not one of the pool's blocks is refused")
    {
        std::byte elsewhere[block];
        CHECK_FALSE(pool.release({elsewhere, block}));
        CHECK_FALSE(pool.release(taken[0].subspan(1)));
        CHECK_FALSE(pool.release(taken[0].first(block - 1)));
        CHECK_FALSE(pool.release({}));
        CHECK(pool.available() == 0);
    }
}

TEST_CASE("a block pool over too little storage holds what fits", "[platform][memory]")
{
    StaticStorage<64> storage;
    BlockPool pool(storage.span(), 40);
    CHECK(pool.capacity() <= 1);

    BlockPool none(storage.span(), 0);
    CHECK(none.capacity() == 0);
    CHECK(none.acquire().empty());

    BlockPool bad_alignment(storage.span(), 8, 3);
    CHECK(bad_alignment.capacity() == 0);
}

TEST_CASE("a block pool aligns its blocks as asked", "[platform][memory]")
{
    StaticStorage<4096> storage;
    BlockPool pool(storage.span(), 24, 64);
    REQUIRE(pool.capacity() >= 2);
    auto a = pool.acquire();
    auto b = pool.acquire();
    CHECK(aligned(a.data(), 64));
    CHECK(aligned(b.data(), 64));
    CHECK(b.data() != a.data());
}

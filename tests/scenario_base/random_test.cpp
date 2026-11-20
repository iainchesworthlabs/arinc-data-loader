// SPDX-License-Identifier: GPL-3.0-or-later
// The random numbers of the scenario files: SplitMix64 pinned to its published outputs and to an independent
// implementation (Python, written from the algorithm's definition), and the draws made from it. If any of these
// numbers changes, a scenario file would no longer repeat a run, so none may.

#include "adl/scenario/random.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

using namespace adl::scenario;

namespace
{

// Evaluated by the compiler: the generator is the same arithmetic at compile time and run time.
static_assert(SplitMix64(0).next() == 0xE220A8397B1DCDAFULL);
static_assert(SplitMix64::mix(splitmix_increment) == 0xE220A8397B1DCDAFULL);

struct Reference
{
    std::uint64_t seed;
    std::array<std::uint64_t, 3> outputs;
};

} // namespace

TEST_CASE("splitmix64: the first outputs of some seeds are the algorithm's", "[scenario][random]")
{
    // Seed 0 and seed 1 are the outputs published with the algorithm; the others come from an independent
    // implementation.
    const std::array<Reference, 4> references{{
        {0, {0xE220A8397B1DCDAFULL, 0x6E789E6AA1B965F4ULL, 0x06C45D188009454FULL}},
        {1, {0x910A2DEC89025CC1ULL, 0xBEEB8DA1658EEC67ULL, 0xF893A2EEFB32555EULL}},
        {20261120, {0x602B8A759A937688ULL, 0xE025F8C776F09921ULL, 0x2FD8A36659CA1ACAULL}},
        {9007199254740991, {0x24B94FACEFB6559FULL, 0x30C3F2F9B73FF198ULL, 0x8784E19B83F9875CULL}},
    }};
    for (const Reference& reference : references)
    {
        INFO("seed " << reference.seed);
        SplitMix64 generator(reference.seed);
        for (const std::uint64_t output : reference.outputs)
        {
            CHECK(generator.next() == output);
        }
    }
}

TEST_CASE("step streams: step s starts from the s-th output of the file's generator", "[scenario][random]")
{
    for (const std::uint64_t seed : {std::uint64_t{0}, std::uint64_t{20261120}})
    {
        SplitMix64 file(seed);
        for (std::size_t step = 1; step <= 5; ++step)
        {
            const std::uint64_t start = file.next();
            SplitMix64 stream = step_stream(seed, step);
            SplitMix64 expected(start);
            CHECK(stream.next() == expected.next());
            CHECK(stream.next() == expected.next());
        }
    }
    // Pinned values (an independent implementation).
    SplitMix64 first = step_stream(0, 1);
    CHECK(first.next() == 0xA706DD2F4D197E6FULL);
    CHECK(first.next() == 0xB382A305F4414F5EULL);
    SplitMix64 second = step_stream(20261120, 2);
    CHECK(second.next() == 0xCB1DF4B424220192ULL);
    CHECK(second.next() == 0xB1C3E96F200711A6ULL);
    SplitMix64 last = step_stream(20261120, 1000);
    CHECK(last.next() == 0xF0BEDC92DFEA74D2ULL);
    // Two steps never share a stream.
    CHECK(step_stream(20261120, 1).next() != step_stream(20261120, 2).next());
}

TEST_CASE("between: a number in a range, by rejection", "[scenario][random]")
{
    SplitMix64 dice(7);
    std::string rolls;
    for (int i = 0; i < 16; ++i)
    {
        rolls += static_cast<char>('0' + dice.between(1, 6));
    }
    CHECK(rolls == "4114545166251511");

    SplitMix64 delays(20261120);
    std::array<std::uint64_t, 8> drawn{};
    for (std::uint64_t& value : drawn)
    {
        value = delays.between(200, 900);
    }
    CHECK(drawn == std::array<std::uint64_t, 8>{417, 706, 777, 540, 727, 630, 719, 670});

    // One number in the range, a range turned round, and the whole of 2^64.
    CHECK(SplitMix64(5).between(5, 5) == 5);
    CHECK(SplitMix64(5).between(9, 2) == 9);
    CHECK(SplitMix64(5).between(0, std::numeric_limits<std::uint64_t>::max()) == 0x63033B0CA389C35AULL);

    // 2^64 mod range is 2^62 here, so an output of 3 * 2^62 or more is discarded: the first output of seed 0
    // (0xE220A8397B1DCDAF) is, and the second one is used.
    CHECK(SplitMix64(0).between(0, 3ULL * (1ULL << 62U)) == 7960286522194355700ULL);
}

TEST_CASE("between: every number of a small range comes, and about equally often", "[scenario][random]")
{
    SplitMix64 dice(12345);
    std::array<int, 6> faces{};
    for (int i = 0; i < 60000; ++i)
    {
        ++faces[dice.between(1, 6) - 1];
    }
    for (const int count : faces)
    {
        CHECK(count > 9500);
        CHECK(count < 10500);
    }
}

TEST_CASE("chance: a draw from 1 to 1000 that is at most the per mille", "[scenario][random]")
{
    SplitMix64 draws(0);
    std::array<std::uint64_t, 6> numbers{};
    for (std::uint64_t& value : numbers)
    {
        value = draws.between(1, 1000);
    }
    CHECK(numbers == std::array<std::uint64_t, 6>{536, 701, 680, 445, 748, 91});

    SplitMix64 by_chance(0);
    CHECK(!by_chance.chance(535));
    CHECK(by_chance.chance(701));
    CHECK(by_chance.chance(680));
    CHECK(by_chance.chance(445));
    CHECK(!by_chance.chance(747));
    CHECK(by_chance.chance(91));

    // A chance of 1000 is certain and costs a draw all the same, so that the next step's numbers do not move.
    SplitMix64 certain(3);
    SplitMix64 same(3);
    CHECK(certain.chance(1000));
    static_cast<void>(same.between(1, 1000));
    CHECK(certain.next() == same.next());

    // A pattern for one step of a file: the same on every system.
    SplitMix64 half = step_stream(20261120, 3);
    std::string pattern;
    for (int i = 0; i < 20; ++i)
    {
        pattern += half.chance(500) ? '1' : '0';
    }
    CHECK(pattern == "01011111001111001111");
}

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The random numbers of the scenario files (the mock target's and the loader's). A scenario says "lose half of the
// status files" or "hold a datagram back for 200 to 900 ms" and gives a seed, so that a run with faults can be
// repeated: the same seed gives the same numbers on every system and with every standard library. The standard
// library cannot promise that (the output of std::uniform_int_distribution is implementation defined), so the
// numbers are made here, from one named algorithm, with integer arithmetic only.
//
// SplitMix64 (Steele, Lea and Flood, "Fast Splittable Pseudorandom Number Generators", 2014; the generator of
// Java's SplittableRandom). The state is a 64-bit unsigned number. Each call adds 0x9E3779B97F4A7C15 to it and
// mixes the sum: z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9, z = (z ^ (z >> 27)) * 0x94D049BB133111EB, and
// z ^ (z >> 31), all modulo 2^64. Seed 0 gives 0xE220A8397B1DCDAF, 0x6E789E6AA1B965F4, 0x06C45D188009454F.
//
// Each step of a scenario has a stream of its own, so that what one step draws never depends on what another did:
// step s (counted from 1) is given the generator that starts from the s-th output of SplitMix64(seed).

#include <cstddef>
#include <cstdint>

namespace adl::scenario
{

inline constexpr std::uint64_t splitmix_increment = 0x9E3779B97F4A7C15ULL;

class SplitMix64
{
public:
    constexpr explicit SplitMix64(std::uint64_t seed = 0) noexcept : state_(seed) {}

    // The mixing step alone: the output that follows a state of `z`.
    [[nodiscard]] static constexpr std::uint64_t mix(std::uint64_t z) noexcept
    {
        z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27U)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31U);
    }

    [[nodiscard]] constexpr std::uint64_t next() noexcept
    {
        state_ += splitmix_increment;
        return mix(state_);
    }

    // A whole number from `low` to `high`, both included (`low` is returned when `high` is smaller). By rejection,
    // so that no number is likelier than another: with range = high - low + 1, an output of
    // 2^64 - (2^64 mod range) or more is discarded and the next one is taken, and the answer is
    // low + output mod range.
    [[nodiscard]] constexpr std::uint64_t between(std::uint64_t low, std::uint64_t high) noexcept
    {
        if (high < low)
        {
            return low;
        }
        const std::uint64_t range = high - low + 1; // 0 stands for 2^64: every output will do
        if (range == 0)
        {
            return next();
        }
        const std::uint64_t surplus = (std::uint64_t{0} - range) % range; // 2^64 mod range
        for (;;)
        {
            const std::uint64_t output = next();
            if (surplus == 0 || output < std::uint64_t{0} - surplus)
            {
                return low + output % range;
            }
        }
    }

    // True with a chance of `per_mille` in a thousand: a draw from 1 to 1000 that is at most `per_mille`.
    [[nodiscard]] constexpr bool chance(unsigned per_mille) noexcept
    {
        return between(1, 1000) <= per_mille;
    }

private:
    std::uint64_t state_;
};

// The generator of step `step` (from 1) of a scenario whose seed is `seed`: it starts from the `step`-th output
// of SplitMix64(seed), which is mix(seed + step * 0x9E3779B97F4A7C15).
[[nodiscard]] constexpr SplitMix64 step_stream(std::uint64_t seed, std::size_t step) noexcept
{
    return SplitMix64(SplitMix64::mix(seed + static_cast<std::uint64_t>(step) * splitmix_increment));
}

} // namespace adl::scenario

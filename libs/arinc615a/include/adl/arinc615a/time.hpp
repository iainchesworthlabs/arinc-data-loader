#pragma once

// Time as the engine sees it: a point on a clock it is told the reading of, and a span. The
// engine has no clock of its own; every call that can start or end a time-out is given the
// time, and the engine answers with the time it wants to be called again. The runner turns
// its platform's monotonic clock into these (the representation is the same, nanoseconds).

#include <chrono>
#include <compare>
#include <cstdint>

namespace adl::arinc615a {

using Duration = std::chrono::nanoseconds;

struct Instant {
    Duration since_origin{};

    friend constexpr auto operator<=>(const Instant&, const Instant&) noexcept = default;
};

// "No wake-up wanted".
inline constexpr Instant never{Duration::max()};

// Adding to an Instant saturates, so `never + x` stays never.
[[nodiscard]] constexpr Instant operator+(Instant at, Duration by) noexcept {
    if (by > Duration::zero() && at.since_origin > Duration::max() - by) {
        return never;
    }
    if (by < Duration::zero() && at.since_origin < Duration::min() - by) {
        return Instant{Duration::min()};
    }
    return Instant{at.since_origin + by};
}

[[nodiscard]] constexpr Instant operator-(Instant at, Duration by) noexcept {
    if (by > Duration::zero() && at.since_origin < Duration::min() + by) {
        return Instant{Duration::min()};
    }
    if (by < Duration::zero() && at.since_origin > Duration::max() + by) {
        return never;
    }
    return Instant{at.since_origin - by};
}

[[nodiscard]] constexpr Duration operator-(Instant later, Instant earlier) noexcept {
    return later.since_origin - earlier.since_origin;
}

[[nodiscard]] constexpr Instant earlier_of(Instant a, Instant b) noexcept {
    return a < b ? a : b;
}

[[nodiscard]] constexpr Duration whole_seconds(std::uint32_t count) noexcept {
    return std::chrono::seconds(count);
}

}  // namespace adl::arinc615a

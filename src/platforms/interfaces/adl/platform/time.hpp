#pragma once

// Time. The protocol runs on durations (a 2 second TFTP time-out, a 13 second
// data-load time-out, a WAIT of up to 65535 seconds), so what the platform has
// to provide is a monotonic clock that does not jump when someone sets the
// date, and a way to sleep. A wall clock is separate and optional: a target
// without a battery-backed clock reports `unsupported`.

#include <chrono>
#include <compare>
#include <cstdint>

#include "adl/platform/error.hpp"

namespace adl::platform {

// Signed nanoseconds in 64 bits: 292 years either way, with no rounding to
// worry about in the protocol's whole-second figures.
using Duration = std::chrono::nanoseconds;

// "No time limit". Every wait takes a Duration, so a wait that may not give up
// says so by name instead of passing a magic number.
inline constexpr Duration forever = Duration::max();

// A point on the monotonic clock. Its origin is arbitrary (boot, process start)
// and is the same only within one Clock; compare Instants of one clock.
struct Instant {
    Duration since_origin{};

    friend constexpr auto operator<=>(const Instant&, const Instant&) noexcept = default;
};

// Adding to an Instant saturates rather than wrapping, so `now + forever` is a
// deadline that never arrives.
[[nodiscard]] constexpr Instant operator+(Instant at, Duration by) noexcept {
    if (by > Duration::zero() && at.since_origin > Duration::max() - by) {
        return Instant{Duration::max()};
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
        return Instant{Duration::max()};
    }
    return Instant{at.since_origin - by};
}

[[nodiscard]] constexpr Duration operator-(Instant later, Instant earlier) noexcept {
    return later.since_origin - earlier.since_origin;
}

// The monotonic clock and the means to wait on it. One per Host.
class Clock {
public:
    virtual ~Clock() = default;

    // Never goes backwards. Resolution is the platform's own (a tick of an RTOS
    // can be a millisecond or ten); the protocol needs no better than 10 ms.
    [[nodiscard]] virtual Instant now() noexcept = 0;

    // Blocks the calling thread for at least `d` (a non-positive `d` returns at
    // once). May overshoot by a scheduling quantum.
    virtual void sleep_for(Duration d) noexcept = 0;

    // Seconds since 1970-01-01T00:00:00Z, for labelling output (a download
    // directory, a log line). `unsupported` where the target has no real-time
    // clock. Never used to measure an interval.
    [[nodiscard]] virtual Result<std::int64_t> wall_time_seconds() noexcept = 0;

    void sleep_until(Instant at) noexcept {
        const Duration remaining = at - now();
        if (remaining > Duration::zero()) {
            sleep_for(remaining);
        }
    }
};

}  // namespace adl::platform

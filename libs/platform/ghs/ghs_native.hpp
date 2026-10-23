#pragma once

// What the two INTEGRITY platforms share: the mapping of the native Error
// values and errno onto adl::platform::ErrorCode, and conversion between the
// INTEGRITY Time type and the interface's Duration.
//
// INTEGRITY's Time is { time_t Seconds; UINT4 Fraction; }, where Fraction is
// the part of a second as a 32-bit fraction of 2^32 (so 0x80000000 is half a
// second). That reading follows the INTEGRITY documentation's description of
// Time and the BSP clocks in this tree; check it against GetClockResolution()
// on the target before trusting sub-tick arithmetic.
//
// This header is internal to libs/platform: it includes OS headers.

#include <INTEGRITY.h>

#include <cstdint>

#include "adl/platform/error.hpp"
#include "adl/platform/threads.hpp"
#include "adl/platform/time.hpp"

namespace adl::platform::ghs {

// The fixed reasons the interface cares about, from the native Error enum. The
// names are the enum members in INTEGRITY.h; `other` carries the number in
// Error::native for the log.
[[nodiscard]] ErrorCode map_error(Error error) noexcept;

// errno as INTEGRITY's ipflite and C library set it. The same table as
// posix/posix_common.cpp keeps; the two files cannot share one because the
// posix sources pull in <pthread.h>, which the 178B tree does not have.
[[nodiscard]] ErrorCode map_errno(int error) noexcept;

[[nodiscard]] inline std::unexpected<Error> fail_native(Error error) noexcept {
    return fail(map_error(error), static_cast<int>(error));
}

[[nodiscard]] inline std::unexpected<Error> fail_errno(int error) noexcept {
    return fail(map_errno(error), error);
}

// Time <-> Duration. Fractions round to the nearest nanosecond, and conversion
// saturates rather than wraps, as the interface's arithmetic does.
[[nodiscard]] Duration to_duration(const Time& time) noexcept;
[[nodiscard]] Time to_time(Duration duration) noexcept;

// base + by, saturated; the arithmetic AddTime performs.
[[nodiscard]] Time add_duration(const Time& base, Duration by) noexcept;

// The priority plan, expressed in the interface's abstract levels. INTEGRITY
// counts down: 1 is the most urgent, 255 the least. The proposal mirrors the
// tree's own default of 127 (ind_crt0.c sets that for an application task) and
// stays clear of the system's top and bottom; settle it against the product's
// priority plan before flying, as the desktops' plans are.
constexpr Value native_priority(Priority level) noexcept {
    switch (level) {
        case Priority::lowest:
            return 250;
        case Priority::low:
            return 200;
        case Priority::normal:
            return 127;
        case Priority::high:
            return 60;
        case Priority::highest:
            return 40;
    }
    return 127;
}

}  // namespace adl::platform::ghs

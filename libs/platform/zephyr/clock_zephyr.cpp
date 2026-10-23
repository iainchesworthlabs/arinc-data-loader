// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/platform/time.hpp"
#include "detail/nothrow.hpp"
#include "zephyr/zephyr_os.hpp"

#include <ctime>

namespace adl::platform::zephyr
{
namespace
{

// Before this a "wall clock" that reads a few seconds is the monotonic clock
// that nobody set (Zephyr's real-time clock starts at its boot): the target has
// no real-time clock, and the interface says so.
constexpr std::int64_t earliest_plausible_wall_time = 1'577'836'800; // 2020-01-01

class ZephyrClock final : public Clock
{
public:
    Instant now() noexcept override
    {
        // The tick counter is 64 bits, so it does not wrap in any lifetime a
        // target has, whatever CONFIG_SYS_CLOCK_TICKS_PER_SEC is.
        const std::int64_t ticks = k_uptime_ticks();
        return Instant{Duration(static_cast<std::int64_t>(k_ticks_to_ns_floor64(static_cast<std::uint64_t>(ticks))))};
    }

    void sleep_for(Duration d) noexcept override
    {
        if (d <= Duration::zero())
        {
            return;
        }
        // k_sleep can return early when another thread wakes this one, and rounds
        // to the tick; going round again for what is left keeps "at least d" true.
        const Instant deadline = now() + d;
        for (;;)
        {
            const Duration remaining = deadline - now();
            if (remaining <= Duration::zero())
            {
                return;
            }
            (void)k_sleep(zephyr_os::to_timeout(remaining));
        }
    }

    Result<std::int64_t> wall_time_seconds() noexcept override
    {
        timespec ts{};
        if (const int error = sys_clock_gettime(SYS_CLOCK_REALTIME, &ts); error != 0)
        {
            return zephyr_os::fail_errno(error);
        }
        if (ts.tv_sec < earliest_plausible_wall_time)
        {
            return fail(ErrorCode::unsupported);
        }
        return static_cast<std::int64_t>(ts.tv_sec);
    }
};

} // namespace

std::unique_ptr<Clock> make_clock() noexcept
{
    return detail::make_unique_nothrow<ZephyrClock>();
}

} // namespace adl::platform::zephyr

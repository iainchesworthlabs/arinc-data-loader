#include "adl/platform/time.hpp"
#include "detail/nothrow.hpp"
#include "freertos/freertos_os.hpp"

#include <atomic>
#include <limits>

namespace adl::platform::freertos
{
namespace
{

constexpr std::int64_t nanoseconds_per_second = 1'000'000'000;

class FreeRtosClock final : public Clock
{
public:
    Instant now() noexcept override
    {
        const std::uint64_t ticks = widen(static_cast<std::uint64_t>(xTaskGetTickCount()));
        const auto rate = static_cast<std::uint64_t>(configTICK_RATE_HZ);
        // Whole seconds and the remainder separately, so that no tick count overflows the
        // multiplication and a rate that does not divide a second still comes out right.
        const std::uint64_t seconds = ticks / rate;
        const std::uint64_t rest = ticks % rate;
        const auto nanoseconds =
            static_cast<std::int64_t>(seconds) * nanoseconds_per_second +
            static_cast<std::int64_t>(rest) * nanoseconds_per_second / static_cast<std::int64_t>(rate);
        return Instant{Duration(nanoseconds)};
    }

    void sleep_for(Duration d) noexcept override
    {
        if (d <= Duration::zero())
        {
            return;
        }
        // vTaskDelay rounds to the tick and a task can be woken early by a notification;
        // going round again for what is left keeps "at least d" true.
        const Instant deadline = now() + d;
        for (;;)
        {
            const Duration remaining = deadline - now();
            if (remaining <= Duration::zero())
            {
                return;
            }
            vTaskDelay(freertos_os::to_ticks(remaining));
        }
    }

    Result<std::int64_t> wall_time_seconds() noexcept override
    {
        // A kernel has no calendar. A product that has a real-time clock gives the loader
        // one by replacing this file's answer; there is no standard place to ask.
        return fail(ErrorCode::unsupported);
    }

private:
    // The kernel's tick count is a TickType_t, which is 32 bits on most targets and wraps in
    // 49.7 days at 1 kHz. It is widened to 64 bits by counting its wraps, which needs now()
    // to be called at least once in half a period; the runner asks every time it wakes.
    [[nodiscard]] std::uint64_t widen(std::uint64_t tick) noexcept
    {
        constexpr int width = std::numeric_limits<TickType_t>::digits;
        if constexpr (width >= 64)
        {
            return tick;
        }
        else
        {
            constexpr std::uint64_t period = width >= 64 ? 0 : std::uint64_t{1} << (width % 64);
            std::uint64_t previous = last_.load(std::memory_order_relaxed);
            for (;;)
            {
                std::uint64_t wide = (previous & ~(period - 1)) | tick;
                if (wide < previous)
                {
                    wide += period;
                }
                if (last_.compare_exchange_weak(previous, wide, std::memory_order_relaxed))
                {
                    return wide;
                }
            }
        }
    }

    std::atomic<std::uint64_t> last_{0};
};

} // namespace

std::unique_ptr<Clock> make_clock() noexcept
{
    return detail::make_unique_nothrow<FreeRtosClock>();
}

} // namespace adl::platform::freertos

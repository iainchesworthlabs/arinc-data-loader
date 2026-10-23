#include <windows.h>

#include "adl/platform/time.hpp"
#include "detail/nothrow.hpp"
#include "windows/windows_services.hpp"

namespace adl::platform::windows {
namespace {

constexpr std::int64_t nanoseconds_per_second = 1'000'000'000;
// Seconds from 1601-01-01 (the FILETIME epoch) to 1970-01-01.
constexpr std::int64_t unix_epoch_in_filetime_seconds = 11'644'473'600;

class WindowsClock final : public Clock {
public:
    WindowsClock() noexcept {
        LARGE_INTEGER frequency{};
        ::QueryPerformanceFrequency(&frequency);
        frequency_ = frequency.QuadPart > 0 ? frequency.QuadPart : 1;
    }

    Instant now() noexcept override {
        LARGE_INTEGER counter{};
        ::QueryPerformanceCounter(&counter);
        // counter * 1e9 / frequency without overflowing 64 bits for years of uptime.
        const std::int64_t whole = counter.QuadPart / frequency_;
        const std::int64_t rest = counter.QuadPart % frequency_;
        return Instant{
            Duration(whole * nanoseconds_per_second + rest * nanoseconds_per_second / frequency_)};
    }

    void sleep_for(Duration d) noexcept override {
        if (d <= Duration::zero()) {
            return;
        }
        // Sleep works in whole milliseconds on a timer that ticks every 1 to 16 ms,
        // and can return before the time asked for has passed by the clock above.
        // Going round again keeps "at least d" true.
        const Instant deadline = now() + d;
        for (;;) {
            const Duration remaining = deadline - now();
            if (remaining <= Duration::zero()) {
                return;
            }
            ::Sleep(to_milliseconds(remaining, 0x7fffffff));
        }
    }

    Result<std::int64_t> wall_time_seconds() noexcept override {
        FILETIME time{};
        ::GetSystemTimePreciseAsFileTime(&time);
        const std::uint64_t ticks =
            (static_cast<std::uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
        return static_cast<std::int64_t>(ticks / 10'000'000) - unix_epoch_in_filetime_seconds;
    }

private:
    std::int64_t frequency_ = 1;
};

}  // namespace

std::unique_ptr<Clock> make_clock() noexcept {
    return detail::make_unique_nothrow<WindowsClock>();
}

}  // namespace adl::platform::windows

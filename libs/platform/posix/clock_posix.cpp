#include <time.h>

#include <cerrno>

#include "adl/platform/time.hpp"
#include "detail/nothrow.hpp"
#include "posix/posix_os.hpp"

namespace adl::platform::posix {
namespace {

constexpr std::int64_t nanoseconds_per_second = 1'000'000'000;

class PosixClock final : public Clock {
public:
    Instant now() noexcept override {
        timespec ts{};
        ::clock_gettime(CLOCK_MONOTONIC, &ts);
        return Instant{
            Duration(static_cast<std::int64_t>(ts.tv_sec) * nanoseconds_per_second + ts.tv_nsec)};
    }

    void sleep_for(Duration d) noexcept override {
        if (d <= Duration::zero()) {
            return;
        }
        // nanosleep returns early when a signal arrives; going round again for what
        // is left keeps "at least d" true.
        const Instant deadline = now() + d;
        for (;;) {
            const Duration remaining = deadline - now();
            if (remaining <= Duration::zero()) {
                return;
            }
            const std::int64_t ns = remaining.count();
            timespec request{};
            request.tv_sec = static_cast<time_t>(ns / nanoseconds_per_second);
            request.tv_nsec = static_cast<long>(ns % nanoseconds_per_second);
            if (::nanosleep(&request, nullptr) != 0 && errno != EINTR) {
                return;
            }
        }
    }

    Result<std::int64_t> wall_time_seconds() noexcept override {
        timespec ts{};
        if (::clock_gettime(CLOCK_REALTIME, &ts) != 0) {
            return posix_os::fail_errno(errno);
        }
        return static_cast<std::int64_t>(ts.tv_sec);
    }
};

}  // namespace

std::unique_ptr<Clock> make_clock() noexcept {
    return detail::make_unique_nothrow<PosixClock>();
}

}  // namespace adl::platform::posix

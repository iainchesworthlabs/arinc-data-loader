#include "freertos/environment.hpp"

#include <algorithm>
#include <limits>

#include "freertos/freertos_os.hpp"

namespace adl::platform {
namespace {

freertos::Environment product_environment;

}  // namespace

namespace freertos {

void set_environment(const Environment& environment) noexcept {
    product_environment = environment;
}

}  // namespace freertos

namespace freertos_os {

const freertos::Environment& environment() noexcept {
    return product_environment;
}

TickType_t to_ticks(Duration d) noexcept {
    if (d <= Duration::zero()) {
        return 0;
    }
    d = std::min(d, longest_kernel_wait);
    // ceil(nanoseconds * configTICK_RATE_HZ / 1e9). The wait is at most an hour, so the
    // product fits 64 bits for any tick rate up to a megahertz.
    constexpr std::int64_t nanoseconds_per_second = 1'000'000'000;
    const auto rate = static_cast<std::int64_t>(configTICK_RATE_HZ);
    const auto ticks = static_cast<std::uint64_t>((d.count() * rate + nanoseconds_per_second - 1) /
                                                  nanoseconds_per_second);
    // portMAX_DELAY means "for ever" to a kernel with INCLUDE_vTaskSuspend, so a finite wait
    // stops short of it.
    return static_cast<TickType_t>(
        std::min<std::uint64_t>(ticks, static_cast<std::uint64_t>(portMAX_DELAY) - 1));
}

}  // namespace freertos_os
}  // namespace adl::platform

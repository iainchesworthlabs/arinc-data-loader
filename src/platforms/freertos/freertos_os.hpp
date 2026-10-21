#pragma once

// What the FreeRTOS implementations share. This header is internal to
// src/platforms/freertos: it includes FreeRTOS's headers, which the rest of the program
// never sees.

#include <cstddef>
#include <cstdint>
#include <memory>

#include "adl/platform/error.hpp"
#include "adl/platform/host.hpp"
#include "adl/platform/time.hpp"
#include "freertos/environment.hpp"

#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>
#include <task.h>

namespace adl::platform::freertos_os {

// The environment the product set (environment.hpp), read when a Host is made.
[[nodiscard]] const freertos::Environment& environment() noexcept;

// The longest wait handed to the kernel in one call: an hour. A longer wait is several
// calls, so that neither the conversion to ticks nor a kernel without INCLUDE_vTaskSuspend
// (where portMAX_DELAY is a long finite wait, not for ever) can matter.
inline constexpr Duration longest_kernel_wait = std::chrono::hours(1);

// Ticks for `d`, rounded up to a whole tick so that a wait never ends early, at most
// longest_kernel_wait's worth. 0 for a non-positive `d`.
[[nodiscard]] TickType_t to_ticks(Duration d) noexcept;

// Calls `attempt(ticks)` (a kernel call that waits up to that many ticks and says whether
// it got what it waited for) until it does or `timeout` has passed by `clock`, whatever the
// kernel's rounding of one wait. `forever` waits for ever. A timeout of zero tries once.
template <class Attempt>
[[nodiscard]] bool wait_for(Clock& clock, Duration timeout, Attempt&& attempt) noexcept {
    const Instant deadline = timeout == forever ? Instant{forever} : clock.now() + timeout;
    for (;;) {
        Duration remaining = forever;
        if (timeout != forever) {
            remaining = deadline - clock.now();
            if (remaining < Duration::zero()) {
                remaining = Duration::zero();
            }
        }
        if (attempt(to_ticks(remaining == forever ? longest_kernel_wait : remaining))) {
            return true;
        }
        if (timeout != forever && deadline - clock.now() <= Duration::zero()) {
            return false;
        }
    }
}

}  // namespace adl::platform::freertos_os

namespace adl::platform::freertos {

[[nodiscard]] std::unique_ptr<Clock> make_clock() noexcept;
[[nodiscard]] std::unique_ptr<MemoryProvider> make_memory() noexcept;
[[nodiscard]] std::unique_ptr<FileSystem> make_files(::lfs* mounted) noexcept;
[[nodiscard]] std::unique_ptr<Threading> make_threading(Clock& clock,
                                                        unsigned network_task_priority) noexcept;
[[nodiscard]] std::unique_ptr<Network> make_network(Clock& clock) noexcept;
[[nodiscard]] std::unique_ptr<Ipc> make_ipc(Clock& clock) noexcept;

}  // namespace adl::platform::freertos

#pragma once

// What the Zephyr implementations share. This header is internal to
// src/platforms/zephyr: it includes Zephyr's kernel header, which the rest of
// the program never sees.
//
// The standard library's headers come first, in this header and in every source
// file here, and Zephyr's after them. Zephyr's toolchain.h defines __unused as
// an attribute, and glibc's pthread types have a member of that name; the C++
// headers that reach them (<memory> does) must be read before the macro exists.

#include <cstdint>
#include <memory>

#include "adl/platform/error.hpp"
#include "adl/platform/host.hpp"
#include "adl/platform/time.hpp"

#include <zephyr/kernel.h>

namespace adl::platform::zephyr_os {

// Zephyr's calls fail with a negative errno or, for the C library and the
// socket layer, return -1 and set errno. Both arrive here as a positive errno
// value.
[[nodiscard]] ErrorCode map_errno(int error) noexcept;
[[nodiscard]] inline std::unexpected<Error> fail_errno(int error) noexcept {
    return fail(map_errno(error), error);
}

// The longest wait handed to the kernel in one call. Longer waits are made of
// several, so that no conversion from nanoseconds to ticks can overflow: a day.
inline constexpr Duration longest_kernel_wait = std::chrono::hours(24);

// The kernel timeout for `d`, rounded up to a whole tick so that a wait never
// ends early, and K_FOREVER for `forever`. Clamped to longest_kernel_wait; a
// caller that wants more loops on the clock.
[[nodiscard]] k_timeout_t to_timeout(Duration d) noexcept;

}  // namespace adl::platform::zephyr_os

namespace adl::platform::zephyr {

[[nodiscard]] std::unique_ptr<Clock> make_clock() noexcept;
[[nodiscard]] std::unique_ptr<MemoryProvider> make_memory() noexcept;
[[nodiscard]] std::unique_ptr<FileSystem> make_files() noexcept;
[[nodiscard]] std::unique_ptr<Threading> make_threading(Clock& clock) noexcept;
[[nodiscard]] std::unique_ptr<Network> make_network(Clock& clock) noexcept;
[[nodiscard]] std::unique_ptr<Ipc> make_ipc(Clock& clock) noexcept;

}  // namespace adl::platform::zephyr

#pragma once

// What the POSIX implementations share, and the few things on which the POSIX
// systems differ. The differences are functions declared here and defined once
// per operating system (linux/os_linux.cpp, macos/os_macos.cpp, and the
// unbuilt qnx/os_qnx.cpp), chosen by that platform's platform.cmake, so that
// the shared code has no #ifdef and asks nobody which system it is on.
//
// This header is internal to src/platforms: it includes OS headers.

#include <pthread.h>

#include <cstddef>

#include "adl/platform/error.hpp"
#include "adl/platform/host.hpp"
#include "adl/platform/threads.hpp"
#include "adl/platform/time.hpp"

namespace adl::platform::posix_os {

// ---- per operating system -------------------------------------------------

[[nodiscard]] const char* host_name() noexcept;

// A condition variable that waits on the monotonic clock, so that a wall-clock
// step (an NTP correction, someone setting the date) cannot make a time-out
// early or endless. Linux sets the clock on the condition's attributes; macOS
// has a relative wait instead. Returns 0 or an errno value.
[[nodiscard]] int init_condition(pthread_cond_t& condition) noexcept;

// Waits on `condition` (with `mutex` held) for at most `timeout`, which is
// positive and finite. Returns 0 when woken (possibly spuriously), ETIMEDOUT
// when the time ran out, or another errno value.
[[nodiscard]] int wait_condition(pthread_cond_t& condition, pthread_mutex_t& mutex,
                                 Duration timeout) noexcept;

// Gives a thread the scheduling the abstract level maps to; the thread calls it
// on itself before its entry function runs. Always called, for `normal` too,
// because a thread inherits its creator's policy and a runner at `high` must not
// start application threads at `high`. `applied` on success,
// `not_permitted` when the system refuses the process that scheduling, and
// `unsupported` otherwise.
[[nodiscard]] PriorityOutcome apply_priority(pthread_t thread, Priority level) noexcept;

// Names the calling thread for a debugger; the name is cut to what the system
// allows.
void name_current_thread(const char* name) noexcept;

// Makes what was written to `fd` durable (fsync on Linux; macOS needs
// F_FULLFSYNC for the disk to be asked, not just the driver). Returns 0 or an
// errno value.
[[nodiscard]] int flush_to_storage(int fd) noexcept;

// ---- shared -----------------------------------------------------------------

[[nodiscard]] ErrorCode map_errno(int error) noexcept;
[[nodiscard]] inline std::unexpected<Error> fail_errno(int error) noexcept {
    return fail(map_errno(error), error);
}

// Milliseconds for poll(): rounded up so that a wait never ends early, capped at
// what an int holds, -1 for `forever`.
[[nodiscard]] int to_poll_milliseconds(Duration timeout) noexcept;

}  // namespace adl::platform::posix_os

namespace adl::platform::posix {

[[nodiscard]] std::unique_ptr<Clock> make_clock() noexcept;
[[nodiscard]] std::unique_ptr<MemoryProvider> make_memory() noexcept;
[[nodiscard]] std::unique_ptr<FileSystem> make_files() noexcept;
[[nodiscard]] std::unique_ptr<Threading> make_threading(Clock& clock) noexcept;
[[nodiscard]] std::unique_ptr<Network> make_network(Clock& clock) noexcept;

}  // namespace adl::platform::posix

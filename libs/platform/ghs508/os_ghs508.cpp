// SPDX-License-Identifier: GPL-3.0-or-later
// What INTEGRITY 5.0.8 does differently from the other POSIX systems; see
// posix/posix_os.hpp for what each function is for. UNBUILT AND UNTESTED: this
// file has never been compiled against the INTEGRITY 5.0.8 headers, and the
// places where INTEGRITY's POSIX may differ are named in docs/platform-ghs.md.

#include "posix/posix_os.hpp"

#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>

namespace adl::platform::posix_os
{

const char* host_name() noexcept
{
    return "integrity";
}

bool is_real_time() noexcept
{
    return true;
}

int init_condition(pthread_cond_t& condition) noexcept
{
    // INTEGRITY's POSIX takes the monotonic clock on a condition variable, as
    // Linux's does. Verify against the installed headers; if it does not, this
    // becomes the macOS pattern (a relative wait) instead.
    pthread_condattr_t attributes;
    if (const int error = ::pthread_condattr_init(&attributes); error != 0)
    {
        return error;
    }
    int error = ::pthread_condattr_setclock(&attributes, CLOCK_MONOTONIC);
    if (error == 0)
    {
        error = ::pthread_cond_init(&condition, &attributes);
    }
    ::pthread_condattr_destroy(&attributes);
    return error;
}

int wait_condition(pthread_cond_t& condition, pthread_mutex_t& mutex, Duration timeout) noexcept
{
    constexpr std::int64_t nanoseconds_per_second = 1'000'000'000;
    timespec now{};
    ::clock_gettime(CLOCK_MONOTONIC, &now);
    std::int64_t seconds = static_cast<std::int64_t>(now.tv_sec);
    std::int64_t nanoseconds = now.tv_nsec + timeout.count() % nanoseconds_per_second;
    seconds += timeout.count() / nanoseconds_per_second;
    if (nanoseconds >= nanoseconds_per_second)
    {
        nanoseconds -= nanoseconds_per_second;
        ++seconds;
    }
    timespec deadline{};
    deadline.tv_sec = static_cast<time_t>(seconds);
    deadline.tv_nsec = static_cast<long>(nanoseconds);
    return ::pthread_cond_timedwait(&condition, &mutex, &deadline);
}

// INTEGRITY's POSIX scheduling runs 1 to 255, higher meaning more urgent. The
// levels are a proposal below the partition ceilings, with `normal` at 10; the
// same numbers as QNX, because the two INTEGRITY ports should feel alike.
PriorityOutcome apply_priority(pthread_t thread, Priority level) noexcept
{
    int priority = 10;
    switch (level)
    {
    case Priority::lowest:
        priority = 5;
        break;
    case Priority::low:
        priority = 8;
        break;
    case Priority::normal:
        priority = 10;
        break;
    case Priority::high:
        priority = 30;
        break;
    case Priority::highest:
        priority = 50;
        break;
    }
    sched_param parameter{};
    parameter.sched_priority = priority;
    const int error = ::pthread_setschedparam(thread, SCHED_FIFO, &parameter);
    if (error == 0)
    {
        return PriorityOutcome::applied;
    }
    return error == EPERM ? PriorityOutcome::not_permitted : PriorityOutcome::unsupported;
}

void name_current_thread(const char* name) noexcept
{
    // INTEGRITY names a thread at creation, and a rename from inside the
    // thread has no portable spelling. The name is for a debugger; nothing is
    // done. (The parameter is deliberately unused.)
    (void)name;
}

int flush_to_storage(int fd) noexcept
{
    return ::fsync(fd) == 0 ? 0 : errno;
}

} // namespace adl::platform::posix_os

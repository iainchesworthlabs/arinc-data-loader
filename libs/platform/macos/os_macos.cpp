// SPDX-License-Identifier: GPL-3.0-or-later
// What macOS does differently from the other POSIX systems; see
// posix/posix_os.hpp for what each function is for.

#include "posix/posix_os.hpp"

#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>

#include <cerrno>

namespace adl::platform::posix_os
{

const char* host_name() noexcept
{
    return "macos";
}

bool is_real_time() noexcept
{
    return false;
}

int init_condition(pthread_cond_t& condition) noexcept
{
    // macOS cannot choose the clock of a condition variable, so waits are made
    // relative (below), which a wall-clock step does not disturb.
    return ::pthread_cond_init(&condition, nullptr);
}

int wait_condition(pthread_cond_t& condition, pthread_mutex_t& mutex, Duration timeout) noexcept
{
    constexpr std::int64_t nanoseconds_per_second = 1'000'000'000;
    // A bounded wait; the caller goes round again for the rest of a long time-out.
    constexpr std::int64_t longest_seconds = 3600;
    std::int64_t seconds = timeout.count() / nanoseconds_per_second;
    std::int64_t nanoseconds = timeout.count() % nanoseconds_per_second;
    if (seconds > longest_seconds)
    {
        seconds = longest_seconds;
        nanoseconds = 0;
    }
    timespec relative{};
    relative.tv_sec = static_cast<time_t>(seconds);
    relative.tv_nsec = static_cast<long>(nanoseconds);
    return ::pthread_cond_timedwait_relative_np(&condition, &mutex, &relative);
}

// macOS gives the ordinary scheduler a priority range per thread (15 to 47,
// default 31), which any process may use, so every level is applied and none is
// refused. The levels are spread across the range the system reports.
PriorityOutcome apply_priority(pthread_t thread, Priority level) noexcept
{
    const int low = ::sched_get_priority_min(SCHED_OTHER);
    const int high = ::sched_get_priority_max(SCHED_OTHER);
    const int span = high - low;
    int step = 0;
    switch (level)
    {
    case Priority::lowest:
        step = 0;
        break;
    case Priority::low:
        step = 1;
        break;
    case Priority::normal:
        step = 2;
        break;
    case Priority::high:
        step = 3;
        break;
    case Priority::highest:
        step = 4;
        break;
    }
    sched_param parameter{};
    parameter.sched_priority = low + span * step / 4;
    const int error = ::pthread_setschedparam(thread, SCHED_OTHER, &parameter);
    if (error == 0)
    {
        return PriorityOutcome::applied;
    }
    return error == EPERM ? PriorityOutcome::not_permitted : PriorityOutcome::unsupported;
}

void name_current_thread(const char* name) noexcept
{
    // macOS names only the calling thread.
    ::pthread_setname_np(name);
}

int flush_to_storage(int fd) noexcept
{
    // fsync on macOS hands the data to the drive; F_FULLFSYNC makes the drive
    // write it out. Not every file system supports it, so fall back.
    if (::fcntl(fd, F_FULLFSYNC) == 0)
    {
        return 0;
    }
    return ::fsync(fd) == 0 ? 0 : errno;
}

} // namespace adl::platform::posix_os

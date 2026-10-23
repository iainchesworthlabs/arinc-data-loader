// What Linux does differently from the other POSIX systems; see
// posix/posix_os.hpp for what each function is for.

#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

#include "posix/posix_os.hpp"

namespace adl::platform::posix_os {

const char* host_name() noexcept {
    return "linux";
}

bool is_real_time() noexcept {
    return false;
}

int init_condition(pthread_cond_t& condition) noexcept {
    pthread_condattr_t attributes;
    if (const int error = ::pthread_condattr_init(&attributes); error != 0) {
        return error;
    }
    int error = ::pthread_condattr_setclock(&attributes, CLOCK_MONOTONIC);
    if (error == 0) {
        error = ::pthread_cond_init(&condition, &attributes);
    }
    ::pthread_condattr_destroy(&attributes);
    return error;
}

int wait_condition(pthread_cond_t& condition, pthread_mutex_t& mutex, Duration timeout) noexcept {
    constexpr std::int64_t nanoseconds_per_second = 1'000'000'000;
    timespec now{};
    ::clock_gettime(CLOCK_MONOTONIC, &now);
    std::int64_t seconds = static_cast<std::int64_t>(now.tv_sec);
    std::int64_t nanoseconds = now.tv_nsec + timeout.count() % nanoseconds_per_second;
    seconds += timeout.count() / nanoseconds_per_second;
    if (nanoseconds >= nanoseconds_per_second) {
        nanoseconds -= nanoseconds_per_second;
        ++seconds;
    }
    timespec deadline{};
    deadline.tv_sec = static_cast<time_t>(seconds);
    deadline.tv_nsec = static_cast<long>(nanoseconds);
    return ::pthread_cond_timedwait(&condition, &mutex, &deadline);
}

// The five levels on Linux. The two below normal are policies of the ordinary
// scheduler that any process may switch a thread to; the two above are real-time
// FIFO priorities, which an unprivileged process is refused (CAP_SYS_NICE or
// RLIMIT_RTPRIO) and then runs at the default. FIFO 25 and 50 leave the top half
// of the range to the kernel's own threads and to the watchdog.
PriorityOutcome apply_priority(pthread_t thread, Priority level) noexcept {
    int policy = SCHED_OTHER;
    sched_param parameter{};
    switch (level) {
        case Priority::lowest:
            policy = SCHED_IDLE;
            break;
        case Priority::low:
            policy = SCHED_BATCH;
            break;
        case Priority::normal:
            break;
        case Priority::high:
        case Priority::highest: {
            policy = SCHED_FIFO;
            const int low = ::sched_get_priority_min(SCHED_FIFO);
            const int high = ::sched_get_priority_max(SCHED_FIFO);
            const int span = high - low;
            parameter.sched_priority = low + (level == Priority::high ? span / 4 : span / 2);
            break;
        }
    }
    const int error = ::pthread_setschedparam(thread, policy, &parameter);
    if (error == 0) {
        return PriorityOutcome::applied;
    }
    return error == EPERM ? PriorityOutcome::not_permitted : PriorityOutcome::unsupported;
}

void name_current_thread(const char* name) noexcept {
    // The kernel keeps 15 characters.
    char shortened[16] = {};
    std::strncpy(shortened, name, sizeof shortened - 1);
    ::pthread_setname_np(::pthread_self(), shortened);
}

int flush_to_storage(int fd) noexcept {
    return ::fsync(fd) == 0 ? 0 : errno;
}

}  // namespace adl::platform::posix_os

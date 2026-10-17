// QNX Neutrino. UNBUILT AND UNTESTED: this file has never been compiled against
// the QNX headers, and nothing in CI or in this repository can run it. It is the
// handful of places where QNX differs from the shared POSIX implementation
// (src/platforms/posix), written from the QNX documentation; see
// docs/platform-abstraction.md ("Status of each platform") before relying on
// any of it. See posix/posix_os.hpp for what each function is for.

#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

#include "posix/posix_os.hpp"

namespace adl::platform::posix_os {

const char* host_name() noexcept {
    return "qnx";
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

// QNX priorities run 1 to 255, higher meaning more urgent, and a normal thread
// is at 10. A process that is not privileged is held to 63 or less. The levels
// are a proposal, all below that limit, with `normal` at QNX's own default.
PriorityOutcome apply_priority(pthread_t thread, Priority level) noexcept {
    int priority = 10;
    switch (level) {
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
    const int error = ::pthread_setschedparam(thread, SCHED_RR, &parameter);
    if (error == 0) {
        return PriorityOutcome::applied;
    }
    return error == EPERM ? PriorityOutcome::not_permitted : PriorityOutcome::unsupported;
}

void name_current_thread(const char* name) noexcept {
    ::pthread_setname_np(::pthread_self(), name);
}

int flush_to_storage(int fd) noexcept {
    return ::fsync(fd) == 0 ? 0 : errno;
}

}  // namespace adl::platform::posix_os

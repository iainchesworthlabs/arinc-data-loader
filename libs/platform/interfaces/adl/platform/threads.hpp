#pragma once

// Threads, locks and signals. The protocol core is sans-I/O and owns no
// thread; what needs threads is the runner around it (one thread that
// waits on sockets and the clock) and the application (another). So this is
// the small set a runner needs: start a thread with a priority, join it, a
// mutex, and a counting semaphore, which is also the event (a semaphore with a
// maximum of 1).
//
// Priorities are abstract. The five levels below are ordered, and a platform
// maps each onto its own range, whichever way that range runs. Code outside
// the platform section never sees a native number, and so never has to know
// that VxWorks counts down, that Zephyr's cooperative priorities are negative,
// that FreeRTOS's top is configMAX_PRIORITIES - 1, or that QNX is 1 to 255.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

#include "adl/platform/error.hpp"
#include "adl/platform/owned.hpp"
#include "adl/platform/time.hpp"

namespace adl::platform {

// Most urgent last. A transfer runner wants `high`: it must answer a TFTP
// packet inside the 2 second time-out even while the application is busy. The
// application and everything else run at `normal`; a log writer or a progress
// publisher takes `low`. `highest` is for work that must pre-empt the runner and
// nothing in the loader needs it; `lowest` is for work that should only use
// idle time.
enum class Priority : std::uint8_t { lowest, low, normal, high, highest };

// What the platform managed to do with the priority a thread asked for. A
// desktop is allowed to say `not_permitted` (Linux will not give a thread a
// real-time policy to an unprivileged process) and carry on at the default;
// the caller can log it, and the loader is correct either way, because it
// relies on time-outs and not on scheduling for its guarantees.
enum class PriorityOutcome : std::uint8_t {
    applied,        // the thread runs at the native priority the level maps to
    not_permitted,  // the platform has priorities, and refused this process them
    unsupported,    // the platform has no thread priorities (or the fake)
};

using ThreadEntry = void (*)(void* argument) noexcept;

struct ThreadSpec {
    const char* name = "adl";  // shown by a debugger; truncated where the OS limits it
    Priority priority = Priority::normal;
    // 0 takes the platform's default. A statically allocated platform (FreeRTOS
    // with xTaskCreateStatic, a VxWorks partition) needs `stack_storage`
    // instead, and refuses a spec that gives neither with `invalid_argument`.
    // The desktops ignore stack_storage.
    std::size_t stack_bytes = 0;
    std::span<std::byte> stack_storage{};
    ThreadEntry entry = nullptr;
    void* argument = nullptr;
};

class Thread {
public:
    // Waits for the thread to finish, if it has not been joined: an RTOS has no
    // way to stop a task from outside, so a Thread that is destroyed while its
    // entry function is still running blocks until it returns.
    virtual ~Thread() = default;

    // Blocks until the entry function has returned. A second call returns at once.
    virtual Result<void> join() noexcept = 0;

    [[nodiscard]] virtual PriorityOutcome priority_outcome() const noexcept = 0;
};

// Not recursive. Priority inheritance is used wherever the platform has it
// (every RTOS target does, and a desktop mutex does not need it).
class Mutex {
public:
    virtual ~Mutex() = default;

    virtual void lock() noexcept = 0;
    [[nodiscard]] virtual bool try_lock() noexcept = 0;
    virtual void unlock() noexcept = 0;
};

class LockGuard {
public:
    explicit LockGuard(Mutex& mutex) noexcept : mutex_(mutex) { mutex_.lock(); }
    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;
    ~LockGuard() { mutex_.unlock(); }

private:
    Mutex& mutex_;
};

// A counting semaphore with a ceiling.
class Semaphore {
public:
    virtual ~Semaphore() = default;

    // Adds one. `resource_exhausted` if the count is already at the ceiling,
    // so a signal that nobody has consumed is not lost silently.
    virtual Result<void> post() noexcept = 0;

    // Takes one, waiting up to `timeout`: `timeout` if none arrived, and
    // `interrupted` if the wait was cut short for a reason that is not the
    // clock. `Duration::zero()` polls.
    virtual Result<void> wait(Duration timeout) noexcept = 0;
};

class Threading {
public:
    virtual ~Threading() = default;

    // The thread is running, or runnable, when this returns. `resource_exhausted`
    // when the platform is out of threads or memory for a stack.
    [[nodiscard]] virtual Result<Owned<Thread>> spawn(const ThreadSpec& spec) noexcept = 0;

    [[nodiscard]] virtual Result<Owned<Mutex>> create_mutex() noexcept = 0;

    // `initial` <= `ceiling`, and `ceiling` > 0.
    [[nodiscard]] virtual Result<Owned<Semaphore>> create_semaphore(unsigned initial,
                                                                    unsigned ceiling) noexcept = 0;
};

// Starts `object.run()` in a thread, so a caller does not write the
// function-pointer plumbing. `object` must outlive the thread.
template <class Object>
[[nodiscard]] Result<Owned<Thread>> spawn_member(Threading& threads, ThreadSpec spec,
                                                 Object& object) noexcept {
    struct Thunk {
        static void call(void* argument) noexcept { static_cast<Object*>(argument)->run(); }
    };
    spec.entry = &Thunk::call;
    spec.argument = std::addressof(object);
    return threads.spawn(spec);
}

}  // namespace adl::platform

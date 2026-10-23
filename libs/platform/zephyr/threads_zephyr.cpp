// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/platform/threads.hpp"
#include "detail/nothrow.hpp"
#include "zephyr/zephyr_os.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>

namespace adl::platform::zephyr
{
namespace
{

using zephyr_os::fail_errno;

// A request for less is raised to this: a stack that cannot hold the entry
// function's frame and an interrupt is not a stack.
constexpr std::size_t minimum_stack_bytes = 1024;

// ---- priorities --------------------------------------------------------------
//
// Zephyr counts down: a smaller number is more urgent, and a negative number is
// a cooperative priority, which no other thread can pre-empt. The runner must
// never be given one (a cooperative runner would starve every thread below it
// until it blocked), so every level maps into the pre-emptible range
// 0 .. CONFIG_NUM_PREEMPT_PRIORITIES - 1, and 0 is left to the system.
//
//   highest  1
//   high     a quarter of the way down the range
//   normal   half way
//   low      three quarters of the way
//   lowest   K_LOWEST_APPLICATION_THREAD_PRIO
//
// docs/platform-abstraction.md has the table and what a product still has to check
// against its own thread plan (the network stack's receive thread, in particular).
[[nodiscard]] int native_priority(Priority level) noexcept
{
    const int lowest = K_LOWEST_APPLICATION_THREAD_PRIO;
    // Fewer than four pre-emptible priorities cannot tell five levels apart; the
    // levels then share what there is, in order.
    const int span = std::max(lowest - 1, 0);
    switch (level)
    {
    case Priority::highest:
        return std::min(1, lowest);
    case Priority::high:
        return std::min(1 + span / 4, lowest);
    case Priority::normal:
        return std::min(1 + span / 2, lowest);
    case Priority::low:
        return std::min(1 + (span * 3) / 4, lowest);
    case Priority::lowest:
        return lowest;
    }
    return lowest;
}

// ---- mutex ---------------------------------------------------------------------

class ZephyrMutex final : public Mutex
{
public:
    ZephyrMutex() noexcept
    {
        (void)k_mutex_init(&mutex_);
    }

    void lock() noexcept override
    {
        (void)k_mutex_lock(&mutex_, K_FOREVER);
    }

    bool try_lock() noexcept override
    {
        if (k_mutex_lock(&mutex_, K_NO_WAIT) != 0)
        {
            return false;
        }
        // A k_mutex is recursive, and the interface's is not: a thread that
        // already holds it would just have deepened its own lock. That is not
        // getting the lock.
        if (mutex_.lock_count > 1)
        {
            (void)k_mutex_unlock(&mutex_);
            return false;
        }
        return true;
    }

    void unlock() noexcept override
    {
        (void)k_mutex_unlock(&mutex_);
    }

private:
    k_mutex mutex_{};
};

// ---- semaphore -----------------------------------------------------------------
//
// k_sem would be the native choice, but it stops at its limit without saying so
// and the interface reports a post past the ceiling. A count under a k_mutex with
// a k_condvar (the shape of the POSIX port's) is exact, and the mutex still
// inherits priority.
class ZephyrSemaphore final : public Semaphore
{
public:
    ZephyrSemaphore(Clock& clock, unsigned initial, unsigned ceiling) noexcept
        : clock_(clock), count_(initial), ceiling_(ceiling)
    {
        (void)k_mutex_init(&mutex_);
        (void)k_condvar_init(&condition_);
    }

    Result<void> post() noexcept override
    {
        (void)k_mutex_lock(&mutex_, K_FOREVER);
        const bool full = count_ >= ceiling_;
        if (!full)
        {
            ++count_;
        }
        (void)k_mutex_unlock(&mutex_);
        if (full)
        {
            return fail(ErrorCode::resource_exhausted);
        }
        (void)k_condvar_signal(&condition_);
        return {};
    }

    Result<void> wait(Duration timeout) noexcept override
    {
        const Instant deadline = timeout == forever ? Instant{forever} : clock_.now() + timeout;
        (void)k_mutex_lock(&mutex_, K_FOREVER);
        while (count_ == 0)
        {
            Duration remaining = forever;
            if (timeout != forever)
            {
                remaining = deadline - clock_.now();
                if (remaining <= Duration::zero())
                {
                    (void)k_mutex_unlock(&mutex_);
                    return fail(ErrorCode::timeout);
                }
            }
            // Returns -EAGAIN when the time ran out; the clock decides on the next
            // turn round, because the kernel rounds to its tick.
            (void)k_condvar_wait(&condition_, &mutex_, zephyr_os::to_timeout(remaining));
        }
        --count_;
        (void)k_mutex_unlock(&mutex_);
        return {};
    }

private:
    Clock& clock_;
    unsigned count_;
    unsigned ceiling_;
    k_mutex mutex_{};
    k_condvar condition_{};
};

// ---- thread --------------------------------------------------------------------

class ZephyrThread final : public Thread
{
public:
    explicit ZephyrThread(const ThreadSpec& spec) noexcept : entry_(spec.entry), argument_(spec.argument)
    {
        // The caller's string need not outlive the call.
        const char* name = spec.name != nullptr ? spec.name : "";
        std::size_t i = 0;
        for (; i + 1 < name_.size() && name[i] != '\0'; ++i)
        {
            name_[i] = name[i];
        }
        name_[i] = '\0';
    }

    ~ZephyrThread() override
    {
        (void)join();
        if (stack_ != nullptr && owns_stack_)
        {
            (void)k_thread_stack_free(stack_);
        }
    }

    [[nodiscard]] Result<void> start(const ThreadSpec& spec) noexcept
    {
        std::size_t stack_bytes = spec.stack_bytes != 0 ? std::max(spec.stack_bytes, minimum_stack_bytes)
                                                        : std::size_t{CONFIG_ADL_THREAD_STACK_BYTES};
        if (!spec.stack_storage.empty())
        {
            // Storage the caller made with K_KERNEL_STACK_DEFINE or K_THREAD_STACK_DEFINE:
            // used as it is, so it has to be aligned the way the architecture needs.
            if (reinterpret_cast<std::uintptr_t>(spec.stack_storage.data()) % Z_KERNEL_STACK_OBJ_ALIGN != 0)
            {
                return fail(ErrorCode::invalid_argument);
            }
            stack_ = reinterpret_cast<k_thread_stack_t*>(spec.stack_storage.data());
            stack_bytes = spec.stack_storage.size();
        }
        else
        {
            stack_ = k_thread_stack_alloc(stack_bytes, 0);
            if (stack_ == nullptr)
            {
                return fail(ErrorCode::resource_exhausted);
            }
            owns_stack_ = true;
        }
        priority_level_ = spec.priority;
        // The priority is part of creation, so the thread never runs at any other.
        tid_ = k_thread_create(&thread_, stack_, stack_bytes, &ZephyrThread::main, this, nullptr, nullptr,
                               native_priority(spec.priority), 0, K_NO_WAIT);
        if (tid_ == nullptr)
        {
            return fail(ErrorCode::resource_exhausted);
        }
        joinable_ = true;
        (void)k_thread_name_set(tid_, name_.data());
        return {};
    }

    Result<void> join() noexcept override
    {
        if (!joinable_)
        {
            return {};
        }
        joinable_ = false;
        if (const int error = k_thread_join(&thread_, K_FOREVER); error != 0)
        {
            return fail_errno(error);
        }
        return {};
    }

    PriorityOutcome priority_outcome() const noexcept override
    {
        return PriorityOutcome::applied;
    }

private:
    static void main(void* self, void* /*unused*/, void* /*unused*/) noexcept
    {
        auto* thread = static_cast<ZephyrThread*>(self);
        // The creator may join and destroy the object as soon as the entry function
        // returns; nothing of it is touched after this.
        ThreadEntry entry = thread->entry_;
        void* argument = thread->argument_;
        entry(argument);
    }

    k_thread thread_{};
    k_tid_t tid_ = nullptr;
    k_thread_stack_t* stack_ = nullptr;
    bool owns_stack_ = false;
    std::array<char, 32> name_{};
    ThreadEntry entry_;
    void* argument_;
    bool joinable_ = false;
    Priority priority_level_ = Priority::normal;
};

class ZephyrThreading final : public Threading
{
public:
    explicit ZephyrThreading(Clock& clock) noexcept : clock_(clock) {}

    Result<Owned<Thread>> spawn(const ThreadSpec& spec) noexcept override
    {
        if (spec.entry == nullptr)
        {
            return fail(ErrorCode::invalid_argument);
        }
        auto thread = make_owned_nothrow<Thread, ZephyrThread>(spec);
        if (!thread)
        {
            return fail(ErrorCode::resource_exhausted);
        }
        if (auto started = static_cast<ZephyrThread&>(*thread).start(spec); !started)
        {
            return std::unexpected(started.error());
        }
        return thread;
    }

    Result<Owned<Mutex>> create_mutex() noexcept override
    {
        auto mutex = make_owned_nothrow<Mutex, ZephyrMutex>();
        if (!mutex)
        {
            return fail(ErrorCode::resource_exhausted);
        }
        return mutex;
    }

    Result<Owned<Semaphore>> create_semaphore(unsigned initial, unsigned ceiling) noexcept override
    {
        if (ceiling == 0 || initial > ceiling)
        {
            return fail(ErrorCode::invalid_argument);
        }
        auto semaphore = make_owned_nothrow<Semaphore, ZephyrSemaphore>(clock_, initial, ceiling);
        if (!semaphore)
        {
            return fail(ErrorCode::resource_exhausted);
        }
        return semaphore;
    }

private:
    Clock& clock_;
};

} // namespace

std::unique_ptr<Threading> make_threading(Clock& clock) noexcept
{
    return detail::make_unique_nothrow<ZephyrThreading>(clock);
}

} // namespace adl::platform::zephyr

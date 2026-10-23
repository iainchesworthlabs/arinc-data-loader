#include "adl/platform/threads.hpp"
#include "detail/nothrow.hpp"
#include "posix/posix_os.hpp"

#include <pthread.h>

#include <array>
#include <cerrno>
#include <climits>
#include <memory>

namespace adl::platform::posix
{
namespace
{

using posix_os::fail_errno;

class PosixMutex final : public Mutex
{
public:
    PosixMutex() noexcept
    {
        ok_ = ::pthread_mutex_init(&mutex_, nullptr) == 0;
    }
    ~PosixMutex() override
    {
        if (ok_)
        {
            ::pthread_mutex_destroy(&mutex_);
        }
    }
    [[nodiscard]] bool ok() const noexcept
    {
        return ok_;
    }

    void lock() noexcept override
    {
        ::pthread_mutex_lock(&mutex_);
    }
    bool try_lock() noexcept override
    {
        return ::pthread_mutex_trylock(&mutex_) == 0;
    }
    void unlock() noexcept override
    {
        ::pthread_mutex_unlock(&mutex_);
    }

private:
    pthread_mutex_t mutex_{};
    bool ok_ = false;
};

// A counting semaphore from a mutex and a condition variable. POSIX has
// sem_init, but macOS does not implement unnamed semaphores, and one
// implementation that serves every POSIX system is worth more than the few
// nanoseconds.
class PosixSemaphore final : public Semaphore
{
public:
    PosixSemaphore(Clock& clock, unsigned initial, unsigned ceiling) noexcept
        : clock_(clock), count_(initial), ceiling_(ceiling)
    {
        ok_ = ::pthread_mutex_init(&mutex_, nullptr) == 0;
        if (ok_ && posix_os::init_condition(condition_) != 0)
        {
            ::pthread_mutex_destroy(&mutex_);
            ok_ = false;
        }
    }
    ~PosixSemaphore() override
    {
        if (ok_)
        {
            ::pthread_cond_destroy(&condition_);
            ::pthread_mutex_destroy(&mutex_);
        }
    }
    [[nodiscard]] bool ok() const noexcept
    {
        return ok_;
    }

    Result<void> post() noexcept override
    {
        ::pthread_mutex_lock(&mutex_);
        const bool full = count_ >= ceiling_;
        if (!full)
        {
            ++count_;
        }
        ::pthread_mutex_unlock(&mutex_);
        if (full)
        {
            return fail(ErrorCode::resource_exhausted);
        }
        ::pthread_cond_signal(&condition_);
        return {};
    }

    Result<void> wait(Duration timeout) noexcept override
    {
        const Instant deadline = clock_.now() + timeout;
        ::pthread_mutex_lock(&mutex_);
        while (count_ == 0)
        {
            if (timeout <= Duration::zero())
            {
                ::pthread_mutex_unlock(&mutex_);
                return fail(ErrorCode::timeout);
            }
            if (timeout == forever)
            {
                ::pthread_cond_wait(&condition_, &mutex_);
                continue;
            }
            const Duration remaining = deadline - clock_.now();
            if (remaining <= Duration::zero())
            {
                ::pthread_mutex_unlock(&mutex_);
                return fail(ErrorCode::timeout);
            }
            const int error = posix_os::wait_condition(condition_, mutex_, remaining);
            if (error != 0 && error != ETIMEDOUT)
            {
                ::pthread_mutex_unlock(&mutex_);
                return fail_errno(error);
            }
        }
        --count_;
        ::pthread_mutex_unlock(&mutex_);
        return {};
    }

private:
    Clock& clock_;
    pthread_mutex_t mutex_{};
    pthread_cond_t condition_{};
    unsigned count_;
    unsigned ceiling_;
    bool ok_ = false;
};

class PosixThread final : public Thread
{
public:
    explicit PosixThread(const ThreadSpec& spec) noexcept : entry_(spec.entry), argument_(spec.argument)
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

    ~PosixThread() override
    {
        (void)join();
        ::pthread_cond_destroy(&start_condition_);
        ::pthread_mutex_destroy(&start_mutex_);
    }

    // Creates the thread and waits until it has taken its priority and is about to
    // call the entry function. On success it is running, at the priority the
    // outcome reports.
    [[nodiscard]] Result<void> start(const ThreadSpec& spec) noexcept
    {
        priority_level_ = spec.priority;
        pthread_attr_t attributes;
        if (const int error = ::pthread_attr_init(&attributes); error != 0)
        {
            return fail_errno(error);
        }
        if (spec.stack_bytes != 0)
        {
            const auto minimum = static_cast<std::size_t>(PTHREAD_STACK_MIN);
            ::pthread_attr_setstacksize(&attributes, spec.stack_bytes < minimum ? minimum : spec.stack_bytes);
        }
        const int error = ::pthread_create(&thread_, &attributes, &PosixThread::main, this);
        ::pthread_attr_destroy(&attributes);
        if (error != 0)
        {
            return fail_errno(error);
        }
        joinable_ = true;
        // The thread sets its own priority before its first instruction of the caller's
        // code, so that nothing runs at its creator's priority by accident (a thread
        // inherits it), and tells us how it went.
        ::pthread_mutex_lock(&start_mutex_);
        while (!started_)
        {
            ::pthread_cond_wait(&start_condition_, &start_mutex_);
        }
        ::pthread_mutex_unlock(&start_mutex_);
        return {};
    }

    Result<void> join() noexcept override
    {
        if (!joinable_)
        {
            return {};
        }
        joinable_ = false;
        if (const int error = ::pthread_join(thread_, nullptr); error != 0)
        {
            return fail_errno(error);
        }
        return {};
    }

    PriorityOutcome priority_outcome() const noexcept override
    {
        return priority_;
    }

private:
    static void* main(void* self) noexcept
    {
        auto* thread = static_cast<PosixThread*>(self);
        posix_os::name_current_thread(thread->name_.data());
        const PriorityOutcome outcome = posix_os::apply_priority(::pthread_self(), thread->priority_level_);
        ThreadEntry entry = thread->entry_;
        void* argument = thread->argument_;
        // After this the creator may return from spawn, and even destroy the object
        // (which joins): nothing of `thread` is touched again.
        ::pthread_mutex_lock(&thread->start_mutex_);
        thread->priority_ = outcome;
        thread->started_ = true;
        ::pthread_cond_signal(&thread->start_condition_);
        ::pthread_mutex_unlock(&thread->start_mutex_);
        entry(argument);
        return nullptr;
    }

    pthread_t thread_{};
    std::array<char, 32> name_{};
    ThreadEntry entry_;
    void* argument_;
    bool joinable_ = false;
    Priority priority_level_ = Priority::normal;
    PriorityOutcome priority_ = PriorityOutcome::unsupported;
    pthread_mutex_t start_mutex_ = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t start_condition_ = PTHREAD_COND_INITIALIZER;
    bool started_ = false;
};

class PosixThreading final : public Threading
{
public:
    explicit PosixThreading(Clock& clock) noexcept : clock_(clock) {}

    Result<Owned<Thread>> spawn(const ThreadSpec& spec) noexcept override
    {
        if (spec.entry == nullptr)
        {
            return fail(ErrorCode::invalid_argument);
        }
        auto thread = make_owned_nothrow<Thread, PosixThread>(spec);
        if (!thread)
        {
            return fail(ErrorCode::resource_exhausted);
        }
        if (auto started = static_cast<PosixThread&>(*thread).start(spec); !started)
        {
            return std::unexpected(started.error());
        }
        return thread;
    }

    Result<Owned<Mutex>> create_mutex() noexcept override
    {
        auto mutex = make_owned_nothrow<Mutex, PosixMutex>();
        if (!mutex || !static_cast<PosixMutex&>(*mutex).ok())
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
        auto semaphore = make_owned_nothrow<Semaphore, PosixSemaphore>(clock_, initial, ceiling);
        if (!semaphore || !static_cast<PosixSemaphore&>(*semaphore).ok())
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
    return detail::make_unique_nothrow<PosixThreading>(clock);
}

} // namespace adl::platform::posix

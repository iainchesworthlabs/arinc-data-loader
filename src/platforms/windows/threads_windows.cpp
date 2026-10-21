#include <windows.h>

#include <process.h>

#include <array>
#include <memory>

#include "adl/platform/threads.hpp"
#include "detail/nothrow.hpp"
#include "windows/windows_services.hpp"

namespace adl::platform::windows {
namespace {

// A slim reader/writer lock used only exclusively: the cheapest non-recursive
// mutex Windows has, and one that needs no handle and cannot fail to be made.
class WindowsMutex final : public Mutex {
public:
    WindowsMutex() noexcept { ::InitializeSRWLock(&lock_); }

    void lock() noexcept override { ::AcquireSRWLockExclusive(&lock_); }
    bool try_lock() noexcept override { return ::TryAcquireSRWLockExclusive(&lock_) != 0; }
    void unlock() noexcept override { ::ReleaseSRWLockExclusive(&lock_); }

private:
    SRWLOCK lock_{};
};

class WindowsSemaphore final : public Semaphore {
public:
    WindowsSemaphore(Clock& clock, unsigned initial, unsigned ceiling) noexcept : clock_(clock) {
        handle_ = ::CreateSemaphoreW(nullptr, static_cast<LONG>(initial),
                                     static_cast<LONG>(ceiling), nullptr);
    }
    ~WindowsSemaphore() override {
        if (handle_ != nullptr) {
            ::CloseHandle(handle_);
        }
    }
    WindowsSemaphore(const WindowsSemaphore&) = delete;
    WindowsSemaphore& operator=(const WindowsSemaphore&) = delete;
    WindowsSemaphore(WindowsSemaphore&&) = delete;
    WindowsSemaphore& operator=(WindowsSemaphore&&) = delete;
    [[nodiscard]] bool ok() const noexcept { return handle_ != nullptr; }

    Result<void> post() noexcept override {
        if (!::ReleaseSemaphore(handle_, 1, nullptr)) {
            return fail_win32(::GetLastError());
        }
        return {};
    }

    Result<void> wait(Duration timeout) noexcept override {
        // A finite wait is limited to 49 days, and the system's timer can end a wait a
        // little before the time asked for by the clock the rest of the program uses, so
        // the wait goes round again until that clock says the time is up.
        constexpr DWORD longest = 0x7fffffff;
        const Instant deadline = clock_.now() + timeout;
        for (;;) {
            const Duration remaining = timeout == forever ? forever : deadline - clock_.now();
            const DWORD ms = remaining == forever ? INFINITE : to_milliseconds(remaining, longest);
            const DWORD result = ::WaitForSingleObject(handle_, ms);
            if (result == WAIT_OBJECT_0) {
                return {};
            }
            if (result != WAIT_TIMEOUT) {
                return fail_win32(result == WAIT_FAILED ? ::GetLastError() : ERROR_INVALID_HANDLE);
            }
            if (timeout != forever && deadline - clock_.now() <= Duration::zero()) {
                return fail(ErrorCode::timeout);
            }
        }
    }

private:
    Clock& clock_;
    HANDLE handle_ = nullptr;
};

class WindowsThread final : public Thread {
public:
    explicit WindowsThread(const ThreadSpec& spec) noexcept
        : entry_(spec.entry), argument_(spec.argument) {}
    ~WindowsThread() override { (void)join(); }
    WindowsThread(const WindowsThread&) = delete;
    WindowsThread& operator=(const WindowsThread&) = delete;
    WindowsThread(WindowsThread&&) = delete;
    WindowsThread& operator=(WindowsThread&&) = delete;

    [[nodiscard]] Result<void> start(const ThreadSpec& spec) noexcept {
        // Created suspended so that the priority and the name are in place before the
        // first instruction of the entry function runs.
        const auto created =
            ::_beginthreadex(nullptr, static_cast<unsigned>(spec.stack_bytes), &WindowsThread::main,
                             this, CREATE_SUSPENDED | STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
        if (created == 0) {
            return fail_win32(::GetLastError());
        }
        handle_ = reinterpret_cast<HANDLE>(created);

        priority_ = ::SetThreadPriority(handle_, native_priority(spec.priority))
                        ? PriorityOutcome::applied
                        : PriorityOutcome::not_permitted;

        std::array<wchar_t, 32> wide{};
        for (std::size_t i = 0; i + 1 < wide.size() && spec.name != nullptr && spec.name[i] != '\0';
             ++i) {
            wide[i] = static_cast<unsigned char>(spec.name[i]);
        }
        ::SetThreadDescription(handle_, wide.data());

        if (::ResumeThread(handle_) == static_cast<DWORD>(-1)) {
            const DWORD error = ::GetLastError();
            // The thread was created suspended and ResumeThread failed, so it has
            // never run a single instruction: TerminateThread only releases it.
#pragma warning(suppress : 6258)
            ::TerminateThread(handle_, 1);
            ::CloseHandle(handle_);
            handle_ = nullptr;
            return fail_win32(error);
        }
        return {};
    }

    Result<void> join() noexcept override {
        if (handle_ == nullptr) {
            return {};
        }
        const DWORD result = ::WaitForSingleObject(handle_, INFINITE);
        const DWORD error = result == WAIT_FAILED ? ::GetLastError() : 0;
        ::CloseHandle(handle_);
        handle_ = nullptr;
        if (result == WAIT_FAILED) {
            return fail_win32(error);
        }
        return {};
    }

    PriorityOutcome priority_outcome() const noexcept override { return priority_; }

private:
    // The levels on Windows: the five that sit around the normal one of a normal
    // process. THREAD_PRIORITY_TIME_CRITICAL is left alone: at 15 it can starve
    // the system, and nothing in a loader wants it.
    static int native_priority(Priority level) noexcept {
        switch (level) {
            case Priority::lowest:
                return THREAD_PRIORITY_LOWEST;
            case Priority::low:
                return THREAD_PRIORITY_BELOW_NORMAL;
            case Priority::normal:
                return THREAD_PRIORITY_NORMAL;
            case Priority::high:
                return THREAD_PRIORITY_ABOVE_NORMAL;
            case Priority::highest:
                return THREAD_PRIORITY_HIGHEST;
        }
        return THREAD_PRIORITY_NORMAL;
    }

    static unsigned __stdcall main(void* self) noexcept {
        auto* thread = static_cast<WindowsThread*>(self);
        thread->entry_(thread->argument_);
        return 0;
    }

    HANDLE handle_ = nullptr;
    ThreadEntry entry_;
    void* argument_;
    PriorityOutcome priority_ = PriorityOutcome::unsupported;
};

class WindowsThreading final : public Threading {
public:
    explicit WindowsThreading(Clock& clock) noexcept : clock_(clock) {}

    Result<Owned<Thread>> spawn(const ThreadSpec& spec) noexcept override {
        if (spec.entry == nullptr) {
            return fail(ErrorCode::invalid_argument);
        }
        auto thread = make_owned_nothrow<Thread, WindowsThread>(spec);
        if (!thread) {
            return fail(ErrorCode::resource_exhausted);
        }
        if (auto started = static_cast<WindowsThread&>(*thread).start(spec); !started) {
            return std::unexpected(started.error());
        }
        return thread;
    }

    Result<Owned<Mutex>> create_mutex() noexcept override {
        auto mutex = make_owned_nothrow<Mutex, WindowsMutex>();
        if (!mutex) {
            return fail(ErrorCode::resource_exhausted);
        }
        return mutex;
    }

    Result<Owned<Semaphore>> create_semaphore(unsigned initial,
                                              unsigned ceiling) noexcept override {
        if (ceiling == 0 || initial > ceiling || ceiling > 0x7fffffffu) {
            return fail(ErrorCode::invalid_argument);
        }
        auto semaphore = make_owned_nothrow<Semaphore, WindowsSemaphore>(clock_, initial, ceiling);
        if (!semaphore || !static_cast<WindowsSemaphore&>(*semaphore).ok()) {
            return fail(ErrorCode::resource_exhausted);
        }
        return semaphore;
    }

private:
    Clock& clock_;
};

}  // namespace

std::unique_ptr<Threading> make_threading(Clock& clock) noexcept {
    return detail::make_unique_nothrow<WindowsThreading>(clock);
}

}  // namespace adl::platform::windows

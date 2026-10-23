#include "adl/platform/threads.hpp"
#include "detail/nothrow.hpp"
#include "freertos/freertos_os.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>

namespace adl::platform::freertos
{
namespace
{

// ---- priorities --------------------------------------------------------------
//
// FreeRTOS counts up: a larger number is more urgent, and 0 is the idle task's. The
// network stack's task (lwIP's tcpip_thread) must run before a thread waiting on one of its
// packets can, so every level maps below it, and none to 0. With the network task at
// priority N, the levels are
//
//   highest  N - 1
//   high     three quarters of that
//   normal   half of it
//   low      a quarter of it
//   lowest   1
//
// each at least 1. docs/platform-abstraction.md has the table.
struct Priorities
{
    UBaseType_t lowest;
    UBaseType_t low;
    UBaseType_t normal;
    UBaseType_t high;
    UBaseType_t highest;

    explicit Priorities(unsigned network_task) noexcept
    {
        const unsigned network = network_task != 0           ? network_task
                                 : configMAX_PRIORITIES >= 3 ? static_cast<unsigned>(configMAX_PRIORITIES) - 2
                                                             : 2;
        const unsigned ceiling = std::max(1U, network - 1);
        highest = ceiling;
        high = std::max(1U, ceiling * 3 / 4);
        normal = std::max(1U, ceiling / 2);
        low = std::max(1U, ceiling / 4);
        lowest = 1;
    }

    [[nodiscard]] UBaseType_t of(Priority level) const noexcept
    {
        switch (level)
        {
        case Priority::highest:
            return highest;
        case Priority::high:
            return high;
        case Priority::normal:
            return normal;
        case Priority::low:
            return low;
        case Priority::lowest:
            return lowest;
        }
        return normal;
    }
};

// ---- mutex ---------------------------------------------------------------------

class FreeRtosMutex final : public Mutex
{
public:
    FreeRtosMutex() noexcept : handle_(xSemaphoreCreateMutex()) {}
    ~FreeRtosMutex() override
    {
        if (handle_ != nullptr)
        {
            vSemaphoreDelete(handle_);
        }
    }
    [[nodiscard]] bool ok() const noexcept
    {
        return handle_ != nullptr;
    }

    void lock() noexcept override
    {
        while (xSemaphoreTake(handle_, to_hour()) != pdTRUE)
        {
        }
    }

    // A FreeRTOS mutex is not recursive and a task that holds it cannot take it again, so a
    // try_lock by the holder answers false, as the interface says.
    bool try_lock() noexcept override
    {
        return xSemaphoreTake(handle_, 0) == pdTRUE;
    }

    void unlock() noexcept override
    {
        (void)xSemaphoreGive(handle_);
    }

private:
    [[nodiscard]] static TickType_t to_hour() noexcept
    {
        return freertos_os::to_ticks(freertos_os::longest_kernel_wait);
    }

    SemaphoreHandle_t handle_;
};

// ---- semaphore -----------------------------------------------------------------

class FreeRtosSemaphore final : public Semaphore
{
public:
    FreeRtosSemaphore(Clock& clock, unsigned initial, unsigned ceiling) noexcept
        : clock_(clock), handle_(xSemaphoreCreateCounting(ceiling, initial))
    {
    }
    ~FreeRtosSemaphore() override
    {
        if (handle_ != nullptr)
        {
            vSemaphoreDelete(handle_);
        }
    }
    [[nodiscard]] bool ok() const noexcept
    {
        return handle_ != nullptr;
    }

    Result<void> post() noexcept override
    {
        // The kernel refuses a give at the ceiling, which is what the interface reports.
        if (xSemaphoreGive(handle_) != pdTRUE)
        {
            return fail(ErrorCode::resource_exhausted);
        }
        return {};
    }

    Result<void> wait(Duration timeout) noexcept override
    {
        const bool got = freertos_os::wait_for(clock_, timeout, [this](TickType_t ticks) noexcept
                                               { return xSemaphoreTake(handle_, ticks) == pdTRUE; });
        if (!got)
        {
            return fail(ErrorCode::timeout);
        }
        return {};
    }

private:
    Clock& clock_;
    SemaphoreHandle_t handle_;
};

// ---- thread --------------------------------------------------------------------
//
// A task has no join, and the kernel deletes one that returns only some time later, in the
// idle task, so a task that has finished is not something its creator can wait for or free
// behind. The task's last act is to signal a semaphore; it then waits to be deleted, and the
// joiner deletes it from outside, after which the kernel (or the caller, for a task made
// from static storage) has nothing left of it.
class FreeRtosThread final : public Thread
{
public:
    explicit FreeRtosThread(const ThreadSpec& spec) noexcept : entry_(spec.entry), argument_(spec.argument)
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

    ~FreeRtosThread() override
    {
        (void)join();
        if (finished_ != nullptr)
        {
            vSemaphoreDelete(finished_);
        }
    }

    [[nodiscard]] Result<void> start(const ThreadSpec& spec, UBaseType_t priority) noexcept
    {
        finished_ = xSemaphoreCreateBinary();
        if (finished_ == nullptr)
        {
            return fail(ErrorCode::resource_exhausted);
        }
        BaseType_t created = pdFAIL;
        if (!spec.stack_storage.empty())
        {
            // Storage the caller made: used as it is, so it has to be aligned for a stack.
            const auto start = reinterpret_cast<std::uintptr_t>(spec.stack_storage.data());
            if (start % portBYTE_ALIGNMENT != 0 ||
                spec.stack_storage.size() < configMINIMAL_STACK_SIZE * sizeof(StackType_t))
            {
                return fail(ErrorCode::invalid_argument);
            }
            handle_ = xTaskCreateStatic(
                &FreeRtosThread::main, name_.data(),
                static_cast<configSTACK_DEPTH_TYPE>(spec.stack_storage.size() / sizeof(StackType_t)), this, priority,
                reinterpret_cast<StackType_t*>(spec.stack_storage.data()), &static_task_);
            created = handle_ != nullptr ? pdPASS : pdFAIL;
        }
        else
        {
            const std::size_t bytes = spec.stack_bytes != 0 ? spec.stack_bytes : default_stack_bytes;
            const std::size_t words = std::max<std::size_t>((bytes + sizeof(StackType_t) - 1) / sizeof(StackType_t),
                                                            configMINIMAL_STACK_SIZE);
            created = xTaskCreate(&FreeRtosThread::main, name_.data(), static_cast<configSTACK_DEPTH_TYPE>(words), this,
                                  priority, &handle_);
        }
        if (created != pdPASS)
        {
            handle_ = nullptr;
            return fail(ErrorCode::resource_exhausted);
        }
        return {};
    }

    Result<void> join() noexcept override
    {
        if (handle_ == nullptr)
        {
            return {};
        }
        while (xSemaphoreTake(finished_, freertos_os::to_ticks(freertos_os::longest_kernel_wait)) != pdTRUE)
        {
        }
        vTaskDelete(handle_);
        handle_ = nullptr;
        return {};
    }

    PriorityOutcome priority_outcome() const noexcept override
    {
        return PriorityOutcome::applied;
    }

private:
    static constexpr std::size_t default_stack_bytes = 8192;

    // Not noexcept, on purpose: a task is deleted from outside by the kernel, which on the POSIX
    // simulator port cancels the thread, and a cancellation unwinds the stack with an exception
    // the C++ runtime treats as foreign. A noexcept frame in its way would end the program.
    static void main(void* self)
    {
        auto* thread = static_cast<FreeRtosThread*>(self);
        ThreadEntry entry = thread->entry_;
        void* argument = thread->argument_;
        SemaphoreHandle_t finished = thread->finished_;
        entry(argument);
        // After this the creator may delete the task and the object; nothing of the object is
        // touched again.
        (void)xSemaphoreGive(finished);
        for (;;)
        {
            vTaskDelay(freertos_os::to_ticks(freertos_os::longest_kernel_wait));
        }
    }

    TaskHandle_t handle_ = nullptr;
    StaticTask_t static_task_{};
    SemaphoreHandle_t finished_ = nullptr;
    std::array<char, 32> name_{};
    ThreadEntry entry_;
    void* argument_;
};

class FreeRtosThreading final : public Threading
{
public:
    FreeRtosThreading(Clock& clock, unsigned network_task_priority) noexcept
        : clock_(clock), priorities_(network_task_priority)
    {
    }

    Result<Owned<Thread>> spawn(const ThreadSpec& spec) noexcept override
    {
        if (spec.entry == nullptr)
        {
            return fail(ErrorCode::invalid_argument);
        }
        auto thread = make_owned_nothrow<Thread, FreeRtosThread>(spec);
        if (!thread)
        {
            return fail(ErrorCode::resource_exhausted);
        }
        if (auto started = static_cast<FreeRtosThread&>(*thread).start(spec, priorities_.of(spec.priority)); !started)
        {
            return std::unexpected(started.error());
        }
        return thread;
    }

    Result<Owned<Mutex>> create_mutex() noexcept override
    {
        auto mutex = make_owned_nothrow<Mutex, FreeRtosMutex>();
        if (!mutex || !static_cast<FreeRtosMutex&>(*mutex).ok())
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
        auto semaphore = make_owned_nothrow<Semaphore, FreeRtosSemaphore>(clock_, initial, ceiling);
        if (!semaphore || !static_cast<FreeRtosSemaphore&>(*semaphore).ok())
        {
            return fail(ErrorCode::resource_exhausted);
        }
        return semaphore;
    }

private:
    Clock& clock_;
    Priorities priorities_;
};

} // namespace

std::unique_ptr<Threading> make_threading(Clock& clock, unsigned network_task_priority) noexcept
{
    return detail::make_unique_nothrow<FreeRtosThreading>(clock, network_task_priority);
}

} // namespace adl::platform::freertos

// lwIP's system layer (sys_arch) on FreeRTOS, for the simulator program: semaphores, mutexes,
// mailboxes, threads and the clock, in the terms lwIP's sys.h asks for them. This is the port
// a product's SDK usually brings with its lwIP; it is here because the simulator is a product
// of its own, and it is not part of the platform layer (libs/platform/freertos), which uses
// lwIP's sockets API and whatever port is under it.

#include <lwip/err.h>
#include <lwip/sys.h>

#include <cstdint>

namespace
{

// A wait of `timeout_ms` (0 means for ever, lwIP's convention) in ticks, in pieces of at most
// an hour so that a kernel without INCLUDE_vTaskSuspend has no special case.
constexpr TickType_t hour_in_ticks()
{
    return static_cast<TickType_t>(3600UL * configTICK_RATE_HZ);
}

[[nodiscard]] u32_t elapsed_ms(TickType_t since)
{
    const TickType_t ticks = xTaskGetTickCount() - since;
    return static_cast<u32_t>(ticks * 1000U / configTICK_RATE_HZ);
}

} // namespace

extern "C"
{

    void sys_init(void) {}

    u32_t sys_now(void)
    {
        return static_cast<u32_t>(xTaskGetTickCount() * 1000U / configTICK_RATE_HZ);
    }

    u32_t sys_jiffies(void)
    {
        return sys_now();
    }

    sys_prot_t sys_arch_protect(void)
    {
        taskENTER_CRITICAL();
        return 1;
    }

    void sys_arch_unprotect(sys_prot_t /*pval*/)
    {
        taskEXIT_CRITICAL();
    }

    // ---- mutexes -----------------------------------------------------------------------

    err_t sys_mutex_new(sys_mutex_t* mutex)
    {
        *mutex = xSemaphoreCreateMutex();
        return *mutex != nullptr ? ERR_OK : ERR_MEM;
    }

    void sys_mutex_lock(sys_mutex_t* mutex)
    {
        while (xSemaphoreTake(*mutex, hour_in_ticks()) != pdTRUE)
        {
        }
    }

    void sys_mutex_unlock(sys_mutex_t* mutex)
    {
        (void)xSemaphoreGive(*mutex);
    }

    void sys_mutex_free(sys_mutex_t* mutex)
    {
        vSemaphoreDelete(*mutex);
        *mutex = nullptr;
    }

    int sys_mutex_valid(sys_mutex_t* mutex)
    {
        return *mutex != nullptr;
    }

    void sys_mutex_set_invalid(sys_mutex_t* mutex)
    {
        *mutex = nullptr;
    }

    // ---- semaphores --------------------------------------------------------------------

    err_t sys_sem_new(sys_sem_t* sem, u8_t count)
    {
        *sem = xSemaphoreCreateBinary();
        if (*sem == nullptr)
        {
            return ERR_MEM;
        }
        if (count != 0)
        {
            (void)xSemaphoreGive(*sem);
        }
        return ERR_OK;
    }

    void sys_sem_signal(sys_sem_t* sem)
    {
        (void)xSemaphoreGive(*sem);
    }

    u32_t sys_arch_sem_wait(sys_sem_t* sem, u32_t timeout_ms)
    {
        const TickType_t start = xTaskGetTickCount();
        if (timeout_ms == 0)
        {
            while (xSemaphoreTake(*sem, hour_in_ticks()) != pdTRUE)
            {
            }
            return elapsed_ms(start);
        }
        const TickType_t ticks =
            static_cast<TickType_t>((static_cast<std::uint64_t>(timeout_ms) * configTICK_RATE_HZ + 999U) / 1000U);
        if (xSemaphoreTake(*sem, ticks) != pdTRUE)
        {
            return SYS_ARCH_TIMEOUT;
        }
        return elapsed_ms(start);
    }

    void sys_sem_free(sys_sem_t* sem)
    {
        vSemaphoreDelete(*sem);
        *sem = nullptr;
    }

    int sys_sem_valid(sys_sem_t* sem)
    {
        return *sem != nullptr;
    }

    void sys_sem_set_invalid(sys_sem_t* sem)
    {
        *sem = nullptr;
    }

    // ---- mailboxes ---------------------------------------------------------------------

    err_t sys_mbox_new(sys_mbox_t* mbox, int size)
    {
        *mbox = xQueueCreate(static_cast<UBaseType_t>(size), sizeof(void*));
        return *mbox != nullptr ? ERR_OK : ERR_MEM;
    }

    void sys_mbox_post(sys_mbox_t* mbox, void* msg)
    {
        while (xQueueSend(*mbox, &msg, hour_in_ticks()) != pdTRUE)
        {
        }
    }

    err_t sys_mbox_trypost(sys_mbox_t* mbox, void* msg)
    {
        return xQueueSend(*mbox, &msg, 0) == pdTRUE ? ERR_OK : ERR_MEM;
    }

    err_t sys_mbox_trypost_fromisr(sys_mbox_t* mbox, void* msg)
    {
        return sys_mbox_trypost(mbox, msg);
    }

    u32_t sys_arch_mbox_fetch(sys_mbox_t* mbox, void** msg, u32_t timeout_ms)
    {
        void* dummy = nullptr;
        void** into = msg != nullptr ? msg : &dummy;
        const TickType_t start = xTaskGetTickCount();
        if (timeout_ms == 0)
        {
            while (xQueueReceive(*mbox, into, hour_in_ticks()) != pdTRUE)
            {
            }
            return elapsed_ms(start);
        }
        const TickType_t ticks =
            static_cast<TickType_t>((static_cast<std::uint64_t>(timeout_ms) * configTICK_RATE_HZ + 999U) / 1000U);
        if (xQueueReceive(*mbox, into, ticks) != pdTRUE)
        {
            *into = nullptr;
            return SYS_ARCH_TIMEOUT;
        }
        return elapsed_ms(start);
    }

    u32_t sys_arch_mbox_tryfetch(sys_mbox_t* mbox, void** msg)
    {
        void* dummy = nullptr;
        void** into = msg != nullptr ? msg : &dummy;
        if (xQueueReceive(*mbox, into, 0) != pdTRUE)
        {
            return SYS_MBOX_EMPTY;
        }
        return 0;
    }

    void sys_mbox_free(sys_mbox_t* mbox)
    {
        vQueueDelete(*mbox);
        *mbox = nullptr;
    }

    int sys_mbox_valid(sys_mbox_t* mbox)
    {
        return *mbox != nullptr;
    }

    void sys_mbox_set_invalid(sys_mbox_t* mbox)
    {
        *mbox = nullptr;
    }

    // ---- threads -----------------------------------------------------------------------

    sys_thread_t sys_thread_new(const char* name, lwip_thread_fn thread, void* arg, int stacksize, int prio)
    {
        TaskHandle_t handle = nullptr;
        // lwIP's stack sizes are in the units FreeRTOS wants (words), as in lwIP's own FreeRTOS port.
        if (xTaskCreate(thread, name, static_cast<configSTACK_DEPTH_TYPE>(stacksize), arg,
                        static_cast<UBaseType_t>(prio), &handle) != pdPASS)
        {
            return nullptr;
        }
        return handle;
    }

} // extern "C"

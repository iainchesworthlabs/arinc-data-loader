#pragma once

// A bounded message queue built from a Threading's own Mutex and Semaphore, as
// an Ipc. It is portable code (no OS header), which is why it is here and not
// under a platform: the desktop platforms use it, and so does the fake. A port
// to an RTOS replaces it with the system's native queue (a FreeRTOS queue, a
// VxWorks msgQ, a Zephyr k_msgq, a QNX channel), which can be used from an
// interrupt and does priority inheritance.

#include <cstddef>
#include <memory>

#include "adl/platform/ipc.hpp"
#include "adl/platform/threads.hpp"

namespace adl::platform {

class RingIpc final : public Ipc {
public:
    explicit RingIpc(Threading& threads) noexcept : threads_(threads) {}

    [[nodiscard]] Result<Owned<MessageQueue>> create_queue(const QueueSpec& spec) noexcept override;

private:
    Threading& threads_;
};

}  // namespace adl::platform

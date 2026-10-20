#include <climits>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>

#include "adl/platform/ipc.hpp"
#include "adl/platform/memory.hpp"
#include "detail/nothrow.hpp"
#include "freertos/freertos_os.hpp"

namespace adl::platform::freertos {
namespace {

// A bounded queue of whole messages, on the kernel's own queues.
//
// A FreeRTOS queue copies items of one fixed size, and a message here is anything up to
// message_bytes() long, so the messages live in slots and two kernel queues carry the slot
// numbers: one of the slots that are free and one of those that are full. A sender takes a
// free slot, fills it and puts its number on the full queue; a receiver takes a number from
// the full queue, copies the slot out and gives the number back. The kernel does the
// blocking, the priority-ordered wake-up and the ordering, and a message cannot be seen half
// written.
class FreeRtosQueue final : public MessageQueue {
public:
    FreeRtosQueue(const QueueSpec& spec, std::size_t slot_bytes, std::byte* slots,
                  std::unique_ptr<std::byte[]> owned_slots, Clock& clock) noexcept
        : message_bytes_(spec.message_bytes),
          capacity_(spec.capacity),
          slot_bytes_(slot_bytes),
          slots_(slots),
          owned_slots_(std::move(owned_slots)),
          clock_(clock),
          free_(xQueueCreate(static_cast<UBaseType_t>(spec.capacity), sizeof(std::uint32_t))),
          full_(xQueueCreate(static_cast<UBaseType_t>(spec.capacity), sizeof(std::uint32_t))) {
        if (free_ != nullptr) {
            for (std::uint32_t slot = 0; slot < capacity_; ++slot) {
                (void)xQueueSend(free_, &slot, 0);
            }
        }
    }

    ~FreeRtosQueue() override {
        if (free_ != nullptr) {
            vQueueDelete(free_);
        }
        if (full_ != nullptr) {
            vQueueDelete(full_);
        }
    }

    [[nodiscard]] bool ok() const noexcept { return free_ != nullptr && full_ != nullptr; }

    Result<void> send(std::span<const std::byte> message, Duration timeout) noexcept override {
        if (message.size() > message_bytes_) {
            return fail(ErrorCode::invalid_argument);
        }
        std::uint32_t slot = 0;
        if (!take(free_, timeout, slot)) {
            return fail(ErrorCode::timeout);
        }
        std::byte* place = slots_ + static_cast<std::size_t>(slot) * slot_bytes_;
        const std::size_t length = message.size();
        std::memcpy(place, &length, sizeof length);
        if (length != 0) {
            std::memcpy(place + sizeof length, message.data(), length);
        }
        // There is room: the full queue holds as many numbers as there are slots.
        (void)xQueueSend(full_, &slot, portMAX_DELAY);
        return {};
    }

    Result<std::size_t> receive(std::span<std::byte> out, Duration timeout) noexcept override {
        if (out.size() < message_bytes_) {
            return fail(ErrorCode::invalid_argument);
        }
        std::uint32_t slot = 0;
        if (!take(full_, timeout, slot)) {
            return fail(ErrorCode::timeout);
        }
        const std::byte* place = slots_ + static_cast<std::size_t>(slot) * slot_bytes_;
        std::size_t length = 0;
        std::memcpy(&length, place, sizeof length);
        if (length != 0) {
            std::memcpy(out.data(), place + sizeof length, length);
        }
        (void)xQueueSend(free_, &slot, portMAX_DELAY);
        return length;
    }

    std::size_t size() const noexcept override { return uxQueueMessagesWaiting(full_); }
    std::size_t capacity() const noexcept override { return capacity_; }
    std::size_t message_bytes() const noexcept override { return message_bytes_; }

private:
    // A slot number from `from`, waiting up to `timeout` by the clock: the kernel rounds to
    // its tick, so the clock decides when the time is up.
    [[nodiscard]] bool take(QueueHandle_t from, Duration timeout, std::uint32_t& slot) noexcept {
        return freertos_os::wait_for(clock_, timeout, [&](TickType_t ticks) noexcept {
            return xQueueReceive(from, &slot, ticks) == pdTRUE;
        });
    }

    std::size_t message_bytes_;
    std::size_t capacity_;
    std::size_t slot_bytes_;
    std::byte* slots_;
    std::unique_ptr<std::byte[]> owned_slots_;
    Clock& clock_;
    QueueHandle_t free_;
    QueueHandle_t full_;
};

class FreeRtosIpc final : public Ipc {
public:
    explicit FreeRtosIpc(Clock& clock) noexcept : clock_(clock) {}

    Result<Owned<MessageQueue>> create_queue(const QueueSpec& spec) noexcept override {
        if (spec.message_bytes == 0 || spec.capacity == 0 || spec.capacity > UINT32_MAX ||
            spec.message_bytes > (static_cast<std::size_t>(-1) / 4) / spec.capacity) {
            return fail(ErrorCode::invalid_argument);
        }
        const std::size_t slot_bytes =
            align_up(sizeof(std::size_t) + spec.message_bytes, alignof(std::size_t));
        const std::size_t needed = slot_bytes * spec.capacity;

        std::byte* slots = nullptr;
        std::unique_ptr<std::byte[]> owned;
        if (spec.storage.empty()) {
            owned.reset(new (std::nothrow) std::byte[needed]);
            if (!owned) {
                return fail(ErrorCode::resource_exhausted);
            }
            slots = owned.get();
        } else {
            // The caller's block need not be aligned for a size_t; storage_required() leaves
            // room for aligning its start.
            const auto start = reinterpret_cast<std::uintptr_t>(spec.storage.data());
            const auto skip = static_cast<std::size_t>(
                ((start + alignof(std::size_t) - 1) & ~(alignof(std::size_t) - 1)) - start);
            if (skip > spec.storage.size() || spec.storage.size() - skip < needed) {
                return fail(ErrorCode::invalid_argument);
            }
            slots = spec.storage.data() + skip;
        }

        auto queue = make_owned_nothrow<MessageQueue, FreeRtosQueue>(spec, slot_bytes, slots,
                                                                     std::move(owned), clock_);
        if (!queue || !static_cast<FreeRtosQueue&>(*queue).ok()) {
            return fail(ErrorCode::resource_exhausted);
        }
        return queue;
    }

private:
    Clock& clock_;
};

}  // namespace

std::unique_ptr<Ipc> make_ipc(Clock& clock) noexcept {
    return detail::make_unique_nothrow<FreeRtosIpc>(clock);
}

}  // namespace adl::platform::freertos

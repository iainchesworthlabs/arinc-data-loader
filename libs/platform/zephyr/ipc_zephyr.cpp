#include "adl/platform/ipc.hpp"
#include "adl/platform/memory.hpp"
#include "detail/nothrow.hpp"
#include "zephyr/zephyr_os.hpp"

#include <climits>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>

namespace adl::platform::zephyr
{
namespace
{

using zephyr_os::fail_errno;

// A bounded queue of whole messages, on the kernel's own k_msgq.
//
// A k_msgq copies fixed-size items from one contiguous buffer, and a message
// here is anything up to message_bytes() long, so the messages live in slots and
// two k_msgq carry the slot numbers: one of the slots that are free and one of
// those that are full. A sender takes a free slot, fills it, and puts its number on
// the full queue; a receiver takes a number from the full queue, copies the slot out
// and gives the number back. The kernel does the blocking, the time-outs and the
// ordering, and a message cannot be seen half written.
class ZephyrQueue final : public MessageQueue
{
public:
    ZephyrQueue(const QueueSpec& spec, std::size_t slot_bytes, std::byte* slots,
                std::unique_ptr<std::byte[]> owned_slots, std::unique_ptr<std::uint32_t[]> numbers,
                Clock& clock) noexcept
        : message_bytes_(spec.message_bytes), capacity_(spec.capacity), slot_bytes_(slot_bytes), slots_(slots),
          owned_slots_(std::move(owned_slots)), numbers_(std::move(numbers)), clock_(clock)
    {
        char* free_buffer = reinterpret_cast<char*>(numbers_.get());
        char* full_buffer = reinterpret_cast<char*>(numbers_.get() + capacity_);
        k_msgq_init(&free_, free_buffer, sizeof(std::uint32_t), static_cast<uint32_t>(capacity_));
        k_msgq_init(&full_, full_buffer, sizeof(std::uint32_t), static_cast<uint32_t>(capacity_));
        for (std::uint32_t slot = 0; slot < capacity_; ++slot)
        {
            (void)k_msgq_put(&free_, &slot, K_NO_WAIT);
        }
    }

    ~ZephyrQueue() override
    {
        // Anything still waiting on the queues is released with an error, not left
        // sleeping on memory that is about to go.
        k_msgq_purge(&free_);
        k_msgq_purge(&full_);
    }

    Result<void> send(std::span<const std::byte> message, Duration timeout) noexcept override
    {
        if (message.size() > message_bytes_)
        {
            return fail(ErrorCode::invalid_argument);
        }
        auto slot = take(free_, timeout);
        if (!slot)
        {
            return std::unexpected(slot.error());
        }
        std::byte* place = slots_ + static_cast<std::size_t>(*slot) * slot_bytes_;
        const std::size_t length = message.size();
        std::memcpy(place, &length, sizeof length);
        if (length != 0)
        {
            std::memcpy(place + sizeof length, message.data(), length);
        }
        const std::uint32_t number = *slot;
        if (const int error = k_msgq_put(&full_, &number, K_FOREVER); error != 0)
        {
            return fail_errno(error);
        }
        return {};
    }

    Result<std::size_t> receive(std::span<std::byte> out, Duration timeout) noexcept override
    {
        if (out.size() < message_bytes_)
        {
            return fail(ErrorCode::invalid_argument);
        }
        auto slot = take(full_, timeout);
        if (!slot)
        {
            return std::unexpected(slot.error());
        }
        const std::byte* place = slots_ + static_cast<std::size_t>(*slot) * slot_bytes_;
        std::size_t length = 0;
        std::memcpy(&length, place, sizeof length);
        if (length != 0)
        {
            std::memcpy(out.data(), place + sizeof length, length);
        }
        const std::uint32_t number = *slot;
        if (const int error = k_msgq_put(&free_, &number, K_FOREVER); error != 0)
        {
            return fail_errno(error);
        }
        return length;
    }

    std::size_t size() const noexcept override
    {
        return k_msgq_num_used_get(const_cast<k_msgq*>(&full_));
    }
    std::size_t capacity() const noexcept override
    {
        return capacity_;
    }
    std::size_t message_bytes() const noexcept override
    {
        return message_bytes_;
    }

private:
    // A slot number from `from`, waiting up to `timeout` by the clock: the kernel
    // rounds to its tick and caps one wait at a day, so the clock decides when the
    // time is up.
    [[nodiscard]] Result<std::uint32_t> take(k_msgq& from, Duration timeout) noexcept
    {
        const Instant deadline = timeout == forever ? Instant{forever} : clock_.now() + timeout;
        for (;;)
        {
            Duration remaining = forever;
            if (timeout != forever)
            {
                remaining = deadline - clock_.now();
                if (remaining < Duration::zero())
                {
                    remaining = Duration::zero();
                }
            }
            std::uint32_t slot = 0;
            const int error = k_msgq_get(&from, &slot, zephyr_os::to_timeout(remaining));
            if (error == 0)
            {
                return slot;
            }
            if (error != -EAGAIN && error != -ENOMSG)
            {
                return fail_errno(error);
            }
            if (timeout != forever && deadline - clock_.now() <= Duration::zero())
            {
                return fail(ErrorCode::timeout);
            }
        }
    }

    std::size_t message_bytes_;
    std::size_t capacity_;
    std::size_t slot_bytes_;
    std::byte* slots_;
    std::unique_ptr<std::byte[]> owned_slots_;
    std::unique_ptr<std::uint32_t[]> numbers_;
    Clock& clock_;
    k_msgq free_{};
    k_msgq full_{};
};

class ZephyrIpc final : public Ipc
{
public:
    explicit ZephyrIpc(Clock& clock) noexcept : clock_(clock) {}

    Result<Owned<MessageQueue>> create_queue(const QueueSpec& spec) noexcept override
    {
        if (spec.message_bytes == 0 || spec.capacity == 0 || spec.capacity > UINT32_MAX ||
            spec.message_bytes > (static_cast<std::size_t>(-1) / 4) / spec.capacity)
        {
            return fail(ErrorCode::invalid_argument);
        }
        const std::size_t slot_bytes = align_up(sizeof(std::size_t) + spec.message_bytes, alignof(std::size_t));
        const std::size_t needed = slot_bytes * spec.capacity;

        std::byte* slots = nullptr;
        std::unique_ptr<std::byte[]> owned;
        if (spec.storage.empty())
        {
            owned.reset(new (std::nothrow) std::byte[needed]);
            if (!owned)
            {
                return fail(ErrorCode::resource_exhausted);
            }
            slots = owned.get();
        }
        else
        {
            // The caller's block need not be aligned for a size_t; storage_required()
            // leaves room for aligning its start.
            const auto start = reinterpret_cast<std::uintptr_t>(spec.storage.data());
            const std::size_t skip =
                static_cast<std::size_t>(((start + alignof(std::size_t) - 1) & ~(alignof(std::size_t) - 1)) - start);
            if (skip > spec.storage.size() || spec.storage.size() - skip < needed)
            {
                return fail(ErrorCode::invalid_argument);
            }
            slots = spec.storage.data() + skip;
        }

        // The two queues of slot numbers. This is the one allocation a queue makes
        // that the caller's storage does not cover.
        std::unique_ptr<std::uint32_t[]> numbers(new (std::nothrow) std::uint32_t[spec.capacity * 2]);
        if (!numbers)
        {
            return fail(ErrorCode::resource_exhausted);
        }
        auto queue = make_owned_nothrow<MessageQueue, ZephyrQueue>(spec, slot_bytes, slots, std::move(owned),
                                                                   std::move(numbers), clock_);
        if (!queue)
        {
            return fail(ErrorCode::resource_exhausted);
        }
        return queue;
    }

private:
    Clock& clock_;
};

} // namespace

std::unique_ptr<Ipc> make_ipc(Clock& clock) noexcept
{
    return detail::make_unique_nothrow<ZephyrIpc>(clock);
}

} // namespace adl::platform::zephyr

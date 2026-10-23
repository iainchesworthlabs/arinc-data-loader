// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/platform/memory.hpp"
#include "adl/platform/ring_ipc.hpp"
#include "detail/nothrow.hpp"

#include <climits>
#include <cstring>

namespace adl::platform
{
namespace
{

class RingQueue final : public MessageQueue
{
public:
    RingQueue(std::size_t message_bytes, std::size_t capacity, std::span<std::byte> storage,
              std::unique_ptr<std::byte[]> owned, Owned<Mutex> lock, Owned<Semaphore> free_slots,
              Owned<Semaphore> items) noexcept
        : message_bytes_(message_bytes), capacity_(capacity),
          slot_bytes_(align_up(sizeof(std::size_t) + message_bytes, alignof(std::size_t))), storage_(storage),
          owned_(std::move(owned)), lock_(std::move(lock)), free_slots_(std::move(free_slots)), items_(std::move(items))
    {
    }

    Result<void> send(std::span<const std::byte> message, Duration timeout) noexcept override
    {
        if (message.size() > message_bytes_)
        {
            return fail(ErrorCode::invalid_argument);
        }
        if (auto taken = free_slots_->wait(timeout); !taken)
        {
            return std::unexpected(taken.error());
        }
        {
            LockGuard guard(*lock_);
            std::byte* slot = storage_.data() + tail_ * slot_bytes_;
            const std::size_t length = message.size();
            std::memcpy(slot, &length, sizeof length);
            if (length != 0)
            {
                std::memcpy(slot + sizeof length, message.data(), length);
            }
            tail_ = (tail_ + 1) % capacity_;
            ++count_;
        }
        return items_->post();
    }

    Result<std::size_t> receive(std::span<std::byte> out, Duration timeout) noexcept override
    {
        if (out.size() < message_bytes_)
        {
            return fail(ErrorCode::invalid_argument);
        }
        if (auto taken = items_->wait(timeout); !taken)
        {
            return std::unexpected(taken.error());
        }
        std::size_t length = 0;
        {
            LockGuard guard(*lock_);
            const std::byte* slot = storage_.data() + head_ * slot_bytes_;
            std::memcpy(&length, slot, sizeof length);
            if (length != 0)
            {
                std::memcpy(out.data(), slot + sizeof length, length);
            }
            head_ = (head_ + 1) % capacity_;
            --count_;
        }
        if (auto given = free_slots_->post(); !given)
        {
            return std::unexpected(given.error());
        }
        return length;
    }

    std::size_t size() const noexcept override
    {
        LockGuard guard(*lock_);
        return count_;
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
    std::size_t message_bytes_;
    std::size_t capacity_;
    std::size_t slot_bytes_;
    std::span<std::byte> storage_;
    std::unique_ptr<std::byte[]> owned_;
    Owned<Mutex> lock_;
    Owned<Semaphore> free_slots_;
    Owned<Semaphore> items_;
    std::size_t head_ = 0;
    std::size_t tail_ = 0;
    std::size_t count_ = 0;
};

} // namespace

Result<Owned<MessageQueue>> RingIpc::create_queue(const QueueSpec& spec) noexcept
{
    if (spec.message_bytes == 0 || spec.capacity == 0 || spec.capacity > UINT_MAX ||
        spec.message_bytes > (static_cast<std::size_t>(-1) / 4) / spec.capacity)
    {
        return fail(ErrorCode::invalid_argument);
    }
    const std::size_t slot_bytes = align_up(sizeof(std::size_t) + spec.message_bytes, alignof(std::size_t));
    const std::size_t needed = slot_bytes * spec.capacity;

    std::span<std::byte> storage = spec.storage;
    std::unique_ptr<std::byte[]> owned;
    if (storage.empty())
    {
        owned.reset(new (std::nothrow) std::byte[needed]);
        if (!owned)
        {
            return fail(ErrorCode::resource_exhausted);
        }
        storage = {owned.get(), needed};
    }
    else if (storage.size() < needed)
    {
        return fail(ErrorCode::invalid_argument);
    }

    auto lock = threads_.create_mutex();
    if (!lock)
    {
        return std::unexpected(lock.error());
    }
    const auto ceiling = static_cast<unsigned>(spec.capacity);
    auto free_slots = threads_.create_semaphore(ceiling, ceiling);
    if (!free_slots)
    {
        return std::unexpected(free_slots.error());
    }
    auto items = threads_.create_semaphore(0, ceiling);
    if (!items)
    {
        return std::unexpected(items.error());
    }
    auto queue =
        make_owned_nothrow<MessageQueue, RingQueue>(spec.message_bytes, spec.capacity, storage, std::move(owned),
                                                    std::move(*lock), std::move(*free_slots), std::move(*items));
    if (!queue)
    {
        return fail(ErrorCode::resource_exhausted);
    }
    return queue;
}

} // namespace adl::platform

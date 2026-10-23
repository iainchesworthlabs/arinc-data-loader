#pragma once

// Memory. The engine and the codec work from storage the caller supplies, and
// the aim is no allocation once the program has started. This header has the
// three pieces that make that practical, and one platform interface.
//
//   Arena      - bump allocation inside a fixed block, reset in one step: the
//                state of one operation, or the scratch of one packet.
//   BlockPool  - equal blocks handed out and taken back in any order: the
//                packet buffers of the transfers in flight.
//   StaticStorage<N> - a block whose size is a compile-time constant, for a
//                program that wants all its memory in .bss.
//   MemoryProvider - where the blocks come from at start-up. This is the only
//                part that differs per platform: pages that are locked in
//                RAM, a memory partition, a linker section.
//
// Why not std::pmr: see docs/platform-abstraction.md. In short, the standard
// allocator interface reports exhaustion by throwing, which this code base
// does not do; it needs a hosted standard library that not every embedded
// toolchain ships; and the ARINC 665 model types are not allocator-aware, so
// it would buy nothing for the part that still allocates.
//
// Nothing here is thread-safe: an Arena or a BlockPool belongs to one thread,
// or to a caller that locks around it.

#include "adl/platform/error.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <span>
#include <type_traits>
#include <utility>

namespace adl::platform
{

[[nodiscard]] constexpr bool is_power_of_two(std::size_t value) noexcept
{
    return value != 0 && (value & (value - 1)) == 0;
}

// `alignment` must be a power of two. Returns 0 when the result would not fit
// in a size_t, which no real block does.
[[nodiscard]] constexpr std::size_t align_up(std::size_t value, std::size_t alignment) noexcept
{
    const std::size_t mask = alignment - 1;
    if (value > static_cast<std::size_t>(-1) - mask)
    {
        return 0;
    }
    return (value + mask) & ~mask;
}

// Storage in the object, aligned for anything: `StaticStorage<4096> pool_memory;`
template <std::size_t Bytes, std::size_t Alignment = alignof(std::max_align_t)>
struct StaticStorage
{
    static_assert(is_power_of_two(Alignment));
    alignas(Alignment) std::array<std::byte, Bytes> bytes{};

    [[nodiscard]] constexpr std::span<std::byte> span() noexcept
    {
        return bytes;
    }
};

class Arena
{
public:
    Arena() = default;
    explicit Arena(std::span<std::byte> storage) noexcept : storage_(storage) {}

    // A position to go back to. Everything allocated after `mark()` is released
    // by `rewind`, which is how a per-packet scratch is reclaimed without
    // forgetting the per-operation state below it.
    struct Mark
    {
        std::size_t used = 0;
    };

    // nullptr when the arena is out of room or `alignment` is not a power of
    // two. A request for 0 bytes succeeds and returns a valid pointer (it may
    // equal the next allocation's).
    [[nodiscard]] void* allocate_bytes(std::size_t bytes, std::size_t alignment) noexcept
    {
        if (!is_power_of_two(alignment) || storage_.data() == nullptr)
        {
            return nullptr;
        }
        const auto base = reinterpret_cast<std::uintptr_t>(storage_.data());
        const std::uintptr_t unaligned = base + used_;
        const std::uintptr_t aligned = (unaligned + (alignment - 1)) & ~(alignment - 1);
        const std::size_t padding = static_cast<std::size_t>(aligned - unaligned);
        if (padding > storage_.size() - used_ || bytes > storage_.size() - used_ - padding)
        {
            return nullptr;
        }
        used_ += padding + bytes;
        high_water_ = std::max(high_water_, used_);
        return reinterpret_cast<void*>(aligned);
    }

    // Zero-initialised room for `count` objects of a type that needs no
    // destructor (the arena never runs one). Empty span on exhaustion.
    template <class T>
    [[nodiscard]] std::span<T> allocate_array(std::size_t count) noexcept
    {
        static_assert(std::is_trivially_destructible_v<T>, "an Arena never runs destructors");
        static_assert(std::is_default_constructible_v<T>);
        if (count > static_cast<std::size_t>(-1) / sizeof(T))
        {
            return {};
        }
        void* raw = allocate_bytes(count * sizeof(T), alignof(T));
        if (raw == nullptr)
        {
            return {};
        }
        T* first = static_cast<T*>(raw);
        for (std::size_t i = 0; i < count; ++i)
        {
            ::new (static_cast<void*>(first + i)) T{};
        }
        return {first, count};
    }

    // One object, constructed in place. nullptr on exhaustion.
    template <class T, class... Args>
    [[nodiscard]] T* create(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
    {
        static_assert(std::is_trivially_destructible_v<T>, "an Arena never runs destructors");
        void* raw = allocate_bytes(sizeof(T), alignof(T));
        if (raw == nullptr)
        {
            return nullptr;
        }
        return ::new (raw) T(std::forward<Args>(args)...);
    }

    [[nodiscard]] Mark mark() const noexcept
    {
        return Mark{used_};
    }
    void rewind(Mark to) noexcept
    {
        used_ = std::min(to.used, used_);
    }
    void reset() noexcept
    {
        used_ = 0;
    }

    [[nodiscard]] std::size_t capacity() const noexcept
    {
        return storage_.size();
    }
    [[nodiscard]] std::size_t used() const noexcept
    {
        return used_;
    }
    [[nodiscard]] std::size_t remaining() const noexcept
    {
        return storage_.size() - used_;
    }
    // The most that was ever in use at once, so a program can be given exactly
    // the storage it needs after a test run has shown how much that is.
    [[nodiscard]] std::size_t high_water() const noexcept
    {
        return high_water_;
    }

private:
    std::span<std::byte> storage_{};
    std::size_t used_ = 0;
    std::size_t high_water_ = 0;
};

class BlockPool
{
public:
    // Bytes of storage that hold `blocks` blocks of `block_bytes` each, aligned
    // to `alignment`, including the slack that aligning the first one can cost.
    [[nodiscard]] static constexpr std::size_t
    storage_required(std::size_t block_bytes, std::size_t blocks,
                     std::size_t alignment = alignof(std::max_align_t)) noexcept
    {
        const std::size_t stride = stride_for(block_bytes, alignment);
        if (stride == 0 || (blocks != 0 && stride > static_cast<std::size_t>(-1) / blocks))
        {
            return 0;
        }
        return stride * blocks + (alignment - 1);
    }

    BlockPool() = default;

    // The pool holds as many whole blocks as `storage` has room for. A block is
    // at least a pointer wide, because a free block holds the link to the next.
    BlockPool(std::span<std::byte> storage, std::size_t block_bytes,
              std::size_t alignment = alignof(std::max_align_t)) noexcept
    {
        if (!is_power_of_two(alignment) || block_bytes == 0)
        {
            return;
        }
        stride_ = stride_for(block_bytes, alignment);
        block_bytes_ = block_bytes;
        const auto base = reinterpret_cast<std::uintptr_t>(storage.data());
        const auto aligned = (base + (alignment - 1)) & ~(alignment - 1);
        const std::size_t slack = static_cast<std::size_t>(aligned - base);
        if (stride_ == 0 || slack > storage.size())
        {
            stride_ = 0;
            return;
        }
        first_ = storage.data() + slack;
        capacity_ = (storage.size() - slack) / stride_;
        for (std::size_t i = capacity_; i > 0; --i)
        {
            push_free(first_ + (i - 1) * stride_);
        }
        available_ = capacity_;
    }

    // An empty span when every block is out.
    [[nodiscard]] std::span<std::byte> acquire() noexcept
    {
        if (free_ == nullptr)
        {
            return {};
        }
        std::byte* block = free_;
        std::byte* next = nullptr;
        std::memcpy(&next, block, sizeof next);
        free_ = next;
        --available_;
        return {block, block_bytes_};
    }

    // Takes back a span that `acquire` returned. A span that is not one of this
    // pool's blocks is ignored and reported as false: releasing it would
    // corrupt the free list, so it is the one case that is refused outright.
    bool release(std::span<std::byte> block) noexcept
    {
        if (!owns(block) || available_ >= capacity_)
        {
            return false;
        }
        push_free(block.data());
        ++available_;
        return true;
    }

    // Whether `block` has the address and size of one of this pool's blocks (not
    // whether it is out at the moment).
    [[nodiscard]] bool owns(std::span<const std::byte> block) const noexcept
    {
        if (stride_ == 0 || capacity_ == 0 || block.data() == nullptr)
        {
            return false;
        }
        const std::byte* p = block.data();
        if (p < first_ || p >= first_ + capacity_ * stride_)
        {
            return false;
        }
        return (static_cast<std::size_t>(p - first_) % stride_) == 0 && block.size() == block_bytes_;
    }

    [[nodiscard]] std::size_t block_bytes() const noexcept
    {
        return block_bytes_;
    }
    [[nodiscard]] std::size_t capacity() const noexcept
    {
        return capacity_;
    }
    [[nodiscard]] std::size_t available() const noexcept
    {
        return available_;
    }

private:
    [[nodiscard]] static constexpr std::size_t stride_for(std::size_t block_bytes, std::size_t alignment) noexcept
    {
        if (!is_power_of_two(alignment))
        {
            return 0;
        }
        return align_up(std::max(block_bytes, sizeof(std::byte*)), alignment);
    }

    void push_free(std::byte* block) noexcept
    {
        std::memcpy(block, &free_, sizeof free_);
        free_ = block;
    }

    std::byte* first_ = nullptr;
    std::byte* free_ = nullptr;
    std::size_t stride_ = 0;
    std::size_t block_bytes_ = 0;
    std::size_t capacity_ = 0;
    std::size_t available_ = 0;
};

class MemoryProvider;

// A block obtained from a MemoryProvider, given back when the object goes
// away. Move-only.
class MemoryRegion
{
public:
    MemoryRegion() = default;
    MemoryRegion(MemoryProvider& owner, std::span<std::byte> bytes) noexcept : owner_(&owner), bytes_(bytes) {}
    MemoryRegion(const MemoryRegion&) = delete;
    MemoryRegion& operator=(const MemoryRegion&) = delete;
    MemoryRegion(MemoryRegion&& other) noexcept
        : owner_(std::exchange(other.owner_, nullptr)), bytes_(std::exchange(other.bytes_, std::span<std::byte>{}))
    {
    }
    MemoryRegion& operator=(MemoryRegion&& other) noexcept
    {
        if (this != &other)
        {
            reset();
            owner_ = std::exchange(other.owner_, nullptr);
            bytes_ = std::exchange(other.bytes_, std::span<std::byte>{});
        }
        return *this;
    }
    ~MemoryRegion()
    {
        reset();
    }

    [[nodiscard]] std::span<std::byte> bytes() const noexcept
    {
        return bytes_;
    }
    [[nodiscard]] std::byte* data() const noexcept
    {
        return bytes_.data();
    }
    [[nodiscard]] std::size_t size() const noexcept
    {
        return bytes_.size();
    }

    inline void reset() noexcept;

private:
    MemoryProvider* owner_ = nullptr;
    std::span<std::byte> bytes_{};
};

enum class MemoryUse : std::uint8_t
{
    general, // ordinary memory
    // Memory that stays in RAM: never paged out, so that the first use of it
    // in the middle of a transfer cannot stall on a page fault. A platform
    // with no paging gives ordinary memory; one that cannot lock any (a quota
    // exhausted, no privilege) says `resource_exhausted` or `permission_denied`
    // and the caller decides whether to carry on without.
    resident,
};

// Where the engine's blocks come from, at start-up. The desktop platforms go
// to the system allocator (and lock pages for `resident`); an RTOS port takes
// them from a partition or a linker section it sized at build time.
class MemoryProvider
{
public:
    virtual ~MemoryProvider() = default;

    // `alignment` is a power of two. The block is zero-filled, so a caller never
    // sees what a previous owner left in it. `invalid_argument` for 0 bytes or
    // an impossible alignment; `resource_exhausted` when there is no room.
    [[nodiscard]] virtual Result<MemoryRegion> acquire(std::size_t bytes, std::size_t alignment,
                                                       MemoryUse use) noexcept = 0;

    // Called by ~MemoryRegion; not for callers.
    virtual void release(std::span<std::byte> bytes) noexcept = 0;
};

inline void MemoryRegion::reset() noexcept
{
    if (owner_ != nullptr)
    {
        owner_->release(bytes_);
    }
    owner_ = nullptr;
    bytes_ = {};
}

} // namespace adl::platform

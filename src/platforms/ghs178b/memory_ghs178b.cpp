// INTEGRITY-178B (GHS). UNBUILT AND UNTESTED: this file has never been
// compiled against the INTEGRITY-178B headers; see docs/platform-ghs.md.
//
// MemoryProvider over one static block: a first-fit allocator whose free list
// is sorted by address, so adjacent free blocks coalesce when released. The
// block is a compile-time 512 KiB constant - the product sizes this; nothing
// is allocated at run time - and lives in .bss as a zero-initialised static
// (StaticStorage), so acquire() never asks the kernel for memory. Every block
// carries its bookkeeping in a small header immediately before the pointer
// handed out: the block's total size and the distance back to the block
// start, which is how release() finds the block it owns; a free block stores
// its link to the next one inside its own payload.
//
// Assumptions named: the provider is single-threaded, like an Arena or a
// BlockPool (the interface says such storage belongs to one thread, and the
// host hands this provider to the one runner that carves its pools at
// start-up); MemoryUse::resident is the same memory, because an application
// partition does not page and there is nothing to lock; and release() trusts
// the contract that only blocks this allocator handed out come back - a
// forged pointer that passes the checks below would corrupt the free list,
// exactly as BlockPool::release documents.

#include <cstddef>
#include <cstring>
#include <memory>

#include "adl/platform/memory.hpp"
#include "detail/nothrow.hpp"
#include "ghs178b/ghs178b_services.hpp"

namespace adl::platform::ghs178b {
namespace {

// The product sizes this: 512 KiB is a proposal that leaves the engine's
// block pools and packet buffers room in .bss beside the program's own
// statics. Nothing is allocated at run time.
constexpr std::size_t provider_bytes = 512 * 1024;

// The header immediately before the pointer acquire() returns. alignas keeps
// a block start aligned for any request a caller makes, and makes the header
// a multiple of that alignment, so splitting a block always leaves an
// aligned remainder.
struct alignas(std::max_align_t) Header {
    std::size_t offset = 0;  // from the block start to the handed-out pointer
    std::size_t size = 0;    // from the block start to the block end
};

// A free block stores its free-list link in its own payload (always at least
// pointer-sized). The list is sorted by address, so a release coalesces with
// both neighbours in one pass.
struct Free {
    Free* next = nullptr;
};

// The smallest block worth keeping: a header and one link.
constexpr std::size_t min_block = sizeof(Header) + sizeof(Free*);

class StaticMemory final : public MemoryProvider {
public:
    explicit StaticMemory(std::span<std::byte> storage) noexcept : storage_(storage) {
        if (storage_.size() >= min_block) {
            free_ = reinterpret_cast<Free*>(storage_.data());
            header(free_)->offset = sizeof(Header);  // the free header's convention
            header(free_)->size = storage_.size();
            link(free_)->next = nullptr;
        }
    }

    Result<MemoryRegion> acquire(std::size_t bytes, std::size_t alignment,
                                 MemoryUse use) noexcept override {
        if (bytes == 0 || !is_power_of_two(alignment)) {
            return fail(ErrorCode::invalid_argument);
        }
        // resident is the same memory: an application partition does not
        // page, and there is nothing to lock.
        (void)use;
        for (Free** previous = &free_; *previous != nullptr;
             previous = &link(*previous)->next) {
            std::byte* const block = reinterpret_cast<std::byte*>(*previous);
            const std::size_t block_size = header(block)->size;
            std::byte* const user = reinterpret_cast<std::byte*>(align_up(
                reinterpret_cast<std::uintptr_t>(block + sizeof(Header)), alignment));
            if (user == nullptr) {
                continue;  // align_up overflowed; no block can hold this
            }
            const std::size_t needed = static_cast<std::size_t>(user - block) + bytes;
            if (needed > block_size) {
                continue;
            }
            // The carved span: header, alignment padding and the request -
            // never below what a free block needs, so the block can be
            // released again, and rounded to a header boundary so the
            // remainder is aligned.
            std::size_t used = needed < min_block ? min_block : needed;
            used = align_up(used, alignof(Header));
            if (used > block_size) {
                used = block_size;
            }
            const std::size_t remainder = block_size - used;
            if (remainder >= min_block) {
                // The remainder takes the block's place in the free list.
                std::byte* const split = block + used;
                header(split)->offset = sizeof(Header);
                header(split)->size = remainder;
                link(split)->next = link(block)->next;
                *previous = reinterpret_cast<Free*>(split);
            } else {
                // Too small to split: the request takes the whole block.
                *previous = link(block)->next;
            }
            Header* const recorded = reinterpret_cast<Header*>(user - sizeof(Header));
            recorded->offset = static_cast<std::size_t>(user - block);
            recorded->size = used;
            std::memset(user, 0, bytes);  // the interface promises zero-filled
            return MemoryRegion(*this, {user, bytes});
        }
        return fail(ErrorCode::resource_exhausted);
    }

    void release(std::span<std::byte> bytes) noexcept override {
        if (bytes.data() == nullptr) {
            return;
        }
        std::byte* const user = bytes.data();
        const Header* const recorded = reinterpret_cast<const Header*>(user - sizeof(Header));
        if (recorded->offset < sizeof(Header) || recorded->size < min_block) {
            return;  // not a block this allocator handed out
        }
        std::byte* const block = user - recorded->offset;
        if (block < storage_.data() || block >= storage_.data() + storage_.size() ||
            reinterpret_cast<std::uintptr_t>(block) % alignof(Header) != 0) {
            return;
        }
        const std::size_t span = recorded->size;
        if (span > static_cast<std::size_t>(storage_.data() + storage_.size() - block)) {
            return;
        }
        // The block becomes free (header at the block start, link in the
        // payload) and takes its address-sorted place in the list.
        header(block)->offset = sizeof(Header);
        header(block)->size = span;
        Free* const node = reinterpret_cast<Free*>(block);
        Free* next = free_;
        Free* preceding = nullptr;
        while (next != nullptr && reinterpret_cast<std::byte*>(next) < block) {
            preceding = next;
            next = link(next)->next;
        }
        link(node)->next = next;
        // Coalesce with the following neighbour, then the preceding one.
        if (next != nullptr && block + span == reinterpret_cast<std::byte*>(next)) {
            header(node)->size += header(next)->size;
            link(node)->next = link(next)->next;
        }
        if (preceding != nullptr &&
            reinterpret_cast<std::byte*>(preceding) + header(preceding)->size == block) {
            header(preceding)->size += header(node)->size;
            link(preceding)->next = link(node)->next;
        } else if (preceding == nullptr) {
            free_ = node;
        } else {
            link(preceding)->next = node;
        }
    }

private:
    static Header* header(void* block) noexcept {
        return reinterpret_cast<Header*>(block);
    }
    static Free* link(void* block) noexcept {
        return reinterpret_cast<Free*>(static_cast<std::byte*>(block) + sizeof(Header));
    }

    std::span<std::byte> storage_;
    Free* free_ = nullptr;
};

// .bss, zero-initialised, aligned for anything: the whole provider, so the
// loader never asks the kernel for memory once it has started.
StaticStorage<provider_bytes> provider_storage;

}  // namespace

std::unique_ptr<MemoryProvider> make_memory() noexcept {
    return detail::make_unique_nothrow<StaticMemory>(provider_storage.span());
}

}  // namespace adl::platform::ghs178b

#include <cstring>

#include "adl/platform/memory.hpp"
#include "detail/nothrow.hpp"
#include "freertos/freertos_os.hpp"

namespace adl::platform::freertos {
namespace {

// The loader's memory is the kernel's heap (pvPortMalloc: heap_3 on a simulator, heap_4 or
// heap_5 on a target, or whatever the product configured). The heap hands out
// portBYTE_ALIGNMENT-aligned blocks, and a caller can ask for more, so a block is taken larger
// by the alignment and the pointer to give back to the heap is kept just before the block
// that is handed out. FreeRTOS has no paging, so `resident` memory is the same memory.
class FreeRtosMemory final : public MemoryProvider {
public:
    Result<MemoryRegion> acquire(std::size_t bytes, std::size_t alignment,
                                 MemoryUse /*use*/) noexcept override {
        if (bytes == 0 || !is_power_of_two(alignment)) {
            return fail(ErrorCode::invalid_argument);
        }
        const std::size_t effective = alignment < alignof(void*) ? alignof(void*) : alignment;
        const std::size_t overhead = effective + sizeof(void*);
        if (bytes > static_cast<std::size_t>(-1) - overhead) {
            return fail(ErrorCode::resource_exhausted);
        }
        void* raw = pvPortMalloc(bytes + overhead);
        if (raw == nullptr) {
            return fail(ErrorCode::resource_exhausted);
        }
        const auto start = reinterpret_cast<std::uintptr_t>(raw) + sizeof(void*);
        const std::uintptr_t aligned = (start + (effective - 1)) & ~(effective - 1);
        auto* block = reinterpret_cast<std::byte*>(aligned);
        std::memcpy(block - sizeof(void*), &raw, sizeof raw);
        std::memset(block, 0, bytes);
        return MemoryRegion(*this, {block, bytes});
    }

    void release(std::span<std::byte> bytes) noexcept override {
        if (bytes.data() == nullptr) {
            return;
        }
        void* raw = nullptr;
        std::memcpy(&raw, bytes.data() - sizeof(void*), sizeof raw);
        vPortFree(raw);
    }
};

}  // namespace

std::unique_ptr<MemoryProvider> make_memory() noexcept {
    return detail::make_unique_nothrow<FreeRtosMemory>();
}

}  // namespace adl::platform::freertos

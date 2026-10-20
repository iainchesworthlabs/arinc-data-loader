#include <array>
#include <cstring>

#include "adl/platform/memory.hpp"
#include "detail/nothrow.hpp"
#include "zephyr/zephyr_os.hpp"

namespace adl::platform::zephyr {
namespace {

// The loader's heap: a k_heap over an array in .bss, sized by the application
// (CONFIG_ADL_HEAP_BYTES). Zephyr has no paging, so `resident` memory is the
// same memory.
struct LoaderHeap {
    alignas(std::max_align_t) std::array<std::byte, CONFIG_ADL_HEAP_BYTES> storage{};
    k_heap heap{};

    LoaderHeap() noexcept { k_heap_init(&heap, storage.data(), storage.size()); }
};

[[nodiscard]] LoaderHeap& loader_heap() noexcept {
    static LoaderHeap instance;
    return instance;
}

class ZephyrMemory final : public MemoryProvider {
public:
    Result<MemoryRegion> acquire(std::size_t bytes, std::size_t alignment,
                                 MemoryUse /*use*/) noexcept override {
        if (bytes == 0 || !is_power_of_two(alignment)) {
            return fail(ErrorCode::invalid_argument);
        }
        void* block = k_heap_aligned_alloc(&loader_heap().heap, alignment, bytes, K_NO_WAIT);
        if (block == nullptr) {
            return fail(ErrorCode::resource_exhausted);
        }
        std::memset(block, 0, bytes);
        return MemoryRegion(*this, {static_cast<std::byte*>(block), bytes});
    }

    void release(std::span<std::byte> bytes) noexcept override {
        if (bytes.data() != nullptr) {
            k_heap_free(&loader_heap().heap, bytes.data());
        }
    }
};

}  // namespace

std::unique_ptr<MemoryProvider> make_memory() noexcept {
    return detail::make_unique_nothrow<ZephyrMemory>();
}

}  // namespace adl::platform::zephyr

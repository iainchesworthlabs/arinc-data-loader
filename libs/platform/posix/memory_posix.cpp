#include <stdlib.h>
#include <sys/mman.h>

#include <cerrno>
#include <cstring>

#include "adl/platform/memory.hpp"
#include "detail/nothrow.hpp"
#include "posix/posix_os.hpp"

namespace adl::platform::posix {
namespace {

class PosixMemory final : public MemoryProvider {
public:
    Result<MemoryRegion> acquire(std::size_t bytes, std::size_t alignment,
                                 MemoryUse use) noexcept override {
        if (bytes == 0 || !is_power_of_two(alignment)) {
            return fail(ErrorCode::invalid_argument);
        }
        // posix_memalign wants a multiple of the pointer size.
        const std::size_t effective_alignment =
            alignment < sizeof(void*) ? sizeof(void*) : alignment;
        void* block = nullptr;
        if (const int error = ::posix_memalign(&block, effective_alignment, bytes); error != 0) {
            return posix_os::fail_errno(error);
        }
        std::memset(block, 0, bytes);
        if (use == MemoryUse::resident && ::mlock(block, bytes) != 0) {
            const int error = errno;
            ::free(block);
            return posix_os::fail_errno(error);
        }
        return MemoryRegion(*this, {static_cast<std::byte*>(block), bytes});
    }

    void release(std::span<std::byte> bytes) noexcept override {
        if (bytes.data() == nullptr) {
            return;
        }
        // munlock of memory that was never locked is harmless, and saves keeping a
        // record of which blocks were.
        ::munlock(bytes.data(), bytes.size());
        ::free(bytes.data());
    }
};

}  // namespace

std::unique_ptr<MemoryProvider> make_memory() noexcept {
    return detail::make_unique_nothrow<PosixMemory>();
}

}  // namespace adl::platform::posix

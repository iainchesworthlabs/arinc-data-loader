// The Windows SDK headers depend on their order (winsock2.h before windows.h, both before mswsock.h), so
// clang-format leaves them as they are.
// clang-format off
#include <windows.h>

#include <cstring>

#include "adl/platform/memory.hpp"
#include "detail/nothrow.hpp"
#include "windows/windows_services.hpp"
// clang-format on

namespace adl::platform::windows
{
namespace
{

// VirtualAlloc hands out whole pages at the allocation granularity (64 KiB), and
// returns memory that is already zero.
constexpr std::size_t largest_alignment = std::size_t{64} * 1024;

class WindowsMemory final : public MemoryProvider
{
public:
    Result<MemoryRegion> acquire(std::size_t bytes, std::size_t alignment, MemoryUse use) noexcept override
    {
        if (bytes == 0 || !is_power_of_two(alignment))
        {
            return fail(ErrorCode::invalid_argument);
        }
        if (alignment > largest_alignment)
        {
            return fail(ErrorCode::unsupported);
        }
        void* block = ::VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (block == nullptr)
        {
            return fail_win32(::GetLastError());
        }
        if (use == MemoryUse::resident && !::VirtualLock(block, bytes))
        {
            const DWORD error = ::GetLastError();
            ::VirtualFree(block, 0, MEM_RELEASE);
            return fail_win32(error);
        }
        return MemoryRegion(*this, {static_cast<std::byte*>(block), bytes});
    }

    void release(std::span<std::byte> bytes) noexcept override
    {
        if (bytes.data() != nullptr)
        {
            ::VirtualFree(bytes.data(), 0, MEM_RELEASE);
        }
    }
};

} // namespace

std::unique_ptr<MemoryProvider> make_memory() noexcept
{
    return detail::make_unique_nothrow<WindowsMemory>();
}

} // namespace adl::platform::windows

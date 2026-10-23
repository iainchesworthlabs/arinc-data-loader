// The Windows SDK headers depend on their order (winsock2.h before windows.h, both before mswsock.h), so
// clang-format leaves them as they are.
// clang-format off
#include <windows.h>
#include <winsock2.h>

#include "windows/windows_services.hpp"
// clang-format on

namespace adl::platform::windows
{

ErrorCode map_win32(unsigned long error) noexcept
{
    switch (error)
    {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_INVALID_DRIVE:
        return ErrorCode::not_found;
    case ERROR_ALREADY_EXISTS:
    case ERROR_FILE_EXISTS:
        return ErrorCode::already_exists;
    case ERROR_DIRECTORY:
        return ErrorCode::not_a_directory;
    case ERROR_DIR_NOT_EMPTY:
        return ErrorCode::directory_not_empty;
    case ERROR_ACCESS_DENIED:
    case ERROR_SHARING_VIOLATION:
    case ERROR_LOCK_VIOLATION:
    case ERROR_WRITE_PROTECT:
    case ERROR_PRIVILEGE_NOT_HELD:
        return ErrorCode::permission_denied;
    case ERROR_INVALID_PARAMETER:
    case ERROR_INVALID_NAME:
    case ERROR_BAD_PATHNAME:
    case ERROR_INVALID_HANDLE:
        return ErrorCode::invalid_argument;
    case ERROR_FILENAME_EXCED_RANGE:
    case ERROR_BUFFER_OVERFLOW:
        return ErrorCode::name_too_long;
    case ERROR_DISK_FULL:
    case ERROR_HANDLE_DISK_FULL:
    case ERROR_DISK_QUOTA_EXCEEDED:
        return ErrorCode::no_space;
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:
    case ERROR_NO_SYSTEM_RESOURCES:
    case ERROR_TOO_MANY_OPEN_FILES:
    case ERROR_TOO_MANY_POSTS:
    case ERROR_COMMITMENT_LIMIT:
    case ERROR_WORKING_SET_QUOTA:
        return ErrorCode::resource_exhausted;
    case ERROR_TIMEOUT:
    case WAIT_TIMEOUT:
        return ErrorCode::timeout;
    case ERROR_NOT_SUPPORTED:
    case ERROR_CALL_NOT_IMPLEMENTED:
        return ErrorCode::unsupported;
    case ERROR_READ_FAULT:
    case ERROR_WRITE_FAULT:
    case ERROR_IO_DEVICE:
    case ERROR_CRC:
        return ErrorCode::io_error;
    default:
        return ErrorCode::other;
    }
}

ErrorCode map_winsock(int error) noexcept
{
    switch (error)
    {
    case WSAEADDRINUSE:
        return ErrorCode::address_in_use;
    case WSAEADDRNOTAVAIL:
        return ErrorCode::address_unavailable;
    case WSAENETUNREACH:
    case WSAEHOSTUNREACH:
    case WSAENETDOWN:
        return ErrorCode::unreachable;
    case WSAEMSGSIZE:
        return ErrorCode::message_too_large;
    case WSAEINTR:
        return ErrorCode::interrupted;
    case WSAEACCES:
        return ErrorCode::permission_denied;
    case WSAEINVAL:
    case WSAENOTSOCK:
    case WSAEFAULT:
    case WSAEAFNOSUPPORT:
        return ErrorCode::invalid_argument;
    case WSAENOBUFS:
    case WSAEMFILE:
    case WSAEWOULDBLOCK:
        return ErrorCode::resource_exhausted;
    case WSAETIMEDOUT:
        return ErrorCode::timeout;
    case WSAECONNRESET:
    case WSAESHUTDOWN:
        return ErrorCode::closed;
    case WSAVERNOTSUPPORTED:
    case WSASYSNOTREADY:
        return ErrorCode::unsupported;
    default:
        return ErrorCode::other;
    }
}

unsigned long to_milliseconds(Duration timeout, unsigned long limit) noexcept
{
    if (timeout <= Duration::zero())
    {
        return 0;
    }
    constexpr std::int64_t nanoseconds_per_millisecond = 1'000'000;
    std::int64_t ms = timeout.count() / nanoseconds_per_millisecond;
    if (timeout.count() % nanoseconds_per_millisecond != 0)
    {
        ++ms;
    }
    return ms > static_cast<std::int64_t>(limit) ? limit : static_cast<unsigned long>(ms);
}

} // namespace adl::platform::windows

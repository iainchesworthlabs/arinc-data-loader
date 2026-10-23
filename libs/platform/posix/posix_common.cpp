// SPDX-License-Identifier: GPL-3.0-or-later
#include "posix/posix_os.hpp"

#include <cerrno>
#include <climits>

namespace adl::platform::posix_os
{

ErrorCode map_errno(int error) noexcept
{
    if (error == EAGAIN || error == EWOULDBLOCK)
    {
        return ErrorCode::resource_exhausted;
    }
    switch (error)
    {
    case ENOENT:
        return ErrorCode::not_found;
    case EEXIST:
        return ErrorCode::already_exists;
    case ENOTDIR:
        return ErrorCode::not_a_directory;
    case EISDIR:
        return ErrorCode::is_a_directory;
    case ENOTEMPTY:
        return ErrorCode::directory_not_empty;
    case EACCES:
    case EPERM:
    case EROFS:
        return ErrorCode::permission_denied;
    case EINVAL:
    case EBADF:
    case EFAULT:
        return ErrorCode::invalid_argument;
    case ENAMETOOLONG:
        return ErrorCode::name_too_long;
    case ENOSPC:
    case EDQUOT:
    case EFBIG:
        return ErrorCode::no_space;
    case EIO:
        return ErrorCode::io_error;
    case ENOMEM:
    case EMFILE:
    case ENFILE:
    case ENOBUFS:
        return ErrorCode::resource_exhausted;
    case EADDRINUSE:
        return ErrorCode::address_in_use;
    case EADDRNOTAVAIL:
        return ErrorCode::address_unavailable;
    case ENETUNREACH:
    case EHOSTUNREACH:
    case ENETDOWN:
        return ErrorCode::unreachable;
    case EMSGSIZE:
        return ErrorCode::message_too_large;
    case EINTR:
        return ErrorCode::interrupted;
    case ETIMEDOUT:
        return ErrorCode::timeout;
    case EPIPE:
    case ECONNRESET:
        return ErrorCode::closed;
    case ENOSYS:
    case EOPNOTSUPP:
        return ErrorCode::unsupported;
    default:
        return ErrorCode::other;
    }
}

int to_poll_milliseconds(Duration timeout) noexcept
{
    if (timeout == forever)
    {
        return -1;
    }
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
    return ms > INT_MAX ? INT_MAX : static_cast<int>(ms);
}

} // namespace adl::platform::posix_os

#include <cerrno>

#include "zephyr/zephyr_os.hpp"

namespace adl::platform::zephyr_os {

ErrorCode map_errno(int error) noexcept {
    // Some Zephyr calls hand back the negative of the value.
    if (error < 0) {
        error = -error;
    }
    // EAGAIN and EWOULDBLOCK, and ENOTSUP and EOPNOTSUPP, are one value on some C
    // libraries and two on others, so they are not cases of the switch below.
    if (error == EAGAIN || error == EWOULDBLOCK) {
        return ErrorCode::resource_exhausted;
    }
    if (error == ENOTSUP) {
        return ErrorCode::unsupported;
    }
    switch (error) {
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

k_timeout_t to_timeout(Duration d) noexcept {
    if (d == forever) {
        return K_FOREVER;
    }
    if (d <= Duration::zero()) {
        return K_NO_WAIT;
    }
    if (d > longest_kernel_wait) {
        d = longest_kernel_wait;
    }
    // K_NSEC converts with a ceiling, which is the rounding a time-out needs.
    return K_NSEC(static_cast<std::uint64_t>(d.count()));
}

}  // namespace adl::platform::zephyr_os

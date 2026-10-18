// The shared INTEGRITY mappings; see ghs_native.hpp. UNBUILT: like everything
// under ghs/, this has never been compiled against the INTEGRITY headers.

#include <cerrno>

#include "ghs/ghs_native.hpp"

namespace adl::platform::ghs {

ErrorCode map_errno(int error) noexcept {
    if (error == EAGAIN || error == EWOULDBLOCK) {
        return ErrorCode::resource_exhausted;
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

ErrorCode map_error(Error error) noexcept {
    switch (error) {
        case Failure:
            return ErrorCode::other;
        case NULLObjectSpecified:
            return ErrorCode::invalid_argument;
        case ObjectTableIsFull:
        case ResourceNotAvailable:
        case MemoryAllocationFailed:
            return ErrorCode::resource_exhausted;
        case SendWouldBlock:
            return ErrorCode::resource_exhausted;
        case OperationTimedOut:
            return ErrorCode::timeout;
        case WriteFailed:
        case ReadFailed:
            return ErrorCode::io_error;
        case IllegalPriority:
        case ExceedsMaximumPriority:
            return ErrorCode::permission_denied;
        default:
            return ErrorCode::other;
    }
}

Duration to_duration(const Time& time) noexcept {
    constexpr std::uint64_t nanoseconds_per_second = 1'000'000'000;
    constexpr std::uint64_t fraction_scale = std::uint64_t{1} << 32;
    const std::uint64_t seconds = static_cast<std::uint64_t>(time.Seconds);
    const std::uint64_t whole = seconds * nanoseconds_per_second;
    if (whole / nanoseconds_per_second != seconds) {
        return forever;
    }
    // Round to the nearest nanosecond: add half a nanosecond's worth of
    // fraction units before dividing.
    const std::uint64_t nanos = (time.Fraction * nanoseconds_per_second + fraction_scale / 2) /
                                fraction_scale;
    if (whole > static_cast<std::uint64_t>(Duration::max().count()) - nanos) {
        return forever;
    }
    return Duration(static_cast<Duration::rep>(whole + nanos));
}

Time to_time(Duration duration) noexcept {
    if (duration <= Duration::zero()) {
        return Time{0, 0};
    }
    constexpr std::uint64_t nanoseconds_per_second = 1'000'000'000;
    constexpr std::uint64_t fraction_scale = std::uint64_t{1} << 32;
    const std::uint64_t total = static_cast<std::uint64_t>(duration.count());
    Time time;
    // Clamp to 2^32 - 1 seconds (136 years), which any 32-bit or wider time_t
    // holds; a saturating add below can reach Duration::max(), which a 32-bit
    // time_t cannot.
    constexpr std::uint64_t seconds_ceiling = 0xFFFFFFFFu;
    const std::uint64_t seconds = total / nanoseconds_per_second;
    time.Seconds = static_cast<time_t>(seconds > seconds_ceiling ? seconds_ceiling : seconds);
    time.Fraction = static_cast<UINT4>(((total % nanoseconds_per_second) * fraction_scale +
                                        nanoseconds_per_second / 2) /
                                       nanoseconds_per_second);
    return time;
}

Time add_duration(const Time& base, Duration by) noexcept {
    const Duration now = to_duration(base);
    if (by > Duration::zero() && now > Duration::max() - by) {
        return to_time(Duration::max());
    }
    return to_time(now + by);
}

}  // namespace adl::platform::ghs

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// How the platform layer reports a failure. Nothing here throws: every
// operation that can fail returns Result<T>, an std::expected whose error is a
// small value type that needs no allocation to build, copy or log.

#include <cstdint>
#include <expected>

namespace adl::platform
{

// The reasons that matter to a caller, in terms every target can express. Each
// platform maps its own error numbers (errno, GetLastError, WSAGetLastError,
// an RTOS's status codes) onto these; anything it cannot place is `other`, and
// the number it started from is kept in Error::native for the log.
enum class ErrorCode : std::uint8_t
{
    timeout,     // the time limit passed first: not a fault, a normal outcome
    interrupted, // a signal or an abort ended a wait early; try again
    not_found,   // no such file, directory or address
    already_exists,
    not_a_directory,
    is_a_directory,
    directory_not_empty,
    permission_denied,
    invalid_argument, // the arguments were wrong whatever the state of the world
    name_too_long,
    no_space, // the storage is full, or a quota ran out
    io_error,
    closed,             // the other end, or the object itself, has been closed
    unsupported,        // this platform cannot do it; see docs/platform-abstraction.md
    resource_exhausted, // out of memory, handles, queue slots or a capacity set by the caller
    address_in_use,
    address_unavailable,
    unreachable, // no route to the network or host
    message_too_large,
    other,
};

struct Error
{
    ErrorCode code = ErrorCode::other;
    // The operating system's own number, for a log line. Never branch on it:
    // it means different things on different targets.
    int native = 0;

    friend constexpr bool operator==(const Error& a, const Error& b) noexcept
    {
        return a.code == b.code;
    }
};

[[nodiscard]] constexpr const char* to_string(ErrorCode code) noexcept
{
    switch (code)
    {
    case ErrorCode::timeout:
        return "timeout";
    case ErrorCode::interrupted:
        return "interrupted";
    case ErrorCode::not_found:
        return "not found";
    case ErrorCode::already_exists:
        return "already exists";
    case ErrorCode::not_a_directory:
        return "not a directory";
    case ErrorCode::is_a_directory:
        return "is a directory";
    case ErrorCode::directory_not_empty:
        return "directory not empty";
    case ErrorCode::permission_denied:
        return "permission denied";
    case ErrorCode::invalid_argument:
        return "invalid argument";
    case ErrorCode::name_too_long:
        return "name too long";
    case ErrorCode::no_space:
        return "no space";
    case ErrorCode::io_error:
        return "I/O error";
    case ErrorCode::closed:
        return "closed";
    case ErrorCode::unsupported:
        return "unsupported";
    case ErrorCode::resource_exhausted:
        return "resource exhausted";
    case ErrorCode::address_in_use:
        return "address in use";
    case ErrorCode::address_unavailable:
        return "address unavailable";
    case ErrorCode::unreachable:
        return "unreachable";
    case ErrorCode::message_too_large:
        return "message too large";
    case ErrorCode::other:
        return "other error";
    }
    return "unknown";
}

template <class T>
using Result = std::expected<T, Error>;

[[nodiscard]] constexpr std::unexpected<Error> fail(ErrorCode code, int native = 0) noexcept
{
    return std::unexpected<Error>{Error{code, native}};
}

} // namespace adl::platform

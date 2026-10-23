// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The rules every platform applies to a path before it touches the file system
// (interfaces/adl/platform/files.hpp states them): not empty, no NUL and no
// backslash. Checked in one place so that no platform is more lenient than
// another, and so a name that reached the loader from a target cannot mean one
// thing on Windows and another on Linux.

#include "adl/platform/error.hpp"

#include <string_view>

namespace adl::platform::detail
{

[[nodiscard]] inline Result<void> check_path(std::string_view path) noexcept
{
    if (path.empty())
    {
        return fail(ErrorCode::invalid_argument);
    }
    for (const char c : path)
    {
        if (c == '\0' || c == '\\')
        {
            return fail(ErrorCode::invalid_argument);
        }
    }
    return {};
}

} // namespace adl::platform::detail

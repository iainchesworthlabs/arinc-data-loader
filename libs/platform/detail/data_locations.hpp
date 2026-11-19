// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Where the POSIX systems keep a user's data, as functions of the text the operating system gave:
// the value of an environment variable and the home directory. They read no environment and touch
// no file system, so every case, including the odd ones, is tested on every host. The platforms
// that use them (Linux and macOS) fetch the text and call these.
//
// Text from the environment is untrusted. A relative path would depend on the folder a program
// happens to run in, so only an absolute path counts, and anything else is treated as unset.

#include <string>
#include <string_view>

namespace adl::platform::detail
{

// An absolute POSIX path: it starts with '/' and holds no NUL.
[[nodiscard]] constexpr bool is_absolute_posix_path(std::string_view text) noexcept
{
    return !text.empty() && text.front() == '/' && text.find('\0') == std::string_view::npos;
}

// `base` and `rest` joined with one '/', whether or not `base` ends with one.
[[nodiscard]] inline std::string join_posix_path(std::string_view base, std::string_view rest)
{
    std::string joined{base};
    if (joined.empty() || joined.back() != '/')
    {
        joined += '/';
    }
    joined += rest;
    return joined;
}

// The XDG Base Directory rule for a user's data: $XDG_DATA_HOME when it is set to an absolute
// path, otherwise <home>/.local/share. Empty when neither is usable.
[[nodiscard]] inline std::string xdg_user_data_directory(std::string_view xdg_data_home, std::string_view home)
{
    if (is_absolute_posix_path(xdg_data_home))
    {
        return std::string{xdg_data_home};
    }
    if (is_absolute_posix_path(home))
    {
        return join_posix_path(home, ".local/share");
    }
    return {};
}

// macOS keeps a user's data in <home>/Library/Application Support. Empty when the home directory
// is not an absolute path.
[[nodiscard]] inline std::string macos_user_data_directory(std::string_view home)
{
    if (!is_absolute_posix_path(home))
    {
        return {};
    }
    return join_posix_path(home, "Library/Application Support");
}

} // namespace adl::platform::detail

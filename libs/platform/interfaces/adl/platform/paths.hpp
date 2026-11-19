// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace adl::platform
{

// The absolute path of the running executable, so that data installed beside
// it (configuration, media sets) can be found whatever the current directory is. Empty if
// the operating system will not say.
[[nodiscard]] std::filesystem::path executable_path();

// The name of this computer, as UTF-8 text: the host name the operating system gives it (the DNS
// host name on Windows, gethostname elsewhere). It is what a licence compares with the computer
// names it lists. Empty if the operating system will not say, and on a platform with no such name.
[[nodiscard]] std::string computer_name();

// The folder in which a program keeps data for the signed-in user: %LOCALAPPDATA% on Windows,
// $XDG_DATA_HOME or ~/.local/share on Linux, ~/Library/Application Support on macOS. It is the
// base only: a program makes a folder of its own below it. The path is absolute, is not made if
// it is not there, and is empty if the operating system will not say, and on a platform with no
// signed-in user. An environment variable counts only when it holds an absolute path.
[[nodiscard]] std::filesystem::path user_data_directory();

// The folder in which an administrator puts files that apply to every user of the computer, which
// the programs only read: %ProgramData% on Windows, /etc on Linux, /Library/Application Support on
// macOS. The base only, as above: absolute, not made, and empty if the platform has none. Windows
// is asked for its known folder and not the environment variable of the same name, because anyone
// can set a variable for a program they start.
[[nodiscard]] std::filesystem::path machine_config_directory();

// The bytes of UTF-8 text held as char8_t (what std::filesystem::path::u8string() returns) as the
// std::string the rest of the code holds its text in.
[[nodiscard]] inline std::string to_utf8(std::u8string_view text)
{
    return {reinterpret_cast<const char*>(text.data()), text.size()};
}

// A path as the FileSystem interface (files.hpp) wants it: UTF-8 with '/' as the separator.
[[nodiscard]] inline std::string to_platform_path(const std::filesystem::path& path)
{
    return to_utf8(path.generic_u8string());
}

} // namespace adl::platform

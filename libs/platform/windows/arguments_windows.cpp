// SPDX-License-Identifier: GPL-3.0-or-later
// clang-format off
#include <windows.h>

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "adl/platform/arguments.hpp"
#include "detail/command_line.hpp"
// clang-format on

namespace adl::platform
{

// main's argv is the command line narrowed to the code page, which loses what the page has no letter
// for. The wide command line of the process is what the system was given, so it is read and split here,
// and argv is used only if the system gives no command line.
std::vector<std::string> arguments(int argc, const char* const* argv)
{
    const wchar_t* const line = ::GetCommandLineW();
    if (line != nullptr)
    {
        std::u16string units;
        for (const wchar_t* unit = line; *unit != L'\0'; ++unit)
        {
            units += static_cast<char16_t>(*unit);
        }
        std::vector<std::string> split = detail::arguments_from_command_line(units);
        if (!split.empty())
        {
            return split;
        }
    }
    return detail::copy_arguments(argc, argv);
}

std::filesystem::path path_from_utf8(std::string_view text)
{
    const std::u16string units = detail::utf8_to_utf16(text);
    std::wstring wide;
    wide.reserve(units.size());
    for (const char16_t unit : units)
    {
        wide += static_cast<wchar_t>(unit);
    }
    return std::filesystem::path{wide};
}

} // namespace adl::platform

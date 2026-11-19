// SPDX-License-Identifier: GPL-3.0-or-later
// The arguments and the paths of every platform that is not Windows: Linux, macOS, QNX, FreeRTOS, Zephyr
// and the two INTEGRITY platforms. The system passes a program the bytes the user typed and names a file
// with bytes, so an argument is already what a path is made from. Windows has its own
// (windows/arguments_windows.cpp), and each platform's platform.cmake names the one it compiles.

#include "adl/platform/arguments.hpp"
#include "detail/command_line.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace adl::platform
{

std::vector<std::string> arguments(int argc, const char* const* argv)
{
    return detail::copy_arguments(argc, argv);
}

std::filesystem::path path_from_utf8(std::string_view text)
{
    return std::filesystem::path{std::string{text}};
}

} // namespace adl::platform

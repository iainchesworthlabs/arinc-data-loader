// SPDX-License-Identifier: GPL-3.0-or-later
// The environment as bytes (Linux and macOS). They are the text the shell gave, which is UTF-8 on every system
// the programs run on, and they are passed on as they are. CMake compiles this file and not
// os/windows/environment.cpp, chosen by the platform's name (libs/licence_platform/CMakeLists.txt).

#include "adl/licence_platform/environment.hpp"

#include <cstdlib>

namespace adl::licence_platform
{

std::optional<std::string> environment_variable(std::string_view name)
{
    const std::string terminated{name};
    const char* const value = std::getenv(terminated.c_str());
    if (value == nullptr)
    {
        return std::nullopt;
    }
    return std::string{value};
}

} // namespace adl::licence_platform

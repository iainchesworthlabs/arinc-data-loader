// SPDX-License-Identifier: GPL-3.0-or-later
// QNX Neutrino. UNBUILT AND UNTESTED, like everything in this directory; see
// docs/platform-abstraction.md ("Status of each platform").

#include "adl/platform/paths.hpp"

#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <string>

namespace adl::platform
{

std::filesystem::path executable_path()
{
    // QNX names the running program in /proc/self/exefile, as text.
    std::FILE* file = std::fopen("/proc/self/exefile", "r");
    if (file == nullptr)
    {
        return {};
    }
    std::string name;
    char chunk[256];
    std::size_t got = 0;
    while ((got = std::fread(chunk, 1, sizeof chunk, file)) > 0)
    {
        name.append(chunk, got);
    }
    std::fclose(file);
    while (!name.empty() && (name.back() == '\n' || name.back() == '\0'))
    {
        name.pop_back();
    }
    return std::filesystem::path{name};
}

std::string computer_name()
{
    // Not written for QNX yet: the interface allows an empty name. Unbuilt like the rest of this
    // directory, so nothing here has been compiled for QNX.
    return {};
}

std::filesystem::path user_data_directory()
{
    // As above: an empty path until someone with a QNX target decides where a user's data goes.
    return {};
}

std::filesystem::path machine_config_directory()
{
    // As above.
    return {};
}

} // namespace adl::platform

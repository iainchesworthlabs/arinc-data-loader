// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/platform/paths.hpp"

#include <filesystem>
#include <string>

namespace adl::platform
{

std::filesystem::path executable_path()
{
    // A Zephyr image is not a file the running program can find, so there is no
    // path to give; the interface allows an empty one.
    return {};
}

std::string computer_name()
{
    // The image has no host name of its own.
    return {};
}

std::filesystem::path user_data_directory()
{
    // No signed-in user, and no folder of theirs; the interface allows an empty path.
    return {};
}

std::filesystem::path machine_config_directory()
{
    // The loader takes its locations from configuration on this platform.
    return {};
}

} // namespace adl::platform

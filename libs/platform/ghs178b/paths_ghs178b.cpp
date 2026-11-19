// SPDX-License-Identifier: GPL-3.0-or-later
// INTEGRITY-178B (GHS). UNBUILT AND UNTESTED: this file has never been
// compiled against the INTEGRITY-178B headers; see docs/platform-ghs.md.

#include "adl/platform/paths.hpp"

#include <filesystem>
#include <string>

namespace adl::platform
{

std::filesystem::path executable_path()
{
    // An application partition has no file from which it came: tasks are
    // configured into the kernel image. The empty path is the honest answer.
    return {};
}

std::string computer_name()
{
    // A partition has no host name of its own.
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

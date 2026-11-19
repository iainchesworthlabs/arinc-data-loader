// SPDX-License-Identifier: GPL-3.0-or-later
// INTEGRITY 5.0.8 (GHS). UNBUILT AND UNTESTED: this file has never been
// compiled against the INTEGRITY headers; see docs/platform-ghs.md.

#include "adl/platform/paths.hpp"

#include <filesystem>
#include <string>

namespace adl::platform
{

std::filesystem::path executable_path()
{
    // INTEGRITY does not name the running program to it, and the loader takes
    // its data locations from configuration, so the empty path is the honest
    // answer (the interface allows it).
    return {};
}

std::string computer_name()
{
    // Not asked of the system on this platform; the interface allows an empty name.
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

// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/platform/paths.hpp"

#include <filesystem>

namespace adl::platform
{

std::filesystem::path executable_path()
{
    // A Zephyr image is not a file the running program can find, so there is no
    // path to give; the interface allows an empty one.
    return {};
}

} // namespace adl::platform

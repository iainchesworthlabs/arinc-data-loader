// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// A path as the UTF-8 text that a program takes as an argument (adl::platform::arguments gives it so, and
// path_from_utf8 makes the path from it). path::string() is the system's code page on Windows and throws for a
// character outside it, so a test that makes an argument from a path does not use it.

#include "adl/platform/paths.hpp"

#include <filesystem>
#include <string>

namespace adl::test
{

[[nodiscard]] inline std::string utf8(const std::filesystem::path& path)
{
    return adl::platform::to_utf8(path.u8string());
}

} // namespace adl::test

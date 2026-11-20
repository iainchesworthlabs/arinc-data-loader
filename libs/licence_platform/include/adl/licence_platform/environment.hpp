// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The one thing the programs that read licences need of the operating system that adl::platform does not give:
// an environment variable, for ADL_SETTINGS_DIR, which moves the licence folders as it moves the settings of the
// programs. It is defined once for each system, in os/<system>/environment.cpp, and CMake compiles the one for
// the platform's name (libs/licence_platform/CMakeLists.txt). It is here, with the desktop programs' licence
// code, and not in adl::platform, because a platform function would need an implementation on every platform,
// the real-time ones included, and only a program with a file system reads a licence.
//
// adl_licence had its own copy of this (apps/licence/os), with the note that a second program to need it meant
// it belonged in a library; adl_load, which writes the licence into its operation record, is that program.

#include <optional>
#include <string>
#include <string_view>

namespace adl::licence_platform
{

// The value of the variable `name` (ASCII) as UTF-8 text, or nothing when it is not set. A value that is
// empty is set and empty. On Windows a lone surrogate in the value becomes U+FFFD.
[[nodiscard]] std::optional<std::string> environment_variable(std::string_view name);

} // namespace adl::licence_platform

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The one thing adl_licence needs of the operating system that adl::platform does not give: an environment
// variable, for ADL_SETTINGS_DIR, which moves the licence folders as it moves the settings of the programs. It is
// defined once for each system, in os/<system>/environment.cpp, and CMake compiles the one for the platform's name
// (apps/licence/CMakeLists.txt), as the owner's tool does for its entry point. If a second program needs it, it
// belongs in adl::platform.

#include <optional>
#include <string>
#include <string_view>

namespace adl::licence_program
{

// The value of the variable `name` (ASCII) as UTF-8 text, or nothing when it is not set. A value that is
// empty is set and empty. On Windows a lone surrogate in the value becomes U+FFFD.
[[nodiscard]] std::optional<std::string> environment_variable(std::string_view name);

} // namespace adl::licence_program

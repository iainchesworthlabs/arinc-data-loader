// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The four commands. `run` (licence_cli.hpp) checks the command line and calls one of these.

#include "adl/licence_cli/licence_cli.hpp"

#include <string_view>

namespace adl::licence_cli
{

// Each returns the exit code and has written what it has to say to the console.
[[nodiscard]] int command_show(const Environment& environment, const Console& console);
[[nodiscard]] int command_import(const Environment& environment, std::string_view file, const Console& console);
[[nodiscard]] int command_remove(const Environment& environment, std::string_view name, const Console& console);
[[nodiscard]] int command_verify(const Environment& environment, std::string_view file, const Console& console);

} // namespace adl::licence_cli

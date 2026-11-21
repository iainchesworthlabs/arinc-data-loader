// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The scenario source of the program on the computer it runs on (scenario.hpp): the file read through the host's file
// system and carried out through the engine's fault hook. On a platform with no scenario libraries it refuses the
// option. CMake compiles one of the two (apps/load/CMakeLists.txt), as it does for the licence and the platform layer.

#include "adl/load/scenario.hpp"

namespace adl::load
{

[[nodiscard]] ScenarioSource system_scenario();

} // namespace adl::load

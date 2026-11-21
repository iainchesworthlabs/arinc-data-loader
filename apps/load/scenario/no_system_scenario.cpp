// SPDX-License-Identifier: GPL-3.0-or-later
// The scenario source of a program on a platform that has no scenario libraries: none, so `--scenario` is refused with
// a sentence. Compiled instead of apps/load/scenario/system_scenario.cpp where the scenario libraries are not built.

#include "adl/load/system_scenario.hpp"

namespace adl::load
{

ScenarioSource system_scenario()
{
    return {};
}

} // namespace adl::load

// SPDX-License-Identifier: GPL-3.0-or-later
// The scenario source of a desktop program: the file read through the host's file system with the strict reader of the
// scenario library, and carried out through the engine's fault hook. Compiled on a desktop;
// apps/load/scenario/no_system_scenario.cpp is the one for the other platforms.

#include "adl/load/system_scenario.hpp"

#include "adl/loader_scenario/scenario.hpp"
#include "scenario_run.hpp"

#include <memory>
#include <string>
#include <utility>

namespace adl::load
{

ScenarioSource system_scenario()
{
    return ScenarioSource(
        [](adl::platform::Host& host, const std::string& path, std::string_view operation,
           EventWriter& events) -> std::expected<std::unique_ptr<FaultPlan>, std::string>
        {
            auto scenario = read_scenario_file(host.files(), path);
            if (!scenario)
            {
                return std::unexpected(std::move(scenario.error()));
            }
            // The words of the events are the operation's names, so a word that is not one is the program's own
            // mistake: the run is then any operation, and every step of the file is in effect.
            const auto name = adl::loader_scenario::operation_from_string(operation);
            return std::make_unique<ScenarioRun>(std::move(*scenario), path,
                                                 name.value_or(adl::loader_scenario::OperationName::any), events);
        });
}

} // namespace adl::load

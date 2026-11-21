// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// What `--scenario` adds to one operation, on a desktop (adl_load_scenario). The file is read and checked before the
// program opens a port or a folder or writes an event (read_scenario_file), and a ScenarioRun then carries it out
// through the engine's fault hook and writes each fault it does as a `scenario_step` event. It is the FaultPlan that
// system_scenario() opens (adl/load/scenario.hpp); nothing here is made when the option is not given, and the engine
// then has no hook at all, so an operation runs as it does without this file.
//
// A ScenarioRun is for one operation and is not reused. The loader holds a pointer to its hook, so it has to stay
// alive until the operation has ended and the `finished` event has been written.

#include "adl/load/events.hpp"
#include "adl/load/scenario.hpp"
#include "adl/loader_scenario/player.hpp"
#include "adl/loader_scenario/scenario.hpp"
#include "adl/platform/files.hpp"

#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace adl::load
{

// Reads and checks the scenario file at `path`, as the operator wrote it (UTF-8; it is turned into the platform's
// path here, so a name outside the system's code page is reached). Why it could not be used is one sentence, with
// no full stop and no name of the file: "the file cannot be read (not found)", "the file is larger than 262144
// bytes", or the reader's own ("step 3: ...").
[[nodiscard]] std::expected<adl::loader_scenario::Scenario, std::string>
read_scenario_file(adl::platform::FileSystem& files, const std::string& path);

class ScenarioRun final : public FaultPlan, private adl::loader_scenario::FaultListener
{
public:
    // `operation` is what the run is: a step for another operation is not in effect. `events` has to outlive the
    // object.
    ScenarioRun(adl::loader_scenario::Scenario scenario, std::string path,
                adl::loader_scenario::OperationName operation, EventWriter& events);

    [[nodiscard]] adl::arinc615a::FaultHook& hook() noexcept override
    {
        return player_;
    }
    [[nodiscard]] ScenarioInfo info() const override;
    [[nodiscard]] std::vector<std::size_t> unused() const override;

private:
    void on_fault(const adl::loader_scenario::FaultEvent& fault) noexcept override;

    std::string path_;
    std::size_t steps_;
    std::optional<std::uint64_t> seed_;
    EventWriter& events_;
    adl::loader_scenario::Player player_;
};

} // namespace adl::load

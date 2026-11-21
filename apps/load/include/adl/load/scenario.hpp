// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// What `--scenario` asks of the program that runs a command: the faults on purpose of one operation
// (docs/file-reference.md, "Loader scenario file"), without this library knowing how the file is read or carried out.
//
// The program that runs a command says how a scenario file is opened: main gives a ScenarioSource that reads the file
// with the scenario libraries and carries it out through the engine's fault hook, a test gives the same or one that
// returns what it wants, and a source that is not given refuses the option with a sentence. The library does not
// include the scenario code itself, because it reads the file with the licence library's JSON reader, which a program
// built for a target with no file system does not have (libs/loader_scenario is a desktop library), and adl_load_cli
// builds on every platform, as it does with the licence (licence.hpp).
//
// A source is asked once for each operation, before anything is opened or written, and what it opens lives until the
// operation has ended.

#include "adl/arinc615a/fault_hook.hpp"
#include "adl/load/events.hpp"
#include "adl/platform/host.hpp"

#include <cstddef>
#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace adl::load
{

// The faults of one operation: what the engine is given as its hook, and what the events say of it. Each fault it does
// is written as a `scenario_step` event to the EventWriter it was opened with.
class FaultPlan
{
public:
    virtual ~FaultPlan() = default;

    // What the engine is to be given as `LoaderServices::faults`.
    [[nodiscard]] virtual adl::arinc615a::FaultHook& hook() noexcept = 0;
    // What `started` says about the file.
    [[nodiscard]] virtual ScenarioInfo info() const = 0;
    // The numbers of the steps that were in effect for this operation and never acted: what `finished` says.
    [[nodiscard]] virtual std::vector<std::size_t> unused() const = 0;
};

class ScenarioSource
{
public:
    // Opens the scenario file at `path`, as the operator wrote it, for the operation called `operation` (the word the
    // events use: `information`, `upload`, `download_media_defined` or `download_operator_defined`); a step that is for
    // another operation is not in effect. Each fault the plan does goes to `events`, which has to outlive it. When the
    // file cannot be used the answer is one sentence that says why, with no full stop and no name of the file.
    using Opener = std::function<std::expected<std::unique_ptr<FaultPlan>, std::string>(
        adl::platform::Host& host, const std::string& path, std::string_view operation, EventWriter& events)>;

    // A source that opens none: the option is refused.
    ScenarioSource() = default;
    explicit ScenarioSource(Opener opener) : opener_(std::move(opener)) {}

    [[nodiscard]] std::expected<std::unique_ptr<FaultPlan>, std::string>
    open(adl::platform::Host& host, const std::string& path, std::string_view operation, EventWriter& events) const
    {
        if (!opener_)
        {
            return std::unexpected(std::string{"scenario files are not used by this program on this system"});
        }
        return opener_(host, path, operation, events);
    }

private:
    Opener opener_;
};

} // namespace adl::load

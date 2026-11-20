// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// One operation on one target, as the program does it, in the steps that the command line and `serve` both go
// through: read and check what the operation needs, say it has started, make the folder of a download, run
// it, and say how it ended. The command line runs the steps one after the other with run_operation between
// them (operation.cpp); `serve` runs the middle one in its pool, with other operations running beside it.
//
// An object is for one operation and is not reused. It holds what the engine points into (the listener that
// writes the events, the media set's source, the download's sink, the gate), so it has to stay alive until the
// operation has ended and end() has been called.

#include "adl/arinc615a/data_loader.hpp"
#include "adl/arinc615a/directory_files.hpp"
#include "adl/load/events.hpp"
#include "adl/load/licence.hpp"
#include "adl/load/options.hpp"
#include "adl/load/reason.hpp"
#include "adl/load/scenario.hpp"
#include "adl/loader_core/gate.hpp"
#include "adl/loader_core/media_set_check.hpp"
#include "adl/loader_core/operation_plan.hpp"
#include "adl/loader_core/run.hpp"
#include "adl/platform/host.hpp"
#include "job.hpp"

#include <memory>
#include <optional>
#include <string>

namespace adl::load
{

class OperationCommand
{
public:
    // `options` and `events` have to outlive the object. `licence` is kept: begin() asks it for the licence the
    // operation runs under, and the record says it. `scenario` is kept: load_scenario() asks it to open the file
    // `--scenario` names, and a source that opens none refuses the option.
    OperationCommand(adl::platform::Host& host, const Options& options, EventWriter& events, LicenceSource licence = {},
                     ScenarioSource scenario = {});

    // With `--scenario`: reads and checks the file and makes what carries it out. Returns the sentence that says why
    // the file cannot be used, which is a local error (exit code 5) and not wrong usage; nothing is written. Call it
    // first, before prepare(): it opens no port and no folder, and writes no event. Does nothing without the option.
    [[nodiscard]] std::optional<std::string> load_scenario();

    // The plan of the operation and, for an upload, the media set read and the loads chosen from it. Returns the
    // sentence of wrong usage when the choice and what it refers to do not fit (a part number the set does not
    // hold, say). Nothing is written. A media set that cannot be read is not wrong usage: it is told by begin().
    [[nodiscard]] std::optional<std::string> prepare();

    // Writes `started`, then for an upload what the media set check found, and makes the folder of a download.
    // Returns the exit code if the operation ended there (the set could not be read or had changed, the folder
    // could not be made): `finished` has then been written, and nothing more is to be done.
    [[nodiscard]] std::optional<int> begin();

    // Says what is about to be done (a `log` event) and makes what the engine will use. Call it once, after
    // begin() has returned nothing and before target(), operation() and services().
    void announce();

    // What run_operation, or the pool, is to be given. `control_port` and `local_address` of the target are
    // the options' own, which are the program's; the pool takes its own.
    [[nodiscard]] adl::loader_core::RunTarget target() const;
    [[nodiscard]] const adl::arinc615a::Operation& operation() const;
    [[nodiscard]] adl::loader_core::RunServices services();

    // Says how the operation ended: the last line of the log, the record if one was asked for, and `finished`.
    // Returns the exit code the single command ends with.
    [[nodiscard]] int end(const adl::loader_core::OperationResult& result);

private:
    [[nodiscard]] std::optional<int> check_media_set_events();
    [[nodiscard]] std::optional<int> make_folder();
    [[nodiscard]] int finish(Reason reason, std::string detail, const std::string& phrase,
                             const std::optional<std::string>& download_folder);

    adl::platform::Host& host_;
    const Options& options_;
    EventWriter& events_;
    LicenceSource licence_;
    ScenarioSource scenario_source_;
    std::string address_;
    Job job_;
    std::unique_ptr<FaultPlan> scenario_;                   // what load_scenario() made; none without `--scenario`
    std::unique_ptr<adl::loader_core::OperationPlan> plan_; // what prepare() made; none for a set that cannot be read
    adl::loader_core::MediaSetReport report_;
    std::shared_ptr<const adl::loader_core::MediaSetIndex> index_;
    std::string folder_; // a download: the folder that was made, as a platform path

    // What the engine uses while the operation runs, made by announce().
    adl::loader_core::OperationGate gate_;
    std::optional<adl::loader_core::MediaTreeSource> source_;
    std::optional<adl::arinc615a::DirectoryFileSink> sink_;
};

} // namespace adl::load

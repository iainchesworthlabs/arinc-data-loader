// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The recorder: an OperationListener that fills an OperationRecord from what the engine reports. It makes
// the decisions the window's own listener makes (apps/app/core, `JobListener`) about what goes in the log and
// when: every status file is a line; a load or a file the target failed is a line the first time it is
// reported and not again; a description is added to a status line only for the status codes whose
// description the standard says carries the reason; a notice is a line of its own. The sentences are the
// window's English source strings, so a log kept here reads as the window's does. A test holds them to the
// window's catalogue.
//
// What the callbacks cannot tell, whoever ran the operation sets on record(): that the target never
// answered, that a port could not be opened, that the media set changed after it was checked, and how the
// operation ended (result, reason, detail). The recorder does not know the request either, so an upload's
// media set and the loads asked for are set the same way.
//
// The callbacks run on the runner's thread, and so does everything else here: a recorder is not shared
// between threads.

#include "adl/arinc615a/data_loader.hpp"
#include "adl/loader_core/record.hpp"
#include "adl/platform/time.hpp"

#include <functional>
#include <string>
#include <vector>

namespace adl::loader_core
{

// The wall clock the times of a record come from: UTC to the second, or nothing where there is none. A test
// gives one that says what it likes; an empty function is the same as one that has no clock.
using WallClock = std::function<RecordTime()>;

// The clock of a platform (adl::platform::Clock::wall_time_seconds), which has none on a target with no
// real-time clock. `clock` has to outlive the function.
[[nodiscard]] WallClock wall_clock_of(adl::platform::Clock& clock);

class OperationRecorder : public adl::arinc615a::OperationListener
{
public:
    // Starts a record of `operation` on the target at `target_address` (dotted IPv4) called `thw_id_pos`, and
    // stamps its start from `clock`.
    OperationRecorder(adl::arinc615a::OperationKind operation, std::string target_address, std::string thw_id_pos,
                      WallClock clock);

    // The record, to read and to set what the recorder cannot know.
    [[nodiscard]] OperationRecord& record() noexcept
    {
        return record_;
    }
    [[nodiscard]] const OperationRecord& record() const noexcept
    {
        return record_;
    }

    // Adds a line to the log, stamped now: for what whoever runs the operation says besides what the engine
    // reports ("Finished: completed.").
    void add_log(Severity severity, std::string text);

    // The folder a download writes into, as the operating system writes it. It is the record's
    // `download_folder`, and each file received afterwards is recorded with its path in it: the folder, its
    // own separator ('/' when it has none) and the file's name.
    void set_download_folder(std::string folder);

    // The operation is over: stamps the end of the record.
    void finish();

    // The engine's callbacks. A listener that wants more derives from this and calls these first.
    void on_initialization(const adl::arinc615a::InitializationFile& file) noexcept override;
    void on_configuration_status(const adl::arinc615a::ConfigurationStatus& status) noexcept override;
    void on_upload_status(const adl::arinc615a::UploadStatus& status) noexcept override;
    void on_download_status(const adl::arinc615a::DownloadStatus& status) noexcept override;
    void on_configuration_list(const adl::arinc615a::ConfigurationList& list) noexcept override;
    // The choice is the operator's and comes later (DataLoader::provide_selection), as in the window: this
    // logs the list and defers. A listener that chooses at once overrides it.
    adl::arinc615a::SelectionDecision on_download_list(const adl::arinc615a::DownloadList& list,
                                                       adl::arinc615a::FileSelection& selection) noexcept override;
    void on_file_received(std::string_view name, const adl::arinc615a::ReceivedFile& file) noexcept override;
    void on_file_sent(std::string_view name, std::uint64_t bytes) noexcept override;
    void on_notice(adl::arinc615a::Notice notice, std::string_view subject, std::uint32_t value) noexcept override;

private:
    [[nodiscard]] RecordTime now() const;
    void note_status(adl::arinc615a::StatusCode status);
    // True the first time it is asked of `name`.
    [[nodiscard]] bool first_failure_of(std::string_view name);

    OperationRecord record_;
    WallClock clock_;
    std::vector<std::string> failed_logged_; // loads (by header file) and files (by name) already reported failed
};

} // namespace adl::loader_core

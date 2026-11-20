// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The listener of one operation: the recorder of adl::loader_core, which keeps the record, with the
// program's events written as the engine reports. Each call of the engine goes first to the recorder, which
// logs it and notes what the record needs; then every line the recorder added to the log is written as a `log`
// event, and the callbacks that have an event of their own write it (progress, configuration, file_list and
// file_received). So the `log` events of a run are the log lines of its record, one for one.
//
// The listener also makes the choice of the download in which the target lists its files: it answers the
// target's list with "deferred" and posts the choice to the operation's gate, which the runner turns into the
// engine's call on its next poll, as the window's choice arrives.

#include "adl/load/events.hpp"
#include "adl/loader_core/gate.hpp"
#include "adl/loader_core/recorder.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace adl::load
{

class Job final : public adl::loader_core::OperationRecorder
{
public:
    Job(EventWriter& events, adl::arinc615a::OperationKind operation, std::string target_address,
        std::string thw_id_pos, adl::loader_core::WallClock clock);

    // Adds a line to the log and writes it as a `log` event.
    void say(adl::loader_core::Severity severity, std::string text);

    // The download in which the target lists its files: which ones to take. `names` are the files picked, or
    // every file the target lists if `all`. The choice is made through `gate` when the list arrives.
    void choose_from_list(adl::loader_core::OperationGate& gate, bool all, std::vector<std::string> names);
    // The names that were picked and that the target does not list, in the order they were given.
    [[nodiscard]] const std::vector<std::string>& unlisted() const noexcept
    {
        return unlisted_;
    }
    // The target listed nothing to take (it listed no file, or none of the picked ones): the operation was
    // ended with the choice "none".
    [[nodiscard]] bool chose_nothing() const noexcept
    {
        return chose_nothing_;
    }

    // What the target said about why, in the last answer or status file that carries a reason (the standard
    // gives a description a meaning for a few status codes only). Empty when it said nothing.
    [[nodiscard]] const std::string& target_text() const noexcept
    {
        return target_text_;
    }

    void on_initialization(const adl::arinc615a::InitializationFile& file) noexcept override;
    void on_configuration_status(const adl::arinc615a::ConfigurationStatus& status) noexcept override;
    void on_upload_status(const adl::arinc615a::UploadStatus& status) noexcept override;
    void on_download_status(const adl::arinc615a::DownloadStatus& status) noexcept override;
    void on_configuration_list(const adl::arinc615a::ConfigurationList& list) noexcept override;
    adl::arinc615a::SelectionDecision on_download_list(const adl::arinc615a::DownloadList& list,
                                                       adl::arinc615a::FileSelection& selection) noexcept override;
    void on_file_received(std::string_view name, const adl::arinc615a::ReceivedFile& file) noexcept override;
    void on_file_sent(std::string_view name, std::uint64_t bytes) noexcept override;
    void on_notice(adl::arinc615a::Notice notice, std::string_view subject, std::uint32_t value) noexcept override;

private:
    // Writes the log lines added since the last time as `log` events.
    void flush_log();
    void note_reason(adl::arinc615a::StatusCode status, std::string_view description);

    EventWriter& events_;
    std::size_t logged_ = 0;
    std::string target_text_;

    adl::loader_core::OperationGate* gate_ = nullptr;
    bool pick_all_ = false;
    std::vector<std::string> picks_;
    std::vector<std::string> unlisted_;
    bool chose_nothing_ = false;
};

} // namespace adl::load

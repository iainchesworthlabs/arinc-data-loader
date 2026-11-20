// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/loader_core/recorder.hpp"

#include "adl/arinc615a/limits.hpp"

#include <algorithm>
#include <chrono>
#include <utility>

namespace adl::loader_core
{

using adl::arinc615a::Notice;
using adl::arinc615a::StatusCode;
using adl::arinc615a::TransferOutcome;

namespace
{

// The sentences are the source strings of the window's translation contexts `JobListener` and `Wording`
// (apps/app/core/src), with their %1, %2 and %n filled in. tests/loader_core/recorder_wording_test.cpp reads
// the window's catalogue and holds them to it.

[[nodiscard]] std::string number(std::uint64_t value)
{
    return std::to_string(value);
}

[[nodiscard]] std::string text_of(std::string_view view)
{
    return std::string{view};
}

// "Status 3: in progress, 40% of the list" and the like.
[[nodiscard]] std::string status_line(std::uint16_t counter, StatusCode status)
{
    return "Status " + number(counter) + ": " + text_of(adl::arinc615a::to_string(status));
}

// ", about 5 s left" when the target gave an estimate.
void append_time_left(std::string& line, std::uint32_t seconds)
{
    if (seconds != 0 && seconds != adl::arinc615a::estimated_time_unknown)
    {
        line += ", about " + number(seconds) + " s left";
    }
}

// What a description adds to a line: " (the reason)", or nothing.
void append_description(std::string& line, std::string_view description)
{
    if (!description.empty())
    {
        line += " (" + text_of(description) + ")";
    }
}

// The description of a load or a file, which counts only for the codes the standard gives one to.
[[nodiscard]] std::string_view meaningful_description(StatusCode status, std::string_view description)
{
    return adl::arinc615a::description_is_meaningful(status) ? description : std::string_view{};
}

// ": the reason" after a failure, or nothing.
[[nodiscard]] std::string reason_suffix(std::string_view description)
{
    return description.empty() ? std::string{} : ": " + text_of(description);
}

// The sentence of a notice: its phrase, the file it is about, and the number in the unit it has.
[[nodiscard]] std::string notice_line(Notice notice, std::string_view subject, std::uint32_t value)
{
    std::string line = adl::arinc615a::to_string(notice);
    if (!subject.empty())
    {
        line += ": " + text_of(subject);
    }
    switch (notice)
    {
    case Notice::status_counter_gap:
    case Notice::files_missing:
        if (value != 0)
        {
            line += " (" + number(value) + ")";
        }
        break;
    case Notice::waiting:
    case Notice::file_not_available:
        line += " (" + number(value) + " s)";
        break;
    case Notice::port_option_ignored:
        line += " (port " + number(value) + ")";
        break;
    case Notice::transfer_failed:
    case Notice::status_file_failed:
        // The value is how the transfer ended (a TransferOutcome); a number that is no outcome is left out.
        if (value <= static_cast<std::uint32_t>(TransferOutcome::aborted))
        {
            line += std::string{" ("} + adl::arinc615a::to_string(static_cast<TransferOutcome>(value)) + ")";
        }
        break;
    case Notice::status_duplicate:
    case Notice::status_stale:
    case Notice::checksum_mismatch:
    case Notice::checksum_unsupported:
    case Notice::request_refused:
    case Notice::option_fallback:
        break;
    }
    return line;
}

} // namespace

WallClock wall_clock_of(adl::platform::Clock& clock)
{
    return [&clock]() -> RecordTime
    {
        const auto seconds = clock.wall_time_seconds();
        if (!seconds)
        {
            return std::nullopt;
        }
        return std::chrono::sys_seconds{std::chrono::seconds{*seconds}};
    };
}

OperationRecorder::OperationRecorder(adl::arinc615a::OperationKind operation, std::string target_address,
                                     std::string thw_id_pos, WallClock clock)
    : clock_(std::move(clock))
{
    record_.operation = operation;
    record_.target_address = std::move(target_address);
    record_.target_position = std::move(thw_id_pos);
    record_.started_at = now();
}

RecordTime OperationRecorder::now() const
{
    return clock_ ? clock_() : RecordTime{};
}

void OperationRecorder::add_log(Severity severity, std::string text)
{
    record_.log.push_back(RecordedLogLine{now(), severity, std::move(text)});
}

void OperationRecorder::set_download_folder(std::string folder)
{
    record_.download_folder = std::move(folder);
}

void OperationRecorder::finish()
{
    record_.ended_at = now();
}

void OperationRecorder::note_status(StatusCode status)
{
    record_.status_code = status;
}

bool OperationRecorder::first_failure_of(std::string_view name)
{
    if (std::ranges::find(failed_logged_, name) != failed_logged_.end())
    {
        return false;
    }
    failed_logged_.emplace_back(name);
    return true;
}

void OperationRecorder::on_initialization(const adl::arinc615a::InitializationFile& file) noexcept
{
    note_status(file.status);
    std::string line = "The target answered: " + text_of(adl::arinc615a::to_string(file.status));
    append_description(line, file.description);
    add_log(file.status == StatusCode::accepted ? Severity::info : Severity::warning, std::move(line));
}

void OperationRecorder::on_configuration_status(const adl::arinc615a::ConfigurationStatus& status) noexcept
{
    note_status(status.status);
    std::string line = status_line(status.counter, status.status);
    append_time_left(line, status.estimated_time);
    append_description(line, status.description);
    add_log(Severity::info, std::move(line));
}

void OperationRecorder::on_upload_status(const adl::arinc615a::UploadStatus& status) noexcept
{
    note_status(status.status);
    std::string line = status_line(status.counter, status.status) + ", " + number(status.list_ratio) + "% of the list";
    append_time_left(line, status.estimated_time);
    append_description(line, meaningful_description(status.status, status.description));
    add_log(Severity::info, std::move(line));

    for (const adl::arinc615a::LoadProgress& load : status.loads)
    {
        const auto known = std::ranges::find_if(record_.loads,
                                                [&load](const RecordedLoad& recorded)
                                                {
                                                    return recorded.header_file == load.header_file_name &&
                                                           recorded.part_number == load.load_part_number_name;
                                                });
        if (known == record_.loads.end())
        {
            record_.loads.push_back(RecordedLoad{text_of(load.load_part_number_name), text_of(load.header_file_name),
                                                 load.status, load.ratio});
        }
        else
        {
            known->status = load.status;
            known->percent = load.ratio;
        }
        if (load.status == StatusCode::item_failed && first_failure_of(load.header_file_name))
        {
            add_log(Severity::error, "The target failed load " + text_of(load.load_part_number_name) + " (" +
                                         text_of(load.header_file_name) + ")" +
                                         reason_suffix(meaningful_description(load.status, load.description)));
        }
    }
}

void OperationRecorder::on_download_status(const adl::arinc615a::DownloadStatus& status) noexcept
{
    note_status(status.status);
    std::string line = status_line(status.counter, status.status) + ", " + number(status.list_ratio) + "% of the list";
    append_time_left(line, status.estimated_time);
    append_description(line, meaningful_description(status.status, status.description));
    add_log(Severity::info, std::move(line));

    for (const adl::arinc615a::FileProgress& file : status.files)
    {
        if (file.status == StatusCode::item_failed && first_failure_of(file.file_name))
        {
            add_log(Severity::error, "The target failed file " + text_of(file.file_name) +
                                         reason_suffix(meaningful_description(file.status, file.description)));
        }
    }
}

void OperationRecorder::on_configuration_list(const adl::arinc615a::ConfigurationList& list) noexcept
{
    std::vector<RecordedHardware> hardware;
    for (const adl::arinc615a::HardwareItem& item : list.hardware)
    {
        RecordedHardware recorded{text_of(item.literal_name), text_of(item.serial_number), {}};
        for (const adl::arinc615a::HardwarePart& part : item.parts)
        {
            recorded.parts.push_back(
                RecordedPart{text_of(part.part_number), text_of(part.amendment), text_of(part.designation)});
        }
        hardware.push_back(std::move(recorded));
    }
    record_.configuration = std::move(hardware);
    add_log(Severity::info, "The target listed its configuration.");
}

adl::arinc615a::SelectionDecision
OperationRecorder::on_download_list(const adl::arinc615a::DownloadList& list,
                                    adl::arinc615a::FileSelection& /*selection*/) noexcept
{
    add_log(Severity::info, "The target listed " + number(list.files.size()) + " file(s) it can download.");
    return adl::arinc615a::SelectionDecision::deferred;
}

void OperationRecorder::on_file_received(std::string_view name, const adl::arinc615a::ReceivedFile& file) noexcept
{
    RecordedFile recorded{text_of(name), file.size, {}};
    if (record_.download_folder)
    {
        const std::string& folder = *record_.download_folder;
        const bool backslashes = folder.find('\\') != std::string::npos && folder.find('/') == std::string::npos;
        const char separator = backslashes ? '\\' : '/';
        recorded.path = folder;
        if (!folder.empty() && folder.back() != '/' && folder.back() != '\\')
        {
            recorded.path += separator;
        }
        recorded.path += recorded.name;
    }
    record_.files.push_back(std::move(recorded));
    add_log(Severity::info, "Received " + text_of(name) + " (" + number(file.size) + " bytes)");
}

void OperationRecorder::on_file_sent(std::string_view name, std::uint64_t bytes) noexcept
{
    add_log(Severity::info, "The target read " + text_of(name) + " (" + number(bytes) + " bytes)");
}

void OperationRecorder::on_notice(Notice notice, std::string_view subject, std::uint32_t value) noexcept
{
    add_log(Severity::note, notice_line(notice, subject, value));
}

} // namespace adl::loader_core

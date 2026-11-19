// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The record of one ARINC 615A operation: what the loader's receipt and log show, as plain data. The
// recorder (recorder.hpp) fills one from the engine's callbacks, whoever ran the operation sets what the
// callbacks cannot tell (that the target never answered, that a local port could not be opened, that the
// media set changed), and record_json.hpp writes it as the file other programs read.
//
// The fields are the keys of that file, in its order, and the file's page (docs/file-reference.md) says what
// each one holds. Every text is English, whatever language a window is in. A text that came from the target
// (a description, a file name) is kept as the target sent it; the file writer makes it valid UTF-8.

#include "adl/arinc615a/data_loader.hpp"
#include "adl/arinc615a/status_code.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace adl::loader_core
{

// A moment in UTC, to the second, or nothing where the platform has no wall clock.
using RecordTime = std::optional<std::chrono::sys_seconds>;

// How serious a line of the log is. The words are the ones the commands' `log` event uses: the window's
// "notice" is `note` here.
enum class Severity : std::uint8_t
{
    info,
    note,
    warning,
    error
};

[[nodiscard]] constexpr std::string_view to_string(Severity severity) noexcept
{
    switch (severity)
    {
    case Severity::info:
        return "info";
    case Severity::note:
        return "note";
    case Severity::warning:
        return "warning";
    case Severity::error:
        return "error";
    }
    return "info";
}

// The name of a status code for a program to read: the enumerator's name in lower case with underscores
// ("in_progress", "item_failed"). It is the same word in every language and every version, which a
// sentence is not: adl::arinc615a::to_string words the status for a person ("in progress", "failed"), and
// the log is built from that. The file writes the status of a load by this name.
[[nodiscard]] constexpr std::string_view status_name(adl::arinc615a::StatusCode status) noexcept
{
    using adl::arinc615a::StatusCode;
    switch (status)
    {
    case StatusCode::accepted:
        return "accepted";
    case StatusCode::in_progress:
        return "in_progress";
    case StatusCode::completed:
        return "completed";
    case StatusCode::in_progress_with_text:
        return "in_progress_with_text";
    case StatusCode::operation_denied:
        return "operation_denied";
    case StatusCode::operation_not_supported:
        return "operation_not_supported";
    case StatusCode::aborted_by_target:
        return "aborted_by_target";
    case StatusCode::aborted_by_data_loader:
        return "aborted_by_data_loader";
    case StatusCode::aborted_by_operator:
        return "aborted_by_operator";
    case StatusCode::item_failed:
        return "item_failed";
    }
    return "unknown";
}

// The media set an upload read its loads from.
struct RecordedMediaSet
{
    std::string path;
    std::string part_number;
    unsigned members = 0;
};

// One load of an upload, as the target last reported it.
struct RecordedLoad
{
    std::string part_number;
    std::string header_file;
    adl::arinc615a::StatusCode status = adl::arinc615a::StatusCode::accepted;
    unsigned percent = 0;
};

// A part of a piece of hardware an Information operation read.
struct RecordedPart
{
    std::string part_number;
    std::string amendment; // empty when there is none
    std::string designation;
};

// A piece of hardware the target listed, with its parts.
struct RecordedHardware
{
    std::string literal_name;
    std::string serial_number;
    std::vector<RecordedPart> parts;
};

// A file a download received.
struct RecordedFile
{
    std::string name;
    std::uint64_t size = 0;
    std::string path; // where it was written, as the operating system writes a path; empty when not known
};

// One line of the log.
struct RecordedLogLine
{
    RecordTime at;
    Severity severity = Severity::info;
    std::string text;
};

// Everything the file holds. A field that does not apply to the operation stays empty (a `std::nullopt`
// or an empty list), and the file writes it as `null` or `[]`.
struct OperationRecord
{
    // The program that wrote the record.
    std::string program_name;
    std::string program_version;

    adl::arinc615a::OperationKind operation = adl::arinc615a::OperationKind::information;
    std::string target_address;  // dotted IPv4
    std::string target_position; // the THW_ID_POS

    RecordTime started_at;
    RecordTime ended_at;

    // How it ended, for a program to read. `result` is "completed" or "failed" and `reason` one of the words of
    // the commands' contract ("completed", "target_refused", "target_aborted", "load_failed", "missing_files",
    // "protocol_error", "aborted", "media_set_unreadable", "media_set_changed", "no_answer", "local_error");
    // `detail` is one English sentence for a person. They are set by whoever ran the operation. The recorder
    // fills `status_code` with the last operation status the target reported, which that program may replace.
    std::string result;
    std::string reason;
    std::optional<adl::arinc615a::StatusCode> status_code;
    std::string detail;

    std::optional<RecordedMediaSet> media_set;                  // an upload
    std::vector<RecordedLoad> loads;                            // an upload
    std::optional<std::vector<RecordedHardware>> configuration; // an Information operation, once it was read
    std::vector<RecordedFile> files;                            // a download
    std::optional<std::string> download_folder;                 // a download, unless no folder was kept

    std::vector<RecordedLogLine> log;
};

} // namespace adl::loader_core

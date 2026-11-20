// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// What the program writes to standard output: the events of docs/load-command.md. With `--json` each event
// is one JSON object on one line, `event` first and `t` (the seconds since the program started, with
// milliseconds) second, written out and flushed at once so that a reader sees it when it happens. Without
// it each event is one plain line a person can read; control characters in what a target sent are shown as
// `?` there, so that a target cannot move the cursor of a terminal. Nothing else goes to standard output.

#include "adl/loader_core/record.hpp"
#include "adl/platform/time.hpp"

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace adl::load
{

// A target that answered FIND.
struct FoundTarget
{
    std::string address;
    std::string hardware_id;
    std::string type_name;
    std::string position;
    std::string literal_name;
    std::string manufacturer_code;
    std::string thw_id_pos;
};

// The media set an upload checked.
struct CheckedMediaSet
{
    std::string path;
    std::string part_number;
    unsigned members = 0;
    std::size_t loads = 0;
    std::vector<std::string> findings;
};

// A file the target lists in an operator defined download.
struct ListedFile
{
    std::string name;
    std::string description;
};

// How the program ended.
struct FinishedEvent
{
    bool completed = false; // `result`: completed or failed
    std::string_view reason;
    std::optional<std::uint16_t> status_code;
    std::string detail;
    int exit_code = 0;
    std::optional<std::size_t> count; // find: how many targets answered
    // A download: the folder the files were written into, or none when none was kept. Only a download has the key.
    bool is_download = false;
    std::optional<std::string> download_folder;
    // The operation record, when `--record` asked for one: the path it was written to. The key is there whenever
    // the option was given, and is null when the record could not be written.
    bool has_record = false;
    std::optional<std::string> record;
};

class EventWriter
{
public:
    // `start` is the moment on `clock` that `t` counts from.
    EventWriter(std::ostream& out, bool json, adl::platform::Clock& clock, adl::platform::Instant start);

    [[nodiscard]] bool json() const noexcept
    {
        return json_;
    }

    // `target` and `position` are for an operation; find has neither.
    void started(std::string_view command, const std::optional<std::string>& target,
                 const std::optional<std::string>& position);
    void target_found(const FoundTarget& target);
    void media_set_checked(const CheckedMediaSet& media_set);
    void log(adl::loader_core::Severity severity, std::string_view text);
    void configuration(std::span<const adl::loader_core::RecordedHardware> hardware);
    // `loads` as the target's status file lists them.
    void progress(unsigned percent, std::span<const adl::loader_core::RecordedLoad> loads);
    void file_list(std::span<const ListedFile> files);
    void file_received(const adl::loader_core::RecordedFile& file);
    void finished(const FinishedEvent& event);

private:
    // The start of an event's object: `event` and `t`.
    [[nodiscard]] std::uint64_t elapsed_milliseconds();
    void write(std::string_view line);

    std::ostream& out_;
    bool json_;
    adl::platform::Clock& clock_;
    adl::platform::Instant start_;
};

} // namespace adl::load

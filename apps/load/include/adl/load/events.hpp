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
#include <utility>
#include <vector>

namespace adl::load
{

class JsonLine;

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

// The scenario file of the operation (`--scenario`), as `started` says it.
struct ScenarioInfo
{
    std::string path; // as the operator wrote it
    std::size_t steps = 0;
    std::optional<std::uint64_t> seed;
};

// A step of the scenario that acted: what the loader did, in the words of the file. The parameter is the one
// number the action used, under the key the file has for it: `ms` (delay), `within_ms` (reorder), `for_ms` (silent,
// which is null when it lasts the rest of the operation), `copies`, `delta`, `code`, `seconds` or `status`; drop,
// corrupt and check_value have none.
struct ScenarioStepEvent
{
    std::size_t step = 0;     // the step's number in the file, from 1
    std::string_view trigger; // request, status_file or after_ms
    std::uint64_t at = 0;     // the request, the status file or the milliseconds that put it in effect
    std::string_view operation;
    std::string_view action;
    // The datagram that was acted on, for an action that acts on one (not abort and silent).
    std::optional<std::string_view> datagram;
    std::string_view file; // empty: none
    std::optional<std::uint16_t> block;
    std::optional<std::string> to; // "address:port"
    std::string_view parameter;    // the key, or empty
    std::optional<std::int64_t> value;
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
    // With `--scenario`: the numbers of the steps that were in effect and never acted, which is empty when every one
    // did. Without it the key is not there.
    std::optional<std::vector<std::size_t>> scenario_unused;
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

    // Every event of this writer then carries `id`, after `t`: the events of one operation of `adl_load serve`.
    void set_id(std::string id)
    {
        id_ = std::move(id);
    }
    // Standard output could not be written to (a closed pipe, a full disk): the stream has failed.
    [[nodiscard]] bool failed() const;

    // The three events of `adl_load serve` that belong to no operation. They are written as JSON lines.
    // `address` is the interface the loader listens on, as dotted numbers.
    void serve_started(std::string_view address, std::uint16_t control_port, std::size_t max_operations);
    // A request that was not done. `id` is the request's, if it had one that could be read, and `request` is
    // `run`, `cancel` or `stop`, or none when the line could not be read that far. `line_number` counts from 1.
    void request_refused(const std::optional<std::string>& id, const std::optional<std::string>& request,
                         std::uint64_t line_number, std::string_view detail);
    // `reason` is `stop`, `end_of_input` or `error`; `operations` how many were started.
    void stopped(std::string_view reason, std::uint64_t operations);

    // `target` and `position` are for an operation; find has neither. `scenario` is there when `--scenario` was
    // given.
    void started(std::string_view command, const std::optional<std::string>& target,
                 const std::optional<std::string>& position,
                 const std::optional<ScenarioInfo>& scenario = std::nullopt);
    // A step of the scenario acted. Written as the loader does it, so before the events of what follows.
    void scenario_step(const ScenarioStepEvent& step);
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
    [[nodiscard]] std::uint64_t elapsed_milliseconds();
    // The start of an event's object: `event` and `t`, and `id` when there is one.
    [[nodiscard]] JsonLine json_event(std::string_view name, std::uint64_t milliseconds) const;
    // A whole line, written out at once.
    void write(std::string_view line);

    std::ostream& out_;
    bool json_;
    adl::platform::Clock& clock_;
    adl::platform::Instant start_;
    std::optional<std::string> id_;
};

} // namespace adl::load

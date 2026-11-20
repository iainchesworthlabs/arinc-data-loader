// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// What adl_mock tells the world, as data, and the two ways it is written: a plain line for a person
// and a JSON object for a program (docs/mock-target.md, "The command"). Both are one line for each event.
// Nothing here knows about sockets or the engine.

#include "adl/arinc615a/status_code.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace adl::mock::cli
{

// The number of this format of events. A reader refuses a number it does not know; a key can be added
// without changing it, and removing, renaming or changing the meaning of one changes it.
inline constexpr int events_format = 1;

// The scenario that was given, for the first event (docs/user-guide/mock-scenarios.md).
struct ScenarioInfo
{
    std::string path; // as it was given
    std::size_t steps = 0;
    std::optional<std::uint64_t> seed = std::nullopt; // none: the file has none
};

// The first event: the target is listening.
struct Started
{
    std::string program;
    std::string version;
    std::string thw_id_pos;
    std::string address;
    std::uint16_t find_port = 0;
    std::uint16_t tftp_port = 0;
    std::vector<std::string> faults;                     // the fault options that are on, as written (--silent)
    std::optional<ScenarioInfo> scenario = std::nullopt; // with --scenario
    std::vector<std::string> features = {}; // every action word of a scenario this build knows; no key when empty
};

// An operation has started: the target accepted a request and is working on it.
struct OperationBegan
{
    std::string operation; // information, upload, download_media_defined or download_operator_defined
};

struct Progress
{
    std::string operation;
    int percent = 0;
};

// An upload completed: the files of one load, read from the loader.
struct UploadReceived
{
    std::string part_number;
    std::vector<std::string> files;
    std::optional<std::string> stored_in; // the load's folder, or none when nothing was kept
};

// An operation is over: its last status file went out, or it was refused and never began.
struct OperationEnded
{
    std::string operation;
    std::optional<std::uint16_t> status; // none: an initialization file of an invalid structure
    std::string status_name;             // completed, aborted_by_target, invalid_initialization_file ...
    bool refused = false;
};

// A step of the scenario whose trigger occurred. Only with --scenario.
struct ScenarioStepFired
{
    std::size_t step = 0;  // from 1: the place in the file
    std::string trigger;   // request, status_file or after_ms
    std::int64_t at = 0;   // the request or status file number, or the milliseconds since the operation began
    std::string operation; // information, upload, download_media_defined or download_operator_defined
    std::string action;    // wait, refuse, delay ...
    bool applied = true;   // false: the step's chance said no
    std::optional<std::int64_t> seconds = std::nullopt;   // wait
    std::optional<std::int64_t> status = std::nullopt;    // refuse: the status code, 4096 or 4098
    std::optional<std::int64_t> ms = std::nullopt;        // delay: the milliseconds that were drawn
    std::optional<std::int64_t> copies = std::nullopt;    // duplicate_datagram
    std::optional<std::int64_t> within_ms = std::nullopt; // reorder_datagram
    // The faults on a datagram: how many datagrams, and which (the keys of the step's "on").
    std::optional<std::int64_t> count = std::nullopt;
    std::optional<std::string> datagram = std::nullopt;
    std::optional<std::string> file = std::nullopt;
    std::optional<std::int64_t> block = std::nullopt;
};

// A fault on a datagram met a datagram. Only with --scenario.
struct ScenarioDatagram
{
    std::size_t step = 0; // from 1
    std::string action;   // delay, drop_datagram, duplicate_datagram or reorder_datagram
    bool applied = true;  // false: the step's chance said no for this datagram
    std::string datagram; // request, option_ack, ack, data, error or other
    std::optional<std::string> file = std::nullopt;
    std::optional<std::int64_t> block = std::nullopt;
    std::string to; // "127.0.0.1:40001"
    std::size_t size = 0;
};

// The last event.
struct Stopped
{
    std::string reason; // operations, seconds or error
    std::uint64_t operations = 0;
    std::optional<std::vector<std::size_t>> scenario_unused =
        std::nullopt; // with --scenario: the steps that never fired
    std::optional<std::vector<std::size_t>> scenario_idle =
        std::nullopt; // with --scenario: the steps on a datagram that were armed and met none
};

using EventData = std::variant<Started, OperationBegan, Progress, UploadReceived, OperationEnded, ScenarioStepFired,
                               ScenarioDatagram, Stopped>;

struct Event
{
    std::chrono::milliseconds t{}; // since the program started
    EventData data;
};

// The "event" key: started, operation_began, progress, upload_received, operation_ended, scenario_step,
// scenario_datagram, stopped.
[[nodiscard]] std::string_view event_name(const EventData& data) noexcept;

// The event as one JSON object, with "event" and "t" first. No line feed.
[[nodiscard]] std::string to_json_line(const Event& event);

// The event as one line of words for a person: the time, the name, then key=value pairs. A value
// with a space or a quote in it is written as a JSON string. No line feed.
[[nodiscard]] std::string to_plain_line(const Event& event);

// "Download (media defined)" as download_media_defined, "Information" as information: the engine's
// words for an operation as one word for a program.
[[nodiscard]] std::string operation_id(std::string_view display_name);

// A status code as one word for a program, the name of its enumerator: completed, operation_denied.
[[nodiscard]] std::string_view status_name(adl::arinc615a::StatusCode code) noexcept;

// The status_name of an operation with no status: the initialization file could not be read.
inline constexpr std::string_view invalid_initialization_file = "invalid_initialization_file";

} // namespace adl::mock::cli

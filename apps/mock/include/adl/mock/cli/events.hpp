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

// The first event: the target is listening.
struct Started
{
    std::string program;
    std::string version;
    std::string thw_id_pos;
    std::string address;
    std::uint16_t find_port = 0;
    std::uint16_t tftp_port = 0;
    std::vector<std::string> faults; // the fault options that are on, as written (--silent)
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

// The last event.
struct Stopped
{
    std::string reason; // operations, seconds or error
    std::uint64_t operations = 0;
};

using EventData = std::variant<Started, OperationBegan, Progress, UploadReceived, OperationEnded, Stopped>;

struct Event
{
    std::chrono::milliseconds t{}; // since the program started
    EventData data;
};

// The "event" key: started, operation_began, progress, upload_received, operation_ended, stopped.
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

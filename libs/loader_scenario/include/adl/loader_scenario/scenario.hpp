// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// A scenario for the data loader: an ordered list of steps, each of which says when it takes effect, which of
// the datagrams the loader sends it acts on, and what it does to them (a fault), so that a run can be repeated
// exactly and a supplier can see how a target reacts to a loader that does something wrong on purpose. The
// file that holds one is in reader.hpp; the Player (player.hpp) carries one out through the engine's
// FaultHook. docs/file-reference.md has the file and the meaning of every action.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace adl::loader_scenario
{

// The `kind` and `format` of the file, and what the reader accepts.
inline constexpr std::string_view scenario_kind = "adl.loader-scenario";
inline constexpr std::int64_t scenario_format = 1;
inline constexpr std::size_t max_file_bytes = 64 * 1024;
inline constexpr std::size_t max_steps = 256;
inline constexpr std::size_t max_name_chars = 80;
inline constexpr std::size_t max_text_chars = 64;
inline constexpr std::size_t max_file_selector_chars = 254;
inline constexpr std::int64_t max_seed = std::int64_t{1} << 53;
inline constexpr std::int64_t max_milliseconds = 600'000;

// What a step does.
enum class Action : std::uint8_t
{
    drop,          // the datagram is not sent
    duplicate,     // it is sent `copies` times in a row
    delay,         // it is sent `milliseconds` later
    reorder,       // it is held and sent after the next datagram the loader sends, or after `milliseconds`
    corrupt,       // one byte of it is exclusive-or'ed with `xor_mask`
    check_value,   // the check value in the next option acknowledgement that carries one is changed
    length,        // it is made `delta` bytes longer or shorter
    error,         // it is replaced by an ERROR packet of `code` and `text`
    wait,          // it is replaced by WAIT:`seconds`
    abort_message, // it is replaced by ABORT:`status`
    abort,         // the loader's own abort, with `status` (1004 or 1005), at that moment
    silence,       // nothing is sent for `milliseconds`, or for the rest of the operation
};

inline constexpr std::size_t action_count = 12;

// "drop", "duplicate", "delay", "reorder", "corrupt", "check_value", "length", "error", "wait", "abort_message",
// "abort", "silence": the words of the file and of the events.
[[nodiscard]] std::string_view to_string(Action action) noexcept;
[[nodiscard]] std::optional<Action> action_from_string(std::string_view text) noexcept;

// Whether the action is done to the datagrams the loader sends one at a time (and so has an `on`, a `count` and
// a `per_mille`), or to the operation (abort and silence).
[[nodiscard]] constexpr bool acts_on_datagrams(Action action) noexcept
{
    return action != Action::abort && action != Action::silence;
}

// The kinds of datagram the loader sends (and `reply` and `any`, which only a selector uses).
enum class DatagramKind : std::uint8_t
{
    any,        // selector: every datagram
    request,    // a read or write request (the initialization file, a list)
    option_ack, // an acknowledgement of the options of a request
    ack,        // an acknowledgement of a block
    data,       // a block of a file
    error,      // an ERROR packet, WAIT and ABORT included
    reply,      // selector: the first datagram the loader sends in answer to a request of the target
};

// "any", "request", "option_ack", "ack", "data", "error", "reply".
[[nodiscard]] std::string_view to_string(DatagramKind kind) noexcept;
[[nodiscard]] std::optional<DatagramKind> datagram_kind_from_string(std::string_view text) noexcept;

// When a step takes effect.
enum class TriggerKind : std::uint8_t
{
    start,       // from the beginning
    request,     // after the nth request of the target
    status_file, // after the nth status file the target has started to write
    time,        // `value` milliseconds after the operation began
};

struct Trigger
{
    TriggerKind kind = TriggerKind::start;
    std::uint64_t value = 0; // n, or milliseconds

    friend bool operator==(const Trigger&, const Trigger&) = default;
};

// Which of the loader's datagrams a step acts on: all that are given have to match.
struct Selector
{
    DatagramKind datagram = DatagramKind::any;
    // The file the datagram's transfer is for: a name (compared without regard to case), or an extension from
    // its dot (".LUS"), or empty for any.
    std::string file;
    std::optional<std::uint16_t> block; // a data or ack datagram of this block

    friend bool operator==(const Selector&, const Selector&) = default;
};

// The parameters of an action. Each action reads the ones it names above and ignores the rest.
struct Fault
{
    Action action = Action::drop;
    unsigned copies = 2;
    std::int64_t milliseconds = 0;
    unsigned offset = 4;
    unsigned xor_mask = 1;
    int delta = 0;
    unsigned code = 0;
    std::string text;
    unsigned seconds = 0;
    unsigned status = 0x1005;

    friend bool operator==(const Fault&, const Fault&) = default;
};

struct Step
{
    Trigger when;
    Selector on;
    Fault fault;
    unsigned count = 1;        // how many datagrams the step is applied to ...
    bool all = false;          // ... or every one that matches
    unsigned per_mille = 1000; // the chance that a datagram that matches is the one

    friend bool operator==(const Step&, const Step&) = default;
};

struct Scenario
{
    std::string name;
    std::uint64_t seed = 1;
    std::vector<Step> steps;

    friend bool operator==(const Scenario&, const Scenario&) = default;
};

} // namespace adl::loader_scenario

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

// The `kind` and `format` of the file, and what the reader accepts. The top level is the mock target's scenario
// file's (docs/file-reference.md): the same `kind` family, `format`, `name`, `description`, `seed`, `steps`, the
// same `when` and `per_mille`; the `do` actions and the `on` and `count` of a step are the loader's own.
inline constexpr std::string_view scenario_kind = "adl-loader-scenario";
inline constexpr std::string_view mock_scenario_kind = "adl-mock-scenario";
inline constexpr std::int64_t scenario_format = 1;
inline constexpr std::size_t max_file_bytes = 256 * 1024;
inline constexpr std::size_t max_steps = 1000;
inline constexpr std::size_t max_name_chars = 64;
inline constexpr std::size_t max_description_chars = 1000;
inline constexpr std::size_t max_comment_chars = 200;
inline constexpr std::size_t max_text_chars = 64;
inline constexpr std::size_t max_file_selector_chars = 254;
inline constexpr std::int64_t max_seed = (std::int64_t{1} << 53) - 1;
inline constexpr std::int64_t max_delay_milliseconds = 60'000;
inline constexpr std::int64_t max_after_milliseconds = 86'400'000;

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
    abort,         // the loader's own abort, with `status` (0x1004 or 0x1005), at that moment
    silent,        // nothing is sent for `milliseconds`, or for the rest of the operation
};

inline constexpr std::size_t action_count = 12;

// "drop", "duplicate", "delay", "reorder", "corrupt", "check_value", "length", "error", "wait", "abort_message",
// "abort", "silent": the words of the file and of the events.
[[nodiscard]] std::string_view to_string(Action action) noexcept;
[[nodiscard]] std::optional<Action> action_from_string(std::string_view text) noexcept;

// Whether the action is done to the datagrams the loader sends one at a time (and so has an `on`, a `count` and
// a `per_mille`), or to the operation (abort and silent).
[[nodiscard]] constexpr bool acts_on_datagrams(Action action) noexcept
{
    return action != Action::abort && action != Action::silent;
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

// What puts a step in effect: the same three as the mock target's file, counted from the loader's side. The loader
// counts what it receives: a `request` is a request the target sends it.
enum class TriggerKind : std::uint8_t
{
    request,     // the nth request of the target
    status_file, // the nth status file the target has started to write
    after_ms,    // `value` milliseconds after the operation began (its first command)
};

// "request", "status_file", "after_ms".
[[nodiscard]] std::string_view to_string(TriggerKind kind) noexcept;

// The operations of the loader, in the words of the events and of the operation record.
enum class OperationName : std::uint8_t
{
    any,
    information,
    upload,
    download_media_defined,
    download_operator_defined,
};

// "any", "information", "upload", "download_media_defined", "download_operator_defined".
[[nodiscard]] std::string_view to_string(OperationName operation) noexcept;
[[nodiscard]] std::optional<OperationName> operation_from_string(std::string_view text) noexcept;

struct When
{
    TriggerKind kind = TriggerKind::after_ms;
    std::uint64_t value = 0; // n, or milliseconds
    std::uint64_t every = 0; // after the first occurrence, again every k requests, status files or milliseconds
    std::uint64_t times = 0; // at most this many occurrences in all; 0 for no limit
    OperationName operation = OperationName::any; // only in a run of that operation

    friend bool operator==(const When&, const When&) = default;
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
    // delay: the time, or a time drawn between min and max (a range when they differ); reorder and silent: how long
    // (0: for the rest of the operation, or the default hold of a reorder).
    std::int64_t milliseconds = 0;
    std::int64_t min_milliseconds = 0;
    std::int64_t max_milliseconds = 0;
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
    When when;
    Selector on;
    Fault fault;
    // Each time the trigger occurs the step can be applied to this many datagrams that match `on` (the next ones),
    // or to every one that matches from then on.
    unsigned count = 1;
    bool all = false;
    // The chance, in thousandths, that the step is applied to a datagram it could be applied to; drawn from the
    // step's own stream of the seed. None: always.
    std::optional<unsigned> per_mille;
    std::string comment; // never used

    friend bool operator==(const Step&, const Step&) = default;
};

struct Scenario
{
    std::string name;
    std::string description;
    std::optional<std::uint64_t> seed; // the reader requires it when a step has a per_mille or a delay range
    std::vector<Step> steps;

    friend bool operator==(const Scenario&, const Scenario&) = default;
};

} // namespace adl::loader_scenario

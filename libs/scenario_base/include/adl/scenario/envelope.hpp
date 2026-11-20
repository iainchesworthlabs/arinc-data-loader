// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The part that every scenario file has in common. A scenario file says which fault happens when, so that a run
// with faults can be repeated. There is one for the mock target (kind "adl-mock-scenario") and one for the
// loader ("adl-loader-scenario"); they share this top level and the way a step is written, and differ in what the
// steps do. This header reads the shared part, with one sentence for every way a file can be wrong, and hands
// each step to the kind's own reader as parts it can look into.
//
//   { "kind": "adl-mock-scenario", "format": 1, "name": "...", "description": "...", "seed": 20261120,
//     "steps": [ { "when": { "request": 1 }, "do": { "action": "wait", "seconds": 2 } },
//                { "when": { "status_file": 2, "every": 1, "times": 3 }, "per_mille": 500,
//                  "do": { "action": "drop_status_file" }, "comment": "half of them" } ] }
//
// The file is read with the licence library's strict JSON reader (adl/licence/json.hpp): whole numbers only, no
// repeated key, no comment, no trailing comma, UTF-8. A key that this version does not know is refused and not
// ignored, so that a mistyped one cannot be a fault that never happens.
//
// The step numbers in the sentences are counted from 1, as they are in the events the programs write.

#include "adl/licence/json.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace adl::scenario
{

namespace json = adl::licence::json;

// The limits that every scenario file keeps.
inline constexpr int format_number = 1;
inline constexpr std::size_t max_file_size = 262144;
inline constexpr std::size_t max_steps = 1000;
inline constexpr std::size_t max_name_characters = 64;
inline constexpr std::size_t max_description_characters = 1000;
inline constexpr std::size_t max_comment_characters = 200;
inline constexpr std::size_t max_action_characters = 32;
inline constexpr std::int64_t max_seed = 9007199254740991; // 2^53 - 1, which every JSON tool keeps exactly
inline constexpr std::int64_t max_occurrence = 1000000;    // a request, a status file, `every` and `times`
inline constexpr std::int64_t max_after_ms = 86400000;     // a day
inline constexpr std::int64_t min_every_ms = 10;           // `every` with `after_ms`
inline constexpr std::int64_t max_per_mille = 1000;

// The kinds of file that are known, and whom each is for.
inline constexpr std::string_view mock_kind = "adl-mock-scenario";
inline constexpr std::string_view loader_kind = "adl-loader-scenario";

// When a step fires: the nth request, the nth status file of an operation, or a time since the operation began.
enum class Trigger : std::uint8_t
{
    request,
    status_file,
    after_ms,
};

[[nodiscard]] std::string_view trigger_name(Trigger trigger) noexcept;

// One sentence for a person that says what is wrong with a file. English, and not translated.
struct ReadError
{
    std::string message;
};

// What a kind adds to the shared shape: the keys it allows beyond the shared ones. The shared keys of a step are
// `when`, `do`, `per_mille` and `comment`; of `when`, the three triggers, `every` and `times`.
struct Family
{
    std::string_view kind;
    std::span<const std::string_view> when_keys;
    std::span<const std::string_view> step_keys;
};

struct When
{
    Trigger trigger = Trigger::request;
    std::int64_t at = 0;               // the request or status file number (from 1), or the milliseconds
    std::optional<std::int64_t> every; // then again every this many requests, status files or milliseconds
    std::optional<std::int64_t> times; // at most this many occurrences in all
    json::Object rest;                 // the members of `when` that the family named, as they were written
};

struct Step
{
    std::size_t number = 0; // from 1
    When when;
    std::optional<std::int64_t> per_mille; // each occurrence is applied with this chance in a thousand
    std::string comment;                   // never used by a program
    std::string action;                    // the `action` of `do`: lower-case letters, digits and "_"
    json::Object action_members;           // every member of `do`, `action` included
    json::Object rest;                     // the members of the step that the family named
};

struct Envelope
{
    std::string name;
    std::string description;
    std::optional<std::uint64_t> seed;
    std::vector<Step> steps;
};

// Reads the shared part of `text` for a file of `family.kind`. A step that uses `per_mille` needs a `seed`; a
// family whose actions are random in some other way (a range of delays) checks that itself, with missing_seed().
[[nodiscard]] std::expected<Envelope, ReadError> read_envelope(std::string_view text, const Family& family);

// ---- What a family needs to read its own keys, with the same sentences ----------------------------------------------

// A whole number from `low` to `high` that `object` must have. `step` is the number of the step the sentence is
// about, or 0 for the top level of the file.
[[nodiscard]] std::expected<std::int64_t, ReadError>
whole_number(const json::Object& object, std::string_view key, std::int64_t low, std::int64_t high, std::size_t step);

// The same, for a key that may be left out.
[[nodiscard]] std::expected<std::optional<std::int64_t>, ReadError>
optional_whole_number(const json::Object& object, std::string_view key, std::int64_t low, std::int64_t high,
                      std::size_t step);

// Text of 1 to `max_characters` characters with no control character, for a key that `object` must have.
[[nodiscard]] std::expected<std::string, ReadError> text(const json::Object& object, std::string_view key,
                                                         std::size_t max_characters, std::size_t step);

// The same, for a key that may be left out.
[[nodiscard]] std::expected<std::optional<std::string>, ReadError>
optional_text(const json::Object& object, std::string_view key, std::size_t max_characters, std::size_t step);

// Refuses a member whose key is not in `allowed`. `part` names the object inside the step ("when" or "do"), or is
// empty for the step itself.
[[nodiscard]] std::optional<ReadError> only_keys(const json::Object& object, std::span<const std::string_view> allowed,
                                                 std::size_t step, std::string_view part = {});

// The sentence for a step that is random (`key` is what makes it so) in a file with no seed.
[[nodiscard]] ReadError missing_seed(std::size_t step, std::string_view key);

// The sentence for a value that the key does not take, with `what` saying what it takes ("one of ...").
[[nodiscard]] ReadError not_taken(std::size_t step, std::string_view key, std::string_view what);

// The number of characters of UTF-8 text (code points, not bytes).
[[nodiscard]] std::size_t character_count(std::string_view utf8) noexcept;

} // namespace adl::scenario

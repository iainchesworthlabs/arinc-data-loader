// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// What the fuzzer of the shared reader of the scenario files (libs/scenario_base/fuzz) and the test that replays its
// seeds share: the bytes as a scenario file, read for each of the kinds there are, and what must hold of whatever the
// reader makes of them. The input is the file itself.

#include "adl/scenario/envelope.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace adl::test
{

// Whether `members` have only keys that are in `allowed`.
[[nodiscard]] inline bool only_named(const adl::scenario::json::Object& members,
                                     std::span<const std::string_view> allowed)
{
    return std::ranges::all_of(members, [allowed](const adl::scenario::json::Member& member)
                               { return std::ranges::find(allowed, member.key) != allowed.end(); });
}

// What must hold of an envelope that was accepted: every number is inside its limit, the steps are numbered from 1,
// a step that is random has a seed, and what the family named is all that was kept of it.
[[nodiscard]] inline bool envelope_holds(const adl::scenario::Envelope& envelope, const adl::scenario::Family& family)
{
    using namespace adl::scenario;
    const auto in = [](std::int64_t value, std::int64_t low, std::int64_t high)
    { return value >= low && value <= high; };
    const auto text_holds = [](const std::string& text, std::size_t most, bool may_be_empty)
    {
        return (may_be_empty || !text.empty()) && character_count(text) <= most &&
               std::ranges::none_of(text, [](char c) { return static_cast<unsigned char>(c) < 0x20 || c == 0x7F; });
    };
    bool holds = envelope.steps.size() <= max_steps && text_holds(envelope.name, max_name_characters, true) &&
                 text_holds(envelope.description, max_description_characters, true) &&
                 (!envelope.seed || *envelope.seed <= static_cast<std::uint64_t>(max_seed));
    std::size_t number = 0;
    for (const Step& step : envelope.steps)
    {
        ++number;
        const bool is_time = step.when.trigger == Trigger::after_ms;
        holds = holds && step.number == number &&
                in(step.when.at, is_time ? 0 : 1, is_time ? max_after_ms : max_occurrence) &&
                (!step.when.every ||
                 in(*step.when.every, is_time ? min_every_ms : 1, is_time ? max_after_ms : max_occurrence)) &&
                (!step.when.times || in(*step.when.times, 1, max_occurrence)) &&
                (!step.per_mille || (in(*step.per_mille, 1, max_per_mille) && envelope.seed.has_value())) &&
                text_holds(step.comment, max_comment_characters, true) && !step.action.empty() &&
                step.action.size() <= max_action_characters && only_named(step.when.rest, family.when_keys) &&
                only_named(step.rest, family.step_keys);
        const json::Value* action = json::find(step.action_members, "action");
        holds = holds && action != nullptr && action->as_string() != nullptr && *action->as_string() == step.action;
    }
    return holds;
}

// Reads the input as a scenario file of each kind, with and without keys of its own. A file that is refused says why
// in one line; one that is accepted holds what envelope_holds checks.
[[nodiscard]] inline bool fuzz_scenario_envelope(const std::uint8_t* data, std::size_t size)
{
    using namespace adl::scenario;
    static constexpr std::array<std::string_view, 1> mock_when{"operation"};
    static constexpr std::array<std::string_view, 2> loader_step{"on", "count"};
    const std::array<Family, 3> families{Family{mock_kind, mock_when, {}}, Family{loader_kind, {}, loader_step},
                                         Family{mock_kind, {}, {}}};
    const std::string_view text{size == 0 ? "" : reinterpret_cast<const char*>(data), size};
    bool holds = true;
    for (const Family& family : families)
    {
        const auto envelope = read_envelope(text, family);
        if (!envelope)
        {
            holds =
                holds && !envelope.error().message.empty() && envelope.error().message.find('\n') == std::string::npos;
        }
        else
        {
            holds = holds && envelope_holds(*envelope, family);
        }
    }
    return holds;
}

} // namespace adl::test

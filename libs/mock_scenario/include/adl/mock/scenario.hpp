// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// A scenario for the mock target: which fault happens when, as data. It is what a scenario file says (docs/user-guide/
// mock-scenarios.md) once it has been read and checked, and what write_scenario writes. The player
// (scenario_player.hpp) makes a FaultPlan of it for the engine.

#include "adl/mock/fault_plan.hpp"
#include "adl/scenario/envelope.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace adl::mock
{

// What a step does, as the file says it. Which members mean something depends on the effect: `seconds` for a wait,
// `code` for a refusal, the milliseconds for a delay.
struct ScenarioAction
{
    Effect effect = Effect::wait;
    std::uint32_t seconds = 0;   // wait: 1 to 65535
    std::uint32_t code = 0x1000; // refuse: 0x1000 (denied) or 0x1002 (not supported)
    std::uint32_t min_ms = 0;    // delay: the milliseconds, or the least of a range
    std::uint32_t max_ms = 0;    // delay: the most of a range (the same as min_ms for a fixed delay)
    bool ranged = false;         // delay: written as min_ms and max_ms, so that it draws from the seed

    friend bool operator==(const ScenarioAction&, const ScenarioAction&) = default;
};

struct ScenarioStep
{
    std::size_t number = 0; // from 1: the place in the file
    adl::scenario::Trigger trigger = adl::scenario::Trigger::request;
    std::int64_t at = 1; // the request or status file number, or the milliseconds since the operation began
    std::optional<std::int64_t> every;
    std::optional<std::int64_t> times;
    std::optional<OperationKind> operation; // only for that operation, and counting only those
    std::optional<std::int64_t> per_mille;  // each occurrence is applied with this chance in a thousand
    ScenarioAction action;
    std::string comment;

    friend bool operator==(const ScenarioStep&, const ScenarioStep&) = default;
};

struct Scenario
{
    std::string name;
    std::string description;
    std::optional<std::uint64_t> seed;
    std::vector<ScenarioStep> steps;

    friend bool operator==(const Scenario&, const Scenario&) = default;
};

// The words of the file and of the events: "wait", "drop_status_file" ... and "information", "upload",
// "download_media_defined", "download_operator_defined".
[[nodiscard]] std::string_view effect_word(Effect effect) noexcept;
[[nodiscard]] std::string_view operation_word(OperationKind kind) noexcept;

} // namespace adl::mock

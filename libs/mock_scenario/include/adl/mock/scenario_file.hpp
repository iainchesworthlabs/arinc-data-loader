// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Reading and writing the mock target's scenario file (kind "adl-mock-scenario", format 1; docs/user-guide/
// mock-scenarios.md). The reader is the shared one of libs/scenario_base for the top level and the shape of a step,
// and adds what is the mock target's own: the actions, which trigger each goes with, and what each takes. Every way a
// file can be wrong is one sentence for a person, English, which names the step.

#include "adl/mock/scenario.hpp"

#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace adl::mock
{

using ScenarioError = adl::scenario::ReadError;

// Every action word this version knows, in the order of the guide's table: what `started` lists as
// `scenario_features`, and what `adl_mock --scenario-actions` prints.
[[nodiscard]] std::span<const std::string_view> action_words() noexcept;

// The scenario the text says, or why it cannot be used.
[[nodiscard]] std::expected<Scenario, ScenarioError> read_scenario(std::string_view text);

// The text of a scenario, in the one form the writer has: two spaces of indentation, keys in the order of the guide, a
// line feed at the end. What read_scenario reads from it is the same scenario.
[[nodiscard]] std::string write_scenario(const Scenario& scenario);

// Reads the file at `path` and the scenario in it. A file that is not there, cannot be read or is larger than the
// limit is refused with a sentence like the others.
[[nodiscard]] std::expected<Scenario, ScenarioError> load_scenario_file(const std::filesystem::path& path);

} // namespace adl::mock

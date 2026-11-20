// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The scenario file: reading it into a Scenario, and writing a Scenario out as the same file. The reader is
// strict: it takes the licence library's strict JSON reader (no comment, no trailing comma, no repeated key, UTF-8
// only, eight levels deep), refuses any key it does not know (a typo would otherwise be a fault that never
// happens), and refuses a whole number written with a fraction or an exponent. Every refusal is one English
// sentence that names the step and the key, so that a supplier can find it: the strict reader itself says only that
// a file is malformed, with no position. docs/file-reference.md has the file.

#include "adl/loader_scenario/scenario.hpp"

#include <expected>
#include <string>
#include <string_view>

namespace adl::loader_scenario
{

// Why a file was refused: one sentence, with no full stop at the end and no name of the file (the program that
// read the file puts that before it).
struct ReadError
{
    std::string message;

    friend bool operator==(const ReadError&, const ReadError&) = default;
};

// Reads the text of a scenario file. A file of more than max_file_bytes is refused before it is looked at.
[[nodiscard]] std::expected<Scenario, ReadError> read_scenario(std::string_view text);

// The scenario as a file: keys in the order the guide shows, two spaces to the level, one step to a line, a line
// feed at the end. Reading it gives back an equal scenario (a scenario the reader could have produced).
[[nodiscard]] std::string write_scenario(const Scenario& scenario);

} // namespace adl::loader_scenario

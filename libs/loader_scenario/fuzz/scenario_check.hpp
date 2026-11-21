// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// What the scenario harness (loader_scenario.cpp) does to the bytes it is given, as a function, so that the unit tests
// can run the same thing on the seeds under the ordinary compilers on every preset.

#include "adl/loader_scenario/reader.hpp"
#include "adl/scenario/envelope.hpp"

#include <string>
#include <string_view>

namespace adl::loader_scenario::fuzz
{

enum class Verdict
{
    refused,         // the reader refused the text, with a sentence
    read_and_agreed, // it was read, written out, read again, and written out again, and all came out the same
    inconsistent,    // what was read does not come back: a bug
};

[[nodiscard]] inline Verdict check_scenario(std::string_view text)
{
    const auto scenario = read_scenario(text);
    if (!scenario)
    {
        return scenario.error().message.empty() ? Verdict::inconsistent : Verdict::refused;
    }
    const std::string written = write_scenario(*scenario);
    if (written.size() > adl::scenario::max_file_size)
    {
        // The writer's form is longer than the file it was read from; a file that large is not read again.
        return Verdict::read_and_agreed;
    }
    const auto again = read_scenario(written);
    if (!again || *again != *scenario || write_scenario(*again) != written)
    {
        return Verdict::inconsistent;
    }
    return Verdict::read_and_agreed;
}

} // namespace adl::loader_scenario::fuzz

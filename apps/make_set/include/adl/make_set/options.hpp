// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The command line of adl_make_set: three commands (make, verify and inspect), their options, and what is
// refused. Arguments are UTF-8 on every system: the program takes them from adl::platform::arguments.

#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace adl::make_set
{

enum class Command
{
    make,
    verify,
    inspect,
};

[[nodiscard]] std::string_view to_string(Command command) noexcept;

struct Options
{
    bool help = false;    // --help: print the usage and stop
    bool version = false; // --version: print the version line and stop
    Command command = Command::make;
    std::string subject;                    // make: the description file. verify and inspect: the media set folder
    std::optional<std::string> output;      // make: the media set folder to make
    std::optional<std::string> source_root; // make: where relative sources start
    bool json = false;
};

// One sentence, in English, that says what is wrong with the command line.
struct UsageError
{
    std::string sentence;
};

[[nodiscard]] std::expected<Options, UsageError> parse_options(std::span<const std::string_view> arguments);

// The text `--help` prints.
[[nodiscard]] std::string_view usage_text() noexcept;

} // namespace adl::make_set

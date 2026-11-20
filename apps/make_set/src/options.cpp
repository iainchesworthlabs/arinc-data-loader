// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/make_set/options.hpp"

#include <algorithm>
#include <array>

namespace adl::make_set
{

namespace
{

constexpr std::string_view usage =
    "usage: adl_make_set make <description> --output <folder> [--source-root <folder>] [--json]\n"
    "       adl_make_set verify <folder> [--json]\n"
    "       adl_make_set inspect <folder> [--json]\n"
    "       adl_make_set --help\n"
    "       adl_make_set --version\n"
    "\n"
    "make reads a description file and writes the media set it describes to --output, reads it back and\n"
    "verifies it, and only then puts it in place. verify checks a media set folder, and inspect says\n"
    "what one holds. With --json the result is JSON on standard output.\n";

UsageError refused(std::string sentence)
{
    return UsageError{std::move(sentence)};
}

bool is_option(std::string_view argument) noexcept
{
    return argument.starts_with("--");
}

} // namespace

std::string_view to_string(Command command) noexcept
{
    switch (command)
    {
    case Command::make:
        return "make";
    case Command::verify:
        return "verify";
    case Command::inspect:
        return "inspect";
    }
    return "make";
}

std::string_view usage_text() noexcept
{
    return usage;
}

std::expected<Options, UsageError> parse_options(std::span<const std::string_view> arguments)
{
    Options options;
    bool have_command = false;
    bool have_subject = false; // a subject that is not empty
    bool given_subject = false;
    bool too_many = false;
    bool have_json = false;

    for (std::size_t i = 0; i < arguments.size(); ++i)
    {
        const std::string_view argument = arguments[i];
        if (argument == "--help")
        {
            options.help = true;
            return options;
        }
        if (argument == "--version")
        {
            options.version = true;
            return options;
        }
        if (argument == "--json")
        {
            if (have_json)
            {
                return std::unexpected{refused("--json is given twice")};
            }
            have_json = true;
            options.json = true;
            continue;
        }
        if (argument == "--output" || argument == "--source-root")
        {
            std::optional<std::string>& target = argument == "--output" ? options.output : options.source_root;
            if (target)
            {
                return std::unexpected{refused(std::string{argument} + " is given twice")};
            }
            if (i + 1 >= arguments.size() || is_option(arguments[i + 1]))
            {
                return std::unexpected{refused(std::string{argument} + " needs a value")};
            }
            target = std::string{arguments[++i]};
            continue;
        }
        if (is_option(argument))
        {
            return std::unexpected{refused("unknown option " + std::string{argument})};
        }
        if (!have_command)
        {
            if (argument == "make")
            {
                options.command = Command::make;
            }
            else if (argument == "verify")
            {
                options.command = Command::verify;
            }
            else if (argument == "inspect")
            {
                options.command = Command::inspect;
            }
            else
            {
                return std::unexpected{refused("unknown command " + std::string{argument})};
            }
            have_command = true;
            continue;
        }
        if (have_subject || given_subject)
        {
            too_many = true;
            continue;
        }
        given_subject = true;
        have_subject = !argument.empty();
        options.subject = std::string{argument};
    }

    if (!have_command)
    {
        return std::unexpected{refused("no command given (make, verify or inspect)")};
    }
    const std::string command{to_string(options.command)};
    if (!have_subject || too_many)
    {
        return std::unexpected{
            refused(command + " takes one " + (options.command == Command::make ? "description file" : "folder"))};
    }
    if (options.command != Command::make)
    {
        if (options.output)
        {
            return std::unexpected{refused("--output is not for " + command)};
        }
        if (options.source_root)
        {
            return std::unexpected{refused("--source-root is not for " + command)};
        }
    }
    else if (!options.output)
    {
        return std::unexpected{refused("make needs --output")};
    }
    return options;
}

} // namespace adl::make_set

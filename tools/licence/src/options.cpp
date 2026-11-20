// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
#include "options.hpp"

#include "messages.hpp"

#include <algorithm>

namespace adl::licence_owner
{

bool Options::has(std::string_view name) const noexcept
{
    return std::ranges::any_of(given_, [name](const auto& entry) { return entry.first == name; });
}

std::string Options::value(std::string_view name) const
{
    for (const auto& entry : given_)
    {
        if (entry.first == name)
        {
            return entry.second;
        }
    }
    return {};
}

std::vector<std::string> Options::values(std::string_view name) const
{
    std::vector<std::string> found;
    for (const auto& entry : given_)
    {
        if (entry.first == name)
        {
            found.push_back(entry.second);
        }
    }
    return found;
}

std::expected<Options, std::string> parse_options(std::string_view command, std::span<const std::string> arguments,
                                                  std::span<const OptionSpec> specs, std::size_t max_positional)
{
    Options options;
    for (std::size_t i = 0; i < arguments.size(); ++i)
    {
        const std::string& word = arguments[i];
        if (!word.starts_with("--"))
        {
            if (options.positional_.size() >= max_positional)
            {
                return std::unexpected(max_positional == 0 ? format(message::needs_no_file, {command})
                                                           : format(message::needs_one_file, {command}));
            }
            options.positional_.push_back(word);
            continue;
        }
        const auto spec = std::ranges::find(specs, std::string_view(word), &OptionSpec::name);
        if (spec == specs.end())
        {
            return std::unexpected(format(message::unknown_option, {word, command}));
        }
        if (!spec->repeatable && options.has(word))
        {
            return std::unexpected(format(message::option_twice, {word}));
        }
        std::string given;
        if (spec->takes_value)
        {
            if (i + 1 >= arguments.size())
            {
                return std::unexpected(format(message::option_needs_value, {word}));
            }
            given = arguments[++i];
        }
        options.given_.emplace_back(word, std::move(given));
    }
    return options;
}

} // namespace adl::licence_owner

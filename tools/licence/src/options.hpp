// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
#pragma once

// The options of one command. A command lists what it takes; anything else is a usage error.

#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace adl::licence_owner
{

struct OptionSpec
{
    std::string_view name; // "--request"
    bool takes_value = false;
    bool repeatable = false;
};

class Options
{
public:
    // The words that are not options, in order.
    [[nodiscard]] const std::vector<std::string>& positional() const noexcept
    {
        return positional_;
    }
    [[nodiscard]] bool has(std::string_view name) const noexcept;
    // The value of an option that was given, or an empty text.
    [[nodiscard]] std::string value(std::string_view name) const;
    [[nodiscard]] std::vector<std::string> values(std::string_view name) const;

private:
    friend std::expected<Options, std::string> parse_options(std::string_view, std::span<const std::string>,
                                                              std::span<const OptionSpec>, std::size_t);
    std::vector<std::string> positional_;
    std::vector<std::pair<std::string, std::string>> given_;
};

// `arguments` are what follows the command's name. At most `max_positional` words that are not options. The
// error is one sentence from messages.hpp.
[[nodiscard]] std::expected<Options, std::string> parse_options(std::string_view command,
                                                                 std::span<const std::string> arguments,
                                                                 std::span<const OptionSpec> specs,
                                                                 std::size_t max_positional);

} // namespace adl::licence_owner

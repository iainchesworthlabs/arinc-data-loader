// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// What the fuzz harness of adl_load's options (apps/load/fuzz) and the test that replays its seeds under the
// ordinary compilers share: the fuzzer's bytes, cut at each NUL into the arguments of a command line, read
// by the parser, and, if they make a command, written out again and read again, which has to give the same
// options. A command line that is not accepted is not an error: refusing is what the parser is for.

#include "adl/load/options.hpp"
#include "adl/loader_core/address.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace adl::test
{

// The arguments the bytes make: the text between the NULs, empty ones included.
[[nodiscard]] inline std::vector<std::string> fuzz_arguments(const std::uint8_t* data, std::size_t size)
{
    std::vector<std::string> arguments;
    std::string current;
    for (std::size_t i = 0; i < size; ++i)
    {
        if (data[i] == 0)
        {
            arguments.push_back(std::move(current));
            current.clear();
        }
        else
        {
            current.push_back(static_cast<char>(data[i]));
        }
    }
    arguments.push_back(std::move(current));
    return arguments;
}

// The command line that gives `options`, written the way the usage writes it.
[[nodiscard]] inline std::vector<std::string> render_options(const adl::load::Options& options)
{
    using adl::load::Command;
    std::vector<std::string> line{std::string{to_string(options.command)}};
    const auto add = [&line](std::string_view name, std::string value)
    {
        line.emplace_back(name);
        line.push_back(std::move(value));
    };
    if (options.json)
    {
        line.emplace_back("--json");
    }
    if (options.local_address)
    {
        add("--local-address", adl::loader_core::format_ipv4(*options.local_address));
    }
    if (options.command == Command::find)
    {
        add("--seconds", std::to_string(options.seconds));
        return line;
    }
    if (options.command == Command::serve)
    {
        add("--control-port", std::to_string(options.control_port));
        add("--max-operations", std::to_string(options.max_operations));
        return line;
    }
    add("--target", adl::loader_core::format_ipv4(options.target));
    add("--position", options.position);
    add("--control-port", std::to_string(options.control_port));
    add("--target-port", std::to_string(options.target_port));
    if (options.record)
    {
        add("--record", *options.record);
    }
    if (options.scenario)
    {
        add("--scenario", *options.scenario);
    }
    if (options.command == Command::upload)
    {
        add("--media-set", options.media_set);
        if (options.all_loads)
        {
            line.emplace_back("--all-loads");
        }
        for (const std::string& load : options.loads)
        {
            add("--load", load);
        }
    }
    if (options.command == Command::download)
    {
        add("--into", options.into);
        if (options.pick_all)
        {
            line.emplace_back("--pick-all");
        }
        for (const std::string& name : options.names)
        {
            add(options.choice == adl::load::FileChoice::named ? "--file" : "--pick", name);
        }
    }
    return line;
}

struct OptionsFuzzResult
{
    bool made_a_command = false; // the bytes were a command line that was accepted
    bool round_trips = true;     // and what it was written as is read again as the same options
};

[[nodiscard]] inline OptionsFuzzResult fuzz_options(const std::uint8_t* data, std::size_t size)
{
    const std::vector<std::string> arguments = fuzz_arguments(data, size);
    const std::vector<std::string_view> views(arguments.begin(), arguments.end());
    const auto parsed = adl::load::parse_options(views);
    OptionsFuzzResult result;
    if (!parsed || parsed->action != adl::load::Action::run)
    {
        return result;
    }
    result.made_a_command = true;
    const std::vector<std::string> written = render_options(parsed->options);
    const std::vector<std::string_view> written_views(written.begin(), written.end());
    const auto again = adl::load::parse_options(written_views);
    result.round_trips = again && again->action == adl::load::Action::run && again->options == parsed->options;
    return result;
}

} // namespace adl::test

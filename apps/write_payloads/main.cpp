// SPDX-License-Identifier: GPL-3.0-or-later
// Writes sample media sets to a directory, so that other loaders can be tried on
// them and the 615A work has something to load. Each scenario gets a directory of
// its own with one directory per media set member (member_001, ...). Every set is
// read back from disk and verified before the program reports it.

#include "adl/arinc665/media_set.hpp"
#include "adl/media_tree/media_tree.hpp"
#include "adl/platform/arguments.hpp"
#include "adl/platform/paths.hpp"
#include "adl/samples/scenarios.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace
{

constexpr int exit_ok = 0;
constexpr int exit_failed = 1;
constexpr int exit_usage = 2;

void print_usage(std::ostream& out)
{
    out << "usage: adl_write_payloads [--scenario <name>]... <output-directory>\n"
           "       adl_write_payloads --list\n"
           "\n"
           "Writes sample ARINC 665 media sets to <output-directory>/<scenario>/member_NNN.\n"
           "With no --scenario, every scenario is written. A scenario directory that exists\n"
           "and has files in it is never overwritten.\n";
}

void print_scenarios(std::ostream& out)
{
    for (const adl::samples::Scenario& scenario : adl::samples::scenarios())
    {
        out << scenario.name << "\n    " << scenario.description << '\n';
    }
}

struct Arguments
{
    std::vector<std::string> scenario_names;
    std::filesystem::path output;
    bool list = false;
};

// A path as UTF-8 text, in the system's own separators. (path::string() is the text in the system's code
// page on Windows, and throws for a name that has a character outside it.)
std::string utf8(const std::filesystem::path& path)
{
    return adl::platform::to_utf8(path.u8string());
}

// False if the arguments are not usable; the reason has been printed. `command_line` is the program name
// and then the arguments, as UTF-8.
bool parse(const std::vector<std::string>& command_line, Arguments& arguments)
{
    for (std::size_t i = 1; i < command_line.size(); ++i)
    {
        const std::string_view argument = command_line[i];
        if (argument == "--list")
        {
            arguments.list = true;
        }
        else if (argument == "--scenario")
        {
            if (++i >= command_line.size())
            {
                std::cerr << "--scenario needs a name\n";
                return false;
            }
            arguments.scenario_names.push_back(command_line[i]);
        }
        else if (argument.starts_with("--"))
        {
            std::cerr << "unknown option " << argument << '\n';
            return false;
        }
        else if (arguments.output.empty())
        {
            arguments.output = adl::platform::path_from_utf8(argument);
        }
        else
        {
            std::cerr << "more than one output directory given\n";
            return false;
        }
    }
    if (arguments.list)
    {
        return arguments.output.empty() && arguments.scenario_names.empty();
    }
    if (arguments.output.empty())
    {
        std::cerr << "no output directory given\n";
        return false;
    }
    return true;
}

const adl::samples::Scenario* find_scenario(std::string_view name)
{
    const auto all = adl::samples::scenarios();
    const auto it =
        std::find_if(all.begin(), all.end(), [&](const adl::samples::Scenario& s) { return s.name == name; });
    return it == all.end() ? nullptr : &*it;
}

// Builds, writes, reads back and verifies one scenario. False if any step failed.
bool write_scenario(const adl::samples::Scenario& scenario, const std::filesystem::path& output)
{
    const auto spec = scenario.make();
    if (!spec)
    {
        std::cerr << scenario.name << ": cannot build: " << to_string(spec.error()) << '\n';
        return false;
    }
    const auto members = adl::arinc665::build_media_set(*spec);
    if (!members)
    {
        std::cerr << scenario.name << ": cannot build: " << to_string(members.error()) << '\n';
        return false;
    }

    const std::filesystem::path directory = output / std::string{scenario.name};
    if (const auto written = adl::media_tree::write_media_set_tree(directory, *members); !written)
    {
        std::cerr << scenario.name << ": cannot write " << utf8(written.error().path) << ": "
                  << to_string(written.error().error) << '\n';
        return false;
    }

    // What matters is what is on the disk now, not what was meant to be put there.
    const auto on_disk = adl::media_tree::read_media_set_tree(directory);
    if (!on_disk)
    {
        std::cerr << scenario.name << ": cannot read back " << utf8(on_disk.error().path) << ": "
                  << to_string(on_disk.error().error) << '\n';
        return false;
    }
    const auto set = adl::arinc665::read_media_set(*on_disk);
    if (!set)
    {
        std::cerr << scenario.name << ": does not verify: " << to_string(set.error()) << '\n';
        return false;
    }

    std::size_t files = 0;
    for (const adl::arinc665::MediaMember& member : *on_disk)
    {
        files += member.files.size();
    }
    const auto count_text = [](std::size_t count, const char* one, const char* many)
    { return std::to_string(count) + ' ' + (count == 1 ? one : many); };
    std::cout << scenario.name << ": " << count_text(on_disk->size(), "member", "members") << ", "
              << count_text(files, "file", "files") << ", " << count_text(set->loads.size(), "load", "loads") << ", "
              << count_text(set->batches.size(), "batch", "batches") << ", "
              << count_text(set->findings.size(), "finding", "findings") << " -> " << utf8(directory) << '\n';
    for (const adl::arinc665::Finding& finding : set->findings)
    {
        std::cout << "    " << finding.clause << ": " << finding.message << '\n';
    }
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    // Not argv: on Windows that has lost any character outside the code page (docs/architecture.md).
    const std::vector<std::string> command_line = adl::platform::arguments(argc, argv);
    Arguments arguments;
    if (!parse(command_line, arguments))
    {
        print_usage(std::cerr);
        return exit_usage;
    }
    if (arguments.list)
    {
        print_scenarios(std::cout);
        return exit_ok;
    }

    std::vector<const adl::samples::Scenario*> selected;
    if (arguments.scenario_names.empty())
    {
        for (const adl::samples::Scenario& scenario : adl::samples::scenarios())
        {
            selected.push_back(&scenario);
        }
    }
    for (const std::string& name : arguments.scenario_names)
    {
        const adl::samples::Scenario* scenario = find_scenario(name);
        if (scenario == nullptr)
        {
            std::cerr << "no scenario called " << name << " (see --list)\n";
            return exit_usage;
        }
        if (std::find(selected.begin(), selected.end(), scenario) == selected.end())
        {
            selected.push_back(scenario);
        }
    }

    for (const adl::samples::Scenario* scenario : selected)
    {
        if (!write_scenario(*scenario, arguments.output))
        {
            return exit_failed;
        }
    }
    return exit_ok;
}

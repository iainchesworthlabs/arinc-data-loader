// SPDX-License-Identifier: GPL-3.0-or-later
// adl_licence with the throwaway test key of RFC 8032, for the tests that start the program as a process:
//
//     adl_licence_test_program [--now <time>] [--computer <name>] <what adl_licence takes>
//
// The two options set the time the commands judge by and the name of the computer, so that a licence is tried in
// each state. It reads ADL_SETTINGS_DIR as adl_licence does. It trusts a key that no program accepts and it is
// never installed or packed: tools/release/check_package_contents.py fails a package that holds it.

#include "adl/licence/licence.hpp"
#include "adl/licence/test_keys.hpp"
#include "adl/licence_cli/licence_cli.hpp"
#include "adl/licence_cli/system.hpp"
#include "adl/platform/arguments.hpp"
#include "environment.hpp"

#include <cstddef>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

int main(int argc, char** argv)
{
    namespace cli = adl::licence_cli;
    namespace lic = adl::licence;

    const std::vector<std::string> command_line = adl::platform::arguments(argc, argv);
    std::optional<lic::TimePoint> now;
    std::optional<std::string> computer;
    std::size_t at = 1;
    while (at + 1 < command_line.size() && (command_line[at] == "--now" || command_line[at] == "--computer"))
    {
        if (command_line[at] == "--now")
        {
            const auto time = lic::parse_time(command_line[at + 1]);
            if (!time)
            {
                std::cerr << "adl_licence_test_program: " << command_line[at + 1]
                          << " is not a time like 2026-11-13T02:10:00Z\n";
                return cli::exit_usage;
            }
            now = *time;
        }
        else
        {
            computer = command_line[at + 1];
        }
        at += 2;
    }
    const std::span<const std::string> arguments = at <= command_line.size()
                                                       ? std::span<const std::string>{command_line}.subspan(at)
                                                       : std::span<const std::string>{};

    const std::string settings = adl::licence_program::environment_variable("ADL_SETTINGS_DIR").value_or(std::string{});
    const auto test_key = lic::test_keys::trusted();
    std::vector<lic::signed_file::TrustedKey> keys(test_key.begin(), test_key.end());
    auto system = cli::SystemEnvironment::open(settings, std::move(keys), lic::TimePoint::min(), "test");
    if (!system)
    {
        std::cerr << "adl_licence_test_program: " << system.error() << '\n';
        return cli::exit_failed;
    }
    cli::Environment& environment = (*system)->environment();
    if (now)
    {
        environment.now = [time = *now]() -> std::optional<lic::TimePoint> { return time; };
    }
    if (computer)
    {
        environment.computer_name = *computer;
    }
    return cli::run(arguments, environment, {std::cout, std::cerr});
}

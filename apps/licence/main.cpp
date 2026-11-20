// SPDX-License-Identifier: GPL-3.0-or-later
// adl_licence: the licence on this computer, from a command line (docs/user-guide/command-line.md). The commands
// are in libs/licence_cli; this reads the arguments and the one environment variable, and gives the commands the
// keys of this release and the time of its build.

#include "adl/licence/signed_file.hpp"
#include "adl/licence_cli/licence_cli.hpp"
#include "adl/licence_cli/system.hpp"
#include "adl/licence_platform/environment.hpp"
#include "adl/platform/arguments.hpp"
#include "adl/ui/build_info.hpp"

#include <iostream>
#include <span>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    namespace cli = adl::licence_cli;

    // Not argv: on Windows that has lost any character outside the code page (docs/architecture.md). The
    // first word is the program's name.
    const std::vector<std::string> command_line = adl::platform::arguments(argc, argv);
    const std::span<const std::string> arguments =
        command_line.empty() ? std::span<const std::string>{} : std::span<const std::string>{command_line}.subspan(1);

    // ADL_SETTINGS_DIR moves the licence folders and the clock file, as it moves the programs' settings.
    const std::string settings =
        adl::licence_platform::environment_variable("ADL_SETTINGS_DIR").value_or(std::string{});
    // The commit time of the source of this build is a time that has certainly passed.
    const auto built = cli::parse_commit_time(adl::ui::kGitCommitTime);
    auto system = cli::SystemEnvironment::open(settings, adl::licence::signed_file::product_trust_list(),
                                               built.value_or(adl::licence::TimePoint::min()), adl::ui::kGitDescribe);
    if (!system)
    {
        std::cerr << "adl_licence: " << system.error() << '\n';
        return cli::exit_failed;
    }
    return cli::run(arguments, (*system)->environment(), {std::cout, std::cerr});
}

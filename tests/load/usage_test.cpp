// SPDX-License-Identifier: GPL-3.0-or-later
// What the program does with a command line before it does anything else: wrong usage goes to standard error
// with exit code 2 and no event, --help and --version go to standard output with exit code 0. The cases of
// the parser itself are in options_test.cpp; these are the whole function, on a host that is not touched.

#include "adl/load/options.hpp"
#include "command_rig.hpp"

#include <catch2/catch_test_macros.hpp>

using adl::test::CommandRig;
using adl::test::CommandRun;

TEST_CASE("wrong usage is told on standard error, with exit code 2 and nothing on standard output",
          "[load][integration][usage]")
{
    CommandRig rig;
    SECTION("no command")
    {
        const CommandRun run = rig.run({});
        CHECK(run.exit_code == 2);
        CHECK(run.out.empty());
        CHECK(run.err.starts_with("adl_load: No command given: find, information, upload or download.\n\nusage:\n"));
        CHECK(run.err.ends_with(std::string{adl::load::usage_text()}));
    }
    SECTION("an option the command lacks, with --json asked for")
    {
        const CommandRun run = rig.run({"information", "--json", "--target", "10.0.0.2"});
        CHECK(run.exit_code == 2);
        // No event, not even `started`: what the options give is not a command yet.
        CHECK(run.out.empty());
        CHECK(run.err.starts_with("adl_load: --position is missing.\n"));
    }
    SECTION("an unknown option")
    {
        const CommandRun run = rig.run({"find", "--verbose"});
        CHECK(run.exit_code == 2);
        CHECK(run.out.empty());
        CHECK(run.err.starts_with("adl_load: Unknown option \"--verbose\".\n"));
    }
    SECTION("nothing was sent to the network")
    {
        static_cast<void>(rig.run({"upload", "--target", "10.0.0.2"}));
        CHECK(rig.rig.target.log().init_requests_at.empty());
    }
}

TEST_CASE("the usage is printed for --help and one line for --version, exit code 0", "[load][integration][usage]")
{
    CommandRig rig;
    const CommandRun help = rig.run({"--help"});
    CHECK(help.exit_code == 0);
    CHECK(help.out == std::string{adl::load::usage_text()});
    CHECK(help.err.empty());

    const CommandRun command_help = rig.run({"download", "--help"});
    CHECK(command_help.exit_code == 0);
    CHECK(command_help.out == std::string{adl::load::usage_text()});

    const CommandRun version = rig.run({"--version"});
    CHECK(version.exit_code == 0);
    CHECK(version.err.empty());
    CHECK(version.out == adl::load::version_line() + "\n");
    CHECK(version.out.starts_with("adl_load "));
    CHECK(version.out.ends_with(" events 1\n"));
}

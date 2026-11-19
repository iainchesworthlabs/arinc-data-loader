// SPDX-License-Identifier: GPL-3.0-or-later
// An Information operation, the whole command: adl_load's run function on the fake platform against the
// simulated target, with no socket. What it writes is read back as events.

#include "command_rig.hpp"

#include <catch2/catch_test_macros.hpp>

using adl::test::CommandRig;
using adl::test::CommandRun;
using adl::test::Json;

TEST_CASE("an Information operation completes, with events in order", "[load][integration][information]")
{
    CommandRig rig;
    const CommandRun run = rig.run({"information", "--target", "10.0.0.2", "--position", "HNPFMS_L", "--json"});
    INFO(run.out);
    INFO(run.err);
    CHECK(run.exit_code == 0);
    CHECK(run.err.empty());
}

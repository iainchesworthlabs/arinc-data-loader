// SPDX-License-Identifier: GPL-3.0-or-later
// An Information operation, the whole command: adl_load's run function on the fake platform against the
// simulated target, with no socket. What it writes is read back as events, and what it keeps as the record.

#include "command_rig.hpp"
#include "faulty_host.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

using adl::test::CommandRig;
using adl::test::CommandRun;
using adl::test::Json;
using adl::test::SimPlan;

namespace
{

using Names = std::vector<std::string>;

// Where the fake world's wall clock starts: 2026-11-19T10:15:30Z, the time of the contract's own example.
constexpr std::int64_t wall_origin = 1'795'083'330;

CommandRun information(CommandRig& rig, std::initializer_list<std::string_view> more = {})
{
    std::vector<std::string_view> arguments{"information", "--target", "10.0.0.2", "--position", "HNPFMS_L", "--json"};
    arguments.insert(arguments.end(), more.begin(), more.end());
    rig.rig.world.set_wall_origin(wall_origin);
    return rig.run(arguments);
}

} // namespace

TEST_CASE("an Information operation completes, and the events come in order", "[load][integration][information]")
{
    CommandRig rig;
    const CommandRun run = information(rig);
    CHECK(run.exit_code == 0);
    CHECK(run.err.empty());
    adl::test::check_stream(run);

    const std::vector<Json> events = run.events();
    const Json& started = events.front();
    CHECK(started.at("program").text == "adl_load");
    CHECK(started.at("command").text == "information");
    CHECK(started.at("target").text == "10.0.0.2");
    CHECK(started.at("position").text == "HNPFMS_L");
    CHECK(started.at("t").text == "0.000");

    // The target's configuration, once, as the hardware and parts it lists.
    const std::vector<Json> configuration = run.events_named("configuration");
    REQUIRE(configuration.size() == 1);
    const Json& hardware = configuration[0].at("hardware");
    REQUIRE(hardware.elements.size() == 1);
    CHECK(hardware.elements[0].at("literal_name").text == "FMS LEFT");
    CHECK(hardware.elements[0].at("serial_number").text == "SN-0001");
    REQUIRE(hardware.elements[0].at("parts").elements.size() == 2);
    CHECK(hardware.elements[0].at("parts").elements[1].at("part_number").text == "ACM47-8765-4321");
    CHECK(hardware.elements[0].at("parts").elements[1].at("amendment").text == "A");

    // Nothing but log events, the configuration and the two ends: an Information operation has no progress.
    for (const Json& event : events)
    {
        const std::string& name = event.at("event").text;
        CHECK((name == "started" || name == "log" || name == "configuration" || name == "finished"));
    }
    // The configuration comes after the answer to the request and before the end.
    const Names names = run.names();
    const auto at = [&names](const std::string& name) { return std::ranges::find(names, name) - names.begin(); };
    CHECK(at("configuration") > 1);
    CHECK(at("configuration") < at("finished"));

    const Json finished = run.finished();
    CHECK(finished.at("result").text == "completed");
    CHECK(finished.at("reason").text == "completed");
    CHECK(finished.at("status_code").as_number() == 3);
    CHECK(finished.at("detail").text == "The operation completed.");
    CHECK(finished.at("exit_code").as_number() == 0);
    CHECK(finished.keys() ==
          std::vector<std::string>{"event", "t", "result", "reason", "status_code", "detail", "exit_code"});

    // The log events are what the window's log says.
    std::vector<std::string> texts;
    for (const Json& log : run.events_named("log"))
    {
        texts.push_back(log.at("text").text);
    }
    CHECK(texts.front() == "Asking 10.0.0.2 (HNPFMS_L) for its configuration...");
    CHECK(texts[1] == "The target answered: accepted");
    CHECK(texts.back() == "Finished: completed.");
}

TEST_CASE("without --json an Information operation writes a line for each event a person can read",
          "[load][integration][information]")
{
    CommandRig rig;
    const CommandRun run = rig.run({"information", "--target", "10.0.0.2", "--position", "HNPFMS_L"});
    CHECK(run.exit_code == 0);
    CHECK(run.err.empty());
    CHECK(run.out.find('{') == std::string::npos);
    CHECK(run.out.starts_with("adl_load "));
    CHECK(run.out.find(": information, target 10.0.0.2 (HNPFMS_L)\n") != std::string::npos);
    CHECK(run.out.find("info: Asking 10.0.0.2 (HNPFMS_L) for its configuration...\n") != std::string::npos);
    CHECK(run.out.find("Configuration: 1 hardware item; FMS LEFT (serial number SN-0001): ACM47-1234-5678, "
                       "ACM47-8765-4321 A\n") != std::string::npos);
    CHECK(run.out.ends_with("Finished: completed (exit code 0): The operation completed.\n"));
}

TEST_CASE("a target that refuses an Information operation is a failure that carries its status code",
          "[load][integration][information]")
{
    SimPlan plan;
    plan.init_status = adl::arinc615a::StatusCode::operation_denied;
    plan.init_text = "not while in flight";
    CommandRig rig(plan);
    const CommandRun run = information(rig);
    CHECK(run.exit_code == 1);
    adl::test::check_stream(run);
    const Json finished = run.finished();
    CHECK(finished.at("result").text == "failed");
    CHECK(finished.at("reason").text == "target_refused");
    // The code is the refusal's: the engine's own last status has none, since no status file ever came.
    CHECK(finished.at("status_code").as_number() == 0x1000);
    CHECK(finished.at("detail").text.find("denied by the target (not while in flight)") != std::string::npos);
    CHECK(run.events_named("configuration").empty());
}

TEST_CASE("a target that does not do Information says so", "[load][integration][information]")
{
    SimPlan plan;
    plan.init_status = adl::arinc615a::StatusCode::operation_not_supported;
    CommandRig rig(plan);
    const CommandRun run = information(rig);
    CHECK(run.exit_code == 1);
    const Json finished = run.finished();
    CHECK(finished.at("reason").text == "target_refused");
    CHECK(finished.at("status_code").as_number() == 0x1002);
    CHECK(finished.at("detail").text == "The target refused the operation: not supported by the target.");
}

TEST_CASE("a target that ends the operation is a failure", "[load][integration][information]")
{
    SimPlan plan;
    plan.abort_after_statuses = 1;
    CommandRig rig(plan);
    const CommandRun run = information(rig);
    CHECK(run.exit_code == 1);
    adl::test::check_stream(run);
    const Json finished = run.finished();
    CHECK(finished.at("reason").text == "target_aborted");
    CHECK(finished.at("status_code").as_number() == 0x1003);
}

TEST_CASE("a target that does not answer is no_answer, exit code 4", "[load][integration][information]")
{
    SimPlan plan;
    plan.init_silent = true;
    CommandRig rig(plan);
    const CommandRun run = information(rig);
    CHECK(run.exit_code == 4);
    adl::test::check_stream(run);
    const Json finished = run.finished();
    CHECK(finished.at("result").text == "failed");
    CHECK(finished.at("reason").text == "no_answer");
    CHECK(finished.at("status_code").is(Json::Kind::null));
    CHECK(finished.at("exit_code").as_number() == 4);
}

TEST_CASE("what the target sends that cannot be understood is a protocol error", "[load][integration][information]")
{
    SECTION("an initialization file that is not one")
    {
        SimPlan plan;
        plan.init_garbage = true;
        CommandRig rig(plan);
        const CommandRun run = information(rig);
        CHECK(run.exit_code == 1);
        const Json finished = run.finished();
        CHECK(finished.at("reason").text == "protocol_error");
        CHECK(finished.at("detail").text == "What the target sent could not be understood: protocol error.");
    }
    SECTION("a protocol version the loader does not know")
    {
        SimPlan plan;
        plan.init_version[1] = '5';
        CommandRig rig(plan);
        const CommandRun run = information(rig);
        CHECK(run.exit_code == 1);
        const Json finished = run.finished();
        CHECK(finished.at("reason").text == "protocol_error");
        CHECK(finished.at("detail").text == "The target's protocol version is not supported.");
    }
}

TEST_CASE("a port that cannot be opened is a local error, exit code 5", "[load][integration][information]")
{
    SECTION("the control port is taken")
    {
        CommandRig rig;
        // Another program on this computer holds the port the loader listens on.
        adl::platform::UdpOptions held;
        held.local.port = 59;
        auto socket = rig.rig.loader_host.network().open_udp(held);
        REQUIRE(socket.has_value());
        const CommandRun run = information(rig);
        CHECK(run.exit_code == 5);
        adl::test::check_stream(run);
        const Json finished = run.finished();
        CHECK(finished.at("reason").text == "local_error");
        CHECK(finished.at("detail").text.starts_with("The operation could not run: the loader's port could not be "
                                                     "opened ("));
        CHECK(finished.at("status_code").is(Json::Kind::null));
    }
    SECTION("the network cannot be waited on")
    {
        CommandRig rig;
        adl::test::FaultyHost faulty(rig.rig.loader_host);
        faulty.faulty_network().wait_fails = adl::platform::ErrorCode::other;
        const CommandRun run =
            rig.run_on(faulty, {"information", "--target", "10.0.0.2", "--position", "HNPFMS_L", "--json"});
        CHECK(run.exit_code == 5);
        const Json finished = run.finished();
        CHECK(finished.at("reason").text == "local_error");
        CHECK(finished.at("detail").text ==
              "The operation could not run: waiting for the network failed (other error).");
    }
}

TEST_CASE("a control port other than 59 is the one the target is told to write to", "[load][integration][information]")
{
    CommandRig rig;
    const CommandRun run = information(rig, {"--control-port", "30901"});
    CHECK(run.exit_code == 0);
    CHECK(rig.rig.target.log().loader_port_used == 30901);
}

TEST_CASE("the interface the sockets use can be chosen", "[load][integration][information]")
{
    CommandRig rig;
    const CommandRun run = information(rig, {"--local-address", "10.0.0.1"});
    CHECK(run.exit_code == 0);
    CHECK(run.finished().at("reason").text == "completed");
}

TEST_CASE("a target that is not on port 59 is reached with --target-port", "[load][integration][information]")
{
    SimPlan plan;
    plan.control_port = 30959;

    SECTION("the port is named")
    {
        CommandRig rig(plan);
        const CommandRun run = information(rig, {"--target-port", "30959"});
        CHECK(run.exit_code == 0);
        adl::test::check_stream(run);
        CHECK(run.finished().at("reason").text == "completed");
        CHECK(run.events_named("configuration").size() == 1);
    }
    SECTION("it is not, and the target on 30959 does not answer a request sent to 59")
    {
        CommandRig rig(plan);
        const CommandRun run = information(rig);
        CHECK(run.exit_code == 4);
        CHECK(run.finished().at("reason").text == "no_answer");
    }
    SECTION("another port is named, and nothing is there")
    {
        CommandRig rig(plan);
        const CommandRun run = information(rig, {"--target-port", "30960"});
        CHECK(run.exit_code == 4);
        CHECK(run.finished().at("reason").text == "no_answer");
    }
    SECTION("the loader's own port is a different thing")
    {
        CommandRig rig(plan);
        const CommandRun run = information(rig, {"--control-port", "30959"});
        CHECK(run.exit_code == 4);
    }
}

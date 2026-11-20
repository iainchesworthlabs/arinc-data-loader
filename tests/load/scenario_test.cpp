// SPDX-License-Identifier: GPL-3.0-or-later
// `adl_load --scenario`, the whole command, on the fake platform against the simulated target: the scenario file is
// read from the fake file system, the loader does what it says to the datagrams it sends, and the events say so. The
// same faults against the mock target's engine over real sockets are in tests/loopback/load_scenario_test.cpp; what
// each action does to a datagram, byte for byte, is in tests/loader_scenario.

#include "adl/load/system_scenario.hpp"
#include "bytes.hpp"
#include "command_rig.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

using adl::test::CommandRun;
using adl::test::DownloadItem;
using adl::test::InstalledMediaSet;
using adl::test::Json;
using adl::test::SimPlan;

namespace
{

// The command rig with the scenario source of this system, which is what main gives the program.
class ScenarioRig : public adl::test::CommandRig
{
public:
    explicit ScenarioRig(SimPlan plan = {}) : CommandRig(std::move(plan))
    {
        scenario = adl::load::system_scenario();
    }
};

using adl::test::CommandRig;

constexpr std::int64_t wall_origin = 1'795'083'330;
constexpr std::string_view scenario_path = "/scenarios/faults.json";

// A scenario file with these steps (the text between the brackets of "steps") and these members before them.
std::string scenario_text(std::string_view steps, std::string_view before = "")
{
    return std::string{R"({"kind":"adl-loader-scenario","format":1,)"} + std::string{before} + R"("steps":[)" +
           std::string{steps} + "]}";
}

void put_scenario(CommandRig& rig, std::string_view text, std::string_view path = scenario_path)
{
    rig.rig.loader_host.fake_files().put(path, text);
}

CommandRun information(CommandRig& rig, std::initializer_list<std::string_view> more = {})
{
    std::vector<std::string_view> arguments{"information", "--target", "10.0.0.2", "--position", "HNPFMS_L", "--json"};
    arguments.insert(arguments.end(), more.begin(), more.end());
    rig.rig.world.set_wall_origin(wall_origin);
    return rig.run(arguments);
}

// `information` with the scenario file at scenario_path.
CommandRun with_scenario(CommandRig& rig, std::string_view text)
{
    put_scenario(rig, text);
    return information(rig, {"--scenario", scenario_path});
}

std::vector<std::string> lines_of(const std::string& out)
{
    std::vector<std::string> lines;
    std::size_t from = 0;
    while (from < out.size())
    {
        const std::size_t end = out.find('\n', from);
        lines.push_back(out.substr(from, end - from));
        from = end == std::string::npos ? out.size() : end + 1;
    }
    return lines;
}

// The `t` of an event, in milliseconds.
std::uint64_t milliseconds_of(const Json& event)
{
    const std::string& t = event.at("t").text;
    return std::stoull(t.substr(0, t.size() - 4)) * 1000 + std::stoull(t.substr(t.size() - 3));
}

std::vector<std::uint64_t> numbers_of(const Json& array)
{
    std::vector<std::uint64_t> numbers;
    for (const Json& element : array.elements)
    {
        numbers.push_back(element.as_number());
    }
    return numbers;
}

using Numbers = std::vector<std::uint64_t>;

// One step that delays the loader's first request by half a second.
constexpr std::string_view delay_the_request =
    R"({"when":{"after_ms":0},"on":{"datagram":"request"},"do":{"action":"delay","ms":500}})";
constexpr std::string_view drop_the_request =
    R"({"when":{"after_ms":0},"on":{"datagram":"request"},"do":{"action":"drop"}})";

} // namespace

TEST_CASE("a scenario with no step changes nothing but the keys it adds", "[load][integration][scenario]")
{
    CommandRig plain_rig;
    const CommandRun plain = information(plain_rig);
    ScenarioRig scripted_rig;
    const CommandRun scripted = with_scenario(scripted_rig, scenario_text(""));
    REQUIRE(plain.exit_code == 0);
    CHECK(scripted.exit_code == 0);
    CHECK(scripted.err.empty());
    adl::test::check_stream(scripted);

    const std::vector<std::string> before = lines_of(plain.out);
    const std::vector<std::string> after = lines_of(scripted.out);
    REQUIRE(before.size() == after.size());
    for (std::size_t i = 0; i < before.size(); ++i)
    {
        INFO("line " << i << ": " << before[i]);
        const std::string_view name = i == 0 ? "started" : i + 1 == before.size() ? "finished" : "";
        const std::string_view kept = std::string_view{before[i]}.substr(0, before[i].size() - 1); // without the brace
        if (name == "started")
        {
            CHECK(after[i] == std::string{kept} + R"(,"scenario":"/scenarios/faults.json","scenario_steps":0,)"
                                                  R"("scenario_seed":null})");
        }
        else if (name == "finished")
        {
            CHECK(after[i] == std::string{kept} + R"(,"scenario_unused":[]})");
        }
        else
        {
            CHECK(after[i] == before[i]);
        }
    }
    CHECK(scripted.events_named("scenario_step").empty());
}

TEST_CASE("without the option the program says nothing of a scenario, whatever else it is given",
          "[load][integration][scenario]")
{
    ScenarioRig rig;
    const CommandRun run = information(rig);
    CHECK(run.out.find("scenario") == std::string::npos);
    CHECK(run.err.empty());
}

TEST_CASE("a request of the loader that is dropped comes again, and the step is told", "[load][integration][scenario]")
{
    CommandRig plain_rig;
    const CommandRun plain = information(plain_rig);
    ScenarioRig rig;
    const CommandRun run = with_scenario(rig, scenario_text(drop_the_request, R"("seed":3,)"));
    // The loader asks again after the timeout of two seconds, and the operation completes, later.
    CHECK(run.exit_code == 0);
    CHECK(run.err.empty());
    adl::test::check_stream(run);

    const std::vector<Json> events = run.events();
    CHECK(events.front().at("scenario").text == scenario_path);
    CHECK(events.front().at("scenario_steps").as_number() == 1);
    CHECK(events.front().at("scenario_seed").as_number() == 3);

    const std::vector<Json> steps = run.events_named("scenario_step");
    REQUIRE(steps.size() == 1);
    const Json& step = steps.front();
    CHECK(step.keys() == std::vector<std::string>{"event", "t", "step", "trigger", "at", "operation", "action",
                                                  "applied", "datagram", "file", "block", "to"});
    CHECK(step.at("step").as_number() == 1);
    CHECK(step.at("trigger").text == "after_ms");
    CHECK(step.at("at").as_number() == 0);
    CHECK(step.at("operation").text == "information");
    CHECK(step.at("action").text == "drop");
    CHECK(step.at("applied").boolean);
    CHECK(step.at("datagram").text == "request");
    CHECK(step.at("block").is(Json::Kind::null));
    CHECK(step.at("to").text == "10.0.0.2:59");

    // It is the first thing the loader does, so the event is before the loader's first line of log that follows.
    const std::vector<std::string> names = run.names();
    CHECK(std::ranges::find(names, "scenario_step") < std::ranges::find(names, "configuration"));
    CHECK(milliseconds_of(run.finished()) >= milliseconds_of(plain.finished()) + 2000);
    CHECK(numbers_of(run.finished().at("scenario_unused")).empty());
    CHECK(run.finished().at("reason").text == "completed");
}

TEST_CASE("a loader that drops every request is not answered, and the operation ends so",
          "[load][integration][scenario]")
{
    ScenarioRig rig;
    const CommandRun run = with_scenario(
        rig,
        scenario_text(R"({"when":{"after_ms":0},"on":{"datagram":"request"},"do":{"action":"drop"},"count":"all"})"));
    CHECK(run.exit_code == 4);
    adl::test::check_stream(run);
    const Json finished = run.finished();
    CHECK(finished.at("result").text == "failed");
    CHECK(finished.at("reason").text == "no_answer");
    // The first request and the one that was sent again were both dropped, and nothing came of either.
    const std::vector<Json> steps = run.events_named("scenario_step");
    CHECK(steps.size() >= 2);
    for (const Json& step : steps)
    {
        CHECK(step.at("step").as_number() == 1);
        CHECK(step.at("action").text == "drop");
    }
    CHECK(numbers_of(finished.at("scenario_unused")).empty());
    CHECK(run.events_named("configuration").empty());
}

TEST_CASE("a step that never acted is named in finished, and a step for another operation is not in effect",
          "[load][integration][scenario]")
{
    ScenarioRig rig;
    const std::string steps =
        // Step 1 waits for a ninth status file, which an Information operation never has.
        std::string{R"({"when":{"status_file":9},"on":{"datagram":"reply"},"do":{"action":"drop"}},)"} +
        std::string{delay_the_request} + "," +
        // Step 3 is for an upload.
        R"({"when":{"after_ms":0,"operation":"upload"},"on":{"datagram":"request"},"do":{"action":"drop"}})";
    const CommandRun run = with_scenario(rig, scenario_text(steps));
    CHECK(run.exit_code == 0);
    CHECK(run.events().front().at("scenario_steps").as_number() == 3);

    const std::vector<Json> acted = run.events_named("scenario_step");
    REQUIRE(acted.size() == 1);
    CHECK(acted[0].at("step").as_number() == 2);
    CHECK(acted[0].at("action").text == "delay");
    CHECK(acted[0].at("ms").as_number() == 500);
    CHECK(numbers_of(run.finished().at("scenario_unused")) == Numbers{1});
}

TEST_CASE("each operation is named in the events as the record names it", "[load][integration][scenario]")
{
    SECTION("information")
    {
        ScenarioRig rig;
        const CommandRun run = with_scenario(rig, scenario_text(delay_the_request));
        CHECK(run.exit_code == 0);
        REQUIRE(run.events_named("scenario_step").size() == 1);
        CHECK(run.events_named("scenario_step")[0].at("operation").text == "information");
    }
    SECTION("upload")
    {
        const adl::test::SampleLoad load = adl::test::make_sample_load(3'000, 3);
        SimPlan plan;
        plan.loads = {load.item};
        ScenarioRig rig(plan);
        const InstalledMediaSet set(rig.rig.loader_host, load.members);
        put_scenario(rig, scenario_text(delay_the_request));
        rig.rig.world.set_wall_origin(wall_origin);
        const std::string path = set.path();
        const CommandRun run = rig.run({"upload", "--target", "10.0.0.2", "--position", "HNPFMS_L", "--media-set", path,
                                        "--all-loads", "--scenario", scenario_path, "--json"});
        CHECK(run.exit_code == 0);
        REQUIRE(run.events_named("scenario_step").size() == 1);
        CHECK(run.events_named("scenario_step")[0].at("operation").text == "upload");
        // The media set is checked after the file and before the target is asked anything.
        const std::vector<std::string> names = run.names();
        CHECK(names[0] == "started");
        CHECK(names[1] == "media_set_checked");
    }
    SECTION("a download in which the loader names the files")
    {
        SimPlan plan;
        plan.files.push_back(DownloadItem{"A.BIN", adl::test::bytes_of("content of A.BIN"), "the A.BIN"});
        ScenarioRig rig(plan);
        put_scenario(rig, scenario_text(delay_the_request));
        rig.rig.world.set_wall_origin(wall_origin);
        const CommandRun run = rig.run({"download", "--target", "10.0.0.2", "--position", "HNPFMS_L", "--into", "/dl",
                                        "--file", "A.BIN", "--scenario", scenario_path, "--json"});
        CHECK(run.exit_code == 0);
        REQUIRE(run.events_named("scenario_step").size() == 1);
        CHECK(run.events_named("scenario_step")[0].at("operation").text == "download_media_defined");
    }
    SECTION("a download in which the target lists its files")
    {
        SimPlan plan;
        plan.files.push_back(DownloadItem{"A.BIN", adl::test::bytes_of("content of A.BIN"), "the A.BIN"});
        ScenarioRig rig(plan);
        put_scenario(rig, scenario_text(delay_the_request));
        rig.rig.world.set_wall_origin(wall_origin);
        const CommandRun run = rig.run({"download", "--target", "10.0.0.2", "--position", "HNPFMS_L", "--into", "/dl",
                                        "--pick-all", "--scenario", scenario_path, "--json"});
        CHECK(run.exit_code == 0);
        REQUIRE(run.events_named("scenario_step").size() == 1);
        CHECK(run.events_named("scenario_step")[0].at("operation").text == "download_operator_defined");
    }
}

TEST_CASE("the loader's own abort at a chosen moment ends the operation as aborted", "[load][integration][scenario]")
{
    ScenarioRig rig;
    const CommandRun run = with_scenario(rig, scenario_text(R"({"when":{"after_ms":1500},"do":{"action":"abort"}})"));
    CHECK(run.exit_code == 1);
    adl::test::check_stream(run);
    const std::vector<Json> steps = run.events_named("scenario_step");
    REQUIRE(steps.size() == 1);
    CHECK(steps[0].at("action").text == "abort");
    CHECK(steps[0].at("trigger").text == "after_ms");
    CHECK(steps[0].at("at").as_number() == 1500);
    CHECK(steps[0].at("datagram").is(Json::Kind::null));
    CHECK(steps[0].at("to").is(Json::Kind::null));
    CHECK(steps[0].at("status").as_number() == 0x1005);
    CHECK(run.finished().at("reason").text == "aborted");
    CHECK(numbers_of(run.finished().at("scenario_unused")).empty());
}

TEST_CASE("the same scenario against the same target gives the same events every time", "[load][integration][scenario]")
{
    // Chances and ranges drawn from the seed: a delay of 10 to 400 milliseconds for about half the datagrams.
    const std::string text = scenario_text(
        R"({"when":{"after_ms":0},"on":{"datagram":"any"},"do":{"action":"delay","min_ms":10,"max_ms":400},)"
        R"("count":"all","per_mille":500})",
        R"("seed":5,)");
    ScenarioRig first_rig;
    const CommandRun first = with_scenario(first_rig, text);
    ScenarioRig second_rig;
    const CommandRun second = with_scenario(second_rig, text);
    CHECK(first.exit_code == second.exit_code);
    CHECK(first.out == second.out);
    CHECK_FALSE(first.events_named("scenario_step").empty());
    for (const Json& step : first.events_named("scenario_step"))
    {
        CHECK(step.at("ms").as_number() >= 10);
        CHECK(step.at("ms").as_number() <= 400);
    }
}

TEST_CASE("without --json the steps are plain lines too", "[load][integration][scenario]")
{
    ScenarioRig rig;
    put_scenario(rig, scenario_text(drop_the_request));
    const CommandRun run =
        rig.run({"information", "--target", "10.0.0.2", "--position", "HNPFMS_L", "--scenario", scenario_path});
    CHECK(run.exit_code == 0);
    CHECK(run.out.find(": information, target 10.0.0.2 (HNPFMS_L), scenario /scenarios/faults.json (1 step)\n") !=
          std::string::npos);
    CHECK(run.out.find("Scenario step 1: drop, request datagram") != std::string::npos);
    CHECK(run.out.find("to 10.0.0.2:59 (after_ms 0, information)\n") != std::string::npos);
    CHECK(run.out.find("never acted") == std::string::npos);
}

TEST_CASE("a scenario file that cannot be used is told before anything else, with exit code 5",
          "[load][integration][scenario]")
{
    const std::initializer_list<std::string_view> base{"information", "--target", "10.0.0.2",  "--position",
                                                       "HNPFMS_L",    "--json",   "--scenario"};
    const auto args = [&base](std::string_view path)
    {
        std::vector<std::string_view> arguments(base);
        arguments.push_back(path);
        return arguments;
    };

    SECTION("a file that is not there")
    {
        ScenarioRig rig;
        const std::vector<std::string_view> arguments = args("/scenarios/none.json");
        const CommandRun run = rig.run(arguments);
        CHECK(run.exit_code == 5);
        CHECK(run.out.empty());
        CHECK(run.err == "adl_load: --scenario /scenarios/none.json: the file cannot be read (not found)\n");
    }
    SECTION("a file that is not JSON")
    {
        ScenarioRig rig;
        put_scenario(rig, "not json");
        const CommandRun run = rig.run(args(scenario_path));
        CHECK(run.exit_code == 5);
        CHECK(run.out.empty());
        CHECK(run.err.starts_with("adl_load: --scenario /scenarios/faults.json: "));
        CHECK(run.err.ends_with("\n"));
        CHECK(std::ranges::count(run.err, '\n') == 1);
    }
    SECTION("a step that says something the loader cannot do, with the step and the key in the sentence")
    {
        ScenarioRig rig;
        put_scenario(rig, scenario_text(R"({"when":{"after_ms":0},"do":{"action":"drop","ms":5}})"));
        const CommandRun run = rig.run(args(scenario_path));
        CHECK(run.exit_code == 5);
        CHECK(run.out.empty());
        CHECK(run.err ==
              "adl_load: --scenario /scenarios/faults.json: step 1: \"drop\" does not take the key \"ms\"\n");
    }
    SECTION("a file larger than the limit is refused whole")
    {
        ScenarioRig rig;
        put_scenario(rig, std::string(262'145, ' '));
        const CommandRun run = rig.run(args(scenario_path));
        CHECK(run.exit_code == 5);
        CHECK(run.out.empty());
        CHECK(run.err == "adl_load: --scenario /scenarios/faults.json: the file is larger than 262144 bytes\n");
    }
    SECTION("it is read before the media set, so a refused file is exit code 5 and not 3")
    {
        ScenarioRig rig;
        put_scenario(rig, "not json");
        const CommandRun run = rig.run({"upload", "--target", "10.0.0.2", "--position", "HNPFMS_L", "--media-set",
                                        "/not/there", "--all-loads", "--scenario", scenario_path, "--json"});
        CHECK(run.exit_code == 5);
        CHECK(run.out.empty());
        CHECK(run.err.starts_with("adl_load: --scenario /scenarios/faults.json: "));
    }
    SECTION("a file that is read leaves the media set's own problems as they are")
    {
        ScenarioRig rig;
        put_scenario(rig, scenario_text(""));
        const CommandRun run = rig.run({"upload", "--target", "10.0.0.2", "--position", "HNPFMS_L", "--media-set",
                                        "/not/there", "--all-loads", "--scenario", scenario_path, "--json"});
        CHECK(run.exit_code == 3);
        CHECK(run.finished().at("reason").text == "media_set_unreadable");
        // The file was read, so the events say so, and the steps there were are all unused.
        CHECK(run.events().front().at("scenario_steps").as_number() == 0);
        CHECK(numbers_of(run.finished().at("scenario_unused")).empty());
    }
}

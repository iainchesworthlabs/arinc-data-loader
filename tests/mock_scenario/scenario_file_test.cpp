// SPDX-License-Identifier: GPL-3.0-or-later
// The mock target's scenario file: what it says, every action with the triggers it goes with, the keys and limits
// of each, and the one sentence for every way it is wrong. The sentences are part of the interface (the guide lists
// them), so they are written out whole.

#include "adl/mock/scenario_file.hpp"
#include "adl/platform/arguments.hpp"
#include "temp_dir.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace adl::mock;
using adl::scenario::Trigger;

namespace
{

// A file with a seed and one step.
std::string with_step(std::string_view step)
{
    return R"({"kind":"adl-mock-scenario","format":1,"seed":7,"steps":[)" + std::string{step} + "]}";
}

// A step with `when` and `do` given.
std::string step_of(std::string_view when, std::string_view action)
{
    return R"({"when":{)" + std::string{when} + R"(},"do":{)" + std::string{action} + "}}";
}

Scenario accepted(std::string_view text)
{
    INFO(text);
    auto scenario = read_scenario(text);
    if (!scenario)
    {
        FAIL(scenario.error().message);
    }
    return std::move(*scenario);
}

void refused(std::string_view text, std::string_view sentence)
{
    INFO(text);
    const auto scenario = read_scenario(text);
    REQUIRE(!scenario);
    CHECK(scenario.error().message == sentence);
}

// The example of the guide: every trigger, every modifier, a name and a seed. It is also exactly what the writer
// writes, so that the guide's text is checked against the program.
constexpr std::string_view guide_example = R"({
  "kind": "adl-mock-scenario",
  "format": 1,
  "name": "Slow start, lost status files, then an abort",
  "description": "The first request waits 2 s. Uploads answer a little late. Half the status files from the 2nd on are lost, 3 times at most. 20 s into an operation the target gives up.",
  "seed": 20261120,
  "steps": [
    {
      "when": { "request": 1 },
      "do": { "action": "wait", "seconds": 2 }
    },
    {
      "comment": "only the second upload request, held back between 200 and 900 ms",
      "when": { "request": 2, "operation": "upload" },
      "do": { "action": "delay", "min_ms": 200, "max_ms": 900 }
    },
    {
      "when": { "status_file": 2, "every": 1, "times": 3 },
      "per_mille": 500,
      "do": { "action": "drop_status_file" }
    },
    {
      "when": { "after_ms": 20000 },
      "do": { "action": "abort" }
    }
  ]
}
)";

} // namespace

TEST_CASE("mock scenario: the example of the guide is read, and written back as it is", "[mock_scenario][file]")
{
    const Scenario scenario = accepted(guide_example);
    CHECK(scenario.name == "Slow start, lost status files, then an abort");
    REQUIRE(scenario.seed.has_value());
    CHECK(*scenario.seed == 20261120);
    REQUIRE(scenario.steps.size() == 4);

    const ScenarioStep& wait = scenario.steps[0];
    CHECK(wait.number == 1);
    CHECK(wait.trigger == Trigger::request);
    CHECK(wait.at == 1);
    CHECK(wait.action.effect == Effect::wait);
    CHECK(wait.action.seconds == 2);

    const ScenarioStep& delay = scenario.steps[1];
    CHECK(delay.comment == "only the second upload request, held back between 200 and 900 ms");
    CHECK(delay.operation == OperationKind::upload);
    CHECK(delay.action.effect == Effect::delay);
    CHECK(delay.action.ranged);
    CHECK(delay.action.min_ms == 200);
    CHECK(delay.action.max_ms == 900);

    const ScenarioStep& drop = scenario.steps[2];
    CHECK(drop.trigger == Trigger::status_file);
    CHECK(drop.at == 2);
    CHECK(drop.every == 1);
    CHECK(drop.times == 3);
    CHECK(drop.per_mille == 500);
    CHECK(drop.action.effect == Effect::drop_status_file);

    const ScenarioStep& abort = scenario.steps[3];
    CHECK(abort.trigger == Trigger::after_ms);
    CHECK(abort.at == 20000);
    CHECK(abort.action.effect == Effect::abort);

    CHECK(write_scenario(scenario) == guide_example);
}

TEST_CASE("mock scenario: every action, with each trigger it goes with, is read and written back",
          "[mock_scenario][file]")
{
    struct Case
    {
        std::string when;
        std::string action;
        Effect effect;
    };
    const std::vector<Case> good{
        {R"("request":1)", R"("action":"wait","seconds":5)", Effect::wait},
        {R"("request":1)", R"("action":"refuse")", Effect::refuse},
        {R"("request":1)", R"("action":"refuse","code":"0x1002")", Effect::refuse},
        {R"("request":1)", R"("action":"invalid_init_file")", Effect::invalid_init_file},
        {R"("request":1)", R"("action":"corrupt_served")", Effect::corrupt_served},
        {R"("request":1)", R"("action":"delay","ms":250)", Effect::delay},
        {R"("status_file":3)", R"("action":"drop_status_file")", Effect::drop_status_file},
        {R"("status_file":3)", R"("action":"abort")", Effect::abort},
        {R"("status_file":3)", R"("action":"silent")", Effect::silent},
        {R"("status_file":3)", R"("action":"corrupt_served")", Effect::corrupt_served},
        {R"("status_file":3)", R"("action":"wrong_checksum")", Effect::wrong_checksum},
        {R"("status_file":3)", R"("action":"delay","min_ms":0,"max_ms":0)", Effect::delay},
        {R"("after_ms":0)", R"("action":"drop_status_file")", Effect::drop_status_file},
        {R"("after_ms":1500)", R"("action":"abort")", Effect::abort},
        {R"("after_ms":1500)", R"("action":"silent")", Effect::silent},
        {R"("after_ms":1500)", R"("action":"corrupt_served")", Effect::corrupt_served},
        {R"("after_ms":1500)", R"("action":"wrong_checksum")", Effect::wrong_checksum},
        {R"("after_ms":1500)", R"("action":"delay","ms":60000)", Effect::delay},
    };
    for (const Case& item : good)
    {
        INFO(item.when << " " << item.action);
        const Scenario scenario = accepted(with_step(step_of(item.when, item.action)));
        REQUIRE(scenario.steps.size() == 1);
        CHECK(scenario.steps[0].action.effect == item.effect);
        // What was read is what is written, and the written text reads as the same scenario.
        const std::string written = write_scenario(scenario);
        const auto again = read_scenario(written);
        REQUIRE(again);
        CHECK(*again == scenario);
        CHECK(write_scenario(*again) == written);
    }
}

TEST_CASE("mock scenario: an action with a trigger it does not go with is refused, saying where it goes",
          "[mock_scenario][file]")
{
    refused(with_step(step_of(R"("status_file":2)", R"("action":"wait","seconds":1)")),
            "step 1: \"wait\" does not go with a \"status_file\" trigger; it goes with \"request\"");
    refused(with_step(step_of(R"("after_ms":2)", R"("action":"refuse")")),
            "step 1: \"refuse\" does not go with a \"after_ms\" trigger; it goes with \"request\"");
    refused(with_step(step_of(R"("after_ms":2)", R"("action":"invalid_init_file")")),
            "step 1: \"invalid_init_file\" does not go with a \"after_ms\" trigger; it goes with \"request\"");
    refused(with_step(step_of(R"("request":2)", R"("action":"drop_status_file")")),
            "step 1: \"drop_status_file\" does not go with a \"request\" trigger; it goes with \"status_file\" or "
            "\"after_ms\"");
    refused(with_step(step_of(R"("request":2)", R"("action":"abort")")),
            "step 1: \"abort\" does not go with a \"request\" trigger; it goes with \"status_file\" or \"after_ms\"");
    refused(with_step(step_of(R"("request":2)", R"("action":"silent")")),
            "step 1: \"silent\" does not go with a \"request\" trigger; it goes with \"status_file\" or \"after_ms\"");
    refused(with_step(step_of(R"("request":2)", R"("action":"wrong_checksum")")),
            "step 1: \"wrong_checksum\" does not go with a \"request\" trigger; it goes with \"status_file\" or "
            "\"after_ms\"");
    // Nothing is refused for corrupt_served and delay: they go with all three.
    refused(with_step(step_of(R"("request":2)", R"("action":"explode")")),
            "step 1: the action \"explode\" is not one this version knows (it may be for a newer version)");
}

TEST_CASE("mock scenario: what an action takes", "[mock_scenario][file]")
{
    // wait
    refused(with_step(step_of(R"("request":1)", R"("action":"wait")")),
            "step 1: \"seconds\" is missing; it is a whole number from 1 to 65535");
    refused(with_step(step_of(R"("request":1)", R"("action":"wait","seconds":0)")),
            "step 1: \"seconds\" must be a whole number from 1 to 65535");
    refused(with_step(step_of(R"("request":1)", R"("action":"wait","seconds":65536)")),
            "step 1: \"seconds\" must be a whole number from 1 to 65535");
    refused(with_step(step_of(R"("request":1)", R"("action":"wait","seconds":1.5)")),
            "step 1: \"seconds\" must be a whole number from 1 to 65535");
    CHECK(
        accepted(with_step(step_of(R"("request":1)", R"("action":"wait","seconds":65535)"))).steps[0].action.seconds ==
        65535);
    refused(with_step(step_of(R"("request":1)", R"("action":"wait","seconds":1,"ms":5)")),
            "step 1: \"do\" has the key \"ms\", which this version does not know");

    // refuse
    CHECK(accepted(with_step(step_of(R"("request":1)", R"("action":"refuse")"))).steps[0].action.code == 0x1000);
    CHECK(accepted(with_step(step_of(R"("request":1)", R"("action":"refuse","code":"0x1000")"))).steps[0].action.code ==
          0x1000);
    CHECK(accepted(with_step(step_of(R"("request":1)", R"("action":"refuse","code":"0x1002")"))).steps[0].action.code ==
          0x1002);
    refused(with_step(step_of(R"("request":1)", R"("action":"refuse","code":"0x1001")")),
            "step 1: \"code\" must be \"0x1000\" or \"0x1002\"");
    refused(with_step(step_of(R"("request":1)", R"("action":"refuse","code":4098)")),
            "step 1: \"code\" must be text of 1 to 6 characters, with no control character");

    // the actions that take nothing
    refused(with_step(step_of(R"("status_file":1)", R"("action":"silent","for_ms":5)")),
            "step 1: \"do\" has the key \"for_ms\", which this version does not know");
    refused(with_step(step_of(R"("status_file":1)", R"("action":"abort","status":"1004")")),
            "step 1: \"do\" has the key \"status\", which this version does not know");

    // delay: a fixed time, or a range
    const Scenario fixed = accepted(with_step(step_of(R"("request":1)", R"("action":"delay","ms":1)")));
    CHECK(fixed.steps[0].action.min_ms == 1);
    CHECK(fixed.steps[0].action.max_ms == 1);
    CHECK(!fixed.steps[0].action.ranged);
    const Scenario ranged =
        accepted(with_step(step_of(R"("request":1)", R"("action":"delay","min_ms":0,"max_ms":60000)")));
    CHECK(ranged.steps[0].action.ranged);
    CHECK(ranged.steps[0].action.max_ms == 60000);
    CHECK(accepted(with_step(step_of(R"("request":1)", R"("action":"delay","min_ms":5,"max_ms":5)")))
              .steps[0]
              .action.ranged);
    refused(with_step(step_of(R"("request":1)", R"("action":"delay")")),
            "step 1: \"delay\" needs \"ms\", or \"min_ms\" and \"max_ms\"");
    refused(with_step(step_of(R"("request":1)", R"("action":"delay","min_ms":5)")),
            "step 1: \"delay\" needs \"ms\", or \"min_ms\" and \"max_ms\"");
    refused(with_step(step_of(R"("request":1)", R"("action":"delay","ms":5,"max_ms":9)")),
            "step 1: \"delay\" takes \"ms\", or \"min_ms\" and \"max_ms\", not both");
    refused(with_step(step_of(R"("request":1)", R"("action":"delay","ms":0)")),
            "step 1: \"ms\" must be a whole number from 1 to 60000");
    refused(with_step(step_of(R"("request":1)", R"("action":"delay","ms":60001)")),
            "step 1: \"ms\" must be a whole number from 1 to 60000");
    refused(with_step(step_of(R"("request":1)", R"("action":"delay","min_ms":-1,"max_ms":5)")),
            "step 1: \"min_ms\" must be a whole number from 0 to 60000");
    refused(with_step(step_of(R"("request":1)", R"("action":"delay","min_ms":500,"max_ms":499)")),
            "step 1: \"max_ms\" must be a whole number from 500 to 60000");
    refused(with_step(step_of(R"("request":1)", R"("action":"delay","min_ms":1,"max_ms":60001)")),
            "step 1: \"max_ms\" must be a whole number from 1 to 60000");
    // A range draws, so it needs the seed that fixes the draw.
    refused(R"({"kind":"adl-mock-scenario","format":1,"steps":[)" +
                step_of(R"("request":1)", R"("action":"delay","min_ms":1,"max_ms":9)") + "]}",
            "step 1 uses \"min_ms\" but the scenario has no \"seed\"");
    CHECK(accepted(R"({"kind":"adl-mock-scenario","format":1,"steps":[)" +
                   step_of(R"("request":1)", R"("action":"delay","ms":9)") + "]}")
              .steps.size() == 1);
}

TEST_CASE("mock scenario: the operation a step is for", "[mock_scenario][file]")
{
    const std::pair<std::string_view, OperationKind> words[] = {
        {"information", OperationKind::information},
        {"upload", OperationKind::upload},
        {"download_media_defined", OperationKind::download_media},
        {"download_operator_defined", OperationKind::download_operator}};
    for (const auto& [word, kind] : words)
    {
        INFO(word);
        const Scenario scenario = accepted(with_step(
            step_of(R"("request":1,"operation":")" + std::string{word} + R"(")", R"("action":"wait","seconds":1)")));
        CHECK(scenario.steps[0].operation == kind);
        CHECK(operation_word(kind) == word);
    }
    CHECK(!accepted(with_step(step_of(R"("request":1)", R"("action":"wait","seconds":1)"))).steps[0].operation);
    refused(with_step(step_of(R"("request":1,"operation":"download")", R"("action":"wait","seconds":1)")),
            "step 1: \"operation\" must be one of \"information\", \"upload\", \"download_media_defined\" or "
            "\"download_operator_defined\"");
    refused(with_step(step_of(R"("request":1,"operation":3)", R"("action":"wait","seconds":1)")),
            "step 1: \"operation\" must be one of \"information\", \"upload\", \"download_media_defined\" or "
            "\"download_operator_defined\"");
}

TEST_CASE("mock scenario: the words of the actions", "[mock_scenario][file]")
{
    for (const auto& [effect, word] :
         std::initializer_list<std::pair<Effect, std::string_view>>{{Effect::wait, "wait"},
                                                                    {Effect::refuse, "refuse"},
                                                                    {Effect::invalid_init_file, "invalid_init_file"},
                                                                    {Effect::drop_status_file, "drop_status_file"},
                                                                    {Effect::abort, "abort"},
                                                                    {Effect::silent, "silent"},
                                                                    {Effect::corrupt_served, "corrupt_served"},
                                                                    {Effect::wrong_checksum, "wrong_checksum"},
                                                                    {Effect::delay, "delay"}})
    {
        CHECK(effect_word(effect) == word);
    }
}

TEST_CASE("mock scenario: the writer", "[mock_scenario][file]")
{
    // Nothing optional, and no step.
    CHECK(write_scenario(Scenario{}) ==
          "{\n  \"kind\": \"adl-mock-scenario\",\n  \"format\": 1,\n  \"steps\": []\n}\n");

    // Text is escaped as JSON, and only as much as it must be.
    Scenario scenario;
    scenario.name = "a \"quoted\" \\ name \xc3\xa9";
    scenario.description = "one \"two\"";
    const std::string written = write_scenario(scenario);
    CHECK(written.find(R"("name": "a \"quoted\" \\ name )") != std::string::npos);
    CHECK(written.find(R"("description": "one \"two\"")") != std::string::npos);
    const auto again = read_scenario(written);
    REQUIRE(again);
    CHECK(*again == scenario);

    // A refusal says its code, a fixed delay its milliseconds.
    scenario.steps.push_back(ScenarioStep{1, Trigger::request, 3, std::nullopt, std::nullopt, std::nullopt,
                                          std::nullopt, ScenarioAction{Effect::refuse, 0, 0x1002, 0, 0, false}, ""});
    scenario.steps.push_back(ScenarioStep{2, Trigger::after_ms, 100, 50, std::nullopt, OperationKind::download_operator,
                                          std::nullopt, ScenarioAction{Effect::delay, 0, 0x1000, 70, 70, false}, ""});
    const std::string with_steps = write_scenario(scenario);
    CHECK(with_steps.find(R"("do": { "action": "refuse", "code": "0x1002" })") != std::string::npos);
    CHECK(with_steps.find(R"("when": { "after_ms": 100, "every": 50, "operation": "download_operator_defined" })") !=
          std::string::npos);
    CHECK(with_steps.find(R"("do": { "action": "delay", "ms": 70 })") != std::string::npos);
    CHECK(*read_scenario(with_steps) == scenario);
}

TEST_CASE("mock scenario: the file on a disk", "[mock_scenario][file]")
{
    adl::test::TempDir dir;
    const auto write = [&](std::string_view name, std::string_view text)
    {
        std::ofstream out(dir.path() / std::string{name}, std::ios::binary);
        out << text;
        return dir.path() / std::string{name};
    };

    const auto good = load_scenario_file(write("good.json", guide_example));
    REQUIRE(good);
    CHECK(good->steps.size() == 4);

    // A file with a refused content says why, as the text of a file does.
    const auto bad = load_scenario_file(write("bad.json", R"({"kind":"adl-mock-scenario","format":1})"));
    REQUIRE(!bad);
    CHECK(bad.error().message == "\"steps\" is missing; it is a list of steps");

    // A file that is not there, a folder, and a file that is too large.
    const auto missing = load_scenario_file(dir.path() / "absent.json");
    REQUIRE(!missing);
    CHECK(missing.error().message.starts_with("the file cannot be read ("));
    const auto folder = load_scenario_file(dir.path());
    REQUIRE(!folder);
    CHECK(folder.error().message.starts_with("the file cannot be read"));
    const auto big = load_scenario_file(write("big.json", std::string(adl::scenario::max_file_size + 1, ' ')));
    REQUIRE(!big);
    CHECK(big.error().message == "the file is larger than 262144 bytes");
    const auto empty = load_scenario_file(write("empty.json", ""));
    REQUIRE(!empty);
    CHECK(empty.error().message.starts_with("it is not valid JSON"));

    // A name outside the code page of a Windows computer is a path like any other.
    // (the name is in byte escapes, so that no compiler setting can change it)
    const std::filesystem::path wide =
        dir.path() / adl::platform::path_from_utf8("szenario-\xc3\xa4\xc3\xb6\xc3\xbc-\xe6\x97\xa5\xe6\x9c\xac.json");
    {
        std::ofstream out(wide, std::ios::binary);
        out << guide_example;
    }
    CHECK(load_scenario_file(wide));
}

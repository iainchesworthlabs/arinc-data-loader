// SPDX-License-Identifier: GPL-3.0-or-later
// The scenario file (libs/loader_scenario): what the reader takes, every sentence it refuses with, the limits, and
// the writer, which gives back what was read.

#include "adl/loader_scenario/reader.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace
{

using namespace adl::loader_scenario;

// The example of the guide, in the form the writer gives.
constexpr std::string_view example = R"({
  "kind": "adl-loader-scenario",
  "format": 1,
  "name": "A block lost twice, then a wrong abort code",
  "description": "Block 2 of LOAD1.BIN is lost twice after the third request. The fourth status file is answered with an ABORT of a code the loader does not use. Twenty seconds in the loader goes silent for five. One data block in fifty is damaged.",
  "seed": 7,
  "steps": [
    { "when": { "request": 3 }, "on": { "datagram": "data", "file": "LOAD1.BIN", "block": 2 }, "do": { "action": "drop" }, "count": 2 },
    { "when": { "status_file": 4 }, "on": { "datagram": "reply" }, "do": { "action": "abort_message", "status": "0x7777" } },
    { "when": { "after_ms": 20000 }, "do": { "action": "silent", "for_ms": 5000 } },
    { "comment": "random damage", "when": { "after_ms": 0, "operation": "upload" }, "on": { "datagram": "data" }, "do": { "action": "corrupt", "offset": 4, "xor": 1 }, "count": "all", "per_mille": 20 }
  ]
}
)";

[[nodiscard]] Scenario read_ok(std::string_view text)
{
    const auto scenario = read_scenario(text);
    INFO(text);
    if (!scenario)
    {
        FAIL(scenario.error().message);
    }
    return *scenario;
}

[[nodiscard]] std::string refused(std::string_view text)
{
    const auto scenario = read_scenario(text);
    INFO(text);
    REQUIRE(!scenario);
    return scenario.error().message;
}

// A file with one step in it.
[[nodiscard]] std::string with_step(std::string_view step, std::string_view seed = R"("seed": 1,)")
{
    return R"({ "kind": "adl-loader-scenario", "format": 1, )" + std::string{seed} + R"( "steps": [ )" +
           std::string{step} + " ] }";
}

[[nodiscard]] Step one_step(std::string_view step)
{
    const Scenario scenario = read_ok(with_step(step));
    REQUIRE(scenario.steps.size() == 1);
    return scenario.steps[0];
}

} // namespace

TEST_CASE("the example of the guide is read, and written back as it is", "[loader_scenario][reader]")
{
    const Scenario scenario = read_ok(example);
    CHECK(scenario.name == "A block lost twice, then a wrong abort code");
    CHECK(scenario.seed == 7);
    REQUIRE(scenario.steps.size() == 4);

    const Step& drop = scenario.steps[0];
    CHECK(drop.when.kind == TriggerKind::request);
    CHECK(drop.when.value == 3);
    CHECK(drop.on.datagram == DatagramKind::data);
    CHECK(drop.on.file == "LOAD1.BIN");
    CHECK(drop.on.block == 2);
    CHECK(drop.fault.action == Action::drop);
    CHECK(drop.count == 2);
    CHECK(!drop.all);

    const Step& abort = scenario.steps[1];
    CHECK(abort.when.kind == TriggerKind::status_file);
    CHECK(abort.on.datagram == DatagramKind::reply);
    CHECK(abort.fault.action == Action::abort_message);
    CHECK(abort.fault.status == 0x7777);

    const Step& silent = scenario.steps[2];
    CHECK(silent.when.kind == TriggerKind::after_ms);
    CHECK(silent.fault.action == Action::silent);
    CHECK(silent.fault.milliseconds == 5000);

    const Step& corrupt = scenario.steps[3];
    CHECK(corrupt.comment == "random damage");
    CHECK(corrupt.when.operation == OperationName::upload);
    CHECK(corrupt.all);
    CHECK(corrupt.per_mille == 20);
    CHECK(corrupt.fault.offset == 4);

    CHECK(write_scenario(scenario) == example);
}

TEST_CASE("a scenario with no steps changes nothing and is a scenario", "[loader_scenario][reader]")
{
    const Scenario scenario = read_ok(R"({ "kind": "adl-loader-scenario", "format": 1, "steps": [] })");
    CHECK(scenario.steps.empty());
    CHECK(!scenario.seed);
    CHECK(write_scenario(scenario) ==
          "{\n  \"kind\": \"adl-loader-scenario\",\n  \"format\": 1,\n  \"steps\": []\n}\n");
}

TEST_CASE("every action is read with its keys and its defaults", "[loader_scenario][reader]")
{
    CHECK(one_step(R"({ "when": { "request": 1 }, "do": { "action": "drop" } })").fault.action == Action::drop);

    const Step duplicate = one_step(R"({ "when": { "request": 1 }, "do": { "action": "duplicate" } })");
    CHECK(duplicate.fault.copies == 2);
    CHECK(one_step(R"({ "when": { "request": 1 }, "do": { "action": "duplicate", "copies": 16 } })").fault.copies ==
          16);

    const Step delay = one_step(R"({ "when": { "request": 1 }, "do": { "action": "delay", "ms": 2500 } })");
    CHECK(delay.fault.milliseconds == 2500);
    CHECK(delay.fault.min_milliseconds == 2500);
    CHECK(delay.fault.max_milliseconds == 2500);
    const Step range =
        one_step(R"({ "when": { "request": 1 }, "do": { "action": "delay", "min_ms": 200, "max_ms": 900 } })");
    CHECK(range.fault.min_milliseconds == 200);
    CHECK(range.fault.max_milliseconds == 900);

    CHECK(one_step(R"({ "when": { "request": 1 }, "do": { "action": "reorder" } })").fault.milliseconds == 0);
    CHECK(one_step(R"({ "when": { "request": 1 }, "do": { "action": "reorder", "within_ms": 300 } })")
              .fault.milliseconds == 300);

    const Step corrupt = one_step(R"({ "when": { "request": 1 }, "do": { "action": "corrupt" } })");
    CHECK(corrupt.fault.offset == 4);
    CHECK(corrupt.fault.xor_mask == 1);
    const Step corrupt_more =
        one_step(R"({ "when": { "request": 1 }, "do": { "action": "corrupt", "offset": 9, "xor": 255 } })");
    CHECK(corrupt_more.fault.offset == 9);
    CHECK(corrupt_more.fault.xor_mask == 255);

    CHECK(one_step(R"({ "when": { "request": 1 }, "do": { "action": "check_value" } })").fault.action ==
          Action::check_value);
    CHECK(one_step(R"({ "when": { "request": 1 }, "do": { "action": "length", "delta": -3 } })").fault.delta == -3);

    const Step error =
        one_step(R"({ "when": { "request": 1 }, "do": { "action": "error", "code": 2, "text": "no" } })");
    CHECK(error.fault.code == 2);
    CHECK(error.fault.text == "no");
    CHECK(one_step(R"({ "when": { "request": 1 }, "do": { "action": "error", "code": 0 } })").fault.text.empty());

    CHECK(one_step(R"({ "when": { "request": 1 }, "do": { "action": "wait", "seconds": 9 } })").fault.seconds == 9);
    CHECK(one_step(R"({ "when": { "request": 1 }, "do": { "action": "abort_message", "status": "0x1004" } })")
              .fault.status == 0x1004);
    CHECK(one_step(R"({ "when": { "request": 1 }, "do": { "action": "abort" } })").fault.status == 0x1005);
    CHECK(one_step(R"({ "when": { "request": 1 }, "do": { "action": "abort", "status": "0x1004" } })").fault.status ==
          0x1004);
    CHECK(one_step(R"({ "when": { "request": 1 }, "do": { "action": "silent" } })").fault.milliseconds == 0);
    CHECK(one_step(R"({ "when": { "request": 1 }, "do": { "action": "silent", "for_ms": 7 } })").fault.milliseconds ==
          7);
}

TEST_CASE("a wait that names no datagram is the answer to a request", "[loader_scenario][reader]")
{
    CHECK(one_step(R"({ "when": { "request": 1 }, "do": { "action": "wait", "seconds": 2 } })").on.datagram ==
          DatagramKind::reply);
    CHECK(
        one_step(R"({ "when": { "request": 1 }, "on": { "file": ".LUS" }, "do": { "action": "wait", "seconds": 2 } })")
            .on.datagram == DatagramKind::reply);
    CHECK(one_step(
              R"({ "when": { "request": 1 }, "on": { "datagram": "any" }, "do": { "action": "wait", "seconds": 2 } })")
              .on.datagram == DatagramKind::any);
}

TEST_CASE("the triggers and what goes with them are read", "[loader_scenario][reader]")
{
    const Step status = one_step(
        R"({ "when": { "status_file": 2, "every": 3, "times": 4, "operation": "download_media_defined" }, "do": { "action": "drop" } })");
    CHECK(status.when.kind == TriggerKind::status_file);
    CHECK(status.when.value == 2);
    CHECK(status.when.every == 3);
    CHECK(status.when.times == 4);
    CHECK(status.when.operation == OperationName::download_media_defined);

    const Step time = one_step(R"({ "when": { "after_ms": 0, "every": 10 }, "do": { "action": "drop" } })");
    CHECK(time.when.kind == TriggerKind::after_ms);
    CHECK(time.when.every == 10);

    CHECK(one_step(R"({ "when": { "request": 1, "operation": "information" }, "do": { "action": "drop" } })")
              .when.operation == OperationName::information);
    CHECK(one_step(
              R"({ "when": { "request": 1, "operation": "download_operator_defined" }, "do": { "action": "drop" } })")
              .when.operation == OperationName::download_operator_defined);
}

TEST_CASE("count is a number or all, and per_mille needs a seed", "[loader_scenario][reader]")
{
    const Step counted = one_step(R"({ "when": { "request": 1 }, "do": { "action": "drop" }, "count": 1000000 })");
    CHECK(counted.count == 1'000'000);
    const Step all = one_step(R"({ "when": { "request": 1 }, "do": { "action": "drop" }, "count": "all" })");
    CHECK(all.all);
    const Step chance = one_step(R"({ "when": { "request": 1 }, "do": { "action": "drop" }, "per_mille": 1000 })");
    CHECK(chance.per_mille == 1000);
    CHECK(refused(with_step(R"({ "when": { "request": 1 }, "do": { "action": "drop" }, "per_mille": 5 })", "")) ==
          "step 1 uses \"per_mille\" but the scenario has no \"seed\"");
    CHECK(refused(with_step(R"({ "when": { "request": 1 }, "do": { "action": "delay", "min_ms": 1, "max_ms": 2 } })",
                            "")) == "step 1 uses a delay range but the scenario has no \"seed\"");
    // A delay of one length needs no draw, and so no seed.
    CHECK(read_scenario(with_step(R"({ "when": { "request": 1 }, "do": { "action": "delay", "ms": 5 } })", "")));
}

TEST_CASE("the selector's keys are read", "[loader_scenario][reader]")
{
    for (const std::string_view kind : {"any", "request", "option_ack", "ack", "data", "error", "reply"})
    {
        const Step step = one_step(R"({ "when": { "request": 1 }, "on": { "datagram": ")" + std::string{kind} +
                                   R"(" }, "do": { "action": "drop" } })");
        CHECK(to_string(step.on.datagram) == kind);
    }
    const Step step =
        one_step(R"({ "when": { "request": 1 }, "on": { "file": ".LUS", "block": 0 }, "do": { "action": "drop" } })");
    CHECK(step.on.file == ".LUS");
    CHECK(step.on.block == 0);
}

// ---- What is refused ------------------------------------------------------------------------------------------

TEST_CASE("a file that is not strict JSON is refused with one sentence", "[loader_scenario][reader]")
{
    const std::string not_json =
        "it is not valid JSON, or it has a repeated key, a comment, a trailing comma, text that is not UTF-8, or is "
        "nested deeper than 8 levels";
    CHECK(refused("") == not_json);
    CHECK(refused("{") == not_json);
    CHECK(refused(R"({ "kind": "adl-loader-scenario", "kind": "x" })") == not_json);                // a repeated key
    CHECK(refused("{ // a comment\n}") == not_json);                                                // a comment
    CHECK(refused(R"({ "kind": "adl-loader-scenario", "format": 1, "steps": [], })") == not_json);  // a comma
    CHECK(refused(R"({ "kind": "adl-loader-scenario", "format": 1, "steps": [] } x)") == not_json); // text after
    CHECK(refused("\xEF\xBB\xBF{ \"kind\": 1 }") == not_json);                                      // a BOM
    CHECK(refused("{ \"kind\": \"\xFF\" }") == not_json);                                           // not UTF-8
    CHECK(refused(R"({ "a": [[[[[[[[[[1]]]]]]]]]] })") == not_json);                                // too deep
}

TEST_CASE("the top level is checked", "[loader_scenario][reader]")
{
    CHECK(refused("[]") == "it must be a JSON object");
    CHECK(refused(R"({ "format": 1, "steps": [] })") == "\"kind\" must be \"adl-loader-scenario\"");
    CHECK(refused(R"({ "kind": "scenario", "format": 1, "steps": [] })") == "\"kind\" must be \"adl-loader-scenario\"");
    CHECK(refused(R"({ "kind": "adl-mock-scenario", "format": 1, "steps": [] })") ==
          "\"kind\" is \"adl-mock-scenario\": that file is for adl_mock, and this is the loader's scenario");
    CHECK(refused(R"({ "kind": "adl-loader-scenario", "steps": [] })") == "\"format\" must be 1");
    CHECK(refused(R"({ "kind": "adl-loader-scenario", "format": "1", "steps": [] })") == "\"format\" must be 1");
    CHECK(refused(R"({ "kind": "adl-loader-scenario", "format": 2, "steps": [] })") ==
          "\"format\" is 2; this version reads 1, so the file was made for a newer version");
    CHECK(refused(R"({ "kind": "adl-loader-scenario", "format": 1 })") == "it needs \"steps\"");
    CHECK(refused(R"({ "kind": "adl-loader-scenario", "format": 1, "steps": {} })") == "\"steps\" must be a list");
    CHECK(refused(R"({ "kind": "adl-loader-scenario", "format": 1, "steps": [], "chanse": 1 })") ==
          "it has the key \"chanse\", which this version does not know");
}

TEST_CASE("name, description and seed have limits", "[loader_scenario][reader]")
{
    const auto top = [](std::string_view member)
    { return R"({ "kind": "adl-loader-scenario", "format": 1, )" + std::string{member} + R"(, "steps": [] })"; };
    CHECK(read_scenario(top(R"("name": ")" + std::string(64, 'n') + R"(")")));
    CHECK(refused(top(R"("name": ")" + std::string(65, 'n') + R"(")")) ==
          "\"name\" must be text of 1 to 64 characters, with no control character");
    CHECK(refused(top(R"("name": "")")) == "\"name\" must be text of 1 to 64 characters, with no control character");
    CHECK(refused(top(R"("name": 3)")) == "\"name\" must be text of 1 to 64 characters, with no control character");
    CHECK(refused(top(R"("name": "a\u0001b")")) ==
          "\"name\" must be text of 1 to 64 characters, with no control character");
    CHECK(read_scenario(top(R"("description": ")" + std::string(1000, 'd') + R"(")")));
    CHECK(refused(top(R"("description": ")" + std::string(1001, 'd') + R"(")")) ==
          "\"description\" must be text of 1 to 1000 characters, with no control character");
    CHECK(read_scenario(top(R"("seed": 0)")));
    CHECK(read_scenario(top(R"("seed": 9007199254740991)")));
    CHECK(refused(top(R"("seed": 9007199254740992)")) == "\"seed\" must be a whole number from 0 to 9007199254740991");
    CHECK(refused(top(R"("seed": -1)")) == "\"seed\" must be a whole number from 0 to 9007199254740991");
    CHECK(refused(top(R"("seed": 1.5)")) == "\"seed\" must be a whole number from 0 to 9007199254740991");
    CHECK(refused(top(R"("seed": 1e3)")) == "\"seed\" must be a whole number from 0 to 9007199254740991");
    CHECK(refused(top(R"("seed": "7")")) == "\"seed\" must be a whole number from 0 to 9007199254740991");
}

TEST_CASE("a step is checked, key by key", "[loader_scenario][reader]")
{
    CHECK(refused(with_step("3")) == "step 1: it must be an object");
    CHECK(refused(with_step(R"({ "do": { "action": "drop" } })")) == "step 1: it needs \"when\"");
    CHECK(refused(with_step(R"({ "when": { "request": 1 } })")) == "step 1: it needs \"do\"");
    CHECK(refused(with_step(R"({ "when": { "request": 1 }, "do": { "action": "drop" }, "chanse": 5 })")) ==
          "step 1 has the key \"chanse\", which this version does not know");
    CHECK(refused(with_step(R"({ "when": 3, "do": { "action": "drop" } })")) == "step 1: \"when\" must be an object");
    CHECK(refused(with_step(R"({ "when": { "request": 1 }, "do": 3 })")) == "step 1: \"do\" must be an object");
    CHECK(refused(with_step(R"({ "when": { "request": 1 }, "on": 3, "do": { "action": "drop" } })")) ==
          "step 1: \"on\" must be an object");
    CHECK(refused(with_step(R"({ "when": { "request": 1 }, "do": { "action": "drop" }, "comment": "" })")) ==
          "step 1: \"comment\" must be text of 1 to 200 characters, with no control character");
    CHECK(refused(with_step(R"({ "when": { "request": 1 }, "do": { "action": "drop" }, "count": 0 })")) ==
          "step 1: \"count\" must be a whole number from 1 to 1000000 or \"all\"");
    CHECK(refused(with_step(R"({ "when": { "request": 1 }, "do": { "action": "drop" }, "count": "some" })")) ==
          "step 1: \"count\" must be a whole number from 1 to 1000000 or \"all\"");
    CHECK(refused(with_step(R"({ "when": { "request": 1 }, "do": { "action": "drop" }, "per_mille": 0 })")) ==
          "step 1: \"per_mille\" must be a whole number from 1 to 1000");
    CHECK(refused(with_step(R"({ "when": { "request": 1 }, "do": { "action": "drop" }, "per_mille": 1001 })")) ==
          "step 1: \"per_mille\" must be a whole number from 1 to 1000");
    // The number is the step's place in the list.
    CHECK(refused(with_step(
              R"({ "when": { "request": 1 }, "do": { "action": "drop" } }, { "do": { "action": "drop" } })")) ==
          "step 2: it needs \"when\"");
}

TEST_CASE("when is checked", "[loader_scenario][reader]")
{
    const auto when = [](std::string_view text)
    { return with_step(R"({ "when": )" + std::string{text} + R"(, "do": { "action": "drop" } })"); };
    const std::string exactly = "step 1: \"when\" needs exactly one of \"request\", \"status_file\" and \"after_ms\"";
    CHECK(refused(when("{}")) == exactly);
    CHECK(refused(when(R"({ "request": 1, "status_file": 1 })")) == exactly);
    CHECK(refused(when(R"({ "every": 3 })")) == exactly);
    CHECK(refused(when(R"({ "request": 0 })")) == "step 1: \"request\" must be a whole number from 1 to 1000000");
    CHECK(refused(when(R"({ "status_file": 1000001 })")) ==
          "step 1: \"status_file\" must be a whole number from 1 to 1000000");
    CHECK(refused(when(R"({ "after_ms": -1 })")) == "step 1: \"after_ms\" must be a whole number from 0 to 86400000");
    CHECK(refused(when(R"({ "after_ms": 86400001 })")) ==
          "step 1: \"after_ms\" must be a whole number from 0 to 86400000");
    CHECK(refused(when(R"({ "after_ms": 1.5 })")) == "step 1: \"after_ms\" must be a whole number from 0 to 86400000");
    CHECK(refused(when(R"({ "request": 1, "every": 0 })")) ==
          "step 1: \"every\" must be a whole number from 1 to 1000000");
    CHECK(refused(when(R"({ "after_ms": 1, "every": 9 })")) ==
          "step 1: \"every\" must be a whole number from 10 to 86400000");
    CHECK(read_scenario(when(R"({ "after_ms": 1, "every": 10 })")));
    CHECK(refused(when(R"({ "request": 1, "times": 0 })")) ==
          "step 1: \"times\" must be a whole number from 1 to 1000000");
    CHECK(refused(when(R"({ "request": 1, "operation": "find" })")) ==
          "step 1: \"operation\" must be \"information\", \"upload\", \"download_media_defined\" or "
          "\"download_operator_defined\"");
    CHECK(refused(when(R"({ "request": 1, "operation": "any" })")) ==
          "step 1: \"operation\" must be \"information\", \"upload\", \"download_media_defined\" or "
          "\"download_operator_defined\"");
    CHECK(refused(when(R"({ "request": 1, "chance": 1 })")) ==
          "step 1: \"when\" has the key \"chance\", which this version does not know");
}

TEST_CASE("on is checked", "[loader_scenario][reader]")
{
    const auto on = [](std::string_view text)
    {
        return with_step(R"({ "when": { "request": 1 }, "on": )" + std::string{text} +
                         R"(, "do": { "action": "drop" } })");
    };
    CHECK(read_scenario(on("{}")));
    CHECK(
        refused(on(R"({ "datagram": "block" })")) ==
        "step 1: \"datagram\" must be \"any\", \"request\", \"option_ack\", \"ack\", \"data\", \"error\" or \"reply\"");
    CHECK(
        refused(on(R"({ "datagram": 3 })")) ==
        "step 1: \"datagram\" must be \"any\", \"request\", \"option_ack\", \"ack\", \"data\", \"error\" or \"reply\"");
    CHECK(refused(on(R"({ "file": "" })")) ==
          "step 1: \"file\" must be a file name or an extension from its dot, of 1 to 254 characters");
    CHECK(refused(on(R"({ "file": ")" + std::string(255, 'f') + R"(" })")) ==
          "step 1: \"file\" must be a file name or an extension from its dot, of 1 to 254 characters");
    CHECK(read_scenario(on(R"({ "file": ")" + std::string(254, 'f') + R"(" })")));
    CHECK(refused(on(R"({ "block": 65536 })")) == "step 1: \"block\" must be a whole number from 0 to 65535");
    CHECK(refused(on(R"({ "block": -1 })")) == "step 1: \"block\" must be a whole number from 0 to 65535");
    CHECK(refused(on(R"({ "file": "A", "name": "A" })")) ==
          "step 1: \"on\" has the key \"name\", which this version does not know");
}

TEST_CASE("do is checked, action by action", "[loader_scenario][reader]")
{
    const auto action = [](std::string_view text)
    { return with_step(R"({ "when": { "request": 1 }, "do": )" + std::string{text} + " }"); };
    CHECK(refused(action("{}")) == "step 1: \"do\" needs an \"action\"");
    CHECK(refused(action(R"({ "action": "explode" })")) ==
          "step 1: \"action\" is \"explode\", which this version does not know");
    CHECK(refused(action(R"({ "action": 3 })")) == "step 1: \"action\" is not text, which this version does not know");
    CHECK(refused(action(R"({ "action": "drop", "ms": 5 })")) == "step 1: \"drop\" does not take the key \"ms\"");
    CHECK(refused(action(R"({ "action": "duplicate", "copies": 1 })")) ==
          "step 1: \"copies\" must be a whole number from 2 to 16");
    CHECK(refused(action(R"({ "action": "duplicate", "copies": 17 })")) ==
          "step 1: \"copies\" must be a whole number from 2 to 16");
    CHECK(refused(action(R"({ "action": "delay" })")) ==
          "step 1: \"delay\" needs \"ms\", or \"min_ms\" and \"max_ms\"");
    CHECK(refused(action(R"({ "action": "delay", "ms": 0 })")) ==
          "step 1: \"ms\" must be a whole number from 1 to 60000");
    CHECK(refused(action(R"({ "action": "delay", "ms": 60001 })")) ==
          "step 1: \"ms\" must be a whole number from 1 to 60000");
    CHECK(refused(action(R"({ "action": "delay", "ms": 5, "min_ms": 1, "max_ms": 2 })")) ==
          "step 1: \"delay\" takes \"ms\", or \"min_ms\" and \"max_ms\", not both");
    CHECK(refused(action(R"({ "action": "delay", "min_ms": 1 })")) == "step 1: it needs \"max_ms\"");
    CHECK(refused(action(R"({ "action": "delay", "min_ms": 9, "max_ms": 2 })")) ==
          "step 1: \"min_ms\" must not be more than \"max_ms\"");
    CHECK(refused(action(R"({ "action": "reorder", "within_ms": 0 })")) ==
          "step 1: \"within_ms\" must be a whole number from 1 to 60000");
    CHECK(refused(action(R"({ "action": "corrupt", "offset": 65536 })")) ==
          "step 1: \"offset\" must be a whole number from 0 to 65535");
    CHECK(refused(action(R"({ "action": "corrupt", "xor": 0 })")) ==
          "step 1: \"xor\" must be a whole number from 1 to 255");
    CHECK(refused(action(R"({ "action": "length" })")) == "step 1: it needs \"delta\"");
    CHECK(refused(action(R"({ "action": "length", "delta": 0 })")) == "step 1: \"delta\" must not be 0");
    CHECK(refused(action(R"({ "action": "length", "delta": 65536 })")) ==
          "step 1: \"delta\" must be a whole number from -65535 to 65535");
    CHECK(refused(action(R"({ "action": "error" })")) == "step 1: it needs \"code\"");
    CHECK(refused(action(R"({ "action": "error", "code": 65536 })")) ==
          "step 1: \"code\" must be a whole number from 0 to 65535");
    CHECK(refused(action(R"({ "action": "error", "code": 1, "text": ")" + std::string(65, 't') + R"(" })")) ==
          "step 1: \"text\" must be text of 1 to 64 characters, with no control character");
    CHECK(refused(action(R"({ "action": "wait" })")) == "step 1: it needs \"seconds\"");
    CHECK(refused(action(R"({ "action": "wait", "seconds": 0 })")) ==
          "step 1: \"seconds\" must be a whole number from 1 to 65535");
    CHECK(refused(action(R"({ "action": "wait", "seconds": 65536 })")) ==
          "step 1: \"seconds\" must be a whole number from 1 to 65535");
    CHECK(refused(action(R"({ "action": "abort_message" })")) == "step 1: it needs \"status\"");
    CHECK(refused(action(R"({ "action": "abort_message", "status": "1004" })")) ==
          "step 1: \"status\" must be text such as \"0x1004\": 0x and four hexadecimal digits");
    CHECK(refused(action(R"({ "action": "abort_message", "status": "0x10045" })")) ==
          "step 1: \"status\" must be text such as \"0x1004\": 0x and four hexadecimal digits");
    CHECK(refused(action(R"({ "action": "abort_message", "status": "0xGGGG" })")) ==
          "step 1: \"status\" must be text such as \"0x1004\": 0x and four hexadecimal digits");
    CHECK(refused(action(R"({ "action": "abort_message", "status": 4100 })")) ==
          "step 1: \"status\" must be text such as \"0x1004\": 0x and four hexadecimal digits");
    CHECK(read_scenario(action(R"({ "action": "abort_message", "status": "0xffff" })")));
    CHECK(refused(action(R"({ "action": "abort", "status": "0x1003" })")) ==
          "step 1: an abort of the loader's own has \"status\" \"0x1004\" or \"0x1005\"");
    CHECK(refused(action(R"({ "action": "silent", "for_ms": 0 })")) ==
          "step 1: \"for_ms\" must be a whole number from 1 to 86400000");
}

TEST_CASE("what an action goes with is checked", "[loader_scenario][reader]")
{
    CHECK(refused(with_step(R"({ "when": { "request": 1 }, "on": {}, "do": { "action": "abort" } })")) ==
          "step 1: \"abort\" acts on the operation, so it takes no \"on\" or \"count\"");
    CHECK(refused(with_step(R"({ "when": { "request": 1 }, "do": { "action": "silent" }, "count": 2 })")) ==
          "step 1: \"silent\" acts on the operation, so it takes no \"on\" or \"count\"");
    CHECK(refused(with_step(R"({ "when": { "request": 1, "every": 2 }, "do": { "action": "abort" } })")) ==
          "step 1: \"abort\" happens once, so its \"when\" takes no \"every\" or \"times\"");
    CHECK(refused(with_step(R"({ "when": { "request": 1, "times": 2 }, "do": { "action": "silent" } })")) ==
          "step 1: \"silent\" happens once, so its \"when\" takes no \"every\" or \"times\"");
    // A chance and an operation do go with them.
    CHECK(read_scenario(with_step(
        R"({ "when": { "request": 1, "operation": "upload" }, "do": { "action": "abort" }, "per_mille": 500 })")));
    CHECK(refused(with_step(
              R"({ "when": { "request": 1 }, "on": { "datagram": "data" }, "do": { "action": "check_value" } })")) ==
          "step 1: \"check_value\" acts on an option acknowledgement, so \"on\" may give only a \"file\" (and "
          "\"datagram\" \"option_ack\")");
    CHECK(refused(
              with_step(R"({ "when": { "request": 1 }, "on": { "block": 1 }, "do": { "action": "check_value" } })")) ==
          "step 1: \"check_value\" acts on an option acknowledgement, so \"on\" may give only a \"file\" (and "
          "\"datagram\" \"option_ack\")");
    CHECK(read_scenario(with_step(
        R"({ "when": { "request": 1 }, "on": { "datagram": "option_ack", "file": "A.BIN" }, "do": { "action": "check_value" } })")));
}

// ---- Limits ---------------------------------------------------------------------------------------------------

TEST_CASE("the number of steps and the size of the file are limited", "[loader_scenario][reader]")
{
    const std::string step = R"({ "when": { "request": 1 }, "do": { "action": "drop" } })";
    const auto many = [&](std::size_t count)
    {
        std::string text = R"({ "kind": "adl-loader-scenario", "format": 1, "steps": [)";
        for (std::size_t i = 0; i < count; ++i)
        {
            text += i == 0 ? step : "," + step;
        }
        return text + "] }";
    };
    CHECK(read_ok(many(1000)).steps.size() == 1000);
    CHECK(refused(many(1001)) == "there are more than 1000 steps");

    std::string padded = R"({ "kind": "adl-loader-scenario", "format": 1, "steps": [] })";
    padded += std::string(max_file_bytes - padded.size(), ' ');
    REQUIRE(padded.size() == max_file_bytes);
    CHECK(read_scenario(padded));
    padded += ' ';
    CHECK(refused(padded) == "the file is larger than 262144 bytes");
}

// ---- Writing --------------------------------------------------------------------------------------------------

TEST_CASE("what the writer writes is read back as the same scenario", "[loader_scenario][reader]")
{
    Scenario scenario;
    scenario.name = "every action";
    scenario.description = "one step for each";
    scenario.seed = 99;
    const auto add = [&](Action action, auto&& set)
    {
        Step step;
        step.fault.action = action;
        set(step);
        scenario.steps.push_back(step);
    };
    add(Action::drop,
        [](Step& step)
        {
            step.on.datagram = DatagramKind::data;
            step.on.block = 3;
            step.count = 4;
        });
    add(Action::duplicate,
        [](Step& step)
        {
            step.fault.copies = 5;
            step.all = true;
        });
    add(Action::delay,
        [](Step& step)
        {
            step.fault.milliseconds = 40;
            step.fault.min_milliseconds = 40;
            step.fault.max_milliseconds = 40;
        });
    add(Action::delay,
        [](Step& step)
        {
            step.fault.milliseconds = 5;
            step.fault.min_milliseconds = 5;
            step.fault.max_milliseconds = 50;
            step.per_mille = 300;
        });
    add(Action::reorder, [](Step& step) { step.fault.milliseconds = 250; });
    add(Action::corrupt,
        [](Step& step)
        {
            step.fault.offset = 7;
            step.fault.xor_mask = 0x40;
            step.on.file = ".LUS";
        });
    add(Action::check_value, [](Step& step) { step.on.file = "A.BIN"; });
    add(Action::length, [](Step& step) { step.fault.delta = -2; });
    add(Action::error,
        [](Step& step)
        {
            step.fault.code = 5;
            step.fault.text = "unknown transfer id";
        });
    add(Action::wait,
        [](Step& step)
        {
            step.fault.seconds = 11;
            step.on.datagram = DatagramKind::reply;
        });
    add(Action::wait,
        [](Step& step)
        {
            step.fault.seconds = 12;
            step.on.datagram = DatagramKind::any;
        });
    add(Action::abort_message,
        [](Step& step)
        {
            step.fault.status = 0x1004;
            step.on.datagram = DatagramKind::reply;
        });
    add(Action::abort,
        [](Step& step)
        {
            step.fault.status = 0x1004;
            step.when = {TriggerKind::status_file, 2, 0, 0, OperationName::upload};
        });
    add(Action::silent,
        [](Step& step)
        {
            step.fault.milliseconds = 900;
            step.when = {TriggerKind::after_ms, 5, 0, 0, OperationName::any};
        });
    add(Action::silent, [](Step&) {});
    scenario.steps[0].when = {TriggerKind::request, 2, 3, 4, OperationName::download_operator_defined};
    scenario.steps[0].comment = "with a comment";

    const std::string text = write_scenario(scenario);
    const Scenario again = read_ok(text);
    CHECK(again == scenario);
    CHECK(write_scenario(again) == text);
}

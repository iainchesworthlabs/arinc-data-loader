// SPDX-License-Identifier: GPL-3.0-or-later
// The shared reader of the scenario files: what it reads, and the one sentence it says for every way a file can be
// wrong. The sentences are part of the interface (the guide lists them), so they are written out here whole.

#include "adl/scenario/envelope.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <string>
#include <string_view>

using namespace adl::scenario;

namespace
{

constexpr std::array<std::string_view, 1> mock_when_keys{"operation"};
constexpr std::array<std::string_view, 2> loader_step_keys{"on", "count"};
const Family mock{mock_kind, mock_when_keys, {}};
const Family loader{loader_kind, {}, loader_step_keys};
const Family plain{mock_kind, {}, {}};

// A file of the mock kind with `body` after kind and format.
std::string file(std::string_view body)
{
    return R"({"kind":"adl-mock-scenario","format":1,)" + std::string{body} + "}";
}

// A file with a seed and one step.
std::string with_step(std::string_view step)
{
    return file(R"("seed":1,"steps":[)" + std::string{step} + "]");
}

const std::string ok_step = R"({"when":{"request":1},"do":{"action":"wait","seconds":2}})";

void refused(std::string_view text, std::string_view sentence, const Family& family = plain)
{
    INFO(text);
    const auto envelope = read_envelope(text, family);
    REQUIRE(!envelope);
    CHECK(envelope.error().message == sentence);
}

Envelope accepted(std::string_view text, const Family& family = plain)
{
    INFO(text);
    auto envelope = read_envelope(text, family);
    if (!envelope)
    {
        FAIL(envelope.error().message);
    }
    return std::move(*envelope);
}

constexpr std::string_view not_json =
    "it is not valid JSON, or it has a repeated key, a comment, a trailing comma, text that is not UTF-8, or is nested "
    "deeper than 8 levels";

} // namespace

TEST_CASE("scenario envelope: a file with everything in it is read", "[scenario][envelope]")
{
    const Envelope envelope = accepted(R"({
        "kind": "adl-mock-scenario", "format": 1, "name": "Slow start", "description": "The first request waits.",
        "seed": 20261120,
        "steps": [
            { "when": { "request": 1 }, "do": { "action": "wait", "seconds": 2 } },
            { "comment": "only uploads", "when": { "request": 2, "operation": "upload" }, "per_mille": 250,
              "do": { "action": "delay", "min_ms": 200, "max_ms": 900 } },
            { "when": { "status_file": 2, "every": 3, "times": 4 }, "do": { "action": "drop_status_file" } },
            { "when": { "after_ms": 20000, "every": 10 }, "do": { "action": "abort" } }
        ] })",
                                       mock);
    CHECK(envelope.name == "Slow start");
    CHECK(envelope.description == "The first request waits.");
    REQUIRE(envelope.seed.has_value());
    CHECK(*envelope.seed == 20261120);
    REQUIRE(envelope.steps.size() == 4);

    const Step& first = envelope.steps[0];
    CHECK(first.number == 1);
    CHECK(first.when.trigger == Trigger::request);
    CHECK(first.when.at == 1);
    CHECK(!first.when.every);
    CHECK(!first.when.times);
    CHECK(!first.per_mille);
    CHECK(first.comment.empty());
    CHECK(first.action == "wait");
    CHECK(*json::find(first.action_members, "seconds")->as_integer() == 2);
    CHECK(first.when.rest.empty());
    CHECK(first.rest.empty());

    const Step& second = envelope.steps[1];
    CHECK(second.number == 2);
    CHECK(second.when.at == 2);
    CHECK(second.per_mille == 250);
    CHECK(second.comment == "only uploads");
    CHECK(second.action == "delay");
    REQUIRE(second.when.rest.size() == 1);
    CHECK(second.when.rest[0].key == "operation");
    CHECK(*second.when.rest[0].value.as_string() == "upload");

    const Step& third = envelope.steps[2];
    CHECK(third.when.trigger == Trigger::status_file);
    CHECK(third.when.every == 3);
    CHECK(third.when.times == 4);

    const Step& fourth = envelope.steps[3];
    CHECK(fourth.when.trigger == Trigger::after_ms);
    CHECK(fourth.when.at == 20000);
    CHECK(fourth.when.every == 10);
    CHECK(trigger_name(Trigger::request) == "request");
    CHECK(trigger_name(Trigger::status_file) == "status_file");
    CHECK(trigger_name(Trigger::after_ms) == "after_ms");
}

TEST_CASE("scenario envelope: a file of the minimum, and of the limits", "[scenario][envelope]")
{
    CHECK(accepted(R"({"kind":"adl-mock-scenario","format":1,"steps":[]})").steps.empty());

    // The top of every range.
    const Envelope top = accepted(
        R"({"kind":"adl-mock-scenario","format":1,"seed":9007199254740991,"steps":[
            {"when":{"after_ms":86400000,"every":86400000,"times":1000000},"per_mille":1000,"do":{"action":"silent"}},
            {"when":{"request":1000000,"every":1000000},"do":{"action":"refuse"}}]})");
    REQUIRE(top.steps.size() == 2);
    CHECK(*top.seed == 9007199254740991ULL);
    // The bottom.
    const Envelope bottom = accepted(
        R"({"kind":"adl-mock-scenario","format":1,"seed":0,"steps":[
            {"when":{"after_ms":0,"every":10,"times":1},"per_mille":1,"do":{"action":"a"}},
            {"when":{"status_file":1,"every":1},"do":{"action":"x_9"}}]})");
    REQUIRE(bottom.steps.size() == 2);
    CHECK(*bottom.seed == 0);

    // 1000 steps are read, 1001 are not.
    std::string steps;
    for (std::size_t i = 0; i < max_steps; ++i)
    {
        steps += (i == 0 ? "" : ",") + ok_step;
    }
    CHECK(accepted(file(R"("steps":[)" + steps + "]")).steps.size() == max_steps);
    refused(file(R"("steps":[)" + steps + "," + ok_step + "]"), "there are more than 1000 steps");

    // Characters, not bytes, are counted: 64 two-byte characters are a name, 65 are not.
    std::string name;
    for (std::size_t i = 0; i < max_name_characters; ++i)
    {
        name += "\xc3\xa9";
    }
    CHECK(accepted(file(R"("name":")" + name + R"(","steps":[])")).name == name);
    refused(file(R"("name":")" + name + R"(e","steps":[])"),
            "\"name\" must be text of 1 to 64 characters, with no control character");
    CHECK(character_count("a\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80") == 4);
    CHECK(character_count("") == 0);
}

TEST_CASE("scenario envelope: a file that is not a scenario file", "[scenario][envelope]")
{
    refused("", not_json);
    refused("{", not_json);
    refused(R"({"kind":"adl-mock-scenario","kind":"adl-mock-scenario"})", not_json);
    refused(R"({"kind":"adl-mock-scenario",/* a note */"format":1,"steps":[]})", not_json);
    refused(R"({"kind":"adl-mock-scenario","format":1,"steps":[],})", not_json);
    refused("\xef\xbb\xbf{}", not_json);
    refused(R"({"kind":"adl-mock-scenario","format":1,"name":")" + std::string("\xc3\x28") + R"(","steps":[]})",
            not_json);
    refused("[]", "the file must be one JSON object");
    refused("12", "the file must be one JSON object");
    refused(std::string(max_file_size + 1, ' '), "the file is larger than 262144 bytes");
    // A file at the size limit is read, if it is a scenario.
    std::string padded = R"({"kind":"adl-mock-scenario","format":1,"steps":[]})";
    padded.resize(max_file_size, ' ');
    CHECK(accepted(padded).steps.empty());
}

TEST_CASE("scenario envelope: the kind and the format", "[scenario][envelope]")
{
    refused("{}", "\"kind\" is missing; it must be \"adl-mock-scenario\"");
    refused(R"({"kind":5})", "\"kind\" must be \"adl-mock-scenario\"");
    refused(R"({"kind":"adl-other","format":1})", "\"kind\" must be \"adl-mock-scenario\"");
    refused(R"({"kind":"adl-loader-scenario","format":1,"steps":[]})",
            "this is a scenario for a loader, not for a mock target (\"kind\" is \"adl-loader-scenario\")");
    refused(R"({"kind":"adl-mock-scenario","format":1,"steps":[]})",
            "this is a scenario for a mock target, not for a loader (\"kind\" is \"adl-mock-scenario\")", loader);
    // The right kind is read by its own family.
    CHECK(accepted(R"({"kind":"adl-loader-scenario","format":1,"steps":[]})", loader).steps.empty());

    refused(R"({"kind":"adl-mock-scenario"})", "\"format\" is missing; it must be 1");
    refused(R"({"kind":"adl-mock-scenario","format":"1"})", "\"format\" must be 1");
    refused(R"({"kind":"adl-mock-scenario","format":1.0})", "\"format\" must be 1");
    refused(R"({"kind":"adl-mock-scenario","format":0})", "\"format\" must be 1");
    refused(R"({"kind":"adl-mock-scenario","format":2,"steps":[]})",
            "\"format\" is 2; this version reads 1, so the file was made for a newer version");
    // A newer file is said to be newer, not reported for the keys it has that this version does not know.
    refused(R"({"kind":"adl-mock-scenario","format":3,"steps":[],"a_new_key":true})",
            "\"format\" is 3; this version reads 1, so the file was made for a newer version");
}

TEST_CASE("scenario envelope: the top level", "[scenario][envelope]")
{
    refused(file(R"("steps":[],"chanse":5)"), "the file has the key \"chanse\", which this version does not know");

    const std::string name_sentence = "\"name\" must be text of 1 to 64 characters, with no control character";
    refused(file(R"("name":"","steps":[])"), name_sentence);
    refused(file(R"("name":5,"steps":[])"), name_sentence);
    refused(file(R"("name":null,"steps":[])"), name_sentence);
    refused(file(R"("name":"a\nb","steps":[])"), name_sentence);
    refused(file(R"("name":"a\u007fb","steps":[])"), name_sentence);
    refused(file(R"("name":")" + std::string(65, 'a') + R"(","steps":[])"), name_sentence);
    CHECK(accepted(file(R"("name":")" + std::string(64, 'a') + R"(","steps":[])")).name.size() == 64);
    refused(file(R"("description":"","steps":[])"),
            "\"description\" must be text of 1 to 1000 characters, with no control character");
    refused(file(R"("description":")" + std::string(1001, 'a') + R"(","steps":[])"),
            "\"description\" must be text of 1 to 1000 characters, with no control character");
    CHECK(accepted(file(R"("description":")" + std::string(1000, 'a') + R"(","steps":[])")).description.size() == 1000);

    const std::string seed_sentence = "\"seed\" must be a whole number from 0 to 9007199254740991";
    refused(file(R"("seed":-1,"steps":[])"), seed_sentence);
    refused(file(R"("seed":9007199254740992,"steps":[])"), seed_sentence);
    refused(file(R"("seed":1.5,"steps":[])"), seed_sentence);
    refused(file(R"("seed":1e3,"steps":[])"), seed_sentence);
    refused(file(R"("seed":"7","steps":[])"), seed_sentence);
    refused(file(R"("seed":99999999999999999999,"steps":[])"), seed_sentence);
    CHECK(!accepted(file(R"("steps":[])")).seed.has_value());

    refused(R"({"kind":"adl-mock-scenario","format":1})", "\"steps\" is missing; it is a list of steps");
    refused(file(R"("steps":{})"), "\"steps\" must be a list");
    refused(file(R"("steps":"none")"), "\"steps\" must be a list");
}

TEST_CASE("scenario envelope: the shape of a step", "[scenario][envelope]")
{
    refused(file(R"("steps":[5])"), "step 1 must be an object");
    refused(file(R"("steps":[)" + ok_step + R"(,[]])"), "step 2 must be an object");
    refused(with_step(R"({"when":{"request":1},"do":{"action":"wait"},"chanse":5})"),
            "step 1 has the key \"chanse\", which this version does not know");
    // The key of a family is known to that family and to none other.
    refused(with_step(R"({"when":{"request":1},"on":{},"do":{"action":"wait"}})"),
            "step 1 has the key \"on\", which this version does not know", mock);
    CHECK(accepted(R"({"kind":"adl-loader-scenario","format":1,"steps":[
                        {"when":{"request":1},"on":{"datagram":"data"},"count":2,"do":{"action":"drop"}}]})",
                   loader)
              .steps[0]
              .rest.size() == 2);

    refused(with_step(R"({"do":{"action":"wait"}})"), "step 1: \"when\" is missing; it says when the step happens");
    refused(with_step(R"({"when":5,"do":{"action":"wait"}})"), "step 1: \"when\" must be an object");
    refused(with_step(R"({"when":{},"do":{"action":"wait"}})"),
            "step 1: \"when\" needs exactly one of \"request\", \"status_file\" and \"after_ms\"");
    refused(with_step(R"({"when":{"request":1,"status_file":2},"do":{"action":"wait"}})"),
            "step 1: \"when\" needs exactly one of \"request\", \"status_file\" and \"after_ms\"");
    refused(with_step(R"({"when":{"every":2},"do":{"action":"wait"}})"),
            "step 1: \"when\" needs exactly one of \"request\", \"status_file\" and \"after_ms\"");
    refused(with_step(R"({"when":{"request":1,"chanse":5},"do":{"action":"wait"}})"),
            "step 1: \"when\" has the key \"chanse\", which this version does not know");
    refused(with_step(R"({"when":{"request":1,"operation":"upload"},"do":{"action":"wait"}})"),
            "step 1: \"when\" has the key \"operation\", which this version does not know");
    CHECK(accepted(with_step(R"({"when":{"request":1,"operation":"upload"},"do":{"action":"wait"}})"), mock)
              .steps[0]
              .when.rest.size() == 1);

    refused(with_step(R"({"when":{"request":1}})"), "step 1: \"do\" is missing; it says what the step does");
    refused(with_step(R"({"when":{"request":1},"do":"wait"})"), "step 1: \"do\" must be an object");
    refused(with_step(R"({"when":{"request":1},"do":{}})"),
            "step 1: \"action\" is missing; it is text of 1 to 32 characters");
    refused(with_step(R"({"when":{"request":1},"do":{"action":5}})"),
            "step 1: \"action\" must be text of 1 to 32 characters, with no control character");
    refused(with_step(R"({"when":{"request":1},"do":{"action":"Wait"}})"),
            "step 1: \"action\" must be a word of lower-case letters, digits and \"_\"");
    refused(with_step(R"({"when":{"request":1},"do":{"action":"wait now"}})"),
            "step 1: \"action\" must be a word of lower-case letters, digits and \"_\"");
    refused(with_step(R"({"when":{"request":1},"do":{"action":")" + std::string(33, 'a') + R"("}})"),
            "step 1: \"action\" must be text of 1 to 32 characters, with no control character");
    CHECK(accepted(with_step(R"({"when":{"request":1},"do":{"action":")" + std::string(32, 'a') + R"("}})"))
              .steps.size() == 1);
}

TEST_CASE("scenario envelope: the numbers of a step", "[scenario][envelope]")
{
    const auto step = [](std::string_view when, std::string_view more = {})
    { return with_step(R"({"when":{)" + std::string{when} + "}," + std::string{more} + R"("do":{"action":"x"}})"); };

    refused(step(R"("request":0)"), "step 1: \"request\" must be a whole number from 1 to 1000000");
    refused(step(R"("request":1000001)"), "step 1: \"request\" must be a whole number from 1 to 1000000");
    refused(step(R"("request":"1")"), "step 1: \"request\" must be a whole number from 1 to 1000000");
    refused(step(R"("request":1.5)"), "step 1: \"request\" must be a whole number from 1 to 1000000");
    refused(step(R"("status_file":0)"), "step 1: \"status_file\" must be a whole number from 1 to 1000000");
    refused(step(R"("after_ms":-1)"), "step 1: \"after_ms\" must be a whole number from 0 to 86400000");
    refused(step(R"("after_ms":86400001)"), "step 1: \"after_ms\" must be a whole number from 0 to 86400000");

    refused(step(R"("request":1,"every":0)"), "step 1: \"every\" must be a whole number from 1 to 1000000");
    refused(step(R"("status_file":1,"every":1000001)"), "step 1: \"every\" must be a whole number from 1 to 1000000");
    refused(step(R"("after_ms":1,"every":9)"), "step 1: \"every\" must be a whole number from 10 to 86400000");
    refused(step(R"("after_ms":1,"every":86400001)"), "step 1: \"every\" must be a whole number from 10 to 86400000");
    refused(step(R"("request":1,"times":0)"), "step 1: \"times\" must be a whole number from 1 to 1000000");
    refused(step(R"("request":1,"times":1000001)"), "step 1: \"times\" must be a whole number from 1 to 1000000");

    const auto chance = [&](std::string_view value)
    { return step(R"("request":1)", R"("per_mille":)" + std::string{value} + ","); };
    refused(chance("0"), "step 1: \"per_mille\" must be a whole number from 1 to 1000");
    refused(chance("1001"), "step 1: \"per_mille\" must be a whole number from 1 to 1000");
    refused(chance("0.5"), "step 1: \"per_mille\" must be a whole number from 1 to 1000");
    CHECK(accepted(chance("1000")).steps[0].per_mille == 1000);
    // A step that draws needs a seed, and a file with none is refused for it.
    refused(
        R"({"kind":"adl-mock-scenario","format":1,"steps":[{"when":{"request":1},"per_mille":10,"do":{"action":"x"}}]})",
        "step 1 uses \"per_mille\" but the scenario has no \"seed\"");
    refused(R"({"kind":"adl-mock-scenario","format":1,"steps":[)" + ok_step +
                R"(,{"when":{"request":1},"per_mille":10,"do":{"action":"x"}}]})",
            "step 2 uses \"per_mille\" but the scenario has no \"seed\"");

    // The comment.
    const std::string comment_sentence =
        "step 1: \"comment\" must be text of 1 to 200 characters, with no control character";
    refused(step(R"("request":1)", R"("comment":"",)"), comment_sentence);
    refused(step(R"("request":1)", R"("comment":5,)"), comment_sentence);
    refused(step(R"("request":1)", R"("comment":")" + std::string(201, 'a') + R"(",)"), comment_sentence);
    CHECK(
        accepted(step(R"("request":1)", R"("comment":")" + std::string(200, 'a') + R"(",)")).steps[0].comment.size() ==
        200);
}

TEST_CASE("scenario envelope: the sentences are about the right step", "[scenario][envelope]")
{
    refused(
        file(R"("seed":1,"steps":[)" + ok_step + "," + ok_step + R"(,{"when":{"after_ms":-5},"do":{"action":"x"}}])"),
        "step 3: \"after_ms\" must be a whole number from 0 to 86400000");
}

TEST_CASE("scenario envelope: a key that is not known is quoted on one line, as JSON writes it", "[scenario][envelope]")
{
    // What the fuzzer found (fuzz/regressions/envelope/step_key_with_new_line.json): a key with a new line in it was
    // copied into the sentence as it was, and the sentence was two lines.
    const auto one_line = [](std::string_view text, std::string_view sentence, const Family& family = plain)
    {
        refused(text, sentence, family);
        const auto envelope = read_envelope(text, family);
        REQUIRE(!envelope);
        CHECK(envelope.error().message.find('\n') == std::string::npos);
    };
    one_line(with_step(R"({"when":{"request":1},"do":{"action":"wait"},"wh\n":5})"),
             R"(step 1 has the key "wh\n", which this version does not know)");
    one_line(file(R"("steps":[],"a\nb":5)"), R"(the file has the key "a\nb", which this version does not know)");
    one_line(with_step(R"({"when":{"request":1,"x\ny":5},"do":{"action":"wait"}})"),
             R"(step 1: "when" has the key "x\ny", which this version does not know)");

    // The other control characters, a quote and a backslash are written the way JSON writes them.
    one_line(with_step(R"({"when":{"request":1},"do":{"action":"wait"},"a\tb\rc\u0001d\u007fe":5})"),
             R"(step 1 has the key "a\tb\rc\u0001d\u007fe", which this version does not know)");
    one_line(with_step(R"({"when":{"request":1},"do":{"action":"wait"},"a\"b\\c":5})"),
             R"(step 1 has the key "a\"b\\c", which this version does not know)");
    // A key that is not ASCII is shown as it is.
    one_line(with_step(R"({"when":{"request":1},"do":{"action":"wait"},"café":5})"),
             "step 1 has the key \"caf\xC3\xA9\", which this version does not know");

    // A long key is cut after 64 characters, not in the middle of one.
    const std::string sixty_four(64, 'k');
    one_line(with_step(R"({"when":{"request":1},"do":{"action":"wait"},")" + sixty_four + R"(":5})"),
             "step 1 has the key \"" + sixty_four + "\", which this version does not know");
    one_line(with_step(R"({"when":{"request":1},"do":{"action":"wait"},")" + sixty_four + R"(k":5})"),
             "step 1 has the key \"" + sixty_four + "...\", which this version does not know");
    std::string accents_in_json;
    std::string accents_cut;
    for (std::size_t i = 0; i < 65; ++i)
    {
        accents_in_json += R"(é)";
        accents_cut += i < 64 ? "\xC3\xA9" : "";
    }
    one_line(with_step(R"({"when":{"request":1},"do":{"action":"wait"},")" + accents_in_json + R"(":5})"),
             "step 1 has the key \"" + accents_cut + "...\", which this version does not know");
    one_line(with_step(R"({"when":{"request":1},"do":{"action":"wait"},")" + std::string(100000, 'k') + R"(":5})"),
             "step 1 has the key \"" + sixty_four + "...\", which this version does not know");
}

TEST_CASE("scenario envelope: what a kind needs to read its own keys", "[scenario][envelope]")
{
    const auto parsed =
        adl::scenario::json::parse_strict(R"({"seconds":7,"ms":"x","text":"abc","empty":"","big":300,"nothing":null})");
    REQUIRE(parsed);
    const json::Object& object = *parsed->as_object();

    CHECK(*whole_number(object, "seconds", 1, 65535, 3) == 7);
    CHECK(whole_number(object, "seconds", 8, 65535, 3).error().message ==
          "step 3: \"seconds\" must be a whole number from 8 to 65535");
    CHECK(whole_number(object, "ms", 1, 10, 3).error().message == "step 3: \"ms\" must be a whole number from 1 to 10");
    CHECK(whole_number(object, "absent", 1, 10, 3).error().message ==
          "step 3: \"absent\" is missing; it is a whole number from 1 to 10");
    CHECK(whole_number(object, "absent", 1, 10, 0).error().message ==
          "\"absent\" is missing; it is a whole number from 1 to 10");
    CHECK(whole_number(object, "big", 1, 255, 0).error().message == "\"big\" must be a whole number from 1 to 255");

    CHECK(!optional_whole_number(object, "absent", 1, 10, 3)->has_value());
    CHECK(**optional_whole_number(object, "seconds", 1, 10, 3) == 7);
    CHECK(!optional_whole_number(object, "big", 1, 10, 3));
    CHECK(!optional_whole_number(object, "nothing", 1, 10, 3));

    CHECK(*text(object, "text", 3, 1) == "abc");
    CHECK(text(object, "text", 2, 1).error().message ==
          "step 1: \"text\" must be text of 1 to 2 characters, with no control character");
    CHECK(!text(object, "empty", 5, 1));
    CHECK(!text(object, "seconds", 5, 1));
    CHECK(text(object, "absent", 5, 0).error().message == "\"absent\" is missing; it is text of 1 to 5 characters");
    CHECK(!optional_text(object, "absent", 5, 1)->has_value());
    CHECK(**optional_text(object, "text", 5, 1) == "abc");
    CHECK(!optional_text(object, "empty", 5, 1));

    constexpr std::array<std::string_view, 3> allowed{"seconds", "ms", "text"};
    CHECK(only_keys(object, allowed, 2, "do")->message ==
          "step 2: \"do\" has the key \"empty\", which this version does not know");
    CHECK(only_keys(object, allowed, 2)->message == "step 2 has the key \"empty\", which this version does not know");
    CHECK(only_keys(object, allowed, 0)->message == "the file has the key \"empty\", which this version does not know");
    constexpr std::array<std::string_view, 6> all{"seconds", "ms", "text", "empty", "big", "nothing"};
    CHECK(!only_keys(object, all, 2).has_value());

    // The keys of a file are quoted as JSON writes them, whichever part the sentence is about.
    const auto odd = adl::scenario::json::parse_strict(R"({"a\nb":1})");
    REQUIRE(odd);
    CHECK(only_keys(*odd->as_object(), allowed, 2, "do")->message ==
          R"(step 2: "do" has the key "a\nb", which this version does not know)");
    CHECK(only_keys(*odd->as_object(), allowed, 0)->message ==
          R"(the file has the key "a\nb", which this version does not know)");

    CHECK(missing_seed(4, "min_ms").message == "step 4 uses \"min_ms\" but the scenario has no \"seed\"");
    CHECK(not_taken(5, "code", "\"0x1000\" or \"0x1002\"").message ==
          "step 5: \"code\" must be \"0x1000\" or \"0x1002\"");
}

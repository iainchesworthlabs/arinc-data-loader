// SPDX-License-Identifier: GPL-3.0-or-later
// The events adl_load writes: every kind, as JSON lines and as plain lines, and the JSON writer under them. The
// JSON a test reads back is read by the strict reader in tests/support, so a line that is not valid JSON, or
// whose keys are not in the order the contract gives, fails here.

#include "adl/load/events.hpp"
#include "adl/load/json_line.hpp"
#include "mini_json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <expected>
#include <sstream>
#include <string>
#include <vector>

using namespace adl::load;
using adl::loader_core::RecordedFile;
using adl::loader_core::RecordedHardware;
using adl::loader_core::RecordedLoad;
using adl::loader_core::RecordedPart;
using adl::loader_core::Severity;
using adl::test::Json;

namespace
{

// A clock that is where a test put it.
class SetClock final : public adl::platform::Clock
{
public:
    adl::platform::Instant now() noexcept override
    {
        return now_;
    }
    void sleep_for(adl::platform::Duration d) noexcept override
    {
        now_ = now_ + d;
    }
    adl::platform::Result<std::int64_t> wall_time_seconds() noexcept override
    {
        return std::unexpected(adl::platform::Error{adl::platform::ErrorCode::unsupported, 0});
    }

    adl::platform::Instant now_{};
};

struct Rig
{
    explicit Rig(bool json) : writer(out, json, clock, adl::platform::Instant{std::chrono::seconds(5)})
    {
        clock.now_ = adl::platform::Instant{std::chrono::seconds(5)};
    }

    // One event, as the one line it was written as (with its line feed).
    std::string line()
    {
        std::string text = out.str();
        out.str("");
        return text;
    }
    // One JSON event, read; the line has to be one valid JSON object with `event` and `t` first.
    Json event()
    {
        const std::string text = line();
        REQUIRE_FALSE(text.empty());
        REQUIRE(text.back() == '\n');
        REQUIRE(text.find('\n') == text.size() - 1);
        const auto parsed = adl::test::parse_json(text);
        REQUIRE(parsed.has_value());
        REQUIRE(parsed->is(Json::Kind::object));
        REQUIRE(parsed->members.size() >= 2);
        CHECK(parsed->members[0].first == "event");
        CHECK(parsed->members[1].first == "t");
        return *parsed;
    }

    std::ostringstream out;
    SetClock clock;
    EventWriter writer;
};

std::vector<std::string> keys_after_t(const Json& event)
{
    std::vector<std::string> keys = event.keys();
    keys.erase(keys.begin(), keys.begin() + 2);
    return keys;
}

using Keys = std::vector<std::string>;

} // namespace

TEST_CASE("JSON text is escaped, and bytes that are not UTF-8 are each one replacement character", "[load][events]")
{
    const auto escaped = [](std::string_view text)
    {
        std::string out;
        append_json_escaped(out, text);
        return out;
    };
    CHECK(escaped("plain") == "plain");
    CHECK(escaped("a\"b\\c") == "a\\\"b\\\\c");
    CHECK(escaped("line\nfeed\r\ttab") == "line\\nfeed\\r\\ttab");
    CHECK(escaped("\b\f") == "\\b\\f");
    CHECK(escaped(std::string{"\x01\x1f", 2}) == "\\u0001\\u001f");
    CHECK(escaped(std::string{"\0", 1}) == "\\u0000");
    CHECK(escaped("caf\xC3\xA9 \xE2\x82\xAC \xF0\x9F\x98\x80") == "caf\xC3\xA9 \xE2\x82\xAC \xF0\x9F\x98\x80");
    CHECK(escaped("\xFF") == "\xEF\xBF\xBD");
    CHECK(escaped("a\x80z") == "a\xEF\xBF\xBDz");
    CHECK(escaped("\xC3") == "\xEF\xBF\xBD");                                                 // cut short
    CHECK(escaped("\xC0\x80") == "\xEF\xBF\xBD\xEF\xBF\xBD");                                 // overlong
    CHECK(escaped("\xED\xA0\x80") == "\xEF\xBF\xBD\xEF\xBF\xBD\xEF\xBF\xBD");                 // a surrogate
    CHECK(escaped("\xF4\x90\x80\x80") == "\xEF\xBF\xBD\xEF\xBF\xBD\xEF\xBF\xBD\xEF\xBF\xBD"); // above U+10FFFF
    CHECK(escaped("\xE0\x80\x80") == "\xEF\xBF\xBD\xEF\xBF\xBD\xEF\xBF\xBD");
    CHECK(escaped("\xF0\x80\x80\x80") == "\xEF\xBF\xBD\xEF\xBF\xBD\xEF\xBF\xBD\xEF\xBF\xBD");
}

TEST_CASE("a JSON line has its keys in the order they were added, and nests", "[load][events]")
{
    JsonLine line;
    line.string("event", "x");
    line.milliseconds_as_seconds("t", 12'345);
    line.number("n", 7);
    line.boolean("yes", true);
    line.boolean("no", false);
    line.null("nothing");
    line.begin_array("list");
    line.string_element("a");
    line.string_element("b\n");
    line.begin_object();
    line.string("k", "v");
    line.end_object();
    line.end_array();
    line.begin_object("inner");
    line.begin_array("empty");
    line.end_array();
    line.end_object();
    CHECK(line.finish() == "{\"event\":\"x\",\"t\":12.345,\"n\":7,\"yes\":true,\"no\":false,\"nothing\":null,"
                           "\"list\":[\"a\",\"b\\n\",{\"k\":\"v\"}],\"inner\":{\"empty\":[]}}\n");

    JsonLine times;
    times.milliseconds_as_seconds("a", 0);
    times.milliseconds_as_seconds("b", 7);
    times.milliseconds_as_seconds("c", 1'000);
    times.milliseconds_as_seconds("d", 60'050);
    CHECK(times.finish() == "{\"a\":0.000,\"b\":0.007,\"c\":1.000,\"d\":60.050}\n");
}

TEST_CASE("the strict reader the tests use refuses what JSON refuses", "[load][events]")
{
    using adl::test::parse_json;
    CHECK(parse_json("{\"a\":[1,2.5,-3,true,false,null,\"x\\u00e9\\ud83d\\ude00\"]}").has_value());
    CHECK(parse_json("  {} ").has_value());
    CHECK_FALSE(parse_json("{\"a\":1,}").has_value());
    CHECK_FALSE(parse_json("[1,]").has_value());
    CHECK_FALSE(parse_json("{\"a\" 1}").has_value());
    CHECK_FALSE(parse_json("{a:1}").has_value());
    CHECK_FALSE(parse_json("{} x").has_value());
    CHECK_FALSE(parse_json("\"a\nb\"").has_value());
    CHECK_FALSE(parse_json("\"\\ud800\"").has_value());
    CHECK_FALSE(parse_json("\"\\x\"").has_value());
    CHECK_FALSE(parse_json("\"\xFF\"").has_value());
    CHECK_FALSE(parse_json("01").has_value());
    CHECK_FALSE(parse_json("1.").has_value());
    CHECK_FALSE(parse_json("").has_value());
    CHECK(adl::test::parse_json_lines("{\"a\":1}\r\n{\"b\":2}\n")[1].at("b").as_number() == 2);
    CHECK(adl::test::parse_json_lines("not json\n")[0].text == "not json");
}

TEST_CASE("started carries the program, its version, the events number and the command", "[load][events]")
{
    Rig rig(true);
    rig.writer.started("find", std::nullopt, std::nullopt);
    Json event = rig.event();
    CHECK(event.at("event").text == "started");
    CHECK(event.at("t").text == "0.000");
    CHECK(event.at("program").text == "adl_load");
    CHECK(event.at("version").text.size() > 0);
    CHECK(event.at("events").text == "1");
    CHECK(event.at("command").text == "find");
    CHECK(keys_after_t(event) == Keys{"program", "version", "events", "command"});

    rig.clock.sleep_for(std::chrono::milliseconds(1'234));
    rig.writer.started("upload", "10.0.0.2", "HNPFMS_L");
    event = rig.event();
    CHECK(event.at("t").text == "1.234");
    CHECK(keys_after_t(event) == Keys{"program", "version", "events", "command", "target", "position"});
    CHECK(event.at("target").text == "10.0.0.2");
    CHECK(event.at("position").text == "HNPFMS_L");
}

TEST_CASE("started says which scenario file the operation runs, how many steps it has and its seed", "[load][events]")
{
    Rig rig(true);
    rig.writer.started("upload", "10.0.0.2", "HNPFMS_L", ScenarioInfo{"faults/lost block.json", 3, 7});
    Json event = rig.event();
    CHECK(keys_after_t(event) == Keys{"program", "version", "events", "command", "target", "position", "scenario",
                                      "scenario_steps", "scenario_seed"});
    CHECK(event.at("scenario").text == "faults/lost block.json");
    CHECK(event.at("scenario_steps").as_number() == 3);
    CHECK(event.at("scenario_seed").as_number() == 7);

    // A file with no seed says null: a reader finds the key whenever there is a scenario.
    rig.writer.started("information", "10.0.0.2", "HNPFMS_L", ScenarioInfo{"f.json", 0, std::nullopt});
    event = rig.event();
    CHECK(event.at("scenario_steps").as_number() == 0);
    CHECK(event.at("scenario_seed").is(Json::Kind::null));

    // And no `scenario` key at all without the option.
    rig.writer.started("information", "10.0.0.2", "HNPFMS_L");
    event = rig.event();
    CHECK(keys_after_t(event) == Keys{"program", "version", "events", "command", "target", "position"});
}

TEST_CASE("scenario_step says what the loader did, with the keys of the action", "[load][events]")
{
    Rig rig(true);

    // A datagram dropped: no parameter.
    ScenarioStepEvent drop;
    drop.step = 2;
    drop.trigger = "request";
    drop.at = 3;
    drop.operation = "upload";
    drop.action = "drop";
    drop.datagram = "data";
    drop.file = "LOAD1.BIN";
    drop.block = 2;
    drop.to = "127.0.0.1:40001";
    rig.clock.sleep_for(std::chrono::milliseconds(250));
    rig.writer.scenario_step(drop);
    Json event = rig.event();
    CHECK(event.at("event").text == "scenario_step");
    CHECK(event.at("t").text == "0.250");
    CHECK(keys_after_t(event) ==
          Keys{"step", "trigger", "at", "operation", "action", "applied", "datagram", "file", "block", "to"});
    CHECK(event.at("step").as_number() == 2);
    CHECK(event.at("trigger").text == "request");
    CHECK(event.at("at").as_number() == 3);
    CHECK(event.at("operation").text == "upload");
    CHECK(event.at("action").text == "drop");
    CHECK(event.at("applied").boolean);
    CHECK(event.at("datagram").text == "data");
    CHECK(event.at("file").text == "LOAD1.BIN");
    CHECK(event.at("block").as_number() == 2);
    CHECK(event.at("to").text == "127.0.0.1:40001");

    // A request has no block, and a transfer with no file says null for both.
    ScenarioStepEvent wait;
    wait.step = 1;
    wait.trigger = "status_file";
    wait.at = 1;
    wait.operation = "information";
    wait.action = "wait";
    wait.datagram = "request";
    wait.to = "127.0.0.1:59";
    wait.parameter = "seconds";
    wait.value = 4;
    rig.writer.scenario_step(wait);
    event = rig.event();
    CHECK(keys_after_t(event) == Keys{"step", "trigger", "at", "operation", "action", "applied", "datagram", "file",
                                      "block", "to", "seconds"});
    CHECK(event.at("file").is(Json::Kind::null));
    CHECK(event.at("block").is(Json::Kind::null));
    CHECK(event.at("seconds").as_number() == 4);

    // A length that is shorter is a negative number.
    ScenarioStepEvent shorter = drop;
    shorter.action = "length";
    shorter.parameter = "delta";
    shorter.value = -3;
    rig.writer.scenario_step(shorter);
    event = rig.event();
    CHECK(event.at("delta").text == "-3");

    // The operation's own steps act on no datagram, and a silence with no end has a null length.
    ScenarioStepEvent silent;
    silent.step = 4;
    silent.trigger = "after_ms";
    silent.at = 20'000;
    silent.operation = "download_operator_defined";
    silent.action = "silent";
    silent.parameter = "for_ms";
    rig.writer.scenario_step(silent);
    event = rig.event();
    CHECK(keys_after_t(event) ==
          Keys{"step", "trigger", "at", "operation", "action", "applied", "datagram", "file", "block", "to", "for_ms"});
    CHECK(event.at("datagram").is(Json::Kind::null));
    CHECK(event.at("file").is(Json::Kind::null));
    CHECK(event.at("block").is(Json::Kind::null));
    CHECK(event.at("to").is(Json::Kind::null));
    CHECK(event.at("for_ms").is(Json::Kind::null));
    silent.value = 5000;
    rig.writer.scenario_step(silent);
    CHECK(rig.event().at("for_ms").as_number() == 5000);

    // The file name of a target's transfer may hold bytes that are not UTF-8: the line is still valid.
    ScenarioStepEvent odd = drop;
    odd.file = "A\xFF.BIN";
    rig.writer.scenario_step(odd);
    CHECK(rig.event().at("file").text == "A\xEF\xBF\xBD.BIN");
}

TEST_CASE("finished lists the steps of the scenario that never acted, when there was one", "[load][events]")
{
    Rig rig(true);

    FinishedEvent done;
    done.completed = true;
    done.reason = "completed";
    done.detail = "The operation completed.";
    done.scenario_unused = std::vector<std::size_t>{2, 5};
    rig.writer.finished(done);
    Json event = rig.event();
    CHECK(keys_after_t(event) == Keys{"result", "reason", "status_code", "detail", "exit_code", "scenario_unused"});
    REQUIRE(event.at("scenario_unused").elements.size() == 2);
    CHECK(event.at("scenario_unused").elements[0].as_number() == 2);
    CHECK(event.at("scenario_unused").elements[1].as_number() == 5);

    // Every step acted: the key is there, empty. Without a scenario it is not there.
    done.scenario_unused = std::vector<std::size_t>{};
    rig.writer.finished(done);
    event = rig.event();
    CHECK(event.at("scenario_unused").is(Json::Kind::array));
    CHECK(event.at("scenario_unused").elements.empty());
    done.scenario_unused.reset();
    rig.writer.finished(done);
    CHECK(keys_after_t(rig.event()) == Keys{"result", "reason", "status_code", "detail", "exit_code"});
}

TEST_CASE("target_found names the answer and the THW_ID_POS it makes", "[load][events]")
{
    Rig rig(true);
    rig.writer.target_found({"10.0.0.2", "HNPFMS", "FMS", "L", "FMS LEFT", "HNP", "HNPFMS_L"});
    const Json event = rig.event();
    CHECK(event.at("event").text == "target_found");
    CHECK(keys_after_t(event) ==
          Keys{"address", "hardware_id", "type_name", "position", "literal_name", "manufacturer_code", "thw_id_pos"});
    CHECK(event.at("address").text == "10.0.0.2");
    CHECK(event.at("hardware_id").text == "HNPFMS");
    CHECK(event.at("type_name").text == "FMS");
    CHECK(event.at("position").text == "L");
    CHECK(event.at("literal_name").text == "FMS LEFT");
    CHECK(event.at("manufacturer_code").text == "HNP");
    CHECK(event.at("thw_id_pos").text == "HNPFMS_L");
}

TEST_CASE("media_set_checked, log, configuration, progress, file_list and file_received", "[load][events]")
{
    Rig rig(true);

    rig.writer.media_set_checked({"sets/one", "PN-100", 2, 3, {"clause 1: odd", "clause 2: odder"}});
    Json event = rig.event();
    CHECK(keys_after_t(event) == Keys{"path", "part_number", "members", "loads", "findings"});
    CHECK(event.at("members").as_number() == 2);
    CHECK(event.at("loads").as_number() == 3);
    REQUIRE(event.at("findings").elements.size() == 2);
    CHECK(event.at("findings").elements[1].text == "clause 2: odder");
    rig.writer.media_set_checked({"sets/two", "PN", 1, 0, {}});
    CHECK(rig.event().at("findings").elements.empty());

    for (const Severity severity : {Severity::info, Severity::note, Severity::warning, Severity::error})
    {
        rig.writer.log(severity, "a \"line\"\n");
        event = rig.event();
        CHECK(event.at("event").text == "log");
        CHECK(keys_after_t(event) == Keys{"severity", "text"});
        CHECK(event.at("severity").text == to_string(severity));
        CHECK(event.at("text").text == "a \"line\"\n");
    }

    const std::vector<RecordedHardware> hardware{{"FMS LEFT", "SN1", {{"PN-1", "A", "Software"}, {"PN-2", "", "Data"}}},
                                                 {"NAV", "SN2", {}}};
    rig.writer.configuration(hardware);
    event = rig.event();
    CHECK(keys_after_t(event) == Keys{"hardware"});
    REQUIRE(event.at("hardware").elements.size() == 2);
    const Json& first = event.at("hardware").elements[0];
    CHECK(first.keys() == Keys{"literal_name", "serial_number", "parts"});
    REQUIRE(first.at("parts").elements.size() == 2);
    CHECK(first.at("parts").elements[0].keys() == Keys{"part_number", "amendment", "designation"});
    CHECK(first.at("parts").elements[1].at("amendment").text.empty());
    CHECK(event.at("hardware").elements[1].at("parts").elements.empty());

    const std::vector<RecordedLoad> loads{{"PN-1", "A.LUH", adl::arinc615a::StatusCode::in_progress, 40},
                                          {"PN-2", "B.LUH", adl::arinc615a::StatusCode::item_failed, 0}};
    rig.writer.progress(25, loads);
    event = rig.event();
    CHECK(keys_after_t(event) == Keys{"percent", "loads"});
    CHECK(event.at("percent").as_number() == 25);
    REQUIRE(event.at("loads").elements.size() == 2);
    CHECK(event.at("loads").elements[0].keys() == Keys{"part_number", "header_file", "status", "percent"});
    CHECK(event.at("loads").elements[0].at("status").text == "in_progress");
    CHECK(event.at("loads").elements[1].at("status").text == "item_failed");
    CHECK(event.at("loads").elements[0].at("percent").as_number() == 40);

    const std::vector<ListedFile> files{{"A.BIN", "the first"}, {"B.BIN", ""}};
    rig.writer.file_list(files);
    event = rig.event();
    CHECK(keys_after_t(event) == Keys{"files"});
    REQUIRE(event.at("files").elements.size() == 2);
    CHECK(event.at("files").elements[0].keys() == Keys{"name", "description"});
    CHECK(event.at("files").elements[0].at("description").text == "the first");

    rig.writer.file_received(RecordedFile{"A.BIN", 12'345, "out/DNLD_DATA_X_1/A.BIN"});
    event = rig.event();
    CHECK(keys_after_t(event) == Keys{"name", "size", "path"});
    CHECK(event.at("size").as_number() == 12'345);
    CHECK(event.at("path").text == "out/DNLD_DATA_X_1/A.BIN");
}

TEST_CASE("what a target sends that is not UTF-8 is still a valid line", "[load][events]")
{
    Rig rig(true);
    rig.writer.file_received(RecordedFile{"bad\xFF"
                                          "name\x01",
                                          1, "p"});
    const Json event = rig.event();
    CHECK(event.at("name").text == "bad\xEF\xBF\xBD"
                                   "name\x01");
}

TEST_CASE("finished says how it ended and carries the keys that apply", "[load][events]")
{
    Rig rig(true);

    FinishedEvent operation;
    operation.completed = true;
    operation.reason = "completed";
    operation.status_code = 3;
    operation.detail = "The operation completed.";
    operation.exit_code = 0;
    rig.writer.finished(operation);
    Json event = rig.event();
    CHECK(event.at("event").text == "finished");
    CHECK(keys_after_t(event) == Keys{"result", "reason", "status_code", "detail", "exit_code"});
    CHECK(event.at("result").text == "completed");
    CHECK(event.at("status_code").as_number() == 3);
    CHECK(event.at("exit_code").as_number() == 0);

    FinishedEvent find;
    find.completed = true;
    find.reason = "completed";
    find.count = 0;
    rig.writer.finished(find);
    event = rig.event();
    CHECK(keys_after_t(event) == Keys{"result", "reason", "status_code", "detail", "exit_code", "count"});
    CHECK(event.at("status_code").is(Json::Kind::null));
    CHECK(event.at("count").as_number() == 0);

    FinishedEvent download;
    download.reason = "missing_files";
    download.status_code = 4096;
    download.exit_code = 1;
    download.is_download = true;
    download.has_record = true;
    rig.writer.finished(download);
    event = rig.event();
    CHECK(keys_after_t(event) ==
          Keys{"result", "reason", "status_code", "detail", "exit_code", "download_folder", "record"});
    CHECK(event.at("result").text == "failed");
    CHECK(event.at("status_code").as_number() == 4096);
    CHECK(event.at("download_folder").is(Json::Kind::null));
    CHECK(event.at("record").is(Json::Kind::null));

    download.download_folder = "out/DNLD_DATA_X_1";
    download.record = "rec.json";
    rig.writer.finished(download);
    event = rig.event();
    CHECK(event.at("download_folder").text == "out/DNLD_DATA_X_1");
    CHECK(event.at("record").text == "rec.json");
}

TEST_CASE("without --json each event is one plain line", "[load][events]")
{
    Rig rig(false);
    CHECK_FALSE(rig.writer.json());

    rig.writer.started("find", std::nullopt, std::nullopt);
    CHECK(rig.line().starts_with("adl_load "));
    rig.writer.started("upload", "10.0.0.2", "HNPFMS_L");
    const std::string started = rig.line();
    CHECK(started.find(": upload, target 10.0.0.2 (HNPFMS_L)\n") != std::string::npos);

    rig.writer.target_found({"10.0.0.2", "HNPFMS", "FMS", "L", "FMS LEFT", "HNP", "HNPFMS_L"});
    CHECK(rig.line() == "Found 10.0.0.2: HNPFMS_L (type FMS, \"FMS LEFT\", manufacturer HNP)\n");

    rig.writer.media_set_checked({"sets/one", "PN-100", 1, 2, {"clause 1: odd", "clause 2: odder"}});
    CHECK(rig.line() ==
          "Checked media set PN-100 in sets/one: 1 member, 2 loads, 2 findings: clause 1: odd; clause 2: odder\n");
    rig.writer.media_set_checked({"sets/one", "PN-100", 2, 1, {}});
    CHECK(rig.line() == "Checked media set PN-100 in sets/one: 2 members, 1 load, 0 findings\n");

    rig.writer.log(Severity::warning, "careful");
    CHECK(rig.line() == "warning: careful\n");

    rig.writer.configuration(std::vector<RecordedHardware>{
        {"FMS LEFT", "SN1", {{"PN-1", "A", "Software"}, {"PN-2", "", ""}}}, {"NAV", "SN2", {}}});
    CHECK(rig.line() == "Configuration: 2 hardware items; FMS LEFT (serial number SN1): PN-1 A, PN-2; NAV (serial "
                        "number SN2): no parts\n");

    rig.writer.progress(40, std::vector<RecordedLoad>{{"PN-1", "A.LUH", adl::arinc615a::StatusCode::in_progress, 40}});
    CHECK(rig.line() == "Progress: 40%; PN-1 in_progress 40%\n");

    rig.writer.file_list(std::vector<ListedFile>{{"A.BIN", ""}, {"B.BIN", ""}});
    CHECK(rig.line() == "The target lists 2 files: A.BIN, B.BIN\n");
    rig.writer.file_list(std::vector<ListedFile>{});
    CHECK(rig.line() == "The target lists 0 files\n");

    rig.writer.file_received(RecordedFile{"A.BIN", 1, "out/A.BIN"});
    CHECK(rig.line() == "Received A.BIN, 1 byte, in out/A.BIN\n");

    FinishedEvent done;
    done.completed = true;
    done.reason = "completed";
    done.detail = "The operation completed.";
    CHECK(rig.writer.json() == false);
    rig.writer.finished(done);
    CHECK(rig.line() == "Finished: completed (exit code 0): The operation completed.\n");

    FinishedEvent failed;
    failed.reason = "no_answer";
    failed.exit_code = 4;
    failed.detail = "The target did not answer.";
    failed.is_download = true;
    failed.download_folder = "out/DNLD_DATA_X_1";
    failed.has_record = true;
    failed.record = "r.json";
    failed.count = 2;
    rig.writer.finished(failed);
    CHECK(rig.line() == "Finished: failed, no_answer (exit code 4): The target did not answer. [2 targets] "
                        "[files in out/DNLD_DATA_X_1] [record in r.json]\n");
}

TEST_CASE("the scenario in plain lines: the file, each step, and the steps that never acted", "[load][events]")
{
    Rig rig(false);
    rig.writer.started("upload", "10.0.0.2", "HNPFMS_L", ScenarioInfo{"faults.json", 3, 7});
    CHECK(rig.line().ends_with(": upload, target 10.0.0.2 (HNPFMS_L), scenario faults.json (3 steps, seed 7)\n"));
    rig.writer.started("information", "10.0.0.2", "HNPFMS_L", ScenarioInfo{"f.json", 1, std::nullopt});
    CHECK(rig.line().ends_with(": information, target 10.0.0.2 (HNPFMS_L), scenario f.json (1 step)\n"));

    ScenarioStepEvent step;
    step.step = 2;
    step.trigger = "request";
    step.at = 3;
    step.operation = "upload";
    step.action = "drop";
    step.datagram = "data";
    step.file = "LOAD1.BIN";
    step.block = 2;
    step.to = "127.0.0.1:40001";
    rig.writer.scenario_step(step);
    CHECK(rig.line() == "Scenario step 2: drop, data datagram of LOAD1.BIN block 2 to 127.0.0.1:40001 (request 3, "
                        "upload)\n");
    step.action = "duplicate";
    step.parameter = "copies";
    step.value = 3;
    step.block.reset();
    step.file = {};
    rig.writer.scenario_step(step);
    CHECK(rig.line() == "Scenario step 2: duplicate copies 3, data datagram to 127.0.0.1:40001 (request 3, upload)\n");
    ScenarioStepEvent silent;
    silent.step = 1;
    silent.trigger = "after_ms";
    silent.at = 500;
    silent.operation = "information";
    silent.action = "silent";
    silent.parameter = "for_ms";
    rig.writer.scenario_step(silent);
    CHECK(rig.line() == "Scenario step 1: silent for_ms none (after_ms 500, information)\n");

    FinishedEvent done;
    done.completed = true;
    done.reason = "completed";
    done.detail = "The operation completed.";
    done.scenario_unused = std::vector<std::size_t>{};
    rig.writer.finished(done);
    CHECK(rig.line() == "Finished: completed (exit code 0): The operation completed.\n");
    done.scenario_unused = std::vector<std::size_t>{2, 5};
    rig.writer.finished(done);
    CHECK(rig.line() == "Finished: completed (exit code 0): The operation completed. [scenario steps that never "
                        "acted: 2, 5]\n");
}

TEST_CASE("a plain line shows a control character a target sent as a question mark", "[load][events]")
{
    Rig rig(false);
    rig.writer.file_received(RecordedFile{"a\nb\x1b[31mc\x7f", 1, "p"});
    const std::string line = rig.line();
    CHECK(line == "Received a?b?[31mc?, 1 byte, in p\n");
    CHECK(std::count(line.begin(), line.end(), '\n') == 1);

    rig.writer.log(Severity::info, "two\xC2\x9b"
                                   "bytes");
    CHECK(rig.line() == "info: two?bytes\n");
    rig.writer.log(Severity::info, "caf\xC3\xA9 \xC2\xA0 kept");
    CHECK(rig.line() == "info: caf\xC3\xA9 \xC2\xA0 kept\n");
}

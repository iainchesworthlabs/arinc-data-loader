// SPDX-License-Identifier: GPL-3.0-or-later
// The events of adl_mock as lines: the JSON a program reads and the words a person reads.

#include "adl/mock/cli/events.hpp"
#include "json_read.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <string>
#include <vector>

using namespace adl::mock::cli;
using adl::arinc615a::StatusCode;
using adl::test::JsonReader;
using namespace std::chrono_literals;

namespace
{

[[nodiscard]] Event started_event()
{
    return Event{2ms,
                 Started{"adl_mock", "0.1.1", "HNPFMS_L", "127.0.0.1", 1001, 59, {"--silent", "--wrong-checksum"}}};
}

} // namespace

TEST_CASE("the started event carries what a reader needs to know the format", "[mock_cli][events]")
{
    CHECK(to_json_line(started_event()) ==
          R"({"event":"started","t":0.002,"program":"adl_mock","version":"0.1.1","events":1,"thw_id_pos":"HNPFMS_L",)"
          R"("address":"127.0.0.1","find_port":1001,"tftp_port":59,"faults":["--silent","--wrong-checksum"]})");
}

TEST_CASE("each event is one JSON object with event first and t second", "[mock_cli][events]")
{
    const std::vector<Event> events = {
        started_event(),
        Event{1250ms, OperationBegan{"information"}},
        Event{1300ms, Progress{"upload", 40}},
        Event{1400ms, UploadReceived{"ACM47-1", {"LOAD.LUH", "DATA.BIN"}, std::string{"/store/ACM47-1"}}},
        Event{1500ms, OperationEnded{"upload", 3, "completed", false}},
        Event{2000ms, Stopped{"operations", 1}},
    };
    const std::vector<std::string_view> names = {"started",         "operation_began", "progress",
                                                 "upload_received", "operation_ended", "stopped"};
    REQUIRE(events.size() == names.size());
    for (std::size_t i = 0; i < events.size(); ++i)
    {
        INFO(names[i]);
        CHECK(event_name(events[i].data) == names[i]);
        const std::string line = to_json_line(events[i]);
        CHECK(line.find('\n') == std::string::npos);
        const auto json = JsonReader::read(line);
        REQUIRE(json);
        REQUIRE(json->is_object());
        REQUIRE(json->members().size() >= 2);
        CHECK(json->members()[0].name == "event");
        CHECK(json->members()[0].value.string() == names[i]);
        CHECK(json->members()[1].name == "t");
    }
}

TEST_CASE("the other events carry the keys of the contract", "[mock_cli][events]")
{
    CHECK(to_json_line(Event{1250ms, OperationBegan{"download_media_defined"}}) ==
          R"({"event":"operation_began","t":1.250,"operation":"download_media_defined"})");
    CHECK(to_json_line(Event{1300ms, Progress{"upload", 40}}) ==
          R"({"event":"progress","t":1.300,"operation":"upload","percent":40})");
    CHECK(
        to_json_line(
            Event{1400ms, UploadReceived{"ACM47-1", {"LOAD.LUH", "DATA.BIN"}, std::string{"/store/ACM47-1"}}}) ==
        R"({"event":"upload_received","t":1.400,"part_number":"ACM47-1","files":["LOAD.LUH","DATA.BIN"],"stored_in":"/store/ACM47-1"})");
    CHECK(to_json_line(Event{1400ms, UploadReceived{"ACM47-1", {}, std::nullopt}}) ==
          R"({"event":"upload_received","t":1.400,"part_number":"ACM47-1","files":[],"stored_in":null})");
    CHECK(
        to_json_line(Event{1500ms, OperationEnded{"upload", 3, "completed", false}}) ==
        R"({"event":"operation_ended","t":1.500,"operation":"upload","status":3,"status_name":"completed","refused":false})");
    CHECK(to_json_line(Event{2000ms, Stopped{"operations", 1}}) ==
          R"({"event":"stopped","t":2.000,"reason":"operations","operations":1})");
}

TEST_CASE("an operation with no status has a null status", "[mock_cli][events]")
{
    CHECK(to_json_line(Event{
              900ms, OperationEnded{"information", std::nullopt, std::string{invalid_initialization_file}, true}}) ==
          R"({"event":"operation_ended","t":0.900,"operation":"information","status":null,)"
          R"("status_name":"invalid_initialization_file","refused":true})");
}

TEST_CASE("a started event with no fault options has an empty list", "[mock_cli][events]")
{
    Event event = started_event();
    std::get<Started>(event.data).faults.clear();
    const auto json = JsonReader::read(to_json_line(event));
    REQUIRE(json);
    CHECK(json->at("faults").items().empty());
}

TEST_CASE("names that came off the wire are escaped in the JSON and the line stays one line", "[mock_cli][events]")
{
    const std::string part_number = std::string("P\"1\\\n\x01", 6);
    const std::string file = std::string("A\xFFZ", 3);
    const Event event{10ms, UploadReceived{part_number, {file}, std::string{"C:\\data\\x y"}}};
    const std::string line = to_json_line(event);
    CHECK(line.find('\n') == std::string::npos);
    const auto json = JsonReader::read(line);
    REQUIRE(json);
    CHECK(json->at("part_number").string() == part_number);
    REQUIRE(json->at("files").items().size() == 1);
    CHECK(json->at("files").items()[0].string() == "A\xEF\xBF\xBDZ");
    CHECK(json->at("stored_in").string() == "C:\\data\\x y");
}

TEST_CASE("the plain line is the time, the event and its keys", "[mock_cli][events]")
{
    CHECK(to_plain_line(started_event()) ==
          "0.002 started program=adl_mock version=0.1.1 events=1 thw_id_pos=HNPFMS_L address=127.0.0.1 find_port=1001 "
          "tftp_port=59 faults=--silent,--wrong-checksum");
    Event event = started_event();
    std::get<Started>(event.data).faults.clear();
    CHECK(to_plain_line(event).ends_with(" faults=none"));
    CHECK(to_plain_line(Event{1250ms, OperationBegan{"information"}}) == "1.250 operation_began operation=information");
    CHECK(to_plain_line(Event{1300ms, Progress{"upload", 40}}) == "1.300 progress operation=upload percent=40");
    CHECK(to_plain_line(Event{1500ms, OperationEnded{"upload", 3, "completed", false}}) ==
          "1.500 operation_ended operation=upload status=3 status_name=completed refused=false");
    CHECK(to_plain_line(Event{
              900ms, OperationEnded{"information", std::nullopt, std::string{invalid_initialization_file}, true}}) ==
          "0.900 operation_ended operation=information status=none status_name=invalid_initialization_file "
          "refused=true");
    CHECK(to_plain_line(Event{2000ms, Stopped{"seconds", 0}}) == "2.000 stopped reason=seconds operations=0");
}

TEST_CASE("a value with a space or a quote in a plain line is written as a JSON string", "[mock_cli][events]")
{
    CHECK(to_plain_line(Event{1ms, UploadReceived{"P 1", {"A.BIN", "B.BIN"}, std::string{"C:\\my files\\x"}}}) ==
          R"(0.001 upload_received part_number="P 1" files=A.BIN,B.BIN stored_in="C:\\my files\\x")");
    CHECK(to_plain_line(Event{1ms, UploadReceived{"", {}, std::nullopt}}) ==
          R"(0.001 upload_received part_number="" files="" stored_in=none)");
    // The line is still one line.
    CHECK(to_plain_line(Event{1ms, UploadReceived{"a\nb", {}, std::nullopt}}).find('\n') == std::string::npos);
}

TEST_CASE("the engine's names for operations become one word each", "[mock_cli][events]")
{
    CHECK(operation_id("Information") == "information");
    CHECK(operation_id("Upload") == "upload");
    CHECK(operation_id("Download (media defined)") == "download_media_defined");
    CHECK(operation_id("Download (operator defined)") == "download_operator_defined");
    CHECK(operation_id("") == "");
    CHECK(operation_id("  odd -- name!  ") == "odd_name");
}

TEST_CASE("every status code has a name that is the name of its enumerator", "[mock_cli][events]")
{
    CHECK(status_name(StatusCode::accepted) == "accepted");
    CHECK(status_name(StatusCode::in_progress) == "in_progress");
    CHECK(status_name(StatusCode::completed) == "completed");
    CHECK(status_name(StatusCode::in_progress_with_text) == "in_progress_with_text");
    CHECK(status_name(StatusCode::operation_denied) == "operation_denied");
    CHECK(status_name(StatusCode::operation_not_supported) == "operation_not_supported");
    CHECK(status_name(StatusCode::aborted_by_target) == "aborted_by_target");
    CHECK(status_name(StatusCode::aborted_by_data_loader) == "aborted_by_data_loader");
    CHECK(status_name(StatusCode::aborted_by_operator) == "aborted_by_operator");
    CHECK(status_name(StatusCode::item_failed) == "item_failed");
    CHECK(status_name(static_cast<StatusCode>(0x7777)) == "unknown");
}

TEST_CASE("with a scenario, the started event says which, and the step and the end say what it did",
          "[mock_cli][events][scenario]")
{
    Event started = started_event();
    std::get<Started>(started.data).faults = {"--scenario"};
    std::get<Started>(started.data).scenario = ScenarioInfo{"rig/slow start.json", 4, 20261120};
    CHECK(to_json_line(started) ==
          R"({"event":"started","t":0.002,"program":"adl_mock","version":"0.1.1","events":1,"thw_id_pos":"HNPFMS_L",)"
          R"("address":"127.0.0.1","find_port":1001,"tftp_port":59,"faults":["--scenario"],)"
          R"("scenario":"rig/slow start.json","scenario_steps":4,"scenario_seed":20261120})");
    std::get<Started>(started.data).scenario = ScenarioInfo{"s.json", 0, std::nullopt};
    CHECK(to_json_line(started).ends_with(R"("scenario":"s.json","scenario_steps":0,"scenario_seed":null})"));

    // A step names its trigger and its action, and the number the action used (only that one).
    ScenarioStepFired wait{1, "request", 1, "upload", "wait", true, 2, std::nullopt, std::nullopt};
    CHECK(event_name(EventData{wait}) == "scenario_step");
    CHECK(to_json_line(Event{1250ms, wait}) ==
          R"({"event":"scenario_step","t":1.250,"step":1,"trigger":"request","at":1,"operation":"upload",)"
          R"("action":"wait","applied":true,"seconds":2})");
    CHECK(to_json_line(Event{1250ms, ScenarioStepFired{3, "after_ms", 20000, "information", "refuse", true,
                                                       std::nullopt, 4098, std::nullopt}}) ==
          R"({"event":"scenario_step","t":1.250,"step":3,"trigger":"after_ms","at":20000,"operation":"information",)"
          R"("action":"refuse","applied":true,"status":4098})");
    CHECK(to_json_line(Event{1250ms, ScenarioStepFired{2, "status_file", 4, "upload", "delay", false, std::nullopt,
                                                       std::nullopt, 611}}) ==
          R"({"event":"scenario_step","t":1.250,"step":2,"trigger":"status_file","at":4,"operation":"upload",)"
          R"("action":"delay","applied":false,"ms":611})");
    CHECK(to_json_line(Event{1250ms, ScenarioStepFired{2, "status_file", 4, "upload", "abort", true, std::nullopt,
                                                       std::nullopt, std::nullopt}}) ==
          R"({"event":"scenario_step","t":1.250,"step":2,"trigger":"status_file","at":4,"operation":"upload",)"
          R"("action":"abort","applied":true})");

    CHECK(to_json_line(Event{2000ms, Stopped{"seconds", 1, std::vector<std::size_t>{2, 5}}}) ==
          R"({"event":"stopped","t":2.000,"reason":"seconds","operations":1,"scenario_unused":[2,5]})");
    CHECK(to_json_line(Event{2000ms, Stopped{"seconds", 1, std::vector<std::size_t>{}}}) ==
          R"({"event":"stopped","t":2.000,"reason":"seconds","operations":1,"scenario_unused":[]})");
}

TEST_CASE("the started event lists the actions of a scenario this build knows, and the faults on a datagram are events",
          "[mock_cli][events][scenario]")
{
    Event started = started_event();
    // With no scenario: the list is there all the same, and there is no key for a file.
    std::get<Started>(started.data).faults = {};
    std::get<Started>(started.data).features = {"wait", "drop_datagram"};
    CHECK(to_json_line(started) ==
          R"({"event":"started","t":0.002,"program":"adl_mock","version":"0.1.1","events":1,"thw_id_pos":"HNPFMS_L",)"
          R"("address":"127.0.0.1","find_port":1001,"tftp_port":59,"faults":[],)"
          R"("scenario_features":["wait","drop_datagram"]})");
    std::get<Started>(started.data).faults = {"--scenario"};
    std::get<Started>(started.data).scenario = ScenarioInfo{"s.json", 2, 7};
    CHECK(to_json_line(started).ends_with(R"("faults":["--scenario"],"scenario_features":["wait","drop_datagram"],)"
                                          R"("scenario":"s.json","scenario_steps":2,"scenario_seed":7})"));
    CHECK(to_plain_line(started).ends_with(
        "faults=--scenario scenario_features=wait,drop_datagram scenario=s.json scenario_steps=2 scenario_seed=7"));

    // A step on a datagram says how many datagrams it is for, and which.
    ScenarioStepFired drop{1, "request", 1, "upload", "drop_datagram", true};
    drop.count = 3;
    drop.datagram = "ack";
    drop.file = ".LUS";
    drop.block = 2;
    CHECK(to_json_line(Event{1250ms, drop}) ==
          R"({"event":"scenario_step","t":1.250,"step":1,"trigger":"request","at":1,"operation":"upload",)"
          R"("action":"drop_datagram","applied":true,"count":3,"datagram":"ack","file":".LUS","block":2})");
    CHECK(to_plain_line(Event{1250ms, drop}) ==
          "1.250 scenario_step step=1 trigger=request at=1 operation=upload action=drop_datagram applied=true count=3 "
          "datagram=ack file=.LUS block=2");
    ScenarioStepFired duplicate{4, "status_file", 2, "upload", "duplicate_datagram", true};
    duplicate.copies = 3;
    duplicate.count = 1;
    CHECK(to_json_line(Event{1250ms, duplicate}) ==
          R"({"event":"scenario_step","t":1.250,"step":4,"trigger":"status_file","at":2,"operation":"upload",)"
          R"("action":"duplicate_datagram","applied":true,"copies":3,"count":1})");
    ScenarioStepFired reorder{5, "after_ms", 500, "information", "reorder_datagram", false};
    reorder.within_ms = 400;
    reorder.count = 2;
    CHECK(to_plain_line(Event{1250ms, reorder}) ==
          "1.250 scenario_step step=5 trigger=after_ms at=500 operation=information action=reorder_datagram "
          "applied=false within_ms=400 count=2");

    // A datagram that met a fault.
    ScenarioDatagram hit;
    hit.step = 2;
    hit.action = "drop_datagram";
    hit.applied = true;
    hit.datagram = "ack";
    hit.file = "APP.LUP";
    hit.block = 2;
    hit.to = "10.0.0.2:40001";
    hit.size = 4;
    CHECK(event_name(EventData{hit}) == "scenario_datagram");
    CHECK(to_json_line(Event{1250ms, hit}) ==
          R"({"event":"scenario_datagram","t":1.250,"step":2,"action":"drop_datagram","applied":true,)"
          R"("datagram":"ack","file":"APP.LUP","block":2,"to":"10.0.0.2:40001","size":4})");
    CHECK(to_plain_line(Event{1250ms, hit}) ==
          "1.250 scenario_datagram step=2 action=drop_datagram applied=true datagram=ack file=APP.LUP block=2 "
          "to=10.0.0.2:40001 size=4");
    hit.applied = false;
    hit.datagram = "option_ack";
    hit.file.reset();
    hit.block.reset();
    CHECK(to_json_line(Event{1250ms, hit}) ==
          R"({"event":"scenario_datagram","t":1.250,"step":2,"action":"drop_datagram","applied":false,)"
          R"("datagram":"option_ack","file":null,"block":null,"to":"10.0.0.2:40001","size":4})");
    CHECK(to_plain_line(Event{1250ms, hit}) ==
          "1.250 scenario_datagram step=2 action=drop_datagram applied=false datagram=option_ack file=none block=none "
          "to=10.0.0.2:40001 size=4");

    // At the end: the steps that never occurred, and the steps that were armed and met no datagram.
    Stopped stopped{"seconds", 1, std::vector<std::size_t>{2, 5}, std::vector<std::size_t>{1}};
    CHECK(to_json_line(Event{2000ms, stopped}) ==
          R"({"event":"stopped","t":2.000,"reason":"seconds","operations":1,"scenario_unused":[2,5],)"
          R"("scenario_idle":[1]})");
    CHECK(to_plain_line(Event{2000ms, stopped}) ==
          "2.000 stopped reason=seconds operations=1 scenario_unused=2,5 scenario_idle=1");
    stopped.scenario_idle = std::vector<std::size_t>{};
    CHECK(to_plain_line(Event{2000ms, stopped}).ends_with("scenario_unused=2,5 scenario_idle=none"));
}

TEST_CASE("the plain lines of a scenario's events have the same keys", "[mock_cli][events][scenario]")
{
    CHECK(to_plain_line(Event{
              1250ms, ScenarioStepFired{1, "request", 1, "upload", "wait", true, 2, std::nullopt, std::nullopt}}) ==
          "1.250 scenario_step step=1 trigger=request at=1 operation=upload action=wait applied=true seconds=2");
    CHECK(to_plain_line(Event{1250ms, ScenarioStepFired{2, "status_file", 4, "upload", "delay", false, std::nullopt,
                                                        std::nullopt, 611}}) ==
          "1.250 scenario_step step=2 trigger=status_file at=4 operation=upload action=delay applied=false ms=611");
    CHECK(to_plain_line(Event{2000ms, Stopped{"seconds", 1, std::vector<std::size_t>{2, 5}}}) ==
          "2.000 stopped reason=seconds operations=1 scenario_unused=2,5");
    CHECK(to_plain_line(Event{2000ms, Stopped{"seconds", 1, std::vector<std::size_t>{}}}) ==
          "2.000 stopped reason=seconds operations=1 scenario_unused=none");
    Event started = started_event();
    std::get<Started>(started.data).scenario = ScenarioInfo{"s.json", 3, std::nullopt};
    CHECK(to_plain_line(started).ends_with("scenario=s.json scenario_steps=3 scenario_seed=none"));
}

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
        CHECK(json->members()[0].first == "event");
        CHECK(json->members()[0].second.string() == names[i]);
        CHECK(json->members()[1].first == "t");
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

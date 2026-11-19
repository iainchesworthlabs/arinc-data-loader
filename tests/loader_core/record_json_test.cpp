// SPDX-License-Identifier: GPL-3.0-or-later
// The operation record as a file: one golden file for each kind of operation and one for text that needs
// escaping, compared byte for byte; the keys the writer writes against the keys of the page that documents
// them; and the cases of the format the golden files do not show (a time that is missing or out of range,
// bytes that are not UTF-8).

#include "adl/loader_core/record_json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using adl::arinc615a::OperationKind;
using adl::arinc615a::StatusCode;
using adl::loader_core::OperationRecord;
using adl::loader_core::record_to_json;
using adl::loader_core::RecordedFile;
using adl::loader_core::RecordedHardware;
using adl::loader_core::RecordedLoad;
using adl::loader_core::RecordedLogLine;
using adl::loader_core::RecordedPart;
using adl::loader_core::RecordTime;
using adl::loader_core::Severity;

namespace
{

// 2026-11-19T10:15:<second>Z
RecordTime at(int second)
{
    using namespace std::chrono;
    return sys_days{year{2026} / November / 19} + hours{10} + minutes{15} + seconds{second};
}

std::string read_file(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    INFO("file " << path.string());
    REQUIRE(in.is_open());
    return std::string{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

std::string golden(const char* name)
{
    return read_file(std::filesystem::path{ADL_LOADER_CORE_GOLDEN_DIR} / name);
}

// What every record of these tests has in common: the program, the target and the times.
OperationRecord base_record(OperationKind kind)
{
    OperationRecord record;
    record.program_name = "adl_load";
    record.program_version = "0.2.0";
    record.operation = kind;
    record.target_address = "10.0.0.2";
    record.target_position = "HNPFMS_L";
    record.started_at = at(30);
    record.ended_at = at(41);
    record.result = "completed";
    record.reason = "completed";
    record.status_code = StatusCode::completed;
    return record;
}

void log(OperationRecord& record, int second, Severity severity, std::string text)
{
    record.log.push_back(RecordedLogLine{at(second), severity, std::move(text)});
}

OperationRecord information_record()
{
    OperationRecord record = base_record(OperationKind::information);
    record.detail = "The operation completed.";
    record.configuration = std::vector<RecordedHardware>{
        {"FMS LEFT",
         "SN-0001",
         {RecordedPart{"ACM47-1234-5678", "", "FMS 1 OPS"}, RecordedPart{"ACM47-8765-4321", "A", "FMS 1 DATA"}}}};
    log(record, 30, Severity::info, "The target answered: accepted");
    log(record, 34, Severity::info, "Status 0: accepted");
    log(record, 35, Severity::info, "The target listed its configuration.");
    log(record, 41, Severity::info, "Status 1: completed");
    return record;
}

OperationRecord upload_record()
{
    OperationRecord record = base_record(OperationKind::upload);
    record.result = "failed";
    record.reason = "load_failed";
    record.detail = "The upload ended and the target failed 1 load.";
    record.media_set = adl::loader_core::RecordedMediaSet{"/srv/media/SAMPLE", "SAMPLE-MS-0001", 2};
    record.loads = {RecordedLoad{"ACM47-1111-0001", "FMS1.LUH", StatusCode::completed, 100},
                    RecordedLoad{"ACM47-2222-0002", "FMS2.LUH", StatusCode::item_failed, 40}};
    log(record, 30, Severity::info, "The target answered: accepted");
    log(record, 34, Severity::info, "Status 0: accepted, 0% of the list");
    log(record, 38, Severity::info, "Status 1: in progress, 55% of the list");
    log(record, 38, Severity::error, "The target failed load ACM47-2222-0002 (FMS2.LUH): no room for the data files");
    log(record, 41, Severity::info, "Status 2: completed, 100% of the list");
    return record;
}

OperationRecord download_media_record()
{
    OperationRecord record = base_record(OperationKind::download_media);
    record.result = "failed";
    record.reason = "missing_files";
    record.detail = "Files received: 2 of 3 requested.";
    const std::string folder = "C:\\Users\\operator\\ArincDownloads\\DNLD_DATA_HNPFMS_L_1";
    record.download_folder = folder;
    record.files = {RecordedFile{"A.BIN", 3000, folder + "\\A.BIN"}, RecordedFile{"B.BIN", 700, folder + "\\B.BIN"}};
    log(record, 30, Severity::info, "The target answered: accepted");
    log(record, 36, Severity::info, "Received A.BIN (3000 bytes)");
    log(record, 37, Severity::info, "Received B.BIN (700 bytes)");
    log(record, 41, Severity::note, "requested files missing (1)");
    log(record, 41, Severity::info, "Status 2: completed, 100% of the list");
    return record;
}

OperationRecord download_operator_record()
{
    OperationRecord record = base_record(OperationKind::download_operator);
    record.detail = "Files received: 1 of 1 requested.";
    record.download_folder = "/home/operator/DNLD_DATA_HNPFMS_L_2";
    record.files = {RecordedFile{"LOG.TXT", 1234, "/home/operator/DNLD_DATA_HNPFMS_L_2/LOG.TXT"}};
    log(record, 30, Severity::info, "The target answered: accepted");
    log(record, 34, Severity::info, "The target listed 2 file(s) it can download.");
    log(record, 38, Severity::info, "Received LOG.TXT (1234 bytes)");
    log(record, 41, Severity::info, "Status 1: completed, 100% of the list");
    return record;
}

// Text that has to be escaped, and bytes that are not UTF-8, in every kind of place a text can be.
OperationRecord escaping_record()
{
    OperationRecord record = base_record(OperationKind::information);
    record.program_name = "quote\" backslash\\ slash/ tab\t newline\n return\r bell\a unit\x1f backspace\b formfeed\f";
    // "Größe 日本語 😀": two-, three- and four-byte sequences, as the bytes they are.
    record.program_version = "Gr\xC3\xB6"
                             "\xC3\x9F"
                             "e \xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E \xF0\x9F\x98\x80";
    record.target_position = "H\xFF"
                             "NP"; // a byte that is no UTF-8 at all
    record.started_at = std::nullopt;
    record.result = "failed";
    record.reason = "local_error";
    record.status_code = std::nullopt;
    record.detail = "said \"no\" at C:\\temp\\x then \xE6\x97"; // ends inside a three-byte sequence
    // "dätei.bin"
    record.files = {
        RecordedFile{"d\xC3\xA4tei.bin", std::numeric_limits<std::uint64_t>::max(), "C:\\dl\\d\xC3\xA4tei.bin"}};
    record.log.push_back(RecordedLogLine{std::nullopt, Severity::warning, std::string{"a"} + '\0' + "b"});
    record.log.push_back(RecordedLogLine{at(41), Severity::error, "line one\nline two"});
    return record;
}

// A record with something in every list and every optional part, to see every key.
OperationRecord full_record()
{
    OperationRecord record = upload_record();
    record.configuration = information_record().configuration;
    record.files = download_media_record().files;
    record.download_folder = "/dl";
    return record;
}

// The key of each member of a record's JSON, in the order the file has them, as paths ("loads.header_file",
// "configuration.parts.part_number") with each path once. The file puts each member on a line of its own,
// indented two spaces for each level, which is all this reads.
std::vector<std::string> key_paths(const std::string& json)
{
    std::vector<std::pair<std::size_t, std::string>> open; // the indentation and path of the members now open
    std::vector<std::string> paths;
    std::size_t start = 0;
    while (start < json.size())
    {
        std::size_t end = json.find('\n', start);
        if (end == std::string::npos)
        {
            end = json.size();
        }
        const std::string_view line{json.data() + start, end - start};
        start = end + 1;

        const std::size_t indent = line.find_first_not_of(' ');
        if (indent == std::string_view::npos || line[indent] != '"')
        {
            continue; // a brace, a bracket or an empty line
        }
        const std::size_t close = line.find("\": ", indent);
        if (close == std::string_view::npos)
        {
            continue;
        }
        const std::string key{line.substr(indent + 1, close - indent - 1)};
        while (!open.empty() && open.back().first >= indent)
        {
            open.pop_back();
        }
        const std::string path = open.empty() ? key : open.back().second + "." + key;
        if (std::ranges::find(paths, path) == paths.end())
        {
            paths.push_back(path);
        }
        const std::string_view value = line.substr(close + 3);
        if (value == "{" || value == "[")
        {
            open.emplace_back(indent, path);
        }
    }
    return paths;
}

// The keys of the table under "The keys of the record" in the page that documents the file.
std::vector<std::string> keys_of_page()
{
    const std::string page = read_file(ADL_FILE_REFERENCE_PAGE);
    const std::size_t heading = page.find("\n### The keys of the record");
    REQUIRE(heading != std::string::npos);
    std::vector<std::string> keys;
    std::size_t start = page.find('\n', heading + 1);
    while (start != std::string::npos && start < page.size())
    {
        ++start;
        std::size_t end = page.find('\n', start);
        if (end == std::string::npos)
        {
            end = page.size();
        }
        std::string_view line{page.data() + start, end - start};
        if (!line.empty() && line.back() == '\r')
        {
            line.remove_suffix(1);
        }
        start = end;
        if (line.starts_with("| `"))
        {
            const std::size_t close = line.find('`', 3);
            REQUIRE(close != std::string_view::npos);
            keys.emplace_back(line.substr(3, close - 3));
        }
        else if (!keys.empty() && !line.starts_with("|"))
        {
            break; // the table is over
        }
    }
    return keys;
}

// The line of a record's JSON that is the member `key` at the first level.
std::string member_line(const std::string& json, std::string_view key)
{
    const std::string wanted = "  \"" + std::string{key} + "\": ";
    const std::size_t at_line = json.find("\n" + wanted);
    REQUIRE(at_line != std::string::npos);
    const std::size_t from = at_line + 1;
    return json.substr(from, json.find('\n', from) - from);
}

} // namespace

// ---- The golden files ----------------------------------------------------------------------------

TEST_CASE("an Information record is written as its golden file", "[loader_core][record_json]")
{
    CHECK(record_to_json(information_record()) == golden("information.json"));
}

TEST_CASE("an upload record is written as its golden file", "[loader_core][record_json]")
{
    CHECK(record_to_json(upload_record()) == golden("upload.json"));
}

TEST_CASE("a media defined download record is written as its golden file", "[loader_core][record_json]")
{
    CHECK(record_to_json(download_media_record()) == golden("download_media.json"));
}

TEST_CASE("an operator defined download record is written as its golden file", "[loader_core][record_json]")
{
    CHECK(record_to_json(download_operator_record()) == golden("download_operator.json"));
}

TEST_CASE("text that needs escaping is written as its golden file", "[loader_core][record_json]")
{
    CHECK(record_to_json(escaping_record()) == golden("escaping.json"));
}

TEST_CASE("the same record gives the same bytes", "[loader_core][record_json]")
{
    CHECK(record_to_json(full_record()) == record_to_json(full_record()));
}

TEST_CASE("the file has the layout the contract gives it", "[loader_core][record_json]")
{
    for (const OperationRecord& record : {OperationRecord{}, information_record(), full_record(), escaping_record()})
    {
        const std::string json = record_to_json(record);
        REQUIRE(json.size() > 3);
        CHECK(json.starts_with("{\n  \"format\": 1,\n  \"kind\": \"adl-operation-record\",\n"));
        CHECK(json.ends_with("\n}\n"));
        CHECK_FALSE(json.ends_with("\n\n"));
        CHECK(json.find('\r') == std::string::npos);
        CHECK(json.find('\t') == std::string::npos);
        // The licence is reserved and written as null, whatever the record holds.
        CHECK(member_line(json, "licence") == "  \"licence\": null,");
    }
}

// ---- The keys ------------------------------------------------------------------------------------

TEST_CASE("every record has every key, in the same order", "[loader_core][record_json]")
{
    const std::vector<std::string> every_key = key_paths(record_to_json(full_record()));
    // The top level of every record, whatever it holds, is the same members in the same order.
    const auto top_level = [](const std::vector<std::string>& paths)
    {
        std::vector<std::string> keys;
        for (const std::string& path : paths)
        {
            if (path.find('.') == std::string::npos)
            {
                keys.push_back(path);
            }
        }
        return keys;
    };
    const std::vector<std::string> expected = top_level(every_key);
    CHECK(expected == std::vector<std::string>{"format", "kind", "program", "operation", "target", "started_at",
                                               "ended_at", "result", "reason", "status_code", "detail", "media_set",
                                               "loads", "configuration", "files", "download_folder", "licence", "log"});
    for (const OperationRecord& record : {OperationRecord{}, information_record(), upload_record(),
                                          download_media_record(), download_operator_record(), escaping_record()})
    {
        CHECK(top_level(key_paths(record_to_json(record))) == expected);
    }
}

TEST_CASE("the keys the writer writes are the keys of the page that documents them", "[loader_core][record_json]")
{
    const std::vector<std::string> written = key_paths(record_to_json(full_record()));
    const std::vector<std::string> documented = keys_of_page();
    CHECK(documented == written);
}

// ---- Times and text ----------------------------------------------------------------------------------

TEST_CASE("a time is written in UTC to the second, or as null", "[loader_core][record_json]")
{
    using namespace std::chrono;
    const auto started = [](RecordTime time)
    {
        OperationRecord record;
        record.started_at = time;
        return member_line(record_to_json(record), "started_at");
    };
    // The day as seconds first: the sum of days, hours and minutes in their own, narrower, counts would
    // overflow for the year 9999.
    const auto on = [](year_month_day date) { return sys_seconds{sys_days{date}}; };
    CHECK(started(sys_seconds{}) == "  \"started_at\": \"1970-01-01T00:00:00Z\",");
    CHECK(started(on(year{2024} / February / 29) + hours{23} + minutes{59} + seconds{59}) ==
          "  \"started_at\": \"2024-02-29T23:59:59Z\",");
    CHECK(started(on(year{2026} / November / 19) + hours{1} + minutes{2} + seconds{3}) ==
          "  \"started_at\": \"2026-11-19T01:02:03Z\",");
    CHECK(started(on(year{9999} / December / 31) + hours{23} + minutes{59} + seconds{59}) ==
          "  \"started_at\": \"9999-12-31T23:59:59Z\",");
    CHECK(started(on(year{0} / January / 1)) == "  \"started_at\": \"0000-01-01T00:00:00Z\",");
    // A moment before the year 0 or after the year 9999 has no place in the format; a clock that has gone
    // wrong is no reason to write a date that no reader would take.
    CHECK(started(on(year{9999} / December / 31) + days{1}) == "  \"started_at\": null,");
    CHECK(started(on(year{0} / January / 1) - seconds{1}) == "  \"started_at\": null,");
    CHECK(started(sys_seconds{seconds{std::numeric_limits<std::int64_t>::max()}}) == "  \"started_at\": null,");
    CHECK(started(sys_seconds{seconds{std::numeric_limits<std::int64_t>::min()}}) == "  \"started_at\": null,");
    CHECK(started(std::nullopt) == "  \"started_at\": null,");
}

TEST_CASE("the status of a load is written as the name of the status, not as a sentence", "[loader_core][record_json]")
{
    struct Case
    {
        StatusCode status;
        std::string name;
    };
    const std::vector<Case> cases{{StatusCode::accepted, "accepted"},
                                  {StatusCode::in_progress, "in_progress"},
                                  {StatusCode::completed, "completed"},
                                  {StatusCode::in_progress_with_text, "in_progress_with_text"},
                                  {StatusCode::operation_denied, "operation_denied"},
                                  {StatusCode::operation_not_supported, "operation_not_supported"},
                                  {StatusCode::aborted_by_target, "aborted_by_target"},
                                  {StatusCode::aborted_by_data_loader, "aborted_by_data_loader"},
                                  {StatusCode::aborted_by_operator, "aborted_by_operator"},
                                  {StatusCode::item_failed, "item_failed"}};
    for (const Case& scenario : cases)
    {
        OperationRecord record;
        record.loads = {RecordedLoad{"ACM47-1111-0001", "FMS1.LUH", scenario.status, 7}};
        CAPTURE(scenario.name);
        CHECK(record_to_json(record).contains("\n      \"status\": \"" + scenario.name + "\",\n"));
    }
}

TEST_CASE("bytes that are not UTF-8 are each written as the replacement character", "[loader_core][record_json]")
{
    struct Case
    {
        std::string text;
        std::string written;
    };
    const std::string replacement = "\xEF\xBF\xBD";
    const std::vector<Case> cases{
        {"plain", "plain"},
        {"\xC3\xA9", "\xC3\xA9"},                 // two bytes
        {"\xE2\x82\xAC", "\xE2\x82\xAC"},         // three bytes
        {"\xF0\x9F\x98\x80", "\xF0\x9F\x98\x80"}, // four bytes
        {"\xF4\x8F\xBF\xBF", "\xF4\x8F\xBF\xBF"}, // the last code point, U+10FFFF
        {"\xEF\xBF\xBD", "\xEF\xBF\xBD"},         // the replacement character itself
        {"\x80", replacement},                    // a continuation byte with no lead
        {"\xBF", replacement},
        {"\xC0\x80", replacement + replacement},                                     // an overlong NUL
        {"\xC1\xBF", replacement + replacement},                                     // an overlong DEL
        {"\xE0\x80\x80", replacement + replacement + replacement},                   // an overlong form
        {"\xED\xA0\x80", replacement + replacement + replacement},                   // a surrogate
        {"\xF4\x90\x80\x80", replacement + replacement + replacement + replacement}, // above U+10FFFF
        {"\xF5\x80\x80\x80", replacement + replacement + replacement + replacement}, // a lead that starts nothing
        {"\xFE\xFF", replacement + replacement},
        {"\xC3", replacement}, // ends early
        {"\xE2\x82", replacement + replacement},
        {"\xF0\x9F\x98", replacement + replacement + replacement},
        {"a\xC3(b", "a" + replacement + "(b"}, // a lead followed by what is not a continuation
        {"\xE2\x28\xA1", replacement + "(" + replacement},
        {"\xC3\xA9\xFF\xC3\xA9", "\xC3\xA9" + replacement + "\xC3\xA9"},
    };
    for (const Case& scenario : cases)
    {
        OperationRecord record;
        record.detail = scenario.text;
        CAPTURE(scenario.text);
        CHECK(member_line(record_to_json(record), "detail") == "  \"detail\": \"" + scenario.written + "\",");
    }
}

TEST_CASE("each control character is escaped, and nothing else is", "[loader_core][record_json]")
{
    OperationRecord record;
    std::string text;
    for (int c = 1; c < 0x20; ++c)
    {
        text += static_cast<char>(c);
    }
    text += " !#$%&'()*+,-./09:;<=>?@AZ[]^_`az{|}~\x7F";
    record.detail = text;
    const std::string expected = "\\u0001\\u0002\\u0003\\u0004\\u0005\\u0006\\u0007\\b\\t\\n\\u000b\\f\\r\\u000e\\u000f"
                                 "\\u0010\\u0011\\u0012\\u0013\\u0014\\u0015\\u0016\\u0017\\u0018\\u0019\\u001a\\u001b"
                                 "\\u001c\\u001d\\u001e\\u001f"
                                 " !#$%&'()*+,-./09:;<=>?@AZ[]^_`az{|}~\x7F";
    CHECK(member_line(record_to_json(record), "detail") == "  \"detail\": \"" + expected + "\",");
}

TEST_CASE("an empty list and a missing part are written as [] and null", "[loader_core][record_json]")
{
    OperationRecord record;
    record.configuration = std::vector<RecordedHardware>{};
    const std::string json = record_to_json(record);
    CHECK(member_line(json, "loads") == "  \"loads\": [],");
    CHECK(member_line(json, "configuration") == "  \"configuration\": [],");
    CHECK(member_line(json, "files") == "  \"files\": [],");
    CHECK(member_line(json, "media_set") == "  \"media_set\": null,");
    CHECK(member_line(json, "download_folder") == "  \"download_folder\": null,");
    CHECK(member_line(json, "status_code") == "  \"status_code\": null,");
    CHECK(member_line(json, "log") == "  \"log\": []");
}

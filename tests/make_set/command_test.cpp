// SPDX-License-Identifier: GPL-3.0-or-later
// adl_make_set as a program: its function run on temporary folders, with the loader's own check to say that
// what it writes is a media set, and the output it gives read back as JSON.

#include "adl/arinc665/sha1.hpp"
#include "adl/licence/json.hpp"
#include "adl/loader_core/media_set_check.hpp"
#include "adl/make_set/command.hpp"
#include "adl/make_set/description.hpp"
#include "adl/make_set/version.hpp"
#include "adl/platform/arguments.hpp"
#include "adl/platform/paths.hpp"
#include "support.hpp"
#include "temp_dir.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{

namespace fs = std::filesystem;
namespace json = adl::licence::json;
using namespace adl::make_set;
using adl::test::set_maker::names_in;
using adl::test::set_maker::tree_bytes;
using adl::test::set_maker::write_file;

struct Run
{
    int code = -1;
    std::string out;
    std::string err;
};

Run run(const std::vector<std::string>& arguments)
{
    std::vector<std::string_view> views(arguments.begin(), arguments.end());
    std::ostringstream out;
    std::ostringstream err;
    Run result;
    result.code = run_command(views, out, err);
    result.out = out.str();
    result.err = err.str();
    return result;
}

std::string utf8(const fs::path& path)
{
    return adl::platform::to_utf8(path.u8string());
}

std::vector<std::string> lines_of(const std::string& text)
{
    std::vector<std::string> lines;
    std::istringstream stream(text);
    for (std::string line; std::getline(stream, line);)
    {
        lines.push_back(line);
    }
    return lines;
}

// Every line of standard output, read as one JSON object that starts with "event" and "t".
std::vector<json::Value> events_of(const std::string& out)
{
    std::vector<json::Value> events;
    for (const std::string& line : lines_of(out))
    {
        auto value = json::parse_strict(line);
        REQUIRE(value.has_value());
        const json::Object* object = value->as_object();
        REQUIRE(object != nullptr);
        REQUIRE(object->size() >= 2);
        CHECK((*object)[0].key == "event");
        CHECK((*object)[1].key == "t");
        events.push_back(std::move(*value));
    }
    return events;
}

const json::Object& object_of(const json::Value& value)
{
    REQUIRE(value.as_object() != nullptr);
    return *value.as_object();
}

const json::Value& get(const json::Object& object, std::string_view key)
{
    const json::Value* value = json::find(object, key);
    INFO(key);
    REQUIRE(value != nullptr);
    return *value;
}

std::string text(const json::Object& object, std::string_view key)
{
    const std::string* value = get(object, key).as_string();
    REQUIRE(value != nullptr);
    return *value;
}

std::int64_t number(const json::Object& object, std::string_view key)
{
    const std::int64_t* value = get(object, key).as_integer();
    REQUIRE(value != nullptr);
    return *value;
}

std::vector<std::string> names(const std::vector<json::Value>& events)
{
    std::vector<std::string> list;
    for (const json::Value& event : events)
    {
        list.push_back(text(object_of(event), "event"));
    }
    return list;
}

std::vector<std::string> keys(const json::Object& object)
{
    std::vector<std::string> list;
    for (const json::Member& member : object)
    {
        list.push_back(member.key);
    }
    return list;
}

// The description of the guide's example and the files it names, in a folder of the test.
constexpr std::string_view description_text = R"({
  "format": 1,
  "kind": "adl-media-set-description",
  "media_set": { "part_number": "ACM-SET-0001", "members": 2, "files_check": "sha1", "files_list_check": "crc32" },
  "loads": [
    {
      "part_number": "ACM47-1234-5678",
      "header_file": "ACM47.LUH",
      "type": "OPS",
      "target_hardware_ids": ["ACME", "ACMEFMS"],
      "check": "sha1",
      "files": [
        { "source": "payloads/app.bin", "name": "APPCODE.LUP", "check": "crc32" },
        { "source": "payloads/nav.bin", "name": "NAVDATA.LUP", "check": "md5" },
        { "source": "payloads/CONFIG.LDR", "kind": "support" }
      ]
    },
    {
      "part_number": "ACM4C-0100-0200",
      "type": { "description": "Test software", "id": 4 },
      "target_hardware_ids": ["ACME"],
      "files": [ { "source": "payloads/selftest.bin", "name": "SELFTEST.LUP", "check": "crc16" } ]
    }
  ],
  "batch": { "part_number": "ACM4D-0100-0300", "comment": "Both loads for the unit" }
})";

// A folder with the description and the four payloads it names.
struct Rig
{
    adl::test::TempDir work;
    fs::path description;
    fs::path payloads;

    explicit Rig(std::string_view folder_name = "rig")
    {
        const fs::path root = work.path() / adl::platform::path_from_utf8(folder_name);
        payloads = root / "payloads";
        write_file(payloads, "app.bin", "0123456789ABCDEFG"); // 17 bytes: not whole words
        write_file(payloads, "nav.bin", "HELLO");
        write_file(payloads, "CONFIG.LDR", "ABCD");
        write_file(payloads, "selftest.bin", "SELFTEST");
        description = write_file(root, "set.json", description_text);
    }

    [[nodiscard]] fs::path root() const
    {
        return description.parent_path();
    }
};

// A copy of the whole tree of `from`, so that two runs read their sources from two places.
void copy_tree(const fs::path& from, const fs::path& to)
{
    fs::copy(from, to, fs::copy_options::recursive);
}

} // namespace

TEST_CASE("command: --version and --help", "[command]")
{
    auto result = run({"--version"});
    CHECK(result.code == exit_ok);
    CHECK(result.out == std::string{"adl_make_set "} + version + " events 1\n");
    CHECK(result.err.empty());

    result = run({"--help"});
    CHECK(result.code == exit_ok);
    CHECK(result.out.starts_with("usage: adl_make_set make <description> --output <folder>"));
    CHECK(result.err.empty());
}

TEST_CASE("command: make writes a set that the loader's own check accepts, and says so in events", "[command]")
{
    Rig rig;
    const fs::path output = rig.work.path() / "out" / "ACM-SET-0001";
    const auto result = run({"make", utf8(rig.description), "--output", utf8(output), "--json"});
    REQUIRE(result.code == exit_ok);
    CHECK(result.err.empty());

    const auto events = events_of(result.out);
    CHECK(names(events) == std::vector<std::string>{"started", "description_read", "progress", "progress", "progress",
                                                    "progress", "set_written", "media_set_checked", "finished"});

    const json::Object& started = object_of(events.front());
    CHECK(keys(started) ==
          std::vector<std::string>{"event", "t", "program", "version", "events", "command", "description", "output"});
    CHECK(text(started, "program") == "adl_make_set");
    CHECK(text(started, "version") == version);
    CHECK(number(started, "events") == 1);
    CHECK(text(started, "command") == "make");
    CHECK(text(started, "description") == utf8(rig.description));
    CHECK(text(started, "output") == utf8(output));

    const json::Object& read = object_of(events[1]);
    CHECK(keys(read) ==
          std::vector<std::string>{"event", "t", "media_set_part_number", "members", "loads", "files", "batch"});
    CHECK(number(read, "members") == 2);
    CHECK(number(read, "loads") == 2);
    CHECK(number(read, "files") == 4);
    CHECK(get(read, "batch").as_integer() == nullptr);
    CHECK(std::get<bool>(get(read, "batch").data));

    // Progress only goes up, and ends at 100.
    std::int64_t last = -1;
    for (std::size_t i = 2; i < 6; ++i)
    {
        const json::Object& progress = object_of(events[i]);
        CHECK(keys(progress) == std::vector<std::string>{"event", "t", "percent"});
        CHECK(number(progress, "percent") > last);
        last = number(progress, "percent");
    }
    CHECK(last == 100);

    const auto report = adl::loader_core::check_media_set(output);
    REQUIRE(report.error.empty());
    const json::Object& written = object_of(events[6]);
    CHECK(number(written, "members") == 2);
    CHECK(number(written, "files") == static_cast<std::int64_t>(tree_bytes(output).size()));

    const json::Object& checked = object_of(events[7]);
    CHECK(keys(checked) == std::vector<std::string>{"event", "t", "path", "part_number", "members", "loads", "batches",
                                                    "files", "findings"});
    CHECK(text(checked, "path") == utf8(output));
    CHECK(text(checked, "part_number") == "ACM-SET-0001");
    CHECK(number(checked, "loads") == 2);
    CHECK(number(checked, "batches") == 1);
    CHECK(get(checked, "findings").as_array()->size() == report.findings.size());

    const json::Object& finished = object_of(events[8]);
    CHECK(keys(finished) == std::vector<std::string>{"event", "t", "result", "reason", "detail", "exit_code", "output",
                                                     "members", "files", "finding_count"});
    CHECK(text(finished, "result") == "completed");
    CHECK(text(finished, "reason") == "completed");
    CHECK(number(finished, "exit_code") == 0);
    CHECK(text(finished, "output") == utf8(output));
    CHECK(number(finished, "finding_count") == static_cast<std::int64_t>(report.findings.size()));
    CHECK(text(finished, "detail").starts_with("Media set ACM-SET-0001 written and verified: 2 members, "));
}

TEST_CASE("command: make without --json says one line and the findings", "[command]")
{
    Rig rig;
    const fs::path output = rig.work.path() / "out";
    const auto result = run({"make", utf8(rig.description), "--output", utf8(output)});
    REQUIRE(result.code == exit_ok);
    CHECK(result.err.empty());
    const auto lines = lines_of(result.out);
    REQUIRE(lines.size() >= 2);
    CHECK(lines[0].starts_with("ACM-SET-0001: 2 members, "));
    CHECK(lines[0].ends_with(" -> " + utf8(output)));
    CHECK(lines[0].find("2 loads, 1 batch, ") != std::string::npos);
    for (std::size_t i = 1; i < lines.size(); ++i)
    {
        CHECK(lines[i].starts_with("    2.2.3.2: the data file ")); // the odd length of the 17 bytes
    }
}

TEST_CASE("command: the same description makes the same bytes, wherever it is made from", "[command]")
{
    Rig rig;
    const fs::path first = rig.work.path() / "first";
    const fs::path second = rig.work.path() / "elsewhere" / "second";
    REQUIRE(run({"make", utf8(rig.description), "--output", utf8(first)}).code == exit_ok);

    // The sources are copied to another place, and read from there by --source-root.
    const fs::path copy = rig.work.path() / "copy";
    fs::create_directories(copy);
    copy_tree(rig.payloads, copy / "payloads");
    REQUIRE(run({"make", utf8(rig.description), "--output", utf8(second), "--source-root", utf8(copy)}).code ==
            exit_ok);
    CHECK(tree_bytes(first) == tree_bytes(second));
    CHECK(tree_bytes(first).size() == 13);

    // And inspecting the two says the same, but for the folder.
    const auto one = run({"inspect", utf8(first), "--json"});
    const auto two = run({"inspect", utf8(second), "--json"});
    REQUIRE(one.code == exit_ok);
    REQUIRE(two.code == exit_ok);
    // Everything from "media_set" on: the folder is the one member that differs, and comes before it.
    const auto facts = [](const std::string& document) { return document.substr(document.find("\"media_set\"")); };
    CHECK(facts(one.out) == facts(two.out));
    CHECK(facts(one.out).size() > 1000);
}

TEST_CASE("command: a description that cannot be used is exit 3, with the sentences, and nothing is written",
          "[command]")
{
    Rig rig;
    const fs::path output = rig.work.path() / "out";

    // The file is not there.
    auto result = run({"make", utf8(rig.work.path() / "missing.json"), "--output", utf8(output), "--json"});
    CHECK(result.code == exit_input);
    auto events = events_of(result.out);
    CHECK(names(events) == std::vector<std::string>{"started", "log", "finished"});
    const json::Object& log = object_of(events[1]);
    CHECK(keys(log) == std::vector<std::string>{"event", "t", "severity", "text"});
    CHECK(text(log, "severity") == "error");
    CHECK(text(log, "text").starts_with("Cannot read "));
    const json::Object& finished = object_of(events[2]);
    CHECK(text(finished, "result") == "failed");
    CHECK(text(finished, "reason") == "description_invalid");
    CHECK(number(finished, "exit_code") == 3);
    CHECK(text(finished, "detail") == text(log, "text"));
    CHECK(get(finished, "output").is_null());
    CHECK(get(finished, "members").is_null());
    CHECK_FALSE(fs::exists(output));

    // A key that is not known, and every problem said: one log event for each, one detail.
    const fs::path bad = write_file(
        rig.root(), "bad.json",
        R"({"format":1,"kind":"adl-media-set-description","media_set":{"part_number":"ACM SET"},"loads":[{"part_number":"nope","files":[{"source":"payloads/app.bin","name":"x y"}]}]})");
    result = run({"make", utf8(bad), "--output", utf8(output), "--json"});
    CHECK(result.code == exit_input);
    events = events_of(result.out);
    REQUIRE(events.size() >= 4);
    CHECK(names(events).front() == "started");
    CHECK(names(events).back() == "finished");
    for (std::size_t i = 1; i + 1 < events.size(); ++i)
    {
        CHECK(text(object_of(events[i]), "event") == "log");
    }
    CHECK(text(object_of(events.back()), "detail") == text(object_of(events[1]), "text"));

    // Without --json the sentences go to the error output, one on each line, and standard output is empty.
    result = run({"make", utf8(bad), "--output", utf8(output)});
    CHECK(result.code == exit_input);
    CHECK(result.out.empty());
    CHECK(lines_of(result.err).size() == events.size() - 2);
    CHECK(result.err.starts_with("adl_make_set: media_set.part_number: "));
    CHECK_FALSE(fs::exists(output));
}

TEST_CASE("command: a source that cannot be read is exit 3 and leaves nothing", "[command]")
{
    Rig rig;
    const fs::path output = rig.work.path() / "out" / "set";
    fs::remove(rig.payloads / "nav.bin");
    const auto result = run({"make", utf8(rig.description), "--output", utf8(output), "--json"});
    CHECK(result.code == exit_input);
    const auto events = events_of(result.out);
    CHECK(names(events) == std::vector<std::string>{"started", "description_read", "finished"});
    const json::Object& finished = object_of(events.back());
    CHECK(text(finished, "reason") == "source_unreadable");
    CHECK(text(finished, "detail") == "Cannot read " + utf8(rig.payloads / "nav.bin") + ".");
    CHECK(number(finished, "exit_code") == 3);
    CHECK_FALSE(fs::exists(output));
    CHECK(names_in(rig.work.path() / "out").empty());
}

TEST_CASE("command: an output folder that is taken is exit 4 and is not touched", "[command]")
{
    Rig rig;
    const fs::path output = write_file(rig.work.path() / "out", "mine.txt", "mine").parent_path();
    const auto result = run({"make", utf8(rig.description), "--output", utf8(output), "--json"});
    CHECK(result.code == exit_output_exists);
    const auto events = events_of(result.out);
    CHECK(names(events) == std::vector<std::string>{"started", "description_read", "finished"});
    const json::Object& finished = object_of(events.back());
    CHECK(text(finished, "reason") == "output_exists");
    CHECK(text(finished, "detail") == utf8(output) + " is there already and is not an empty folder.");
    CHECK(names_in(output) == std::vector<std::string>{"mine.txt"});
    CHECK(names_in(rig.work.path() / "out").size() == 1);

    // An empty folder is taken over, and the run says so.
    fs::remove(output / "mine.txt");
    const auto again = run({"make", utf8(rig.description), "--output", utf8(output), "--json"});
    CHECK(again.code == exit_ok);
    const auto taken = events_of(again.out);
    CHECK(std::ranges::count(names(taken), "log") == 1);
}

TEST_CASE("command: a folder that cannot be made is exit 5", "[command]")
{
    Rig rig;
    const fs::path blocker = write_file(rig.work.path(), "blocker", "a file where a folder is wanted");
    const auto result = run({"make", utf8(rig.description), "--output", utf8(blocker / "set"), "--json"});
    CHECK(result.code == exit_local);
    const auto events = events_of(result.out);
    const json::Object& finished = object_of(events.back());
    CHECK(text(finished, "reason") == "local_error");
    CHECK(text(finished, "detail").starts_with("The media set could not be written below " + utf8(blocker)));
    CHECK(number(finished, "exit_code") == 5);
}

TEST_CASE("command: standard output that cannot be written is exit 5", "[command]")
{
    Rig rig;
    const fs::path output = rig.work.path() / "out";
    const std::vector<std::string> arguments = {"make", utf8(rig.description), "--output", utf8(output), "--json"};
    const std::vector<std::string_view> views(arguments.begin(), arguments.end());
    std::ostream broken{nullptr}; // badbit from the start
    std::ostringstream err;
    CHECK(run_command(views, broken, err) == exit_local);
}

TEST_CASE("command: wrong usage is exit 2, with the sentence and the usage on the error output", "[command]")
{
    for (const std::vector<std::string>& arguments :
         std::vector<std::vector<std::string>>{{},
                                               {"make"},
                                               {"make", "x.json"},
                                               {"verify"},
                                               {"inspect", "a", "b"},
                                               {"compare", "a"},
                                               {"make", "x", "--output"},
                                               {"verify", "x", "--output", "y"}})
    {
        const auto result = run(arguments);
        CHECK(result.code == exit_usage);
        CHECK(result.out.empty());
        CHECK(result.err.starts_with("adl_make_set: "));
        CHECK(result.err.find("\nusage: adl_make_set make <description>") != std::string::npos);
    }
    CHECK(run({"make", "x.json"}).err.starts_with("adl_make_set: make needs --output\n"));
}

TEST_CASE("command: verify says what the loader's own check found", "[command]")
{
    Rig rig;
    const fs::path output = rig.work.path() / "out";
    REQUIRE(run({"make", utf8(rig.description), "--output", utf8(output)}).code == exit_ok);

    auto result = run({"verify", utf8(output), "--json"});
    CHECK(result.code == exit_ok);
    auto events = events_of(result.out);
    CHECK(names(events) == std::vector<std::string>{"started", "media_set_checked", "finished"});
    CHECK(text(object_of(events[0]), "command") == "verify");
    CHECK(text(object_of(events[0]), "folder") == utf8(output));
    CHECK(keys(object_of(events[0])) ==
          std::vector<std::string>{"event", "t", "program", "version", "events", "command", "folder"});
    CHECK(text(object_of(events[2]), "reason") == "completed");
    CHECK(text(object_of(events[2]), "detail").starts_with("Media set ACM-SET-0001 verified: 2 members, "));
    CHECK(keys(object_of(events[2])) == std::vector<std::string>{"event", "t", "result", "reason", "detail",
                                                                 "exit_code", "members", "files", "finding_count"});

    // Plain.
    result = run({"verify", utf8(output)});
    CHECK(result.code == exit_ok);
    CHECK(lines_of(result.out).front().starts_with("ACM-SET-0001: 2 members, "));

    // One byte changed in a data file: the set does not verify, and that is the result, exit 1.
    const fs::path data = output / "member_001" / "ACM4712345678" / "ACM4712345678" / "APPCODE.LUP";
    {
        std::fstream stream(data, std::ios::in | std::ios::out | std::ios::binary);
        stream.put('J');
    }
    result = run({"verify", utf8(output), "--json"});
    CHECK(result.code == exit_set_invalid);
    events = events_of(result.out);
    CHECK(names(events) == std::vector<std::string>{"started", "finished"});
    CHECK(text(object_of(events[1]), "reason") == "set_invalid");
    CHECK(text(object_of(events[1]), "detail").starts_with("The media set could not be read: "));
    CHECK(number(object_of(events[1]), "exit_code") == 1);
    result = run({"verify", utf8(output)});
    CHECK(result.code == exit_set_invalid);
    CHECK(result.out.empty());
    CHECK(result.err.starts_with("adl_make_set: The media set could not be read: "));

    // A folder that is not there is the folder's fault.
    result = run({"verify", utf8(rig.work.path() / "nothing"), "--json"});
    CHECK(result.code == exit_input);
    CHECK(text(object_of(events_of(result.out).back()), "reason") == "folder_unreadable");
}

TEST_CASE("command: inspect writes one document that has the facts of the description", "[command]")
{
    Rig rig;
    const fs::path output = rig.work.path() / "out";
    REQUIRE(run({"make", utf8(rig.description), "--output", utf8(output)}).code == exit_ok);

    const auto result = run({"inspect", utf8(output), "--json"});
    CHECK(result.code == exit_ok);
    CHECK(result.err.empty());
    CHECK(lines_of(result.out).size() == 1);
    CHECK(result.out.ends_with("}\n"));
    const auto document = json::parse_strict(lines_of(result.out)[0]);
    REQUIRE(document.has_value());
    const json::Object& top = object_of(*document);
    CHECK(keys(top) == std::vector<std::string>{"format", "kind", "program", "result", "reason", "detail", "exit_code",
                                                "folder", "media_set", "loads", "batches", "files", "unlisted_files",
                                                "findings"});
    CHECK(number(top, "format") == 1);
    CHECK(text(top, "kind") == "adl-media-set-inspection");
    CHECK(keys(object_of(get(top, "program"))) == std::vector<std::string>{"name", "version"});
    CHECK(text(top, "result") == "completed");
    CHECK(text(top, "reason") == "completed");
    CHECK(number(top, "exit_code") == 0);
    CHECK(text(top, "folder") == utf8(output));

    const json::Object& media_set = object_of(get(top, "media_set"));
    CHECK(keys(media_set) == std::vector<std::string>{"part_number", "members", "members_present", "format_versions",
                                                      "user_defined_data", "files_list_check_type",
                                                      "files_list_check_value"});
    CHECK(text(media_set, "part_number") == "ACM-SET-0001");
    CHECK(number(media_set, "members") == 2);
    CHECK(text(media_set, "files_list_check_type") == "crc32");

    const json::Array& loads = *get(top, "loads").as_array();
    REQUIRE(loads.size() == 2);
    const json::Object& first = object_of(loads[0]);
    CHECK(keys(first) == std::vector<std::string>{"part_number", "header_file", "header_member", "header_pathname",
                                                  "format_version", "type", "is_download", "target_hardware_ids",
                                                  "target_hardware_ids_with_positions", "check_type", "check_value",
                                                  "load_crc", "user_defined_data", "data_files", "support_files"});
    CHECK(text(first, "part_number") == "ACM47-1234-5678");
    CHECK(text(first, "header_file") == "ACM47.LUH");
    CHECK(text(first, "check_type") == "sha1");
    CHECK(text(object_of(get(first, "type")), "description") == "Operational software");
    const json::Array& data = *get(first, "data_files").as_array();
    REQUIRE(data.size() == 2);
    CHECK(keys(object_of(data[0])) == std::vector<std::string>{"name", "part_number", "size", "crc16", "check_type",
                                                               "check_value", "member", "pathname"});
    CHECK(text(object_of(data[0]), "name") == "APPCODE.LUP");
    CHECK(number(object_of(data[0]), "size") == 17);
    CHECK(text(object_of(data[0]), "check_type") == "crc32");
    CHECK(text(object_of(data[1]), "check_type") == "md5");
    CHECK(number(object_of(data[1]), "size") == 5);
    CHECK(number(object_of(data[1]), "member") == 2);
    const json::Object& second = object_of(loads[1]);
    CHECK(text(second, "header_file") == "ACM.LUH");
    CHECK(number(object_of(get(second, "type")), "id") == 4);
    CHECK(text(object_of((*get(second, "data_files").as_array())[0]), "check_type") == "crc16");

    const json::Array& batches = *get(top, "batches").as_array();
    REQUIRE(batches.size() == 1);
    CHECK(text(object_of(batches[0]), "comment") == "Both loads for the unit");
    CHECK(get(top, "unlisted_files").as_array()->empty());
    CHECK(get(top, "files").as_array()->size() + 2 == tree_bytes(output).size());
    CHECK(get(top, "findings").as_array()->size() == 2); // the two data files that are not whole words
}

TEST_CASE("command: inspect of what is not a media set is a document that says so", "[command]")
{
    Rig rig;
    const fs::path output = rig.work.path() / "out";
    REQUIRE(run({"make", utf8(rig.description), "--output", utf8(output)}).code == exit_ok);
    const fs::path data = output / "member_002" / "ACM4712345678" / "ACM4712345678" / "NAVDATA.LUP";
    {
        std::fstream stream(data, std::ios::in | std::ios::out | std::ios::binary);
        stream.put('J');
    }

    auto result = run({"inspect", utf8(output), "--json"});
    CHECK(result.code == exit_set_invalid);
    CHECK(result.err.empty());
    REQUIRE(lines_of(result.out).size() == 1);
    auto document = json::parse_strict(lines_of(result.out)[0]);
    REQUIRE(document.has_value());
    const json::Object& top = object_of(*document);
    CHECK(text(top, "result") == "failed");
    CHECK(text(top, "reason") == "set_invalid");
    CHECK(number(top, "exit_code") == 1);
    CHECK(text(top, "detail").starts_with("The media set could not be read: "));
    CHECK(get(top, "media_set").is_null());
    for (const char* list : {"loads", "batches", "files", "unlisted_files", "findings"})
    {
        CHECK(get(top, list).as_array()->empty());
    }

    result = run({"inspect", utf8(rig.work.path() / "nothing"), "--json"});
    CHECK(result.code == exit_input);
    document = json::parse_strict(lines_of(result.out)[0]);
    REQUIRE(document.has_value());
    CHECK(text(object_of(*document), "reason") == "folder_unreadable");

    // Without --json it is a few lines for a person, or a sentence on the error output.
    result = run({"inspect", utf8(rig.work.path() / "nothing")});
    CHECK(result.code == exit_input);
    CHECK(result.out.empty());
    CHECK(result.err.ends_with(" is not a folder, or cannot be read.\n"));
}

TEST_CASE("command: names of any language, in the description, the sources and the output", "[command]")
{
    // An accented letter, Japanese and a character outside the Basic Multilingual Plane, which no Windows code
    // page holds all of: the paths reach the program as UTF-8 and have to come out the same.
    const std::string folder = "rig-\xC3\xA9-\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E-\xF0\x9F\x98\x80 and spaces";
    Rig rig{folder};
    fs::rename(rig.payloads / "app.bin", rig.payloads / adl::platform::path_from_utf8("app-\xC3\xA9-\xE6\x97\xA5.bin"));
    std::string moved{description_text};
    moved.replace(moved.find("payloads/app.bin"), std::string_view{"payloads/app.bin"}.size(),
                  "payloads/app-\xC3\xA9-\xE6\x97\xA5.bin");
    const fs::path description = write_file(rig.root(), "set-\xC3\xA9.json", moved);
    const fs::path output = rig.work.path() / adl::platform::path_from_utf8(
                                                  "out-\xC3\xA9-\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E-\xF0\x9F\x98\x80");

    const auto result = run({"make", utf8(description), "--output", utf8(output), "--json"});
    CHECK(result.code == exit_ok);
    const auto events = events_of(result.out);
    CHECK(text(object_of(events.front()), "output") == utf8(output));
    CHECK(text(object_of(events.back()), "output") == utf8(output));
    CHECK(adl::loader_core::check_media_set(output).error.empty());
    CHECK(run({"verify", utf8(output)}).code == exit_ok);
    CHECK(run({"inspect", utf8(output), "--json"}).code == exit_ok);
}

// The bytes of every file of the set that the guide's example makes from the files in tests/make_set/data/golden,
// as SHA-1, so that the three desktops are compared with one fixed answer. They were written down when the program
// first made this set, and are the answer to "did anything that is in a set change".
TEST_CASE("command: the golden description makes the golden set on every desktop", "[command][golden]")
{
    const fs::path data = fs::path{ADL_MAKE_SET_DATA_DIR} / "golden";
    adl::test::TempDir out;
    const auto result = run({"make", utf8(data / "description.json"), "--output", utf8(out.path() / "set")});
    REQUIRE(result.code == exit_ok);

    std::map<std::string, std::string> sha1;
    for (const auto& [name, bytes] : tree_bytes(out.path() / "set"))
    {
        adl::arinc665::Sha1 digest;
        digest.update(bytes);
        std::string hex;
        for (const std::byte byte : digest.digest())
        {
            constexpr std::string_view digits = "0123456789abcdef";
            hex += digits[std::to_integer<unsigned>(byte) >> 4U];
            hex += digits[std::to_integer<unsigned>(byte) & 15U];
        }
        sha1[name] = hex;
    }
    const std::map<std::string, std::string> expected = {
        {"member_001/ACM.LUB", "3a249b1f70a5f990b382bbdba2d81ec815b43028"},
        {"member_001/ACM4712345678/ACM47.LUH", "62b19c302f8b74feca88b0e5a1d5fa50a9674ab9"},
        {"member_001/ACM4712345678/ACM4712345678/APPCODE.LUP", "8149a643caace2113f5fc4dc98bbce9625496fad"},
        {"member_001/ACM4712345678/CONFIG.LDR", "fb2f85c88567f3c8ce9b799c7c54642d0c7b41f6"},
        {"member_001/ACM4C01000200/ACM.LUH", "399ec3206dc8c317817f14c0f73b8662cd0ea6e5"},
        {"member_001/ACM4C01000200/ACM4C01000200/SELFTEST.LUP", "b99669413a8beee5c9c75ae7b69d686d2ba6be8a"},
        {"member_001/BATCHES.LUM", "41ccb79f358ffc1e5803ba62547c63295526d5fb"},
        {"member_001/FILES.LUM", "f88fd7302ee24189f446948dd5b94a96fc2e0fcc"},
        {"member_001/LOADS.LUM", "4a49883d21a91b38d3f5036ca9fe7367e14d2c12"},
        {"member_002/ACM4712345678/ACM4712345678/NAVDATA.LUP", "c65f99f8c5376adadddc46d5cbcf5762f9e55eb7"},
        {"member_002/BATCHES.LUM", "092aa23ffa88af2513158c98647296e5eec367a3"},
        {"member_002/FILES.LUM", "d5b31fea6674d2b719fdac797b4897c8f7fece46"},
        {"member_002/LOADS.LUM", "823964fe02a22e91e1aa405abb5a99351401d099"},
    };
    CHECK(sha1 == expected);
}

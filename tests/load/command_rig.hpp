// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// What a test of a whole adl_load command needs: the loader's host and a simulated target on one fake
// network (tests/engine/rig.hpp), the files of a media set both on the disk (where the program checks the set)
// and in the fake file system (where an upload reads it from), and the command's output read back as events.

#include "adl/arinc665/load.hpp"
#include "adl/arinc665/media_set.hpp"
#include "adl/load/program.hpp"
#include "adl/load/run.hpp"
#include "adl/loader_core/media_set_check.hpp"
#include "adl/media_tree/media_tree.hpp"
#include "adl/samples/scenarios.hpp"
#include "mini_json.hpp"
#include "rig.hpp"
#include "temp_dir.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace adl::test
{

// What one command printed and returned.
struct CommandRun
{
    int exit_code = -1;
    std::string out;
    std::string err;

    // Standard output as the events of `--json`, each of which has to be one valid JSON object.
    [[nodiscard]] std::vector<Json> events() const
    {
        std::vector<Json> parsed = parse_json_lines(out);
        for (const Json& event : parsed)
        {
            INFO("a line of standard output that is not a JSON object: " << event.text);
            REQUIRE(event.is(Json::Kind::object));
        }
        return parsed;
    }
    // The `event` of each event, in order.
    [[nodiscard]] std::vector<std::string> names() const
    {
        std::vector<std::string> names;
        for (const Json& event : events())
        {
            names.push_back(event.at("event").text);
        }
        return names;
    }
    // The events called `name`.
    [[nodiscard]] std::vector<Json> events_named(std::string_view name) const
    {
        std::vector<Json> found;
        for (const Json& event : events())
        {
            if (event.at("event").text == name)
            {
                found.push_back(event);
            }
        }
        return found;
    }
    // The last event, which has to be `finished`.
    [[nodiscard]] Json finished() const
    {
        const std::vector<Json> all = events();
        REQUIRE_FALSE(all.empty());
        REQUIRE(all.back().at("event").text == "finished");
        return all.back();
    }
};

class CommandRig
{
public:
    explicit CommandRig(SimPlan plan = {}) : rig(std::move(plan)) {}

    // Runs a command on the loader's host, to its end, and lets the target see the loader's last datagram.
    CommandRun run(std::initializer_list<std::string_view> arguments)
    {
        return run_on(rig.loader_host, std::vector<std::string_view>(arguments));
    }
    CommandRun run(const std::vector<std::string_view>& arguments)
    {
        return run_on(rig.loader_host, arguments);
    }

    // The same on a host that is the loader's host with something made to go wrong.
    CommandRun run_on(adl::platform::Host& host, std::initializer_list<std::string_view> arguments)
    {
        return run_on(host, std::vector<std::string_view>(arguments));
    }
    CommandRun run_on(adl::platform::Host& host, const std::vector<std::string_view>& arguments)
    {
        std::ostringstream out;
        std::ostringstream err;
        CommandRun result;
        result.exit_code = adl::load::run(host, arguments, out, err);
        result.out = out.str();
        result.err = err.str();
        rig.world.advance_by(std::chrono::seconds(1));
        return result;
    }

    Rig rig;
};

// What every `--json` run of an operation has in common: `started` first and `finished` once and last, `event`
// and `t` first in every event, `t` that does not go back, and the exit code the process returned in `finished`.
inline void check_stream(const CommandRun& run)
{
    const std::vector<Json> events = run.events();
    REQUIRE(events.size() >= 2);
    CHECK(events.front().at("event").text == "started");
    CHECK(events.front().at("events").text == "1");
    CHECK(events.back().at("event").text == "finished");
    std::uint64_t last_milliseconds = 0;
    std::size_t finished = 0;
    for (const Json& event : events)
    {
        const std::vector<std::string> keys = event.keys();
        REQUIRE(keys.size() >= 2);
        CHECK(keys[0] == "event");
        CHECK(keys[1] == "t");
        const std::string& t = event.at("t").text;
        REQUIRE(t.size() >= 5);
        REQUIRE(t[t.size() - 4] == '.');
        const std::uint64_t milliseconds = std::stoull(t.substr(0, t.size() - 4)) * 1000 + std::stoull(t.substr(t.size() - 3));
        CHECK(milliseconds >= last_milliseconds);
        last_milliseconds = milliseconds;
        finished += event.at("event").text == "finished" ? 1 : 0;
    }
    CHECK(finished == 1);
    CHECK(events.back().at("exit_code").as_number() == static_cast<std::uint64_t>(run.exit_code));
}

[[nodiscard]] inline std::string read_text(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// The bytes of the file `path` in the fake file system, as text.
[[nodiscard]] inline std::string fake_text(adl::fake::FakeHost& host, std::string_view path)
{
    const auto bytes = host.fake_files().get(path);
    REQUIRE(bytes.has_value());
    return std::string(reinterpret_cast<const char*>(bytes->data()), bytes->size());
}

// `actual` against the golden file `name` in tests/load/golden. The version of the program is written as
// @VERSION@ in the file, and so are the directories of a test as @DIR@, since they are not what is tested.
// A golden file that is not there is written from the run, and the test fails until it is read and kept; to
// make one again, delete it.
inline void check_golden(const std::string& name, const std::string& actual, const std::string& directory = {})
{
    const std::filesystem::path path = std::filesystem::path{ADL_LOAD_GOLDEN_DIR} / name;
    const auto replace_all = [](std::string text, const std::string& from, const std::string& to)
    {
        for (std::size_t at = text.find(from); !from.empty() && at != std::string::npos; at = text.find(from, at + to.size()))
        {
            text.replace(at, from.size(), to);
        }
        return text;
    };
    std::string expected_form = actual;
    expected_form = replace_all(expected_form, std::string{adl::load::program_version}, "@VERSION@");
    if (!directory.empty())
    {
        expected_form = replace_all(expected_form, directory, "@DIR@");
    }
    if (!std::filesystem::exists(path))
    {
        // The first run of a new test writes what it saw, for a person to read before it is committed.
        std::ofstream out(path, std::ios::binary);
        out << expected_form;
        FAIL("the golden file " << path.string() << " was not there; it has been written from this run");
    }
    INFO("golden file " << path.string());
    CHECK(expected_form == read_text(path));
}

// What the target reads of one load: its header file, then its data and support files, each with its CRC.
[[nodiscard]] inline UploadItem upload_item_of(const adl::arinc665::Load& load, std::string part_number)
{
    UploadItem item;
    item.header_name = load.header_file_name;
    item.load_part_number = std::move(part_number);
    const auto add = [&item](const std::string& name, const Bytes& bytes)
    { item.reads.push_back(ReadItem{name, bytes, crc16_value(bytes)}); };
    add(load.header_file_name, load.header_bytes);
    for (const auto& file : load.data_files)
    {
        add(file.name, file.bytes);
    }
    for (const auto& file : load.support_files)
    {
        add(file.name, file.bytes);
    }
    return item;
}

// A media set with two loads: the sample's, and one of its own with another header file, part number and files.
struct TwoLoadSet
{
    std::vector<adl::arinc665::MediaMember> members;
    UploadItem first;
    UploadItem second;
};

inline constexpr const char* second_load_part_number = "ACM48-0000-0002";

[[nodiscard]] inline TwoLoadSet make_two_load_set()
{
    TwoLoadSet set;
    auto spec = adl::samples::sample_media_set_spec();
    REQUIRE(spec.has_value());
    set.first = upload_item_of(spec->loads[0].load, adl::samples::sample_load_part_number);

    adl::arinc665::LoadSpec second = adl::samples::sample_load_spec();
    second.header_file_name = "ACM48.LUH";
    second.part_number = adl::arinc665::PartNumber{second_load_part_number};
    second.data_files[0].name = "APP48.LUP";
    second.data_files[1].name = "DATA48.LUP";
    second.support_files[0].name = "CONFIG48.LDR";
    auto built = adl::arinc665::build_load(second);
    REQUIRE(built.has_value());
    set.second = upload_item_of(*built, second_load_part_number);
    adl::arinc665::PlacedLoad placed;
    placed.load = std::move(*built);
    placed.header_member = 1;
    spec->loads.push_back(std::move(placed));

    auto members = adl::arinc665::build_media_set(*spec);
    REQUIRE(members.has_value());
    set.members = std::move(*members);
    return set;
}

// A media set on the disk, in a directory of its own that goes when this does, and its files in the fake
// file system at the paths the program will look for them at: the program checks the set where it is, with the
// standard library, and sends it from where the platform's file system has it.
class InstalledMediaSet
{
public:
    InstalledMediaSet(adl::fake::FakeHost& host, const std::vector<adl::arinc665::MediaMember>& members)
        : root_(dir_.path() / "set")
    {
        const auto written = adl::media_tree::write_media_set_tree(root_, members);
        REQUIRE(written.has_value());
        std::shared_ptr<const adl::loader_core::MediaSetIndex> index;
        const adl::loader_core::MediaSetReport report = adl::loader_core::check_media_set(root_, &index);
        REQUIRE(report.error.empty());
        for (const adl::loader_core::MediaSetIndex::Entry& entry : index->entries())
        {
            for (const adl::arinc665::MediaMember& member : members)
            {
                for (const adl::arinc665::MediaFile& file : member.files)
                {
                    if (member.sequence_number == entry.member && file.pathname == entry.pathname &&
                        file.name == entry.name)
                    {
                        host.fake_files().put(entry.path, file.bytes);
                        paths_.push_back(entry.path);
                    }
                }
            }
        }
    }

    // The folder, as an operator would type it.
    [[nodiscard]] std::string path() const
    {
        return root_.string();
    }
    // The platform path of every file of the set in the fake file system.
    [[nodiscard]] const std::vector<std::string>& files() const noexcept
    {
        return paths_;
    }

private:
    TempDir dir_;
    std::filesystem::path root_;
    std::vector<std::string> paths_;
};

} // namespace adl::test

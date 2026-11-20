// SPDX-License-Identifier: GPL-3.0-or-later
// make_set(): a plan becomes a media set that the loader's own check accepts, and whatever goes wrong leaves
// nothing behind.

#include "adl/loader_core/media_set_check.hpp"
#include "adl/media_tree/media_tree.hpp"
#include "adl/set_maker/make.hpp"
#include "support.hpp"
#include "temp_dir.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{

namespace fs = std::filesystem;
using adl::arinc665::CheckValueType;
using namespace adl::set_maker;
using adl::test::set_maker::names_in;
using adl::test::set_maker::one_load_plan;
using adl::test::set_maker::tree_bytes;
using adl::test::set_maker::write_file;

// Says what happened, and stops at one place if asked to.
class RecordingHooks final : public Hooks
{
public:
    std::string stop_at;
    std::string throw_at;
    bool throw_length_error = false;
    bool throw_runtime_error = false;

    std::vector<std::string> places;
    std::vector<int> percents;
    bool written_called = false;
    std::size_t written_members = 0;
    std::size_t written_files = 0;
    std::uint64_t written_bytes = 0;
    int checked_calls = 0;
    std::vector<fs::path> taken_over;

    void progress(int percent) override
    {
        percents.push_back(percent);
    }
    bool checkpoint(std::string_view place) override
    {
        places.emplace_back(place);
        if (place == throw_at)
        {
            if (throw_runtime_error)
            {
                throw std::runtime_error("a failure nobody expected");
            }
            if (throw_length_error)
            {
                throw std::length_error("too long");
            }
            throw std::bad_alloc();
        }
        return place == stop_at;
    }
    void written(std::size_t members, std::size_t files, std::uint64_t bytes) override
    {
        written_called = true;
        written_members = members;
        written_files = files;
        written_bytes = bytes;
    }
    void checked(const adl::loader_core::MediaSetReport&) override
    {
        ++checked_calls;
    }
    void took_over_empty_folder(const fs::path& folder) override
    {
        taken_over.push_back(folder);
    }
};

} // namespace

TEST_CASE("make_set: one load on one member is a set that the loader's own check accepts", "[make]")
{
    adl::test::TempDir sources;
    adl::test::TempDir out;
    const SetPlan plan = one_load_plan(sources.path());
    RecordingHooks hooks;

    const auto made = make_set(plan, Destination::exact(out.path() / "set"), hooks);
    REQUIRE(made.has_value());
    CHECK(made->path == out.path() / "set");
    CHECK(made->members == 1);
    CHECK(made->data_files == 2);
    CHECK(made->support_files == 1);
    CHECK(made->source_bytes == 17 + 5 + 4);

    const auto report = adl::loader_core::check_media_set(out.path() / "set");
    REQUIRE(report.error.empty());
    CHECK(report.part_number == "ACM-SET-0001");
    CHECK(report.members == 1);
    REQUIRE(report.loads.size() == 1);
    CHECK(report.loads[0].part_number == "ACM47-1234-5678");
    CHECK(report.loads[0].header_file == "ACM.LUH");
    CHECK(report.loads[0].data_files == 2);
    CHECK(report.loads[0].support_files == 1);
    CHECK(report.loads[0].target_hardware_ids == std::vector<std::string>{"ACME"});
    CHECK(report.batches == 0);

    // The member holds the three lists and the load's own folder, and no file of the plan is anywhere else.
    CHECK(names_in(out.path() / "set") == std::vector<std::string>{"member_001"});
    const auto tree = tree_bytes(out.path() / "set");
    CHECK(tree.contains("member_001/LOADS.LUM"));
    CHECK(tree.contains("member_001/FILES.LUM"));
    CHECK(tree.contains("member_001/ACM4712345678/ACM.LUH"));
    CHECK(tree.contains("member_001/ACM4712345678/APP.LUP"));
    CHECK(tree.contains("member_001/ACM4712345678/DATA.LUP"));
    CHECK(tree.contains("member_001/ACM4712345678/CONFIG.LDR"));
    CHECK_FALSE(tree.contains("member_001/BATCHES.LUM"));

    // What the hooks were told.
    CHECK(hooks.written_called);
    CHECK(hooks.written_members == 1);
    CHECK(hooks.written_files == tree.size());
    CHECK(hooks.checked_calls == 1);
    REQUIRE_FALSE(hooks.percents.empty());
    CHECK(hooks.percents.back() == 100);
    CHECK(std::ranges::is_sorted(hooks.percents));
    CHECK(hooks.places == std::vector<std::string>{"read", "allocate", "read", "allocate", "read", "allocate", "load",
                                                   "write", "written", "verified"});
}

TEST_CASE("make_set: the data files of several members are dealt out as the window does", "[make]")
{
    adl::test::TempDir sources;
    adl::test::TempDir out;
    SetPlan plan = one_load_plan(sources.path(), 2);
    // A third data file, so that member 1 gets two and member 2 gets one.
    plan.loads[0].files.insert(
        plan.loads[0].files.begin() + 2,
        PlannedFile{write_file(sources.path(), "more.bin", "MORE"), "MORE.LUP", FileKind::data, CheckValueType::sha1});
    RecordingHooks hooks;
    REQUIRE(make_set(plan, Destination::exact(out.path() / "set"), hooks).has_value());

    const auto tree = tree_bytes(out.path() / "set");
    // The header file and the support file stay on member 1, in the part root directory.
    CHECK(tree.contains("member_001/ACM4712345678/ACM.LUH"));
    CHECK(tree.contains("member_001/ACM4712345678/CONFIG.LDR"));
    // The data files go round-robin, each into a folder of the part root's name, member 1 included.
    CHECK(tree.contains("member_001/ACM4712345678/ACM4712345678/APP.LUP"));
    CHECK(tree.contains("member_002/ACM4712345678/ACM4712345678/DATA.LUP"));
    CHECK(tree.contains("member_001/ACM4712345678/ACM4712345678/MORE.LUP"));
    // Every member has its own lists.
    CHECK(tree.contains("member_001/LOADS.LUM"));
    CHECK(tree.contains("member_002/LOADS.LUM"));
    CHECK(tree.contains("member_001/FILES.LUM"));
    CHECK(tree.contains("member_002/FILES.LUM"));

    const auto report = adl::loader_core::check_media_set(out.path() / "set");
    REQUIRE(report.error.empty());
    CHECK(report.members == 2);
}

TEST_CASE("make_set: two loads and a batch", "[make]")
{
    adl::test::TempDir sources;
    adl::test::TempDir out;
    SetPlan plan = one_load_plan(sources.path());
    PlannedLoad second;
    second.part_number = "ACM4C-0100-0200";
    second.header_file_name = "ACM4C.LUH";
    second.type = adl::arinc665::LoadType{"Test software", 4};
    second.target_hardware_ids = {"ACME", "ACMEFMS"};
    second.check = CheckValueType::sha1;
    // The same name as a file of the first load: each load has a folder of its own.
    second.files.push_back(PlannedFile{write_file(sources.path(), "app2.bin", "SECOND"), "APP.LUP", FileKind::data,
                                       CheckValueType::crc16});
    plan.loads.push_back(std::move(second));
    plan.batch = PlannedBatch{"ACM8E-0100-0300", "ACM.LUB", "both loads", derive_batch_targets(plan.loads)};
    RecordingHooks hooks;

    const auto made = make_set(plan, Destination::exact(out.path() / "set"), hooks);
    REQUIRE(made.has_value());
    const auto& report = made->report;
    CHECK(report.loads.size() == 2);
    CHECK(report.batches == 1);
    const auto tree = tree_bytes(out.path() / "set");
    CHECK(tree.contains("member_001/BATCHES.LUM"));
    CHECK(tree.contains("member_001/ACM.LUB"));
    CHECK(tree.contains("member_001/ACM4712345678/APP.LUP"));
    CHECK(tree.contains("member_001/ACM4C01000200/APP.LUP"));
}

TEST_CASE("make_set: every check value type is recorded and verifies", "[make]")
{
    for (const CheckValueType type : {CheckValueType::none, CheckValueType::crc8, CheckValueType::crc16,
                                      CheckValueType::crc32, CheckValueType::md5, CheckValueType::sha1})
    {
        INFO(check_type_word(type));
        adl::test::TempDir sources;
        adl::test::TempDir out;
        SetPlan plan = one_load_plan(sources.path());
        plan.files_check = type;
        plan.files_list_check = type;
        plan.loads[0].check = type;
        for (PlannedFile& file : plan.loads[0].files)
        {
            file.check = type;
        }
        RecordingHooks hooks;
        const auto made = make_set(plan, Destination::exact(out.path() / "set"), hooks);
        REQUIRE(made.has_value());
        // The only findings are the two data files that are not whole words (17 and 5 bytes).
        CHECK(made->report.findings.size() == 2);
    }
}

TEST_CASE("make_set: the same plan gives the same bytes every time", "[make]")
{
    adl::test::TempDir sources;
    adl::test::TempDir out;
    SetPlan plan = one_load_plan(sources.path(), 2);
    plan.batch = PlannedBatch{"ACM4C-0100-0200", "ACM.LUB", "", derive_batch_targets(plan.loads)};
    plan.files_check = CheckValueType::sha1;
    plan.files_list_check = CheckValueType::crc32;
    RecordingHooks hooks;
    REQUIRE(make_set(plan, Destination::exact(out.path() / "first"), hooks).has_value());
    REQUIRE(make_set(plan, Destination::exact(out.path() / "deeper" / "second"), hooks).has_value());
    CHECK(tree_bytes(out.path() / "first") == tree_bytes(out.path() / "deeper" / "second"));
}

TEST_CASE("make_set: a stop at any place leaves nothing", "[make]")
{
    for (const std::string place : {"read", "load", "write", "written", "verified"})
    {
        INFO(place);
        adl::test::TempDir sources;
        adl::test::TempDir out;
        const SetPlan plan = one_load_plan(sources.path());
        RecordingHooks hooks;
        hooks.stop_at = place;
        const auto made = make_set(plan, Destination::exact(out.path() / "set"), hooks);
        REQUIRE_FALSE(made.has_value());
        CHECK(made.error().kind == FailureKind::cancelled);
        // The stops after the write find the whole set on disk and take it away, and say so.
        const bool written = place == "written" || place == "verified";
        CHECK(made.error().discard.attempted == written);
        CHECK(made.error().discard.removed == written);
        CHECK(names_in(out.path()).empty());
    }
}

TEST_CASE("make_set: a file that cannot be allocated, and any other failure of memory, end the job", "[make]")
{
    adl::test::TempDir sources;
    adl::test::TempDir out;
    const SetPlan plan = one_load_plan(sources.path());

    RecordingHooks too_big;
    too_big.throw_at = "allocate";
    auto made = make_set(plan, Destination::exact(out.path() / "set"), too_big);
    REQUIRE_FALSE(made.has_value());
    CHECK(made.error().kind == FailureKind::source_does_not_fit);
    CHECK(made.error().path == plan.loads[0].files[0].source);

    RecordingHooks too_long;
    too_long.throw_at = "allocate";
    too_long.throw_length_error = true;
    made = make_set(plan, Destination::exact(out.path() / "set"), too_long);
    REQUIRE_FALSE(made.has_value());
    CHECK(made.error().kind == FailureKind::source_too_large);

    RecordingHooks elsewhere;
    elsewhere.throw_at = "load";
    made = make_set(plan, Destination::exact(out.path() / "set"), elsewhere);
    REQUIRE_FALSE(made.has_value());
    CHECK(made.error().kind == FailureKind::out_of_memory);

    RecordingHooks unexpected;
    unexpected.throw_at = "write";
    unexpected.throw_runtime_error = true;
    made = make_set(plan, Destination::exact(out.path() / "set"), unexpected);
    REQUIRE_FALSE(made.has_value());
    CHECK(made.error().kind == FailureKind::error);
    CHECK(made.error().detail == "a failure nobody expected");

    CHECK(names_in(out.path()).empty());
}

TEST_CASE("make_set: a source that cannot be read ends the job before anything is written", "[make]")
{
    adl::test::TempDir sources;
    adl::test::TempDir out;
    SetPlan plan = one_load_plan(sources.path());
    plan.loads[0].files[1].source = sources.path() / "missing.bin";
    RecordingHooks hooks;
    auto made = make_set(plan, Destination::exact(out.path() / "set"), hooks);
    REQUIRE_FALSE(made.has_value());
    CHECK(made.error().kind == FailureKind::source_unreadable);
    CHECK(made.error().path == sources.path() / "missing.bin");
    CHECK(names_in(out.path()).empty());

    // A folder is not a file that can be read.
    plan.loads[0].files[1].source = sources.path();
    made = make_set(plan, Destination::exact(out.path() / "set"), hooks);
    REQUIRE_FALSE(made.has_value());
    CHECK(made.error().kind == FailureKind::source_unreadable);
}

TEST_CASE("make_set: a place that is taken is refused before the files are read", "[make]")
{
    adl::test::TempDir sources;
    adl::test::TempDir out;
    SetPlan plan = one_load_plan(sources.path());
    RecordingHooks hooks;

    // A folder with something in it, and a file: both are taken, and what is there is not touched.
    write_file(out.path() / "set", "keep.txt", "mine");
    auto made = make_set(plan, Destination::exact(out.path() / "set"), hooks);
    REQUIRE_FALSE(made.has_value());
    CHECK(made.error().kind == FailureKind::output_exists);
    CHECK(hooks.places.empty()); // not one source was looked at
    CHECK(names_in(out.path() / "set") == std::vector<std::string>{"keep.txt"});

    write_file(out.path(), "file", "a file");
    made = make_set(plan, Destination::exact(out.path() / "file"), hooks);
    REQUIRE_FALSE(made.has_value());
    CHECK(made.error().kind == FailureKind::output_exists);
    CHECK(names_in(out.path()) == std::vector<std::string>{"file", "set"});
}

TEST_CASE("make_set: an empty folder is taken over, and a folder that does not exist is made", "[make]")
{
    adl::test::TempDir sources;
    adl::test::TempDir out;
    const SetPlan plan = one_load_plan(sources.path());
    fs::create_directories(out.path() / "empty");
    RecordingHooks hooks;

    REQUIRE(make_set(plan, Destination::exact(out.path() / "empty"), hooks).has_value());
    CHECK(hooks.taken_over == std::vector<fs::path>{out.path() / "empty"});
    CHECK(names_in(out.path() / "empty") == std::vector<std::string>{"member_001"});

    RecordingHooks fresh;
    REQUIRE(make_set(plan, Destination::exact(out.path() / "a" / "b" / "c"), fresh).has_value());
    CHECK(fresh.taken_over.empty());
    CHECK(names_in(out.path() / "a" / "b" / "c") == std::vector<std::string>{"member_001"});
    // Nothing is left beside them.
    CHECK(names_in(out.path() / "a" / "b") == std::vector<std::string>{"c"});
}

TEST_CASE("make_set: the first free name is taken, and nothing is overwritten", "[make]")
{
    adl::test::TempDir sources;
    adl::test::TempDir out;
    const SetPlan plan = one_load_plan(sources.path());
    write_file(out.path() / "ACM-SET-0001", "keep.txt", "mine");
    RecordingHooks hooks;

    const auto made = make_set(plan, Destination::first_free(out.path(), "ACM-SET-0001"), hooks);
    REQUIRE(made.has_value());
    CHECK(made->path == out.path() / "ACM-SET-0001_2");
    CHECK(names_in(out.path() / "ACM-SET-0001") == std::vector<std::string>{"keep.txt"});
    CHECK(names_in(out.path()) == std::vector<std::string>{"ACM-SET-0001", "ACM-SET-0001_2"});
}

TEST_CASE("make_set: unfinished sets beside the place are never taken over, and there is a limit", "[make]")
{
    adl::test::TempDir sources;
    adl::test::TempDir out;
    const SetPlan plan = one_load_plan(sources.path());
    // A run that was killed left its folder: the next one writes beside it.
    fs::create_directories(out.path() / "set.writing");
    RecordingHooks hooks;
    REQUIRE(make_set(plan, Destination::exact(out.path() / "set"), hooks).has_value());
    CHECK(names_in(out.path()) == std::vector<std::string>{"set", "set.writing"});

    adl::test::TempDir crowded;
    fs::create_directories(crowded.path() / "set.writing");
    for (int attempt = 2; attempt <= 100; ++attempt)
    {
        fs::create_directories(crowded.path() / ("set.writing_" + std::to_string(attempt)));
    }
    const auto made = make_set(plan, Destination::exact(crowded.path() / "set"), hooks);
    REQUIRE_FALSE(made.has_value());
    CHECK(made.error().kind == FailureKind::too_many_unfinished);
    CHECK(made.error().path == fs::path{"set"});
}

TEST_CASE("make_set: a folder that cannot be made says where", "[make]")
{
    adl::test::TempDir sources;
    adl::test::TempDir out;
    const SetPlan plan = one_load_plan(sources.path());
    const fs::path blocker = write_file(out.path(), "blocker", "a file where a folder is wanted");
    RecordingHooks hooks;

    const auto made = make_set(plan, Destination::exact(blocker / "set"), hooks);
    REQUIRE_FALSE(made.has_value());
    CHECK(made.error().kind == FailureKind::not_written_below);
    CHECK(made.error().path == blocker);
}

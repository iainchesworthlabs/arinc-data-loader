// SPDX-License-Identifier: GPL-3.0-or-later
// An upload, the whole command: the media set is read and checked on the disk, as the window does, and sent
// to the simulated target from the fake file system. The program refuses a set that cannot be read or that
// changed, before the target is asked anything, and tells wrong usage that only the set can show (a load it
// does not hold) with no event at all.

#include "command_rig.hpp"
#include "editing_host.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

using adl::test::CommandRig;
using adl::test::CommandRun;
using adl::test::InstalledMediaSet;
using adl::test::Json;
using adl::test::SimPlan;

namespace
{

constexpr std::int64_t wall_origin = 1'795'083'330;

CommandRun upload(CommandRig& rig, const InstalledMediaSet& set, std::initializer_list<std::string_view> loads,
                  std::initializer_list<std::string_view> more = {})
{
    const std::string path = set.path();
    std::vector<std::string_view> arguments{"upload",   "--target",    "10.0.0.2", "--position",
                                            "HNPFMS_L", "--media-set", path,       "--json"};
    arguments.insert(arguments.end(), loads.begin(), loads.end());
    arguments.insert(arguments.end(), more.begin(), more.end());
    rig.rig.world.set_wall_origin(wall_origin);
    return rig.run(arguments);
}

bool contains(const std::vector<std::string>& names, const std::string& name)
{
    return std::ranges::find(names, name) != names.end();
}

} // namespace

TEST_CASE("an upload of one load completes, and the events say what was checked and how it went",
          "[load][integration][upload]")
{
    const adl::test::SampleLoad load = adl::test::make_sample_load(5'000, 3);
    SimPlan plan;
    plan.loads = {load.item};
    CommandRig rig(plan);
    const InstalledMediaSet set(rig.rig.loader_host, load.members);

    const CommandRun run = upload(rig, set, {"--load", adl::samples::sample_load_part_number});
    CHECK(run.exit_code == 0);
    CHECK(run.err.empty());
    adl::test::check_stream(run);

    const std::vector<Json> events = run.events();
    CHECK(events.front().at("command").text == "upload");
    CHECK(events.front().at("target").text == "10.0.0.2");

    // `media_set_checked` is the second event: the set was read before anything was sent.
    const std::vector<std::string> names = run.names();
    CHECK(names[1] == "media_set_checked");
    const Json& checked = events[1];
    CHECK(checked.keys() ==
          std::vector<std::string>{"event", "t", "path", "part_number", "members", "loads", "findings"});
    CHECK(checked.at("path").text == set.path());
    CHECK(checked.at("part_number").text == adl::samples::sample_media_set_part_number);
    CHECK(checked.at("members").as_number() == 2);
    CHECK(checked.at("loads").as_number() == 1);
    CHECK(checked.at("findings").is(Json::Kind::array));

    // Progress, on each status the target sends, ending with the load completed.
    const std::vector<Json> progress = run.events_named("progress");
    REQUIRE_FALSE(progress.empty());
    const Json& last = progress.back();
    CHECK(last.keys() == std::vector<std::string>{"event", "t", "percent", "loads"});
    REQUIRE(last.at("loads").elements.size() == 1);
    CHECK(last.at("loads").elements[0].at("part_number").text == adl::samples::sample_load_part_number);
    CHECK(last.at("loads").elements[0].at("header_file").text == "ACM47.LUH");
    CHECK(last.at("loads").elements[0].at("status").text == "completed");
    CHECK(last.at("loads").elements[0].at("percent").as_number() == 100);
    CHECK(last.at("percent").as_number() == 100);

    const Json finished = run.finished();
    CHECK(finished.at("reason").text == "completed");
    CHECK(finished.at("status_code").as_number() == 3);
    CHECK(finished.at("detail").text ==
          "The operation completed. The target read " + std::to_string(load.item.reads.size()) + " file(s).");

    // What the target got is the load, every file of it.
    CHECK(rig.rig.target.log().files_read.size() == load.item.reads.size());
    CHECK(rig.rig.target.log().files_corrupt.empty());
}

TEST_CASE("an upload of all loads takes every load of the set, and --load takes only the one named",
          "[load][integration][upload]")
{
    const adl::test::TwoLoadSet two = adl::test::make_two_load_set();
    SimPlan plan;
    plan.loads = {two.first, two.second};

    SECTION("--all-loads")
    {
        CommandRig rig(plan);
        const InstalledMediaSet set(rig.rig.loader_host, two.members);
        const CommandRun run = upload(rig, set, {"--all-loads"});
        CHECK(run.exit_code == 0);
        adl::test::check_stream(run);
        CHECK(run.events_named("media_set_checked")[0].at("loads").as_number() == 2);
        const Json last = run.events_named("progress").back();
        REQUIRE(last.at("loads").elements.size() == 2);
        CHECK(last.at("loads").elements[0].at("status").text == "completed");
        CHECK(last.at("loads").elements[1].at("status").text == "completed");
        CHECK(rig.rig.target.log().files_read.size() == two.first.reads.size() + two.second.reads.size());
    }
    SECTION("--load names one of the two")
    {
        CommandRig rig(plan);
        const InstalledMediaSet set(rig.rig.loader_host, two.members);
        const CommandRun run = upload(rig, set, {"--load", adl::test::second_load_part_number});
        CHECK(run.exit_code == 0);
        // The target is asked for the one load, and reads the files of that load alone.
        CHECK(rig.rig.target.log().files_read.size() == two.second.reads.size());
        CHECK(contains(rig.rig.target.log().files_read, "ACM48.LUH"));
        CHECK_FALSE(contains(rig.rig.target.log().files_read, "ACM47.LUH"));
    }
    SECTION("--load names both, in the order given")
    {
        CommandRig rig(plan);
        const InstalledMediaSet set(rig.rig.loader_host, two.members);
        const CommandRun run = upload(
            rig, set, {"--load", adl::test::second_load_part_number, "--load", adl::samples::sample_load_part_number});
        CHECK(run.exit_code == 0);
        // Asked for in the order given: the second load's header file is the first thing the target reads.
        REQUIRE_FALSE(rig.rig.target.log().files_read.empty());
        CHECK(rig.rig.target.log().files_read.front() == "ACM48.LUH");
        CHECK(rig.rig.target.log().files_read.size() == two.first.reads.size() + two.second.reads.size());
    }
}

TEST_CASE("an upload in which the target fails a load is load_failed", "[load][integration][upload]")
{
    const adl::test::SampleLoad load = adl::test::make_sample_load(2'000, 4);
    SimPlan plan;
    plan.loads = {load.item};
    plan.failed_loads[load.item.header_name] = "flash is write protected";
    CommandRig rig(plan);
    const InstalledMediaSet set(rig.rig.loader_host, load.members);

    const CommandRun run = upload(rig, set, {"--all-loads"});
    CHECK(run.exit_code == 1);
    adl::test::check_stream(run);
    const Json finished = run.finished();
    CHECK(finished.at("result").text == "failed");
    CHECK(finished.at("reason").text == "load_failed");
    CHECK(finished.at("detail").text == std::string{"The upload ended and the target failed 1 load(s): "} +
                                            adl::samples::sample_load_part_number + ".");
    // The failure is in the log with the target's reason.
    bool told = false;
    for (const Json& log : run.events_named("log"))
    {
        told = told || log.at("text").text.find("flash is write protected") != std::string::npos;
    }
    CHECK(told);
    const Json last = run.events_named("progress").back();
    CHECK(last.at("loads").elements[0].at("status").text == "item_failed");
}

TEST_CASE("an upload that the target refuses, ends or does not answer", "[load][integration][upload]")
{
    const adl::test::SampleLoad load = adl::test::make_sample_load(2'000, 4);
    SimPlan plan;
    plan.loads = {load.item};

    SECTION("refused")
    {
        plan.init_status = adl::arinc615a::StatusCode::operation_denied;
        plan.init_text = "busy";
        CommandRig rig(plan);
        const InstalledMediaSet set(rig.rig.loader_host, load.members);
        const CommandRun run = upload(rig, set, {"--all-loads"});
        CHECK(run.exit_code == 1);
        const Json finished = run.finished();
        CHECK(finished.at("reason").text == "target_refused");
        CHECK(finished.at("status_code").as_number() == 0x1000);
        CHECK(run.events_named("progress").empty());
        // The set was read and checked all the same: the refusal is the target's.
        CHECK(run.events_named("media_set_checked").size() == 1);
        CHECK(rig.rig.target.log().files_read.empty());
    }
    SECTION("ended by the target")
    {
        plan.abort_after_statuses = 1;
        CommandRig rig(plan);
        const InstalledMediaSet set(rig.rig.loader_host, load.members);
        const CommandRun run = upload(rig, set, {"--all-loads"});
        CHECK(run.exit_code == 1);
        CHECK(run.finished().at("reason").text == "target_aborted");
    }
    SECTION("no answer")
    {
        plan.init_silent = true;
        CommandRig rig(plan);
        const InstalledMediaSet set(rig.rig.loader_host, load.members);
        const CommandRun run = upload(rig, set, {"--all-loads"});
        CHECK(run.exit_code == 4);
        CHECK(run.finished().at("reason").text == "no_answer");
    }
}

TEST_CASE("a media set that cannot be read is refused, exit code 3", "[load][integration][upload]")
{
    const adl::test::SampleLoad load = adl::test::make_sample_load(2'000, 4);
    SimPlan plan;
    plan.loads = {load.item};

    SECTION("there is no such folder")
    {
        CommandRig rig(plan);
        const adl::test::TempDir empty;
        const std::string missing = (empty.path() / "no-such-set").string();
        rig.rig.world.set_wall_origin(wall_origin);
        const CommandRun run = rig.run({"upload", "--target", "10.0.0.2", "--position", "HNPFMS_L", "--media-set",
                                        missing, "--all-loads", "--json"});
        CHECK(run.exit_code == 3);
        adl::test::check_stream(run);
        const std::vector<std::string> names = run.names();
        CHECK(names.front() == "started");
        CHECK(names.back() == "finished");
        CHECK(run.events_named("media_set_checked").empty());
        CHECK(run.events_named("progress").empty());
        const Json finished = run.finished();
        CHECK(finished.at("result").text == "failed");
        CHECK(finished.at("reason").text == "media_set_unreadable");
        CHECK(finished.at("status_code").is(Json::Kind::null));
        CHECK(finished.at("detail").text.starts_with("The media set could not be read: "));
        // The target was not asked anything.
        CHECK(rig.rig.target.log().init_requests_at.empty());
    }
    SECTION("a file of it is damaged")
    {
        CommandRig rig(plan);
        const InstalledMediaSet set(rig.rig.loader_host, load.members);
        // The header file on the disk loses its last bytes.
        std::error_code error;
        std::vector<std::filesystem::path> found;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(set.path(), error))
        {
            if (entry.path().filename() == "ACM47.LUH")
            {
                found.push_back(entry.path());
            }
        }
        REQUIRE(found.size() == 1);
        std::filesystem::resize_file(found[0], std::filesystem::file_size(found[0]) - 4);
        const CommandRun run = upload(rig, set, {"--all-loads"});
        CHECK(run.exit_code == 3);
        CHECK(run.finished().at("reason").text == "media_set_unreadable");
        CHECK(run.events_named("media_set_checked").empty());
    }
}

TEST_CASE("a media set that changed after it was checked is not sent, exit code 3", "[load][integration][upload]")
{
    const adl::test::SampleLoad load = adl::test::make_sample_load(2'000, 4);
    SimPlan plan;
    plan.loads = {load.item};
    CommandRig rig(plan);
    const InstalledMediaSet set(rig.rig.loader_host, load.members);

    // The file the program sends is not the one it checked: the set was edited between the two.
    const std::string& edited = set.files().front();
    rig.rig.loader_host.fake_files().put(edited, "edited after the check");
    const CommandRun run = upload(rig, set, {"--all-loads"});
    CHECK(run.exit_code == 3);
    adl::test::check_stream(run);
    CHECK(run.events_named("media_set_checked").size() == 1);
    const Json finished = run.finished();
    CHECK(finished.at("reason").text == "media_set_changed");
    CHECK(finished.at("detail").text.starts_with("The media set changed after it was checked: "));
    // Not a byte went to the target, and it was not asked anything.
    CHECK(rig.rig.target.log().init_requests_at.empty());
    CHECK(rig.rig.target.log().files_read.empty());
}

TEST_CASE("a media set that changes while it is being sent stops the upload, exit code 3",
          "[load][integration][upload]")
{
    const adl::test::SampleLoad load = adl::test::make_sample_load(2'000, 4);
    SimPlan plan;
    plan.loads = {load.item};
    CommandRig rig(plan);
    const InstalledMediaSet set(rig.rig.loader_host, load.members);

    // The set is as it was checked when the program looks again before it starts, and the header file is edited
    // as the target asks for it: the program opened every file of the set once, and this is the next open.
    adl::test::EditingHost host(rig.rig.loader_host);
    const auto header =
        std::ranges::find_if(set.files(), [](const std::string& path) { return path.ends_with("ACM47.LUH"); });
    REQUIRE(header != set.files().end());
    host.edit_after_opens(static_cast<unsigned>(set.files().size()), *header, "edited as it was being sent");

    const std::string path = set.path();
    const CommandRun run = rig.run_on(host, {"upload", "--target", "10.0.0.2", "--position", "HNPFMS_L", "--media-set",
                                             path, "--all-loads", "--json"});
    CHECK(run.exit_code == 3);
    adl::test::check_stream(run);
    const Json finished = run.finished();
    CHECK(finished.at("result").text == "failed");
    CHECK(finished.at("reason").text == "media_set_changed");
    CHECK(finished.at("detail").text.starts_with("The media set changed after it was checked: ACM47.LUH"));
    // The target was reached, and was not given the edited file.
    CHECK_FALSE(rig.rig.target.log().init_requests_at.empty());
    CHECK(rig.rig.target.log().files_read.empty());
}

TEST_CASE("a media set with no load, or with a part number that names two, cannot be chosen from",
          "[load][integration][upload]")
{
    SECTION("no load")
    {
        auto spec = adl::samples::sample_media_set_spec();
        REQUIRE(spec.has_value());
        spec->loads.clear();
        spec->batches.clear();
        const auto members = adl::arinc665::build_media_set(*spec);
        REQUIRE(members.has_value());
        CommandRig rig;
        const InstalledMediaSet set(rig.rig.loader_host, *members);
        const CommandRun run = upload(rig, set, {"--all-loads"});
        CHECK(run.exit_code == 2);
        CHECK(run.out.empty());
        CHECK(run.err == "adl_load: The media set holds no load to send.\n");
    }
    SECTION("two loads of one part number")
    {
        auto spec = adl::samples::sample_media_set_spec();
        REQUIRE(spec.has_value());
        adl::arinc665::LoadSpec second = adl::samples::sample_load_spec();
        second.header_file_name = "ACM49.LUH";
        second.data_files[0].name = "APP49.LUP";
        second.data_files[1].name = "DATA49.LUP";
        second.support_files[0].name = "CONFIG49.LDR";
        auto built = adl::arinc665::build_load(second);
        REQUIRE(built.has_value());
        adl::arinc665::PlacedLoad placed;
        placed.load = std::move(*built);
        placed.part_root_directory = "\\SECOND\\";
        placed.header_member = 1;
        spec->loads.push_back(std::move(placed));
        const auto members = adl::arinc665::build_media_set(*spec);
        REQUIRE(members.has_value());
        CommandRig rig;
        const InstalledMediaSet set(rig.rig.loader_host, *members);
        const CommandRun run = upload(rig, set, {"--load", adl::samples::sample_load_part_number});
        CHECK(run.exit_code == 2);
        CHECK(run.out.empty());
        CHECK(run.err == std::string{"adl_load: The part number \""} + adl::samples::sample_load_part_number +
                             "\" names more than one load of the media set.\n");
    }
}

TEST_CASE("wrong usage that only the media set shows is told on standard error, with no event",
          "[load][integration][upload]")
{
    const adl::test::SampleLoad load = adl::test::make_sample_load(2'000, 4);
    SimPlan plan;
    plan.loads = {load.item};
    CommandRig rig(plan);
    const InstalledMediaSet set(rig.rig.loader_host, load.members);

    const CommandRun run = upload(rig, set, {"--load", "NOT-IN-THIS-SET"});
    CHECK(run.exit_code == 2);
    CHECK(run.out.empty());
    CHECK(run.err == "adl_load: The media set has no load with the part number \"NOT-IN-THIS-SET\".\n");
    CHECK(rig.rig.target.log().init_requests_at.empty());
}

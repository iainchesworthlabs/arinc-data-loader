// SPDX-License-Identifier: GPL-3.0-or-later
// The operation record, written by the whole command with --record: its bytes for each kind of operation and
// for the ways one can end, compared with files that have been read and kept (tests/load/golden), the same
// bytes for the same events, and what happens when it cannot be written. The record is the one of
// docs/file-reference.md; what it holds is what the recorder of loader_core filled in, with what only the
// program knows added.

#include "bytes.hpp"
#include "command_rig.hpp"
#include "utf8_path.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using adl::test::CommandRig;
using adl::test::CommandRun;
using adl::test::DownloadItem;
using adl::test::InstalledMediaSet;
using adl::test::Json;
using adl::test::SimPlan;

namespace
{

// 2026-11-19T10:15:30Z: the time of the contract's own example, which the record's times count on from.
constexpr std::int64_t wall_origin = 1'795'083'330;

SimPlan holding(std::initializer_list<const char*> names)
{
    SimPlan plan;
    for (const char* name : names)
    {
        plan.files.push_back(
            DownloadItem{name, adl::test::bytes_of(std::string{"content of "} + name), std::string{"the "} + name});
    }
    return plan;
}

CommandRun with_record(CommandRig& rig, std::vector<std::string_view> arguments, std::string_view record = "/rec.json")
{
    arguments.push_back("--json");
    arguments.push_back("--record");
    arguments.push_back(record);
    rig.rig.world.set_wall_origin(wall_origin);
    return rig.run(arguments);
}

std::string record_of(CommandRig& rig, std::string_view path = "/rec.json")
{
    return adl::test::fake_text(rig.rig.loader_host, path);
}

} // namespace

TEST_CASE("the record of an Information operation", "[load][integration][record]")
{
    CommandRig rig;
    const CommandRun run = with_record(rig, {"information", "--target", "10.0.0.2", "--position", "HNPFMS_L"});
    CHECK(run.exit_code == 0);
    adl::test::check_golden("record_information.json", record_of(rig));

    // The event says where it is, and it is valid JSON that ends in a line feed.
    const Json finished = run.finished();
    CHECK(finished.at("record").text == "/rec.json");
    CHECK(finished.keys() ==
          std::vector<std::string>{"event", "t", "result", "reason", "status_code", "detail", "exit_code", "record"});
    const std::string text = record_of(rig);
    CHECK(text.back() == '\n');
    CHECK(adl::test::parse_json(text).has_value());
    // No temporary file is left beside it.
    CHECK_FALSE(rig.rig.loader_host.fake_files().get("/rec.json.part").has_value());
}

TEST_CASE("the same events write the same record, byte for byte", "[load][integration][record]")
{
    const auto record_of_a_run = []
    {
        CommandRig rig;
        const CommandRun result = with_record(rig, {"information", "--target", "10.0.0.2", "--position", "HNPFMS_L"});
        REQUIRE(result.exit_code == 0);
        return record_of(rig);
    };
    const std::string first = record_of_a_run();
    CHECK_FALSE(first.empty());
    CHECK(record_of_a_run() == first);
}

TEST_CASE("the record of an upload holds the media set and the loads as the target last reported them",
          "[load][integration][record]")
{
    const adl::test::SampleLoad load = adl::test::make_sample_load(3'000, 3);
    SimPlan plan;
    plan.loads = {load.item};
    CommandRig rig(plan);
    const InstalledMediaSet set(rig.rig.loader_host, load.members);
    const std::string path = set.path();
    const CommandRun run = with_record(
        rig, {"upload", "--target", "10.0.0.2", "--position", "HNPFMS_L", "--media-set", path, "--all-loads"});
    CHECK(run.exit_code == 0);
    adl::test::check_golden("record_upload.json", record_of(rig), path);
}

TEST_CASE("the record of a download holds the files and the folder they were written into",
          "[load][integration][record]")
{
    SECTION("the loader names the files")
    {
        CommandRig rig(holding({"A.BIN", "B.BIN"}));
        const CommandRun run = with_record(rig, {"download", "--target", "10.0.0.2", "--position", "HNPFMS_L", "--into",
                                                 "/dl", "--file", "A.BIN", "--file", "B.BIN"});
        CHECK(run.exit_code == 0);
        adl::test::check_golden("record_download_media.json", record_of(rig));
    }
    SECTION("the target lists its files and --pick-all takes them")
    {
        CommandRig rig(holding({"A.BIN", "B.BIN"}));
        const CommandRun run = with_record(
            rig, {"download", "--target", "10.0.0.2", "--position", "HNPFMS_L", "--into", "/dl", "--pick-all"});
        CHECK(run.exit_code == 0);
        adl::test::check_golden("record_download_operator.json", record_of(rig));
    }
    SECTION("a download that received nothing keeps no folder")
    {
        CommandRig rig(holding({"A.BIN"}));
        const CommandRun run = with_record(rig, {"download", "--target", "10.0.0.2", "--position", "HNPFMS_L", "--into",
                                                 "/dl", "--file", "NOT-HELD.BIN"});
        CHECK(run.exit_code == 1);
        adl::test::check_golden("record_download_nothing.json", record_of(rig));
    }
}

TEST_CASE("a record is written whatever the outcome", "[load][integration][record]")
{
    SECTION("the target refused: the status code is the refusal's")
    {
        SimPlan plan;
        plan.init_status = adl::arinc615a::StatusCode::operation_denied;
        plan.init_text = "not while in flight";
        CommandRig rig(plan);
        const CommandRun run = with_record(rig, {"information", "--target", "10.0.0.2", "--position", "HNPFMS_L"});
        CHECK(run.exit_code == 1);
        adl::test::check_golden("record_refused.json", record_of(rig));
    }
    SECTION("the target did not answer")
    {
        SimPlan plan;
        plan.init_silent = true;
        CommandRig rig(plan);
        const CommandRun run = with_record(rig, {"information", "--target", "10.0.0.2", "--position", "HNPFMS_L"});
        CHECK(run.exit_code == 4);
        adl::test::check_golden("record_no_answer.json", record_of(rig));
    }
    SECTION("the media set could not be read")
    {
        CommandRig rig;
        const adl::test::TempDir empty;
        const std::string missing = adl::test::utf8(empty.path() / "no-such-set");
        const CommandRun run = with_record(
            rig, {"upload", "--target", "10.0.0.2", "--position", "HNPFMS_L", "--media-set", missing, "--all-loads"});
        CHECK(run.exit_code == 3);
        adl::test::check_golden("record_media_set_unreadable.json", record_of(rig), adl::test::utf8(empty.path()));
    }
}

TEST_CASE("a record that cannot be written is a local error, and the operation's own reason is in the detail",
          "[load][integration][record]")
{
    SECTION("the folder is not there")
    {
        CommandRig rig;
        const CommandRun run = with_record(rig, {"information", "--target", "10.0.0.2", "--position", "HNPFMS_L"},
                                           "/no/such/dir/rec.json");
        CHECK(run.exit_code == 5);
        adl::test::check_stream(run);
        const Json finished = run.finished();
        CHECK(finished.at("result").text == "failed");
        CHECK(finished.at("reason").text == "local_error");
        CHECK(finished.at("exit_code").as_number() == 5);
        CHECK(finished.at("status_code").as_number() == 3);
        CHECK(finished.at("detail").text ==
              "The operation record could not be written: cannot create /no/such/dir/rec.json.part (not found). "
              "The operation had ended with the reason completed.");
        CHECK(finished.at("record").is(Json::Kind::null));
        // The operation itself ran to its end: the events say so before the finish.
        CHECK(run.events_named("configuration").size() == 1);
    }
    SECTION("the name is a folder")
    {
        CommandRig rig;
        rig.rig.loader_host.fake_files().put("/rec.json/inside", "x");
        const CommandRun run = with_record(rig, {"information", "--target", "10.0.0.2", "--position", "HNPFMS_L"});
        CHECK(run.exit_code == 5);
        const Json finished = run.finished();
        CHECK(finished.at("reason").text == "local_error");
        CHECK(finished.at("detail").text.starts_with(
            "The operation record could not be written: cannot replace /rec.json"));
        CHECK(finished.at("record").is(Json::Kind::null));
        // The temporary file was cleared away.
        CHECK_FALSE(rig.rig.loader_host.fake_files().get("/rec.json.part").has_value());
    }
    SECTION("an operation that failed anyway says both")
    {
        SimPlan plan;
        plan.init_status = adl::arinc615a::StatusCode::operation_denied;
        CommandRig rig(plan);
        const CommandRun run = with_record(rig, {"information", "--target", "10.0.0.2", "--position", "HNPFMS_L"},
                                           "/no/such/dir/rec.json");
        CHECK(run.exit_code == 5);
        CHECK(run.finished().at("detail").text.ends_with("The operation had ended with the reason target_refused."));
        // The log says it too.
        bool told = false;
        for (const Json& log : run.events_named("log"))
        {
            told = told || (log.at("severity").text == "error" &&
                            log.at("text").text.starts_with("The operation record could not be written: "));
        }
        CHECK(told);
    }
}

TEST_CASE("without --record there is no record and no key for it", "[load][integration][record]")
{
    CommandRig rig;
    const CommandRun run = rig.run({"information", "--target", "10.0.0.2", "--position", "HNPFMS_L", "--json"});
    CHECK(run.finished().find("record") == nullptr);
}

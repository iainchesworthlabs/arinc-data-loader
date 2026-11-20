// SPDX-License-Identifier: GPL-3.0-or-later
// A download, the whole command, in both kinds: the one in which the loader names the files (--file) and the
// one in which the target lists its files and the command chooses from the list (--pick, --pick-all). The files
// go into a folder of their own inside the one named, in the fake file system.

#include "bytes.hpp"
#include "command_rig.hpp"
#include "faulty_host.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

using adl::test::CommandRig;
using adl::test::CommandRun;
using adl::test::DownloadItem;
using adl::test::Json;
using adl::test::SimPlan;

namespace
{

constexpr std::int64_t wall_origin = 1'795'083'330;

// The target holds these files.
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

CommandRun download(CommandRig& rig, std::initializer_list<std::string_view> choice,
                    std::initializer_list<std::string_view> more = {})
{
    std::vector<std::string_view> arguments{"download", "--target", "10.0.0.2", "--position",
                                            "HNPFMS_L", "--into",   "/dl",      "--json"};
    arguments.insert(arguments.end(), choice.begin(), choice.end());
    arguments.insert(arguments.end(), more.begin(), more.end());
    rig.rig.world.set_wall_origin(wall_origin);
    return rig.run(arguments);
}

// The folder a download made, as the operating system writes it.
std::string made_folder(int number = 1)
{
    std::filesystem::path path{"/dl/DNLD_DATA_HNPFMS_L_" + std::to_string(number)};
    return path.make_preferred().string();
}

std::string file_path(const std::string& name, int number = 1)
{
    return (std::filesystem::path{made_folder(number)} / name).string();
}

bool has_event(const std::vector<std::string>& names, const std::string& name)
{
    return std::ranges::find(names, name) != names.end();
}

} // namespace

TEST_CASE("a download in which the loader names the files writes them into a folder of their own",
          "[load][integration][download]")
{
    CommandRig rig(holding({"A.BIN", "B.BIN", "C.BIN"}));
    const CommandRun run = download(rig, {"--file", "A.BIN", "--file", "C.BIN"});
    CHECK(run.exit_code == 0);
    CHECK(run.err.empty());
    adl::test::check_stream(run);

    CHECK(run.events().front().at("command").text == "download");
    // Nothing is listed: the files were named.
    CHECK_FALSE(has_event(run.names(), "file_list"));

    const std::vector<Json> received = run.events_named("file_received");
    REQUIRE(received.size() == 2);
    CHECK(received[0].keys() == std::vector<std::string>{"event", "t", "name", "size", "path"});
    CHECK(received[0].at("name").text == "A.BIN");
    CHECK(received[0].at("size").as_number() == std::string{"content of A.BIN"}.size());
    CHECK(received[0].at("path").text == file_path("A.BIN"));
    CHECK(received[1].at("name").text == "C.BIN");

    const Json finished = run.finished();
    CHECK(finished.at("reason").text == "completed");
    CHECK(finished.at("status_code").as_number() == 3);
    CHECK(finished.at("download_folder").text == made_folder());
    CHECK(finished.at("detail").text == "The operation completed. Received 2 file(s).");
    CHECK(finished.keys() == std::vector<std::string>{"event", "t", "result", "reason", "status_code", "detail",
                                                      "exit_code", "download_folder"});

    // The files are where the events say, and only the files asked for.
    CHECK(adl::test::fake_text(rig.rig.loader_host, "/dl/DNLD_DATA_HNPFMS_L_1/A.BIN") == "content of A.BIN");
    CHECK(adl::test::fake_text(rig.rig.loader_host, "/dl/DNLD_DATA_HNPFMS_L_1/C.BIN") == "content of C.BIN");
    CHECK_FALSE(rig.rig.loader_host.fake_files().get("/dl/DNLD_DATA_HNPFMS_L_1/B.BIN").has_value());
}

TEST_CASE("a second download goes into the next folder", "[load][integration][download]")
{
    CommandRig rig(holding({"A.BIN"}));
    CHECK(download(rig, {"--file", "A.BIN"}).finished().at("download_folder").text == made_folder(1));
    rig.rig.target.reset_operation();
    const CommandRun again = download(rig, {"--file", "A.BIN"});
    CHECK(again.exit_code == 0);
    CHECK(again.finished().at("download_folder").text == made_folder(2));
}

TEST_CASE("a file the target does not have is missing_files, and the files that came are kept",
          "[load][integration][download]")
{
    CommandRig rig(holding({"A.BIN"}));
    const CommandRun run = download(rig, {"--file", "A.BIN", "--file", "NOT-HELD.BIN"});
    CHECK(run.exit_code == 1);
    adl::test::check_stream(run);
    const Json finished = run.finished();
    CHECK(finished.at("result").text == "failed");
    CHECK(finished.at("reason").text == "missing_files");
    CHECK(finished.at("detail").text.starts_with("Received 1 of 2 requested file(s)."));
    CHECK(finished.at("download_folder").text == made_folder());
    CHECK(run.events_named("file_received").size() == 1);
    CHECK(adl::test::fake_text(rig.rig.loader_host, "/dl/DNLD_DATA_HNPFMS_L_1/A.BIN") == "content of A.BIN");
}

TEST_CASE("a download that received nothing leaves no folder, and says so", "[load][integration][download]")
{
    SECTION("the target holds none of what was asked")
    {
        CommandRig rig(holding({"A.BIN"}));
        const CommandRun run = download(rig, {"--file", "NOT-HELD.BIN"});
        CHECK(run.exit_code == 1);
        const Json finished = run.finished();
        CHECK(finished.at("reason").text == "missing_files");
        CHECK(finished.at("download_folder").is(Json::Kind::null));
        bool told = false;
        for (const Json& log : run.events_named("log"))
        {
            told = told || log.at("text").text.starts_with("Nothing was received, so the empty download folder ");
        }
        CHECK(told);
        const auto listed = rig.rig.loader_host.fake_files().stat("/dl/DNLD_DATA_HNPFMS_L_1");
        CHECK_FALSE(listed.has_value());
    }
    SECTION("the target refuses")
    {
        SimPlan plan = holding({"A.BIN"});
        plan.init_status = adl::arinc615a::StatusCode::operation_denied;
        plan.init_text = "not now";
        CommandRig rig(plan);
        const CommandRun run = download(rig, {"--file", "A.BIN"});
        CHECK(run.exit_code == 1);
        const Json finished = run.finished();
        CHECK(finished.at("reason").text == "target_refused");
        CHECK(finished.at("status_code").as_number() == 0x1000);
        CHECK(finished.at("download_folder").is(Json::Kind::null));
    }
    SECTION("the target does not answer")
    {
        SimPlan plan = holding({"A.BIN"});
        plan.init_silent = true;
        CommandRig rig(plan);
        const CommandRun run = download(rig, {"--file", "A.BIN"});
        CHECK(run.exit_code == 4);
        CHECK(run.finished().at("reason").text == "no_answer");
        CHECK(run.finished().at("download_folder").is(Json::Kind::null));
    }
}

TEST_CASE("a download in which the target lists its files writes file_list, then takes what --pick names",
          "[load][integration][download]")
{
    CommandRig rig(holding({"A.BIN", "B.BIN", "C.BIN"}));
    const CommandRun run = download(rig, {"--pick", "C.BIN", "--pick", "A.BIN"});
    CHECK(run.exit_code == 0);
    adl::test::check_stream(run);

    const std::vector<Json> listed = run.events_named("file_list");
    REQUIRE(listed.size() == 1);
    CHECK(listed[0].keys() == std::vector<std::string>{"event", "t", "files"});
    REQUIRE(listed[0].at("files").elements.size() == 3);
    CHECK(listed[0].at("files").elements[0].keys() == std::vector<std::string>{"name", "description"});
    CHECK(listed[0].at("files").elements[1].at("name").text == "B.BIN");
    CHECK(listed[0].at("files").elements[1].at("description").text == "the B.BIN");

    // The list comes before the first file, and the files that arrive are the two that were picked.
    const std::vector<std::string> names = run.names();
    CHECK(std::ranges::find(names, "file_list") < std::ranges::find(names, "file_received"));
    const std::vector<Json> received = run.events_named("file_received");
    REQUIRE(received.size() == 2);
    std::vector<std::string> got{received[0].at("name").text, received[1].at("name").text};
    std::ranges::sort(got);
    CHECK(got == std::vector<std::string>{"A.BIN", "C.BIN"});
    CHECK(rig.rig.target.log().selected_files.size() == 2);

    const Json finished = run.finished();
    CHECK(finished.at("reason").text == "completed");
    CHECK(finished.at("download_folder").text == made_folder());
    CHECK_FALSE(rig.rig.loader_host.fake_files().get("/dl/DNLD_DATA_HNPFMS_L_1/B.BIN").has_value());
}

TEST_CASE("a download with --pick-all takes every file the target lists", "[load][integration][download]")
{
    CommandRig rig(holding({"A.BIN", "B.BIN", "C.BIN"}));
    const CommandRun run = download(rig, {"--pick-all"});
    CHECK(run.exit_code == 0);
    CHECK(run.events_named("file_list").size() == 1);
    CHECK(run.events_named("file_received").size() == 3);
    CHECK(run.finished().at("detail").text == "The operation completed. Received 3 file(s).");
    for (const char* name : {"A.BIN", "B.BIN", "C.BIN"})
    {
        CHECK(adl::test::fake_text(rig.rig.loader_host, std::string{"/dl/DNLD_DATA_HNPFMS_L_1/"} + name) ==
              std::string{"content of "} + name);
    }
}

TEST_CASE("a name that --pick gives and the target does not list is a failure, with the name in the detail",
          "[load][integration][download]")
{
    SECTION("the rest of what was picked is still taken")
    {
        CommandRig rig(holding({"A.BIN", "B.BIN"}));
        const CommandRun run = download(rig, {"--pick", "A.BIN", "--pick", "Z.BIN"});
        CHECK(run.exit_code == 1);
        adl::test::check_stream(run);
        const Json finished = run.finished();
        CHECK(finished.at("result").text == "failed");
        CHECK(finished.at("reason").text == "missing_files");
        CHECK(finished.at("detail").text == "Received 1 of 2 picked file(s). The target does not list Z.BIN.");
        CHECK(finished.at("download_folder").text == made_folder());
        CHECK(run.events_named("file_received").size() == 1);
    }
    SECTION("nothing that was picked is listed")
    {
        CommandRig rig(holding({"A.BIN", "B.BIN"}));
        const CommandRun run = download(rig, {"--pick", "Y.BIN", "--pick", "Z.BIN"});
        CHECK(run.exit_code == 1);
        const Json finished = run.finished();
        CHECK(finished.at("reason").text == "missing_files");
        CHECK(finished.at("detail").text.ends_with("The target does not list Y.BIN, Z.BIN."));
        CHECK(run.events_named("file_received").empty());
        CHECK(finished.at("download_folder").is(Json::Kind::null));
    }
}

TEST_CASE("a target that lists no file leaves --pick-all nothing to take", "[load][integration][download]")
{
    CommandRig rig(holding({}));
    const CommandRun run = download(rig, {"--pick-all"});
    INFO(run.out);
    CHECK(run.exit_code == 1);
    const Json finished = run.finished();
    CHECK(finished.at("reason").text == "missing_files");
    CHECK(finished.at("detail").text == "The target listed no file to download.");
    CHECK(run.events_named("file_list").size() == 1);
}

TEST_CASE("a download folder that cannot be used is a local error, exit code 5", "[load][integration][download]")
{
    SECTION("the folder named is a file")
    {
        CommandRig rig(holding({"A.BIN"}));
        rig.rig.loader_host.fake_files().put("/dl", "a file where a folder should be");
        const CommandRun run = download(rig, {"--file", "A.BIN"});
        CHECK(run.exit_code == 5);
        adl::test::check_stream(run);
        const Json finished = run.finished();
        CHECK(finished.at("reason").text == "local_error");
        CHECK(finished.at("detail").text == "The download folder could not be used: /dl is not a folder.");
        CHECK(finished.at("download_folder").is(Json::Kind::null));
        CHECK(rig.rig.target.log().init_requests_at.empty());
    }
    SECTION("a folder cannot be made")
    {
        CommandRig rig(holding({"A.BIN"}));
        adl::test::FaultyHost faulty(rig.rig.loader_host);
        faulty.faulty_files().make_directory_fails["/dl"] = adl::platform::ErrorCode::permission_denied;
        const CommandRun run = rig.run_on(faulty, {"download", "--target", "10.0.0.2", "--position", "HNPFMS_L",
                                                   "--into", "/dl", "--file", "A.BIN", "--json"});
        CHECK(run.exit_code == 5);
        CHECK(run.finished().at("reason").text == "local_error");
        CHECK(run.finished().at("detail").text.starts_with("The download folder could not be used: cannot create /dl"));
    }
}

TEST_CASE("a file name that is not a plain name is wrong usage, told on standard error with no event",
          "[load][integration][download]")
{
    CommandRig rig(holding({"A.BIN"}));
    const CommandRun run = download(rig, {"--file", "../A.BIN"});
    CHECK(run.exit_code == 2);
    CHECK(run.out.empty());
    CHECK(run.err == "adl_load: \"../A.BIN\" is not a safe file name.\n");
    CHECK(rig.rig.target.log().init_requests_at.empty());
    CHECK_FALSE(rig.rig.loader_host.fake_files().stat("/dl").has_value());
}

TEST_CASE("a download from a target that is not on port 59 is made with --target-port", "[load][integration][download]")
{
    SimPlan plan = holding({"A.BIN", "B.BIN"});
    plan.control_port = 30959;

    SECTION("the loader names the files")
    {
        CommandRig rig(plan);
        const CommandRun run = download(rig, {"--file", "A.BIN"}, {"--target-port", "30959"});
        CHECK(run.exit_code == 0);
        CHECK(run.events_named("file_received").size() == 1);
    }
    SECTION("the target lists its files")
    {
        CommandRig rig(plan);
        const CommandRun run = download(rig, {"--pick-all"}, {"--target-port", "30959"});
        CHECK(run.exit_code == 0);
        CHECK(run.events_named("file_received").size() == 2);
    }
    SECTION("the port is not named")
    {
        CommandRig rig(plan);
        const CommandRun run = download(rig, {"--file", "A.BIN"});
        CHECK(run.exit_code == 4);
        CHECK(run.finished().at("reason").text == "no_answer");
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
// Whole operations through run_operation, from the runner on the fake platform to a simulated target and
// back, and what each one comes to: the result, the reason, the last status, and the folder a download
// keeps. The files are the fake platform's, so nothing here needs a network, a disk or Qt.

#include "adl/arinc615a/directory_files.hpp"
#include "adl/loader_core/download_directory.hpp"
#include "adl/loader_core/gate.hpp"
#include "adl/loader_core/media_source.hpp"
#include "adl/loader_core/operation_plan.hpp"
#include "adl/loader_core/run.hpp"
#include "bytes.hpp"
#include "faulty_host.hpp"
#include "rig.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace adl::loader_core;
using adl::arinc615a::OutcomeCode;
using adl::arinc615a::StatusCode;
using adl::test::Rig;
using adl::test::SimPlan;

namespace
{

RunTarget target_of(const std::string_view thw_id_pos = "HNPFMS_L")
{
    RunTarget target;
    target.address = adl::arinc615a::Address{Rig::target_address.octets};
    target.thw_id_pos = thw_id_pos;
    return target;
}

SimPlan with_files(std::initializer_list<const char*> names)
{
    SimPlan plan;
    for (const char* name : names)
    {
        plan.files.push_back(
            adl::test::DownloadItem{name, adl::test::bytes_of(std::string{"content of "} + name), "held"});
    }
    return plan;
}

// Puts the files of the sample load's media set into the rig's file system, as a tree under /set, and
// makes the index an upload reads them through.
MediaSetIndex put_media_set(Rig& rig, const adl::test::SampleLoad& load)
{
    const auto set = adl::arinc665::read_media_set(load.members);
    REQUIRE(set.has_value());
    auto index = MediaSetIndex::make("/set", set->files_list, load.members);
    REQUIRE(index.has_value());
    for (const MediaSetIndex::Entry& entry : index->entries())
    {
        for (const adl::arinc665::MediaMember& member : load.members)
        {
            for (const adl::arinc665::MediaFile& file : member.files)
            {
                if (member.sequence_number == entry.member && file.pathname == entry.pathname &&
                    file.name == entry.name)
                {
                    rig.loader_host.fake_files().put(entry.path, file.bytes);
                }
            }
        }
    }
    return std::move(*index);
}

// The folder a download is written into, made as the window makes it.
std::string make_folder(Rig& rig)
{
    const auto folder = make_download_directory(rig.loader_host.files(), "/dl", "HNPFMS_L");
    REQUIRE(folder.has_value());
    return *folder;
}

} // namespace

TEST_CASE("an Information operation that completes", "[loader_core][run]")
{
    Rig rig;
    const OperationPlan plan = OperationPlan::information();
    RunServices services;
    services.listener = &rig.recorder;

    const OperationResult result = run_operation(rig.loader_host, target_of(), plan.operation(), services);
    CHECK(result.success());
    CHECK(result.reason == Reason::completed);
    CHECK(result.status_code() == StatusCode::completed);
    CHECK_FALSE(result.not_run.has_value());
    CHECK(result.outcome.code == OutcomeCode::completed);
    CHECK(result.outcome.statuses_received == 2);
    CHECK(result.download_folder.empty());
    CHECK_FALSE(result.changed_file.has_value());
    CHECK(rig.recorder.configuration_lists == 1); // the listener was told

    // The sockets can be bound to an address of the caller's choosing.
    Rig other;
    RunTarget target = target_of();
    target.local_address = Rig::loader_address;
    CHECK(run_operation(other.loader_host, target, plan.operation(), services).success());
}

TEST_CASE("a target that listens on another port is reached when the request names it, and not otherwise",
          "[loader_core][run]")
{
    constexpr std::uint16_t other_port = 6969;
    SimPlan sim;
    sim.control_port = other_port;

    SECTION("an Information operation")
    {
        Rig rig(sim);
        const OperationPlan plan = OperationPlan::information();
        RunServices services;
        services.listener = &rig.recorder;
        RunTarget target = target_of();
        target.port = other_port;

        const OperationResult result = run_operation(rig.loader_host, target, plan.operation(), services);
        CHECK(result.success());
        CHECK(result.reason == Reason::completed);
        CHECK(result.outcome.statuses_received == 2);
        CHECK(rig.recorder.configuration_lists == 1);
        CHECK(rig.target.log().init_requests_at.size() == 1);
    }

    SECTION("an upload, with the loader's own port moved as well")
    {
        const adl::test::SampleLoad load = adl::test::make_sample_load(5'000, 3);
        sim.loads = {load.item};
        Rig rig(sim);
        MediaSetIndex index = put_media_set(rig, load);
        MediaTreeSource source(rig.loader_host.files(), index);
        const std::vector<LoadChoice> choice{{load.item.header_name, load.item.load_part_number}};
        const auto plan = OperationPlan::upload(choice);
        REQUIRE(plan.has_value());
        RunServices services;
        services.listener = &rig.recorder;
        services.upload_source = &source;
        RunTarget target = target_of();
        target.port = other_port;
        target.control_port = 1615; // the two ports are independent

        const OperationResult result = run_operation(rig.loader_host, target, plan->operation(), services);
        CHECK(result.success());
        CHECK(result.reason == Reason::completed);
        CHECK(result.outcome.files_served == load.item.reads.size());
        CHECK(rig.target.log().loader_port_used == 1615);
    }

    SECTION("without the port the request goes to 59, where this target is not")
    {
        Rig rig(sim);
        const OperationPlan plan = OperationPlan::information();
        RunServices services;
        services.listener = &rig.recorder;

        const OperationResult result = run_operation(rig.loader_host, target_of(), plan.operation(), services);
        CHECK_FALSE(result.success());
        CHECK(result.reason == Reason::no_answer);
        CHECK(result.outcome.code == OutcomeCode::transfer_failed);
        CHECK(rig.target.log().init_requests_at.empty()); // it never heard the request
    }

    SECTION("a target on 59 is not reached by naming another port")
    {
        Rig rig;
        const OperationPlan plan = OperationPlan::information();
        RunServices services;
        services.listener = &rig.recorder;
        RunTarget target = target_of();
        target.port = other_port;

        const OperationResult result = run_operation(rig.loader_host, target, plan.operation(), services);
        CHECK(result.reason == Reason::no_answer);
        CHECK(rig.target.log().init_requests_at.empty());
    }
}

TEST_CASE("an upload that completes, and one whose set changed after it was checked", "[loader_core][run]")
{
    const adl::test::SampleLoad load = adl::test::make_sample_load(5'000, 3);
    SimPlan sim;
    sim.loads = {load.item};
    const std::vector<LoadChoice> choice{{load.item.header_name, load.item.load_part_number}};
    const auto plan = OperationPlan::upload(choice);
    REQUIRE(plan.has_value());

    SECTION("as it was checked")
    {
        Rig rig(sim);
        MediaSetIndex index = put_media_set(rig, load);
        MediaTreeSource source(rig.loader_host.files(), index);
        RunServices services;
        services.listener = &rig.recorder;
        services.upload_source = &source;

        const OperationResult result = run_operation(rig.loader_host, target_of(), plan->operation(), services);
        CHECK(result.success());
        CHECK(result.reason == Reason::completed);
        CHECK(result.outcome.files_served == load.item.reads.size());
        CHECK(result.outcome.items_failed == 0);
        CHECK_FALSE(result.changed_file.has_value());
    }

    SECTION("with the header file edited since")
    {
        Rig rig(sim);
        MediaSetIndex index = put_media_set(rig, load);
        MediaTreeSource source(rig.loader_host.files(), index);
        for (const MediaSetIndex::Entry& entry : index.entries())
        {
            if (entry.name == load.item.header_name)
            {
                rig.loader_host.fake_files().put(entry.path, "edited after the check");
            }
        }
        RunServices services;
        services.listener = &rig.recorder;
        services.upload_source = &source;

        const OperationResult result = run_operation(rig.loader_host, target_of(), plan->operation(), services);
        CHECK_FALSE(result.success());
        CHECK(result.reason == Reason::media_set_changed);
        REQUIRE(result.changed_file.has_value());
        CHECK(result.changed_file->find(load.item.header_name) != std::string::npos);
        CHECK(result.outcome.files_served == 0); // not a byte of it went to the target
    }
}

TEST_CASE("a media defined download keeps its folder, and is short when the target lacks a file", "[loader_core][run]")
{
    SECTION("every file arrives")
    {
        Rig rig(with_files({"A.BIN", "B.BIN"}));
        const std::string folder = make_folder(rig);
        adl::arinc615a::DirectoryFileSink sink(rig.loader_host.files(), folder);
        const auto plan = OperationPlan::download_media(std::vector<std::string>{"A.BIN", "B.BIN"});
        REQUIRE(plan.has_value());
        RunServices services;
        services.listener = &rig.recorder;
        services.download_sink = &sink;
        services.download_folder = folder;

        const OperationResult result = run_operation(rig.loader_host, target_of(), plan->operation(), services);
        CHECK(result.success());
        CHECK(result.outcome.files_requested == 2);
        CHECK(result.outcome.files_received == 2);
        CHECK_FALSE(result.missing_files);
        CHECK(result.download_folder == "/dl/DNLD_DATA_HNPFMS_L_1");
        CHECK_FALSE(result.download_folder_removed);
        CHECK(rig.loader_host.fake_files().get(folder + "/A.BIN").has_value());
        CHECK(rig.loader_host.fake_files().get(folder + "/B.BIN").has_value());
    }

    SECTION("one file the target does not have")
    {
        Rig rig(with_files({"A.BIN", "B.BIN"}));
        const std::string folder = make_folder(rig);
        adl::arinc615a::DirectoryFileSink sink(rig.loader_host.files(), folder);
        const auto plan = OperationPlan::download_media(std::vector<std::string>{"A.BIN", "B.BIN", "D.BIN"});
        REQUIRE(plan.has_value());
        RunServices services;
        services.listener = &rig.recorder;
        services.download_sink = &sink;
        services.download_folder = folder;

        const OperationResult result = run_operation(rig.loader_host, target_of(), plan->operation(), services);
        CHECK_FALSE(result.success());
        CHECK(result.reason == Reason::missing_files);
        CHECK(result.missing_files);
        CHECK(result.outcome.files_requested == 3);
        CHECK(result.outcome.files_received == 2);
        CHECK(result.download_folder == folder); // what did come is kept
    }
}

TEST_CASE("an operator defined download brings back the files chosen, and none cancels it", "[loader_core][run]")
{
    const OperationPlan plan = OperationPlan::download_operator();

    SECTION("one of three chosen")
    {
        Rig rig(with_files({"A.BIN", "B.BIN", "C.BIN"}));
        rig.recorder.decision = adl::arinc615a::SelectionDecision::deferred;
        OperationGate gate;
        gate.post_selection({"B.BIN"}); // made before the list is there: kept until it is
        const std::string folder = make_folder(rig);
        adl::arinc615a::DirectoryFileSink sink(rig.loader_host.files(), folder);
        RunServices services;
        services.listener = &rig.recorder;
        services.controller = &gate;
        services.download_sink = &sink;
        services.download_folder = folder;

        const OperationResult result = run_operation(rig.loader_host, target_of(), plan.operation(), services);
        CHECK(result.success());
        CHECK(result.outcome.files_received == 1);
        CHECK(rig.loader_host.fake_files().get(folder + "/B.BIN").has_value());
        CHECK_FALSE(rig.loader_host.fake_files().get(folder + "/A.BIN").has_value());
    }

    SECTION("none chosen")
    {
        Rig rig(with_files({"A.BIN"}));
        rig.recorder.decision = adl::arinc615a::SelectionDecision::deferred;
        OperationGate gate;
        gate.post_selection({});
        const std::string folder = make_folder(rig);
        adl::arinc615a::DirectoryFileSink sink(rig.loader_host.files(), folder);
        RunServices services;
        services.listener = &rig.recorder;
        services.controller = &gate;
        services.download_sink = &sink;
        services.download_folder = folder;

        const OperationResult result = run_operation(rig.loader_host, target_of(), plan.operation(), services);
        CHECK_FALSE(result.success());
        CHECK(result.reason == Reason::aborted);
        CHECK(result.outcome.code == OutcomeCode::aborted_by_operator);
        // Nothing came, so nothing is kept.
        CHECK(result.download_folder.empty());
        CHECK(result.download_folder_removed);
        CHECK_FALSE(rig.loader_host.fake_files().stat(folder).has_value());
    }
}

TEST_CASE("an operator's abort ends the operation as an abort", "[loader_core][run]")
{
    Rig rig;
    OperationGate gate;
    gate.request_abort();
    const OperationPlan plan = OperationPlan::information();
    RunServices services;
    services.listener = &rig.recorder;
    services.controller = &gate;

    const OperationResult result = run_operation(rig.loader_host, target_of(), plan.operation(), services);
    CHECK_FALSE(result.success());
    CHECK(result.reason == Reason::aborted);
    CHECK(result.outcome.code == OutcomeCode::aborted_by_operator);
}

TEST_CASE("a target that refuses, or does not answer, is told apart", "[loader_core][run]")
{
    const OperationPlan plan = OperationPlan::information();

    for (const StatusCode status : {StatusCode::operation_denied, StatusCode::operation_not_supported})
    {
        SimPlan sim;
        sim.init_status = status;
        sim.init_text = "not now";
        Rig rig(sim);
        RunServices services;
        services.listener = &rig.recorder;
        const OperationResult result = run_operation(rig.loader_host, target_of(), plan.operation(), services);
        CHECK_FALSE(result.success());
        CHECK(result.reason == Reason::target_refused);
    }

    SimPlan silent;
    silent.init_silent = true;
    Rig rig(silent);
    RunServices services;
    services.listener = &rig.recorder;
    const OperationResult result = run_operation(rig.loader_host, target_of(), plan.operation(), services);
    CHECK_FALSE(result.success());
    CHECK(result.reason == Reason::no_answer);
    CHECK(result.outcome.code == OutcomeCode::transfer_failed);
}

TEST_CASE("a target that aborts, and one whose answer cannot be understood, are told apart", "[loader_core][run]")
{
    const OperationPlan plan = OperationPlan::information();

    SECTION("the target ends the operation")
    {
        SimPlan sim;
        sim.abort_after_statuses = 1;
        Rig rig(sim);
        RunServices services;
        services.listener = &rig.recorder;
        const OperationResult result = run_operation(rig.loader_host, target_of(), plan.operation(), services);
        CHECK_FALSE(result.success());
        CHECK(result.reason == Reason::target_aborted);
        CHECK(result.outcome.code == OutcomeCode::aborted_by_target);
    }

    SECTION("an initialization file that is not a valid structure")
    {
        SimPlan sim;
        sim.init_garbage = true;
        Rig rig(sim);
        RunServices services;
        services.listener = &rig.recorder;
        const OperationResult result = run_operation(rig.loader_host, target_of(), plan.operation(), services);
        CHECK_FALSE(result.success());
        CHECK(result.reason == Reason::protocol_error);
        CHECK(result.outcome.code == OutcomeCode::protocol_error);
    }

    SECTION("a protocol version the loader does not know")
    {
        SimPlan sim;
        sim.init_version[0] = 'B';
        sim.init_version[1] = '5';
        Rig rig(sim);
        RunServices services;
        services.listener = &rig.recorder;
        const OperationResult result = run_operation(rig.loader_host, target_of(), plan.operation(), services);
        CHECK_FALSE(result.success());
        CHECK(result.reason == Reason::protocol_error);
        CHECK(result.outcome.code == OutcomeCode::unsupported_protocol_version);
    }
}

TEST_CASE("an operation that cannot run says where it stopped, and a download that never began leaves no folder",
          "[loader_core][run]")
{
    const OperationPlan plan = OperationPlan::download_operator();

    SECTION("a request the loader refuses")
    {
        Rig rig;
        const std::string folder = make_folder(rig);
        adl::arinc615a::DirectoryFileSink sink(rig.loader_host.files(), folder);
        RunTarget target = target_of();
        target.control_port = 0; // not a port the loader can listen on
        RunServices services;
        services.download_sink = &sink;
        services.download_folder = folder;

        const OperationResult result = run_operation(rig.loader_host, target, plan.operation(), services);
        CHECK_FALSE(result.success());
        CHECK(result.reason == Reason::local_error);
        REQUIRE(result.not_run.has_value());
        CHECK(result.not_run->stage == adl::arinc615a::RunError::Stage::start);
        CHECK_FALSE(result.status_code().has_value());
        CHECK(result.download_folder.empty());
        CHECK(result.download_folder_removed);
    }

    SECTION("a target port of 0")
    {
        Rig rig;
        const std::string folder = make_folder(rig);
        adl::arinc615a::DirectoryFileSink sink(rig.loader_host.files(), folder);
        RunTarget target = target_of();
        target.port = 0; // no datagram goes to port 0
        RunServices services;
        services.listener = &rig.recorder;
        services.download_sink = &sink;
        services.download_folder = folder;

        const OperationResult result = run_operation(rig.loader_host, target, plan.operation(), services);
        CHECK(result.reason == Reason::local_error);
        REQUIRE(result.not_run.has_value());
        CHECK(result.not_run->stage == adl::arinc615a::RunError::Stage::start);
        CHECK(result.download_folder_removed);
        CHECK(rig.target.log().init_requests_at.empty());
    }

    SECTION("a port that cannot be opened")
    {
        Rig rig;
        adl::test::FaultyHost host(rig.loader_host);
        host.faulty_network().open_fails = adl::platform::ErrorCode::address_in_use;
        const OperationResult result = run_operation(host, target_of(), plan.operation(), {});
        CHECK(result.reason == Reason::local_error);
        REQUIRE(result.not_run.has_value());
        CHECK(result.not_run->stage == adl::arinc615a::RunError::Stage::control_socket);
        CHECK(result.not_run->platform == adl::platform::ErrorCode::address_in_use);
    }

    SECTION("a wait on the network that fails")
    {
        Rig rig;
        adl::test::FaultyHost host(rig.loader_host);
        host.faulty_network().wait_fails = adl::platform::ErrorCode::io_error;
        const OperationResult result = run_operation(host, target_of(), plan.operation(), {});
        CHECK(result.reason == Reason::local_error);
        REQUIRE(result.not_run.has_value());
        CHECK(result.not_run->stage == adl::arinc615a::RunError::Stage::wait);
    }
}

TEST_CASE("a folder that holds anything is kept even when nothing was received", "[loader_core][run]")
{
    SimPlan sim;
    sim.init_status = StatusCode::operation_denied;
    Rig rig(sim);
    const std::string folder = make_folder(rig);
    rig.loader_host.fake_files().put(folder + "/stray.txt", "another program's");
    adl::arinc615a::DirectoryFileSink sink(rig.loader_host.files(), folder);
    const OperationPlan plan = OperationPlan::download_operator();
    RunServices services;
    services.listener = &rig.recorder;
    services.download_sink = &sink;
    services.download_folder = folder;

    const OperationResult result = run_operation(rig.loader_host, target_of(), plan.operation(), services);
    CHECK(result.reason == Reason::target_refused);
    CHECK(result.download_folder == folder);
    CHECK_FALSE(result.download_folder_removed);
}

TEST_CASE("the reasons have the words a command prints", "[loader_core][run]")
{
    CHECK(to_string(Reason::completed) == "completed");
    CHECK(to_string(Reason::target_refused) == "target_refused");
    CHECK(to_string(Reason::target_aborted) == "target_aborted");
    CHECK(to_string(Reason::load_failed) == "load_failed");
    CHECK(to_string(Reason::missing_files) == "missing_files");
    CHECK(to_string(Reason::protocol_error) == "protocol_error");
    CHECK(to_string(Reason::aborted) == "aborted");
    CHECK(to_string(Reason::media_set_changed) == "media_set_changed");
    CHECK(to_string(Reason::no_answer) == "no_answer");
    CHECK(to_string(Reason::local_error) == "local_error");
}

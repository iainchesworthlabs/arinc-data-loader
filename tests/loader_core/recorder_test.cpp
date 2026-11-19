// SPDX-License-Identifier: GPL-3.0-or-later
// The recorder: whole operations run against the simulated target, with what the record holds at the end of
// each, and the decisions it makes about the log (what is a line, what is said once, which description counts)
// checked callback by callback.

#include "adl/loader_core/recorder.hpp"
#include "rig.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using adl::arinc615a::FileListing;
using adl::arinc615a::FileProgress;
using adl::arinc615a::FileSelection;
using adl::arinc615a::InformationOperation;
using adl::arinc615a::LoadProgress;
using adl::arinc615a::LoadReference;
using adl::arinc615a::MediaDownloadOperation;
using adl::arinc615a::Notice;
using adl::arinc615a::OperationKind;
using adl::arinc615a::OperatorDownloadOperation;
using adl::arinc615a::OutcomeCode;
using adl::arinc615a::SelectionDecision;
using adl::arinc615a::StatusCode;
using adl::loader_core::OperationRecord;
using adl::loader_core::OperationRecorder;
using adl::loader_core::RecordedLogLine;
using adl::loader_core::RecordTime;
using adl::loader_core::Severity;
using adl::loader_core::WallClock;
using adl::test::DownloadItem;
using adl::test::MediaSource;
using adl::test::Rig;
using adl::test::SampleLoad;
using adl::test::SimPlan;

namespace
{

using std::chrono::seconds;

// 2026-11-19T10:00:00Z, the moment the clocks of these tests start at.
const std::chrono::sys_seconds test_start =
    std::chrono::sys_days{std::chrono::year{2026} / std::chrono::November / 19} + std::chrono::hours{10};

RecordTime at(int offset_seconds)
{
    return test_start + seconds{offset_seconds};
}

// A clock that stands still.
WallClock fixed_clock()
{
    return [] { return at(0); };
}

// A clock that says one second more each time it is read.
WallClock ticking_clock()
{
    auto reads = std::make_shared<int>(0);
    return [reads] { return at((*reads)++); };
}

OperationRecorder recorder_for(OperationKind kind, WallClock clock = fixed_clock())
{
    return OperationRecorder{kind, "10.0.0.2", "HNPFMS_L", std::move(clock)};
}

std::vector<std::string> texts(const OperationRecord& record)
{
    std::vector<std::string> result;
    for (const RecordedLogLine& line : record.log)
    {
        result.push_back(line.text);
    }
    return result;
}

bool has_line(const OperationRecord& record, const std::string& text)
{
    const std::vector<std::string> lines = texts(record);
    return std::ranges::find(lines, text) != lines.end();
}

// The lines that begin with `start`.
std::vector<RecordedLogLine> lines_starting(const OperationRecord& record, const std::string& start)
{
    std::vector<RecordedLogLine> result;
    for (const RecordedLogLine& line : record.log)
    {
        if (line.text.starts_with(start))
        {
            result.push_back(line);
        }
    }
    return result;
}

std::vector<DownloadItem> three_files()
{
    return {DownloadItem{"A.BIN", adl::test::pattern(3000, 1), "the first"},
            DownloadItem{"B.BIN", adl::test::pattern(700, 2), "the second"}, DownloadItem{"C.BIN", {}, "an empty one"}};
}

// A recorder for an operator defined download that chooses at once, as a program with no window does.
class ChoosingRecorder final : public OperationRecorder
{
public:
    using OperationRecorder::OperationRecorder;

    std::vector<std::string> choose; // empty: everything listed

    SelectionDecision on_download_list(const adl::arinc615a::DownloadList& list,
                                       FileSelection& selection) noexcept override
    {
        static_cast<void>(OperationRecorder::on_download_list(list, selection));
        for (const FileListing& file : list.files)
        {
            if (choose.empty() || std::ranges::find(choose, std::string{file.file_name}) != choose.end())
            {
                selection.select(file.file_name);
            }
        }
        return SelectionDecision::selected;
    }
};

} // namespace

// ---- A record, begun and ended -----------------------------------------------------------------

TEST_CASE("a record begins with the operation, the target and the time it started", "[loader_core][recorder]")
{
    OperationRecorder recorder = recorder_for(OperationKind::download_operator, ticking_clock());
    const OperationRecord& record = recorder.record();
    CHECK(record.operation == OperationKind::download_operator);
    CHECK(record.target_address == "10.0.0.2");
    CHECK(record.target_position == "HNPFMS_L");
    CHECK(record.started_at == at(0));
    CHECK_FALSE(record.ended_at);
    CHECK(record.log.empty());
    CHECK_FALSE(record.configuration);
    CHECK_FALSE(record.media_set);
    CHECK_FALSE(record.download_folder);
    CHECK_FALSE(record.status_code);

    recorder.add_log(Severity::warning, "something the caller says");
    recorder.finish();
    REQUIRE(record.log.size() == 1);
    CHECK(record.log[0].at == at(1));
    CHECK(record.log[0].severity == Severity::warning);
    CHECK(record.log[0].text == "something the caller says");
    CHECK(record.ended_at == at(2));
}

TEST_CASE("a recorder with no wall clock has no times", "[loader_core][recorder]")
{
    OperationRecorder recorder = recorder_for(OperationKind::information, WallClock{});
    recorder.add_log(Severity::info, "a line");
    recorder.finish();
    CHECK_FALSE(recorder.record().started_at);
    CHECK_FALSE(recorder.record().ended_at);
    CHECK_FALSE(recorder.record().log[0].at);
}

namespace
{

// A platform clock with no real-time clock behind it.
class ClockWithoutDate final : public adl::platform::Clock
{
public:
    adl::platform::Instant now() noexcept override
    {
        return {};
    }
    void sleep_for(adl::platform::Duration) noexcept override {}
    adl::platform::Result<std::int64_t> wall_time_seconds() noexcept override
    {
        return adl::platform::fail(adl::platform::ErrorCode::unsupported);
    }
};

} // namespace

TEST_CASE("the wall clock of a platform is its clock's seconds since 1970, or nothing", "[loader_core][recorder]")
{
    Rig rig;
    rig.world.set_wall_origin(1'700'000'000);
    const WallClock clock = adl::loader_core::wall_clock_of(rig.loader_host.clock());
    const RecordTime first = clock();
    REQUIRE(first);
    CHECK(first->time_since_epoch() >= seconds{1'700'000'000});
    rig.world.advance_by(seconds(5));
    const RecordTime later = clock();
    REQUIRE(later);
    CHECK(*later - *first == seconds{5});

    ClockWithoutDate none;
    CHECK_FALSE(adl::loader_core::wall_clock_of(none)());
}

// ---- Whole operations ---------------------------------------------------------------------------

TEST_CASE("an Information operation leaves the configuration the target listed", "[loader_core][recorder][integration]")
{
    Rig rig;
    OperationRecorder recorder = recorder_for(OperationKind::information, ticking_clock());
    rig.listener_override = &recorder;
    const auto outcome = rig.run(InformationOperation{});
    REQUIRE(outcome);
    REQUIRE(outcome->code == OutcomeCode::completed);
    recorder.finish();

    const OperationRecord& record = recorder.record();
    REQUIRE(record.configuration);
    REQUIRE(record.configuration->size() == 1);
    const auto& hardware = record.configuration->front();
    CHECK(hardware.literal_name == "FMS LEFT");
    CHECK(hardware.serial_number == "SN-0001");
    REQUIRE(hardware.parts.size() == 2);
    CHECK(hardware.parts[0].part_number == "ACM47-1234-5678");
    CHECK(hardware.parts[0].amendment.empty());
    CHECK(hardware.parts[0].designation == "FMS 1 OPS");
    CHECK(hardware.parts[1].part_number == "ACM47-8765-4321");
    CHECK(hardware.parts[1].amendment == "A");
    CHECK(hardware.parts[1].designation == "FMS 1 DATA");

    CHECK(record.status_code == StatusCode::completed);
    CHECK(record.loads.empty());
    CHECK(record.files.empty());
    CHECK(record.started_at == at(0));
    CHECK(record.ended_at > record.started_at);

    CHECK(record.log.front().text == "The target answered: accepted");
    CHECK(record.log.front().severity == Severity::info);
    CHECK(has_line(record, "Status 0: accepted"));
    CHECK(has_line(record, "Status 1: completed"));
    CHECK(has_line(record, "The target listed its configuration."));
    // The lines are in the order they happened, and each is stamped when it was made.
    for (std::size_t i = 1; i < record.log.size(); ++i)
    {
        CHECK(record.log[i].at > record.log[i - 1].at);
    }
}

TEST_CASE("an upload leaves its loads with their last status and percentage", "[loader_core][recorder][integration]")
{
    const SampleLoad load = adl::test::make_sample_load();
    SimPlan plan;
    plan.loads = {load.item};
    Rig rig(plan);
    rig.source = std::make_unique<MediaSource>(load.files);
    OperationRecorder recorder = recorder_for(OperationKind::upload);
    rig.listener_override = &recorder;
    std::vector<LoadReference> storage;
    const auto outcome = rig.run(adl::test::upload_of(load, storage));
    REQUIRE(outcome);
    REQUIRE(outcome->code == OutcomeCode::completed);

    const OperationRecord& record = recorder.record();
    REQUIRE(record.loads.size() == 1);
    CHECK(record.loads[0].part_number == load.item.load_part_number);
    CHECK(record.loads[0].header_file == load.item.header_name);
    CHECK(record.loads[0].status == StatusCode::completed);
    CHECK(record.loads[0].percent == 100);
    CHECK(record.status_code == StatusCode::completed);

    // The status lines say how far the list is, and one line says what the target read, for each file.
    CHECK(has_line(record, "The target answered: accepted"));
    CHECK(has_line(record, "Status 0: accepted, 0% of the list"));
    const auto statuses = lines_starting(record, "Status ");
    REQUIRE(statuses.size() >= 2);
    CHECK(statuses.back().text.ends_with(": completed, 100% of the list"));
    const auto reads = lines_starting(record, "The target read ");
    REQUIRE(reads.size() == load.item.reads.size());
    for (std::size_t i = 0; i < reads.size(); ++i)
    {
        CHECK(reads[i].text == "The target read " + load.item.reads[i].name + " (" +
                                   std::to_string(load.item.reads[i].expected.size()) + " bytes)");
        CHECK(reads[i].severity == Severity::info);
    }
    // Nothing failed, so nothing was logged as an error.
    CHECK(
        std::ranges::none_of(record.log, [](const RecordedLogLine& line) { return line.severity == Severity::error; }));
}

TEST_CASE("a media defined download leaves the files it received, with their sizes and paths",
          "[loader_core][recorder][integration]")
{
    SimPlan plan;
    plan.files = three_files();
    Rig rig(plan);
    OperationRecorder recorder = recorder_for(OperationKind::download_media);
    recorder.set_download_folder("C:\\Users\\operator\\ArincDownloads\\DNLD_DATA_HNPFMS_L_1");
    rig.listener_override = &recorder;
    std::vector<adl::arinc615a::FileRequest> storage;
    for (const DownloadItem& file : plan.files)
    {
        storage.push_back(adl::arinc615a::FileRequest{file.name, {}});
    }
    const auto outcome = rig.run(MediaDownloadOperation{adl::arinc615a::DownloadRequest{storage}});
    REQUIRE(outcome);
    REQUIRE(outcome->code == OutcomeCode::completed);

    const OperationRecord& record = recorder.record();
    REQUIRE(record.files.size() == 3);
    const std::string folder = "C:\\Users\\operator\\ArincDownloads\\DNLD_DATA_HNPFMS_L_1";
    CHECK(record.download_folder == folder);
    CHECK(record.files[0].name == "A.BIN");
    CHECK(record.files[0].size == 3000);
    CHECK(record.files[0].path == folder + "\\A.BIN");
    CHECK(record.files[1].name == "B.BIN");
    CHECK(record.files[1].size == 700);
    CHECK(record.files[2].name == "C.BIN");
    CHECK(record.files[2].size == 0);
    CHECK(record.files[2].path == folder + "\\C.BIN");
    CHECK(has_line(record, "Received A.BIN (3000 bytes)"));
    CHECK(has_line(record, "Received B.BIN (700 bytes)"));
    CHECK(has_line(record, "Received C.BIN (0 bytes)"));
    CHECK(record.status_code == StatusCode::completed);
    CHECK_FALSE(record.configuration);
    CHECK(record.loads.empty());
}

TEST_CASE("an operator defined download leaves the files that were chosen from the target's list",
          "[loader_core][recorder][integration]")
{
    SimPlan plan;
    plan.files = three_files();
    Rig rig(plan);
    ChoosingRecorder recorder{OperationKind::download_operator, "10.0.0.2", "HNPFMS_L", fixed_clock()};
    recorder.choose = {"B.BIN", "C.BIN"};
    recorder.set_download_folder("/home/operator/DNLD_DATA_HNPFMS_L_1/");
    rig.listener_override = &recorder;
    const auto outcome = rig.run(OperatorDownloadOperation{});
    REQUIRE(outcome);
    REQUIRE(outcome->code == OutcomeCode::completed);

    const OperationRecord& record = recorder.record();
    CHECK(has_line(record, "The target listed 3 file(s) it can download."));
    REQUIRE(record.files.size() == 2);
    CHECK(record.files[0].name == "B.BIN");
    CHECK(record.files[0].size == 700);
    CHECK(record.files[0].path == "/home/operator/DNLD_DATA_HNPFMS_L_1/B.BIN");
    CHECK(record.files[1].name == "C.BIN");
    CHECK(record.files[1].path == "/home/operator/DNLD_DATA_HNPFMS_L_1/C.BIN");
}

TEST_CASE("a download that is not told where its files go records them with no path",
          "[loader_core][recorder][integration]")
{
    SimPlan plan;
    plan.files = three_files();
    Rig rig(plan);
    ChoosingRecorder recorder{OperationKind::download_operator, "10.0.0.2", "HNPFMS_L", fixed_clock()};
    recorder.choose = {"A.BIN"};
    rig.listener_override = &recorder;
    REQUIRE(rig.run(OperatorDownloadOperation{}));
    REQUIRE(recorder.record().files.size() == 1);
    CHECK(recorder.record().files[0].name == "A.BIN");
    CHECK(recorder.record().files[0].path.empty());
    CHECK_FALSE(recorder.record().download_folder);
}

TEST_CASE("a target that refuses the operation leaves its answer and nothing else",
          "[loader_core][recorder][integration]")
{
    SimPlan plan;
    plan.init_status = StatusCode::operation_denied;
    plan.init_text = "not now";
    Rig rig(plan);
    OperationRecorder recorder = recorder_for(OperationKind::information);
    rig.listener_override = &recorder;
    const auto outcome = rig.run(InformationOperation{});
    REQUIRE(outcome);
    REQUIRE(outcome->code == OutcomeCode::denied);

    const OperationRecord& record = recorder.record();
    REQUIRE(record.log.size() == 1);
    CHECK(record.log[0].text == "The target answered: denied (not now)");
    CHECK(record.log[0].severity == Severity::warning);
    CHECK(record.status_code == StatusCode::operation_denied);
    CHECK_FALSE(record.configuration);
    CHECK(record.loads.empty());
    CHECK(record.files.empty());
}

TEST_CASE("a target that aborts leaves the status it aborted with and its reason",
          "[loader_core][recorder][integration]")
{
    const SampleLoad load = adl::test::make_sample_load(60'000);
    SimPlan plan;
    plan.loads = {load.item};
    plan.request_block_size = 32;
    plan.abort_after_statuses = 2;
    Rig rig(plan);
    adl::fake::LinkProfile slow;
    slow.latency = std::chrono::milliseconds(5);
    rig.network.set_profile(slow);
    rig.source = std::make_unique<MediaSource>(load.files);
    OperationRecorder recorder = recorder_for(OperationKind::upload);
    rig.listener_override = &recorder;
    std::vector<LoadReference> storage;
    const auto outcome = rig.run(adl::test::upload_of(load, storage));
    REQUIRE(outcome);
    REQUIRE(outcome->code == OutcomeCode::aborted_by_target);

    const OperationRecord& record = recorder.record();
    CHECK(record.status_code == StatusCode::aborted_by_target);
    // The description of an abort by the target says why, so it is on the line.
    const auto statuses = lines_starting(record, "Status ");
    REQUIRE_FALSE(statuses.empty());
    CHECK(statuses.back().text.contains(": aborted by the target, "));
    CHECK(statuses.back().text.ends_with("% of the list (target gave up)"));
    REQUIRE_FALSE(record.loads.empty());
    CHECK(record.loads[0].status != StatusCode::completed);
}

TEST_CASE("a load the target fails is logged as an error once and kept with its status",
          "[loader_core][recorder][integration]")
{
    const SampleLoad load = adl::test::make_sample_load();
    SimPlan plan;
    plan.loads = {load.item};
    plan.failed_loads[load.item.header_name] = "no room for the data files";
    plan.work_time = seconds(10); // so that the target reports the failed load in several status files
    Rig rig(plan);
    rig.source = std::make_unique<MediaSource>(load.files);
    OperationRecorder recorder = recorder_for(OperationKind::upload);
    rig.listener_override = &recorder;
    std::vector<LoadReference> storage;
    const auto outcome = rig.run(adl::test::upload_of(load, storage));
    REQUIRE(outcome);
    REQUIRE(outcome->code == OutcomeCode::completed);
    CHECK(outcome->items_failed == 1);
    // Reported in the status files after the first one, which is more than once.
    REQUIRE(outcome->statuses_received >= 3);

    const OperationRecord& record = recorder.record();
    REQUIRE(record.loads.size() == 1);
    CHECK(record.loads[0].status == StatusCode::item_failed);
    const auto failures = lines_starting(record, "The target failed load ");
    REQUIRE(failures.size() == 1);
    CHECK(failures[0].text == "The target failed load " + load.item.load_part_number + " (" + load.item.header_name +
                                  "): no room for the data files");
    CHECK(failures[0].severity == Severity::error);
    CHECK(record.status_code == StatusCode::completed);
}

// ---- What goes in the log, callback by callback -------------------------------------------------------

TEST_CASE("a description is added to a status line only for the codes that carry one", "[loader_core][recorder]")
{
    struct Case
    {
        StatusCode status;
        bool counts;
    };
    for (const Case& scenario : {Case{StatusCode::accepted, false}, Case{StatusCode::in_progress, false},
                                 Case{StatusCode::completed, false}, Case{StatusCode::in_progress_with_text, true},
                                 Case{StatusCode::operation_denied, true}, Case{StatusCode::aborted_by_target, true},
                                 Case{StatusCode::aborted_by_operator, false}, Case{StatusCode::item_failed, true}})
    {
        CAPTURE(static_cast<int>(scenario.status));
        adl::arinc615a::UploadStatus upload;
        upload.status = scenario.status;
        upload.description = "left over";
        upload.list_ratio = 40;
        adl::arinc615a::DownloadStatus download;
        download.status = scenario.status;
        download.description = "left over";
        download.list_ratio = 40;

        OperationRecorder recorder = recorder_for(OperationKind::upload);
        recorder.on_upload_status(upload);
        recorder.on_download_status(download);
        for (const RecordedLogLine& line : recorder.record().log)
        {
            CHECK(line.text.ends_with(" (left over)") == scenario.counts);
            CHECK(line.text.starts_with("Status 0: "));
            CHECK(line.text.contains(", 40% of the list"));
        }
        CHECK(recorder.record().log.size() == 2);
    }
}

TEST_CASE("the answer to the start and a status of an Information operation carry their description as sent",
          "[loader_core][recorder]")
{
    // The window adds the description of these two whatever the code, and so does the record.
    OperationRecorder recorder = recorder_for(OperationKind::information);
    recorder.on_initialization({StatusCode::accepted, "ready"});
    recorder.on_initialization({StatusCode::operation_not_supported, ""});
    adl::arinc615a::ConfigurationStatus status;
    status.counter = 7;
    status.status = StatusCode::in_progress;
    status.description = "reading";
    recorder.on_configuration_status(status);

    const auto& log = recorder.record().log;
    REQUIRE(log.size() == 3);
    CHECK(log[0].text == "The target answered: accepted (ready)");
    CHECK(log[0].severity == Severity::info);
    CHECK(log[1].text == "The target answered: not supported");
    CHECK(log[1].severity == Severity::warning);
    CHECK(log[2].text == "Status 7: in progress (reading)");
    CHECK(recorder.record().status_code == StatusCode::in_progress);
}

TEST_CASE("the time left is told only when the target gave an estimate", "[loader_core][recorder]")
{
    OperationRecorder recorder = recorder_for(OperationKind::upload);
    adl::arinc615a::UploadStatus status;
    status.status = StatusCode::in_progress;
    status.list_ratio = 10;
    for (const std::uint16_t estimate : {std::uint16_t{0}, adl::arinc615a::estimated_time_unknown, std::uint16_t{90}})
    {
        status.estimated_time = estimate;
        recorder.on_upload_status(status);
    }
    adl::arinc615a::ConfigurationStatus configuration;
    configuration.status = StatusCode::in_progress;
    configuration.estimated_time = 5;
    recorder.on_configuration_status(configuration);

    CHECK(texts(recorder.record()) ==
          std::vector<std::string>{"Status 0: in progress, 10% of the list", "Status 0: in progress, 10% of the list",
                                   "Status 0: in progress, 10% of the list, about 90 s left",
                                   "Status 0: in progress, about 5 s left"});
}

TEST_CASE("a load is recorded when it first appears and updated after that", "[loader_core][recorder]")
{
    OperationRecorder recorder = recorder_for(OperationKind::upload);
    adl::arinc615a::UploadStatus first;
    first.status = StatusCode::accepted;
    recorder.on_upload_status(first); // before and during the list, a status has no loads
    CHECK(recorder.record().loads.empty());

    const std::array<LoadProgress, 2> early{LoadProgress{"A.LUH", "PN-A", 10, StatusCode::in_progress, ""},
                                            LoadProgress{"B.LUH", "PN-B", 0, StatusCode::accepted, ""}};
    adl::arinc615a::UploadStatus second;
    second.status = StatusCode::in_progress;
    second.loads = early;
    recorder.on_upload_status(second);

    const std::array<LoadProgress, 2> later{LoadProgress{"B.LUH", "PN-B", 55, StatusCode::in_progress, ""},
                                            LoadProgress{"A.LUH", "PN-A", 100, StatusCode::completed, ""}};
    adl::arinc615a::UploadStatus third;
    third.status = StatusCode::in_progress;
    third.loads = later;
    recorder.on_upload_status(third);

    const auto& loads = recorder.record().loads;
    REQUIRE(loads.size() == 2);
    // In the order they first appeared.
    CHECK(loads[0].header_file == "A.LUH");
    CHECK(loads[0].part_number == "PN-A");
    CHECK(loads[0].status == StatusCode::completed);
    CHECK(loads[0].percent == 100);
    CHECK(loads[1].header_file == "B.LUH");
    CHECK(loads[1].status == StatusCode::in_progress);
    CHECK(loads[1].percent == 55);
}

TEST_CASE("a load or a file that failed is logged once, however often the target says so", "[loader_core][recorder]")
{
    OperationRecorder recorder = recorder_for(OperationKind::upload);
    const std::array<LoadProgress, 2> loads{LoadProgress{"A.LUH", "PN-A", 30, StatusCode::item_failed, "no space"},
                                            LoadProgress{"B.LUH", "PN-B", 0, StatusCode::item_failed, ""}};
    adl::arinc615a::UploadStatus upload;
    upload.status = StatusCode::in_progress;
    upload.loads = loads;
    recorder.on_upload_status(upload);
    recorder.on_upload_status(upload);

    const std::array<FileProgress, 2> files{FileProgress{"X.BIN", StatusCode::item_failed, "unreadable"},
                                            FileProgress{"Y.BIN", StatusCode::item_failed, ""}};
    adl::arinc615a::DownloadStatus download;
    download.status = StatusCode::in_progress;
    download.files = files;
    recorder.on_download_status(download);
    recorder.on_download_status(download);

    CHECK(lines_starting(recorder.record(), "The target failed load PN-A (A.LUH): no space").size() == 1);
    CHECK(has_line(recorder.record(), "The target failed load PN-B (B.LUH)"));
    CHECK(has_line(recorder.record(), "The target failed file X.BIN: unreadable"));
    CHECK(has_line(recorder.record(), "The target failed file Y.BIN"));
    CHECK(lines_starting(recorder.record(), "The target failed ").size() == 4);
    for (const RecordedLogLine& line : lines_starting(recorder.record(), "The target failed "))
    {
        CHECK(line.severity == Severity::error);
    }
    // The status lines are there each time.
    CHECK(lines_starting(recorder.record(), "Status ").size() == 4);
}

TEST_CASE("a file that is not failed is not logged as failed", "[loader_core][recorder]")
{
    OperationRecorder recorder = recorder_for(OperationKind::download_media);
    const std::array<FileProgress, 2> files{FileProgress{"X.BIN", StatusCode::in_progress, "ignored"},
                                            FileProgress{"Y.BIN", StatusCode::completed, ""}};
    adl::arinc615a::DownloadStatus download;
    download.status = StatusCode::in_progress;
    download.files = files;
    recorder.on_download_status(download);
    CHECK(recorder.record().log.size() == 1);
}

TEST_CASE("a notice is a line of the note kind, with its number in its own unit", "[loader_core][recorder]")
{
    // The table of the window's notice_text_test (apps/app/tests).
    const auto line = [](Notice notice, const char* subject, std::uint32_t value)
    {
        OperationRecorder recorder = recorder_for(OperationKind::upload);
        recorder.on_notice(notice, subject, value);
        REQUIRE(recorder.record().log.size() == 1);
        CHECK(recorder.record().log[0].severity == Severity::note);
        return recorder.record().log[0].text;
    };
    CHECK(line(Notice::waiting, "HNPFMS_L.LUI", 5) == "target asked to wait: HNPFMS_L.LUI (5 s)");
    CHECK(line(Notice::file_not_available, "A.BIN", 3) == "file not available: A.BIN (3 s)");
    CHECK(line(Notice::port_option_ignored, "HNPFMS_L.LCI", 5900) ==
          "port option not taken by the target: HNPFMS_L.LCI (port 5900)");
    CHECK(line(Notice::status_counter_gap, "", 2) == "status files lost (2)");
    CHECK(line(Notice::files_missing, "", 0) == "requested files missing");
    // The value of these two is a TransferOutcome: 2 is a time-out.
    CHECK(line(Notice::status_file_failed, "HNPFMS_L.LCS", 2) ==
          "status file transfer failed: HNPFMS_L.LCS (timed out)");
    CHECK(line(Notice::transfer_failed, "A.BIN", 3) == "transfer failed: A.BIN (the peer sent an error)");
    // A number that is no outcome is left out, not shown as nonsense.
    CHECK(line(Notice::transfer_failed, "A.BIN", 200) == "transfer failed: A.BIN");
    CHECK(line(Notice::checksum_mismatch, "A.BIN", 0) == "checksum did not match: A.BIN");
    CHECK(line(Notice::status_duplicate, "", 0) == "status file repeated");
    CHECK(line(Notice::request_refused, "X", 0) == "request refused: X");
    CHECK(line(Notice::status_stale, "", 7) == "status file out of order");
    CHECK(line(Notice::checksum_unsupported, "A.BIN", 9) == "checksum algorithm not supported: A.BIN");
    CHECK(line(Notice::option_fallback, "", 0) == "options refused, asked again without them");
}

TEST_CASE("every notice has a sentence of its own", "[loader_core][recorder]")
{
    std::vector<std::string> sentences;
    for (int value = 0; value <= static_cast<int>(Notice::files_missing); ++value)
    {
        OperationRecorder recorder = recorder_for(OperationKind::upload);
        recorder.on_notice(static_cast<Notice>(value), "", 0);
        const std::string text = recorder.record().log.at(0).text;
        CHECK_FALSE(text.empty());
        CHECK(text != "unknown");
        CHECK(std::ranges::find(sentences, text) == sentences.end());
        sentences.push_back(text);
    }
    CHECK(sentences.size() == static_cast<std::size_t>(Notice::files_missing) + 1);
}

TEST_CASE("the list of a download is logged and the choice is left to the operator", "[loader_core][recorder]")
{
    OperationRecorder recorder = recorder_for(OperationKind::download_operator);
    const std::array<FileListing, 2> files{FileListing{"A.BIN", "the first"}, FileListing{"B.BIN", ""}};
    adl::arinc615a::DownloadList list;
    list.files = files;
    FileSelection selection;
    CHECK(recorder.on_download_list(list, selection) == SelectionDecision::deferred);
    CHECK(selection.size() == 0);
    CHECK(texts(recorder.record()) == std::vector<std::string>{"The target listed 2 file(s) it can download."});
}

TEST_CASE("a file the target read is a line, whatever the name", "[loader_core][recorder]")
{
    OperationRecorder recorder = recorder_for(OperationKind::upload);
    recorder.on_file_sent("LOAD.BIN", 123456);
    CHECK(texts(recorder.record()) == std::vector<std::string>{"The target read LOAD.BIN (123456 bytes)"});
}

TEST_CASE("the folder of a download is joined to a name with the separator the folder uses", "[loader_core][recorder]")
{
    const adl::arinc615a::ReceivedFile received{10, adl::arinc615a::ChecksumVerdict::matched};
    const auto path_of = [&](const std::string& folder)
    {
        OperationRecorder recorder = recorder_for(OperationKind::download_media);
        recorder.set_download_folder(folder);
        recorder.on_file_received("A.BIN", received);
        return recorder.record().files.at(0).path;
    };
    CHECK(path_of("/data/dl") == "/data/dl/A.BIN");
    CHECK(path_of("/data/dl/") == "/data/dl/A.BIN");
    CHECK(path_of("C:\\data\\dl") == "C:\\data\\dl\\A.BIN");
    CHECK(path_of("C:\\data\\dl\\") == "C:\\data\\dl\\A.BIN");
    CHECK(path_of("C:/data/dl") == "C:/data/dl/A.BIN");
    CHECK(path_of("dl") == "dl/A.BIN");
}

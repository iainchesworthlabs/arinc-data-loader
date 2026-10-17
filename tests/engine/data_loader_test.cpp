// Whole operations, from the runner on the fake platform to a simulated target and back: an
// Information, an Upload of a media set built with adl::arinc665, the two Downloads, and what
// happens to them when datagrams are lost, repeated and reordered, the target asks to wait,
// goes silent, aborts, or the operator does.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "rig.hpp"

using namespace adl::arinc615a;
using namespace adl::test;

namespace {

using std::chrono::milliseconds;
using std::chrono::seconds;

[[nodiscard]] std::size_t packets_containing(const std::vector<adl::fake::CapturedPacket>& packets,
                                             std::string_view text, std::uint16_t to_port = 0) {
    std::size_t count = 0;
    for (const auto& packet : packets) {
        if (to_port != 0 && packet.to.port != to_port) {
            continue;
        }
        const std::string_view bytes{reinterpret_cast<const char*>(packet.payload.data()),
                                     packet.payload.size()};
        if (bytes.find(text) != std::string_view::npos) {
            ++count;
        }
    }
    return count;
}

// What the target did, for a failure message.
std::string dump(const SimLog& log) {
    std::string text;
    for (const std::string& event : log.events) {
        text += event;
        text += '\n';
    }
    return text;
}

// A network slow enough that a few kilobytes in small blocks take many seconds.
adl::fake::LinkProfile slow_link() {
    adl::fake::LinkProfile profile;
    profile.latency = milliseconds(5);
    return profile;
}

SimPlan upload_plan(const SampleLoad& load) {
    SimPlan plan;
    plan.loads = {load.item};
    return plan;
}

std::vector<std::string> names_of(const std::vector<ReadItem>& reads) {
    std::vector<std::string> names;
    for (const ReadItem& item : reads) {
        names.push_back(item.name);
    }
    return names;
}

std::vector<DownloadItem> three_files() {
    return {DownloadItem{"A.BIN", pattern(3000, 1), "the first"},
            DownloadItem{"B.BIN", pattern(700, 2), "the second"},
            DownloadItem{"C.BIN", {}, "an empty one"}};
}

// The request for all of `files`, as .LNR carries it. `storage` holds what the list views.
DownloadRequest request_for(const std::vector<DownloadItem>& files,
                            std::vector<FileRequest>& storage) {
    storage.clear();
    for (const DownloadItem& file : files) {
        storage.push_back(FileRequest{file.name, {}});
    }
    return DownloadRequest{storage};
}

// Aborts at a given time.
class AbortAt final : public RunController {
public:
    AbortAt(Instant at, StatusCode status) : at_(at), status_(status) {}
    void on_poll(DataLoader& loader, Instant now) noexcept override {
        if (!done_ && now >= at_) {
            done_ = true;
            loader.abort(now, status_);
        }
    }

private:
    Instant at_;
    StatusCode status_;
    bool done_ = false;
};

}  // namespace

// ---- Information ----------------------------------------------------------------------------

TEST_CASE("an Information operation with no status files ends with the list",
          "[engine][operation][information]") {
    SimPlan plan;
    plan.fast_information = true;
    Rig rig(plan);
    const auto outcome = rig.run(InformationOperation{});
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::completed);
    CHECK(outcome->statuses_received == 0);
    CHECK(rig.recorder.initialization == std::vector<StatusCode>{StatusCode::accepted});
    CHECK(rig.recorder.configuration_lists == 1);
    CHECK(rig.recorder.hardware_names == std::vector<std::string>{"FMS LEFT"});
    CHECK(rig.recorder.part_numbers ==
          std::vector<std::string>{"ACM47-1234-5678", "ACM47-8765-4321"});
    CHECK(rig.recorder.status_counters.empty());
    // The checksum the target put on the list was acknowledged and matched.
    CHECK(rig.target.log().checksum_acknowledged);
    CHECK(rig.loader->idle());
    CHECK(rig.loader->outcome().files_received == 0);
}

TEST_CASE("an Information operation with status files ends with the last of them",
          "[engine][operation][information]") {
    Rig rig;
    const auto outcome = rig.run(InformationOperation{});
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::completed);
    CHECK(rig.recorder.status_codes ==
          std::vector<StatusCode>{StatusCode::accepted, StatusCode::completed});
    CHECK(rig.recorder.status_counters == std::vector<std::uint16_t>{0, 1});
    CHECK(rig.recorder.configuration_lists == 1);
    CHECK(outcome->last_status == StatusCode::completed);
    CHECK(outcome->statuses_received == 2);
    CHECK(outcome->statuses_lost == 0);
    CHECK(rig.target.finished());
}

// ---- Upload
// -----------------------------------------------------------------------------------------

TEST_CASE("a load of a media set is uploaded to the target", "[engine][operation][upload]") {
    const SampleLoad load = make_sample_load();
    Rig rig(upload_plan(load));
    rig.source = std::make_unique<MediaSource>(load.files);
    std::vector<LoadReference> storage;
    const auto outcome = rig.run(upload_of(load, storage));
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::completed);
    const SimLog& log = rig.target.log();
    CHECK(log.files_corrupt.empty());
    CHECK(log.read_failures.empty());
    CHECK(log.files_read == names_of(load.item.reads));
    CHECK(log.requested_files == std::vector<std::string>{load.item.header_name});
    CHECK(outcome->files_served == load.item.reads.size());
    CHECK(rig.recorder.sent_names == names_of(load.item.reads));
    CHECK(rig.recorder.last_status_loads == 1);
    CHECK(rig.recorder.status_codes.front() == StatusCode::accepted);
    CHECK(rig.recorder.status_codes.back() == StatusCode::completed);
    CHECK(rig.recorder.list_ratios.back() == 100);
    CHECK(rig.source->open_files() == 0);
    // The target put the header file's CRC on each request, and the source saw it.
    REQUIRE(rig.source->requests.size() == load.item.reads.size());
    for (std::size_t i = 0; i < load.item.reads.size(); ++i) {
        CHECK(rig.source->requests[i].crc16 == load.item.reads[i].crc16);
    }
    CHECK(outcome->items_failed == 0);
    CHECK(rig.loader->idle());
}

TEST_CASE("a large load goes across in a few thousand blocks", "[engine][operation][upload]") {
    const SampleLoad load = make_sample_load(150'000);
    SimPlan plan = upload_plan(load);
    plan.request_block_size = 64;
    Rig rig(plan);
    rig.source = std::make_unique<MediaSource>(load.files);
    std::vector<LoadReference> storage;
    const auto outcome = rig.run(upload_of(load, storage));
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::completed);
    CHECK(rig.target.log().files_corrupt.empty());
    CHECK(rig.target.log().files_read == names_of(load.item.reads));
    // The target's own heartbeat kept the loader satisfied meanwhile.
    CHECK(rig.recorder.status_codes.size() >= 2);
}

TEST_CASE("a short load is the target asking for fewer files", "[engine][operation][upload]") {
    const SampleLoad load = make_sample_load();
    SimPlan plan = upload_plan(load);
    plan.loads[0].reads.resize(2);  // the header and the first data file only
    Rig rig(plan);
    rig.source = std::make_unique<MediaSource>(load.files);
    std::vector<LoadReference> storage;
    const auto outcome = rig.run(upload_of(load, storage));
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::completed);
    CHECK(rig.target.log().files_read.size() == 2);
    CHECK(outcome->files_served == 2);
}

TEST_CASE("two files of one name are told apart by the checksum option",
          "[engine][operation][upload]") {
    auto spec = adl::samples::arinc615_media_set_spec();
    REQUIRE(spec);
    SampleLoad load;
    load.members = std::move(*adl::arinc665::build_media_set(*spec));
    load.files = MediaFiles::of(load.members);
    load.load = spec->loads[0].load;
    load.item.header_name = load.load.header_file_name;
    load.item.load_part_number = "ACM60-1234-5678";
    const auto add = [&](const std::string& name, const Bytes& bytes) {
        load.item.reads.push_back(ReadItem{name, bytes, crc16_value(bytes)});
    };
    add(load.load.header_file_name, load.load.header_bytes);
    for (const auto& file : load.load.data_files) {
        add(file.name, file.bytes);
    }
    for (const auto& file : load.load.support_files) {
        add(file.name, file.bytes);
    }
    // The disks' two LOAD.BIN differ in content, so in CRC.
    REQUIRE(load.load.data_files.size() == 2);
    REQUIRE(load.load.data_files[0].name == load.load.data_files[1].name);
    REQUIRE(load.item.reads[1].crc16 != load.item.reads[2].crc16);

    Rig rig(upload_plan(load));
    rig.source = std::make_unique<MediaSource>(load.files);
    std::vector<LoadReference> storage;
    const auto outcome = rig.run(upload_of(load, storage));
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::completed);
    // Each LOAD.BIN the target read is the one it expected, not the first of the name.
    CHECK(rig.target.log().files_corrupt.empty());
    CHECK(std::ranges::count(rig.target.log().files_read, "LOAD.BIN") == 2);
}

TEST_CASE("a file that is not there yet is answered with WAIT and read again",
          "[engine][operation][upload][wait]") {
    const SampleLoad load = make_sample_load();
    Rig rig(upload_plan(load));
    rig.source = std::make_unique<MediaSource>(load.files);
    rig.source->not_yet[load.item.header_name] = 3;  // [File_Not_Available]: insert the media
    std::vector<LoadReference> storage;
    const auto outcome = rig.run(upload_of(load, storage));
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::completed);
    CHECK(rig.target.log().waits_received == 1);
    CHECK(rig.target.log().files_corrupt.empty());
    CHECK(rig.target.log().files_read == names_of(load.item.reads));
    REQUIRE(rig.recorder.count(Notice::file_not_available) == 1);
    CHECK(rig.recorder.notices[0].value == 3);
    const auto header_requests = std::ranges::count(rig.source->requests, load.item.header_name,
                                                    &MediaSource::Request::name);
    CHECK(header_requests == 2);
}

TEST_CASE("a file the source will not give is refused with an error",
          "[engine][operation][upload]") {
    const SampleLoad load = make_sample_load();
    Rig rig(upload_plan(load));
    rig.source = std::make_unique<MediaSource>(load.files);
    rig.source->denied.insert(load.item.reads[1].name);
    std::vector<LoadReference> storage;
    const auto outcome = rig.run(upload_of(load, storage));
    REQUIRE(outcome);
    // The target gave up on that file and says so with a status of its own (1003).
    CHECK(outcome->code == OutcomeCode::aborted_by_target);
    CHECK(rig.target.log().read_failures == std::vector<std::string>{load.item.reads[1].name});
    CHECK(rig.recorder.count(Notice::request_refused) >= 1);
}

TEST_CASE("a file that does not exist is error 1", "[engine][operation][upload]") {
    const SampleLoad load = make_sample_load();
    SimPlan plan = upload_plan(load);
    plan.loads[0].reads.push_back(ReadItem{"MISSING.BIN", Bytes(3), std::nullopt});
    Rig rig(plan);
    rig.source = std::make_unique<MediaSource>(load.files);
    std::vector<LoadReference> storage;
    const auto outcome = rig.run(upload_of(load, storage));
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::aborted_by_target);
    CHECK(rig.target.log().read_failures == std::vector<std::string>{"MISSING.BIN"});
}

// ---- Downloads
// ------------------------------------------------------------------------------------------

TEST_CASE("a media defined download brings back the files asked for",
          "[engine][operation][download]") {
    SimPlan plan;
    plan.files = three_files();
    Rig rig(plan);
    std::vector<FileRequest> storage;
    const auto outcome = rig.run(MediaDownloadOperation{request_for(plan.files, storage)});
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::completed);
    CHECK(outcome->files_requested == 3);
    CHECK(outcome->files_received == 3);
    for (const DownloadItem& file : plan.files) {
        CAPTURE(file.name);
        REQUIRE(rig.sink.files.contains(file.name));
        CHECK(rig.sink.files.at(file.name) == file.content);
    }
    CHECK(rig.recorder.received_names == std::vector<std::string>{"A.BIN", "B.BIN", "C.BIN"});
    CHECK(std::ranges::all_of(rig.recorder.received_checksums,
                              [](ChecksumVerdict v) { return v == ChecksumVerdict::matched; }));
    CHECK(rig.sink.open_files() == 0);
    CHECK(rig.sink.discarded.empty());
    CHECK(rig.target.log().requested_files == std::vector<std::string>{"A.BIN", "B.BIN", "C.BIN"});
    CHECK(rig.target.log().checksum_acknowledged);
    CHECK(rig.recorder.status_codes.back() == StatusCode::completed);
    CHECK(rig.recorder.count(Notice::files_missing) == 0);
}

TEST_CASE("a wrong checksum is reported and the file is kept all the same",
          "[engine][operation][download][checksum]") {
    SimPlan plan;
    plan.files = three_files();
    plan.checksum = ChecksumMode::wrong;
    Rig rig(plan);
    std::vector<FileRequest> storage;
    const auto outcome = rig.run(MediaDownloadOperation{request_for(plan.files, storage)});
    REQUIRE(outcome);
    // 5.3.2.3.8.5: the data loader does not fail the transfer, nor the operation, and handles the
    // file as it does one that matched.
    CHECK(outcome->code == OutcomeCode::completed);
    CHECK(outcome->files_received == 3);
    CHECK(outcome->checksum_mismatches == 3);
    CHECK(rig.recorder.count(Notice::checksum_mismatch) == 3);
    for (const DownloadItem& file : plan.files) {
        CAPTURE(file.name);
        REQUIRE(rig.sink.files.contains(file.name));
        CHECK(rig.sink.files.at(file.name) == file.content);
    }
    CHECK(std::ranges::all_of(rig.recorder.received_checksums,
                              [](ChecksumVerdict v) { return v == ChecksumVerdict::mismatched; }));
}

TEST_CASE("a file with no checksum has none to check", "[engine][operation][download][checksum]") {
    SimPlan plan;
    plan.files = three_files();
    plan.checksum = ChecksumMode::none;
    Rig rig(plan);
    std::vector<FileRequest> storage;
    const auto outcome = rig.run(MediaDownloadOperation{request_for(plan.files, storage)});
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::completed);
    CHECK(std::ranges::all_of(rig.recorder.received_checksums,
                              [](ChecksumVerdict v) { return v == ChecksumVerdict::not_given; }));
    CHECK(rig.sink.last_expected_size.has_value());  // the size is announced even so
}

TEST_CASE("a file nobody asked for is refused", "[engine][operation][download]") {
    SimPlan plan;
    plan.files = three_files();
    plan.list_unrequested_file = true;
    Rig rig(plan);
    std::vector<FileRequest> storage;
    const auto outcome = rig.run(MediaDownloadOperation{request_for(plan.files, storage)});
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::completed);
    CHECK_FALSE(rig.sink.files.contains("ROGUE.BIN"));
    CHECK(std::ranges::find(rig.sink.opened, "ROGUE.BIN") == rig.sink.opened.end());
    CHECK(rig.recorder.count(Notice::request_refused) >= 1);
    CHECK(std::ranges::any_of(rig.recorder.notices, [](const Recorder::NoticeRecord& record) {
        return record.notice == Notice::request_refused && record.subject == "ROGUE.BIN";
    }));
    CHECK(outcome->files_received == 3);
}

TEST_CASE("files missing at the end are reported", "[engine][operation][download]") {
    SimPlan plan;
    plan.files = three_files();
    Rig rig(plan);
    // The loader asks for four files; the target only has three.
    std::vector<FileRequest> storage;
    DownloadRequest request = request_for(plan.files, storage);
    storage.push_back(FileRequest{"D.BIN", {}});
    request = DownloadRequest{storage};
    const auto outcome = rig.run(MediaDownloadOperation{request});
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::completed);
    CHECK(outcome->files_requested == 4);
    CHECK(outcome->files_received == 3);
    REQUIRE(rig.recorder.count(Notice::files_missing) == 1);
}

TEST_CASE("a file that cannot be stored ends the operation", "[engine][operation][download]") {
    SimPlan plan;
    plan.files = three_files();
    Rig rig(plan);
    rig.sink.fail_finish = true;
    std::vector<FileRequest> storage;
    const auto outcome = rig.run(MediaDownloadOperation{request_for(plan.files, storage)});
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::local_error);
    CHECK(rig.sink.open_files() == 0);
}

TEST_CASE("a sink that refuses a file is told so by an error to the target",
          "[engine][operation][download]") {
    SimPlan plan;
    plan.files = three_files();
    Rig rig(plan);
    rig.sink.refuse_open = true;
    std::vector<FileRequest> storage;
    const auto outcome = rig.run(MediaDownloadOperation{request_for(plan.files, storage)});
    REQUIRE(outcome);
    // Nothing arrived; the target went on to its final status anyway.
    CHECK(outcome->files_received == 0);
    CHECK(rig.recorder.count(Notice::request_refused) >= 3);
}

TEST_CASE("an operator defined download brings back the files the operator chose",
          "[engine][operation][download][operator]") {
    SimPlan plan;
    plan.files = three_files();
    Rig rig(plan);
    rig.recorder.choose = {"B.BIN", "C.BIN"};
    const auto outcome = rig.run(OperatorDownloadOperation{});
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::completed);
    CHECK(rig.recorder.listed_names == std::vector<std::string>{"A.BIN", "B.BIN", "C.BIN"});
    CHECK(rig.target.log().selected_files == std::vector<std::string>{"B.BIN", "C.BIN"});
    CHECK(rig.sink.files.size() == 2);
    CHECK(rig.sink.files.at("B.BIN") == plan.files[1].content);
    CHECK_FALSE(rig.sink.files.contains("A.BIN"));
    CHECK(outcome->files_requested == 2);
    CHECK(outcome->files_received == 2);
}

TEST_CASE("an operator defined download whose choice is made later",
          "[engine][operation][download][operator]") {
    SimPlan plan;
    plan.files = three_files();
    Rig rig(plan);
    rig.recorder.decision = SelectionDecision::deferred;

    // Chooses A.BIN a couple of seconds after the list has come.
    class Chooser final : public RunController {
    public:
        explicit Chooser(Rig& rig) : rig_(rig) {}
        void on_poll(DataLoader& loader, Instant now) noexcept override {
            if (!rig_.recorder.deferred_names.empty() && !provided_) {
                if (!seen_) {
                    seen_ = now;
                }
                if (now - *seen_ >= seconds(2)) {
                    provided_ = true;
                    const std::array<std::string_view, 1> names{"A.BIN"};
                    CHECK(loader.provide_selection(now, names));
                }
            }
        }
        bool provided() const { return provided_; }

    private:
        Rig& rig_;
        std::optional<Instant> seen_;
        bool provided_ = false;
    } chooser(rig);
    RunOptions options;
    options.controller = &chooser;
    const auto outcome = rig.run(OperatorDownloadOperation{}, options);
    REQUIRE(outcome);
    CHECK(chooser.provided());
    CHECK(outcome->code == OutcomeCode::completed);
    CHECK(rig.sink.files.size() == 1);
    CHECK(rig.sink.files.at("A.BIN") == plan.files[0].content);
}

TEST_CASE("a selection given when none is awaited is refused",
          "[engine][operation][download][operator]") {
    SimPlan plan;
    plan.files = three_files();
    Rig rig(plan);
    const auto outcome = rig.run(OperatorDownloadOperation{});
    REQUIRE(outcome);
    const std::array<std::string_view, 1> names{"A.BIN"};
    const auto late = rig.loader->provide_selection(rig.now(), names);
    REQUIRE_FALSE(late);
    CHECK(late.error() == Error::invalid_value);
}

TEST_CASE("an operator who chooses nothing aborts the operation",
          "[engine][operation][download][operator]") {
    SimPlan plan;
    plan.files = three_files();
    Rig rig(plan);
    rig.recorder.decision = SelectionDecision::cancelled;
    const auto outcome = rig.run(OperatorDownloadOperation{});
    REQUIRE(outcome);
    INFO(dump(rig.target.log()));
    CHECK(outcome->code == OutcomeCode::aborted_by_operator);
    CHECK(outcome->abort_confirmed);
    CHECK(rig.sink.files.empty());
    CHECK(rig.target.log().aborts_seen.size() == 1);
}

// ---- A load goes in and comes back --------------------------------------------------------

TEST_CASE("a load sent to the target comes back from it unchanged",
          "[engine][operation][roundtrip]") {
    const SampleLoad load = make_sample_load(60'000, 11);
    SimPlan plan = upload_plan(load);
    plan.request_block_size = 512;
    Rig rig(plan);
    rig.source = std::make_unique<MediaSource>(load.files);
    std::vector<LoadReference> loads;
    const auto uploaded = rig.run(upload_of(load, loads));
    REQUIRE(uploaded);
    REQUIRE(uploaded->code == OutcomeCode::completed);
    REQUIRE(rig.target.stored().size() == load.item.reads.size());

    // Everything the target now holds is asked for back, in a media defined download.
    rig.target.reset_operation();
    plan = SimPlan{};
    for (const auto& [name, bytes] : rig.target.stored()) {
        plan.files.push_back(DownloadItem{name, bytes, ""});
    }
    rig.target.plan() = plan;
    std::vector<FileRequest> storage;
    const auto downloaded =
        rig.run(MediaDownloadOperation{request_for(rig.target.plan().files, storage)});
    REQUIRE(downloaded);
    CHECK(downloaded->code == OutcomeCode::completed);
    CHECK(downloaded->files_received == load.item.reads.size());
    // What came back is what the media set holds, byte for byte.
    for (const ReadItem& item : load.item.reads) {
        CAPTURE(item.name);
        REQUIRE(rig.sink.files.contains(item.name));
        CHECK(rig.sink.files.at(item.name) == item.expected);
    }
    CHECK(downloaded->checksum_mismatches == 0);
}

// ---- The initialization step
// ---------------------------------------------------------------------------

TEST_CASE("a target that denies or does not support an operation ends it at once",
          "[engine][operation][init]") {
    struct Case {
        StatusCode status;
        OutcomeCode code;
    };
    for (const Case& scenario :
         {Case{StatusCode::operation_denied, OutcomeCode::denied},
          Case{StatusCode::operation_not_supported, OutcomeCode::not_supported}}) {
        for (int kind = 0; kind < 4; ++kind) {
            CAPTURE(static_cast<int>(scenario.status), kind);
            SimPlan plan;
            plan.init_status = scenario.status;
            plan.init_text = "not now";
            plan.files = three_files();
            const SampleLoad load = make_sample_load();
            plan.loads = {load.item};
            Rig rig(plan);
            rig.source = std::make_unique<MediaSource>(load.files);
            std::vector<LoadReference> loads;
            std::vector<FileRequest> files;
            Operation operation = InformationOperation{};
            if (kind == 1) {
                operation = upload_of(load, loads);
            } else if (kind == 2) {
                operation = MediaDownloadOperation{request_for(plan.files, files)};
            } else if (kind == 3) {
                operation = OperatorDownloadOperation{};
            }
            const auto outcome = rig.run(operation);
            REQUIRE(outcome);
            CHECK(outcome->code == scenario.code);
            CHECK(rig.recorder.initialization == std::vector<StatusCode>{scenario.status});
            CHECK(rig.recorder.initialization_text == std::vector<std::string>{"not now"});
            CHECK(rig.elapsed < seconds(3));
            // Nothing but the initialization file was asked for or sent.
            CHECK(rig.target.log().lists_received == 0);
            CHECK(rig.recorder.status_codes.empty());
        }
    }
}

TEST_CASE("a WAIT answer to the first request makes the loader ask again after the wait",
          "[engine][operation][init][wait]") {
    SimPlan plan;
    plan.init_wait_seconds = 5;
    plan.fast_information = true;
    Rig rig(plan);
    const auto outcome = rig.run(InformationOperation{});
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::completed);
    const auto& requests = rig.target.log().init_requests_at;
    REQUIRE(requests.size() == 2);
    const Duration gap = requests[1] - requests[0];
    CHECK(gap >= seconds(5));
    CHECK(gap < seconds(5) + milliseconds(10));
    CHECK(rig.recorder.count(Notice::waiting) == 1);
    CHECK(rig.recorder.notices[0].value == 5);
}

TEST_CASE("a target that keeps saying WAIT is given up on", "[engine][operation][init][wait]") {
    SimPlan plan;
    plan.init_wait_seconds = 2;
    plan.init_wait_always = true;
    Rig rig(plan);
    rig.config.max_waits = 2;
    const auto outcome = rig.run(InformationOperation{});
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::transfer_failed);
    CHECK(outcome->failed_transfer == TransferOutcome::wait_requested);
    CHECK(rig.target.log().init_requests_at.size() == 3);
}

TEST_CASE("a target that never answers the first request fails the operation after the retries",
          "[engine][operation][init]") {
    SimPlan plan;
    plan.init_silent = true;
    Rig rig(plan);
    const auto outcome = rig.run(InformationOperation{});
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::transfer_failed);
    CHECK(outcome->failed_transfer == TransferOutcome::timed_out);
    // Attachment 4: two tries of the request (4 s), and the whole transfer once more (4 s).
    CHECK(rig.target.log().init_requests_at.size() == 4);
    CHECK(rig.elapsed >= seconds(8));
    CHECK(rig.elapsed < seconds(8) + milliseconds(100));
}

TEST_CASE("a target that refuses options is asked again without them",
          "[engine][operation][init][options]") {
    SimPlan plan;
    plan.init_refuses_options = true;
    plan.fast_information = true;
    Rig rig(plan);
    const auto outcome = rig.run(InformationOperation{});
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::completed);
    CHECK(rig.recorder.count(Notice::option_fallback) == 1);
    CHECK(rig.target.log().init_requests_at.size() == 2);
}

TEST_CASE("a file that is not a valid structure is a fatal error", "[engine][operation][init]") {
    SimPlan plan;
    plan.init_garbage = true;
    Rig rig(plan);
    const auto outcome = rig.run(InformationOperation{});
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::protocol_error);
    CHECK(rig.target.log().init_requests_at.size() == 1);  // not a transfer failure: no retry
}

TEST_CASE("the port option moves the control port the target writes to",
          "[engine][operation][port]") {
    SimPlan plan;
    plan.fast_information = true;
    Rig rig(plan);
    rig.config.control_port = 1615;
    const auto outcome = rig.run(InformationOperation{});
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::completed);
    CHECK(rig.target.log().init_options_seen == 1);
    CHECK(rig.target.log().loader_port_used == 1615);
    CHECK(rig.recorder.count(Notice::port_option_ignored) == 0);
    CHECK(rig.recorder.configuration_lists == 1);
}

TEST_CASE("the default control port is not sent as an option", "[engine][operation][port]") {
    SimPlan plan;
    plan.fast_information = true;
    Rig rig(plan);
    const auto outcome = rig.run(InformationOperation{});
    REQUIRE(outcome);
    CHECK(packets_containing(rig.network.captured(), "port", 59) == 0);
    CHECK(rig.target.log().loader_port_used == 59);
}

TEST_CASE("a target that ignores the port option is noticed", "[engine][operation][port]") {
    SimPlan plan;
    plan.honor_port_option = false;
    Rig rig(plan);
    rig.config.control_port = 1615;
    const auto outcome = rig.run(InformationOperation{});
    REQUIRE(outcome);
    // Its status files go to port 59, where nobody listens: the time-out ends the operation.
    CHECK(outcome->code == OutcomeCode::dlp_timed_out);
    CHECK(rig.recorder.count(Notice::port_option_ignored) == 1);
    CHECK(rig.target.log().loader_port_used == 59);
}

// ---- Status files
// -----------------------------------------------------------------------------------------

TEST_CASE("a lost status file is seen in the counter", "[engine][operation][status]") {
    const SampleLoad load = make_sample_load();
    SimPlan plan = upload_plan(load);
    plan.work_time = seconds(20);
    plan.skip_counter_at = 1;
    Rig rig(plan);
    rig.source = std::make_unique<MediaSource>(load.files);
    std::vector<LoadReference> storage;
    const auto outcome = rig.run(upload_of(load, storage));
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::completed);
    CHECK(outcome->statuses_lost == 1);
    CHECK(rig.recorder.count(Notice::status_counter_gap) == 1);
    CHECK(rig.recorder.notices.front().value == 1);
    CHECK(outcome->statuses_repeated == 0);
}

TEST_CASE("a status file that arrives twice is shown once", "[engine][operation][status]") {
    const SampleLoad load = make_sample_load();
    SimPlan plan = upload_plan(load);
    plan.work_time = seconds(20);
    plan.repeat_status_index = 1;
    Rig rig(plan);
    rig.source = std::make_unique<MediaSource>(load.files);
    std::vector<LoadReference> storage;
    const auto outcome = rig.run(upload_of(load, storage));
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::completed);
    CHECK(outcome->statuses_repeated == 1);
    CHECK(outcome->statuses_lost == 0);
    CHECK(rig.recorder.count(Notice::status_duplicate) == 1);
    // Each counter reached the operator once, in order.
    CHECK(std::ranges::is_sorted(rig.recorder.status_counters));
    CHECK(std::ranges::adjacent_find(rig.recorder.status_counters) ==
          rig.recorder.status_counters.end());
}

TEST_CASE("the first status file may carry counter 0 or 1, and a larger one means files were lost",
          "[engine][operation][status]") {
    for (const std::uint16_t first : {std::uint16_t{0}, std::uint16_t{1}, std::uint16_t{5}}) {
        CAPTURE(first);
        SimPlan plan;
        plan.first_counter = first;
        Rig rig(plan);
        const auto outcome = rig.run(InformationOperation{});
        REQUIRE(outcome);
        CHECK(outcome->code == OutcomeCode::completed);
        CHECK(outcome->statuses_lost == (first > 1 ? first - 1U : 0U));
        CHECK(rig.recorder.count(Notice::status_counter_gap) == (first > 1 ? 1U : 0U));
    }
}

TEST_CASE("a status file that is not a valid structure is a fatal error",
          "[engine][operation][status]") {
    const SampleLoad load = make_sample_load();
    SimPlan plan = upload_plan(load);
    plan.work_time = seconds(20);
    plan.truncate_status = true;
    Rig rig(plan);
    rig.source = std::make_unique<MediaSource>(load.files);
    std::vector<LoadReference> storage;
    const auto outcome = rig.run(upload_of(load, storage));
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::protocol_error);
}

// ---- Time-outs
// ----------------------------------------------------------------------------------------------

TEST_CASE("thirteen seconds without a status file ends the operation",
          "[engine][operation][timeout]") {
    const SampleLoad load = make_sample_load();
    SimPlan plan = upload_plan(load);
    plan.work_time = seconds(60);
    plan.silent_after_statuses = 2;  // two status files, then nothing
    Rig rig(plan);
    rig.source = std::make_unique<MediaSource>(load.files);
    std::vector<LoadReference> storage;
    const auto outcome = rig.run(upload_of(load, storage));
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::dlp_timed_out);
    REQUIRE(rig.target.log().last_status_end);
    // Attachment 4, 4-4.1: 13 seconds from the last packet of the last status file.
    const Duration silent = rig.ended - *rig.target.log().last_status_end;
    CHECK(silent >= seconds(13) - milliseconds(5));
    CHECK(silent <= seconds(13) + milliseconds(5));
    CHECK(outcome->statuses_received == 2);
}

TEST_CASE("a target that never sends a first status file times out too",
          "[engine][operation][timeout]") {
    const SampleLoad load = make_sample_load();
    SimPlan plan = upload_plan(load);
    plan.silent_after_statuses = 0;
    Rig rig(plan);
    rig.source = std::make_unique<MediaSource>(load.files);
    std::vector<LoadReference> storage;
    const auto outcome = rig.run(upload_of(load, storage));
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::dlp_timed_out);
    CHECK(rig.elapsed >= seconds(13));
    CHECK(rig.elapsed < seconds(13) + milliseconds(50));
}

TEST_CASE("the exception timer lets the target stay silent for longer",
          "[engine][operation][timeout]") {
    const SampleLoad load = make_sample_load();
    SimPlan plan = upload_plan(load);
    plan.work_time = seconds(40);
    plan.exception_timer = 20;             // in the third status file...
    plan.exception_silence = seconds(18);  // ...and it is silent for 18 seconds
    Rig rig(plan);
    rig.source = std::make_unique<MediaSource>(load.files);
    std::vector<LoadReference> storage;
    const auto outcome = rig.run(upload_of(load, storage));
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::completed);
    // The announced silence is on the record, and the gap really was longer than 13 s.
    CHECK(std::ranges::find(rig.recorder.exception_timers, 20) !=
          rig.recorder.exception_timers.end());
    const auto& starts = rig.target.log().status_started_at;
    Duration longest{};
    for (std::size_t i = 1; i < starts.size(); ++i) {
        longest = std::max(longest, starts[i] - starts[i - 1]);
    }
    CHECK(longest > seconds(13));
}

TEST_CASE("a target that is silent for longer than its exception timer is timed out",
          "[engine][operation][timeout]") {
    const SampleLoad load = make_sample_load();
    SimPlan plan = upload_plan(load);
    plan.work_time = seconds(60);
    plan.exception_timer = 20;
    plan.exception_silence = seconds(40);
    Rig rig(plan);
    rig.source = std::make_unique<MediaSource>(load.files);
    std::vector<LoadReference> storage;
    const auto outcome = rig.run(upload_of(load, storage));
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::exception_timer_expired);
    REQUIRE(rig.target.log().last_status_end);
    const Duration silent = rig.ended - *rig.target.log().last_status_end;
    CHECK(silent >= seconds(20) - milliseconds(5));
    CHECK(silent <= seconds(20) + milliseconds(5));
}

// ---- Aborts
// ---------------------------------------------------------------------------------------------------

TEST_CASE("the operator's abort stops an upload and the target confirms it",
          "[engine][operation][abort]") {
    for (const StatusCode status :
         {StatusCode::aborted_by_operator, StatusCode::aborted_by_data_loader}) {
        CAPTURE(static_cast<int>(status));
        const SampleLoad load = make_sample_load(60'000);
        SimPlan plan = upload_plan(load);
        plan.request_block_size =
            32;  // 1900 blocks at 10 ms each: still at it when the abort comes
        Rig rig(plan);
        rig.network.set_profile(slow_link());
        rig.source = std::make_unique<MediaSource>(load.files);
        std::vector<LoadReference> storage;
        AbortAt abort(Instant{seconds(8)}, status);
        RunOptions options;
        options.controller = &abort;
        const auto outcome = rig.run(upload_of(load, storage), options);
        REQUIRE(outcome);
        INFO(dump(rig.target.log()));
        const OutcomeCode expected = status == StatusCode::aborted_by_operator
                                         ? OutcomeCode::aborted_by_operator
                                         : OutcomeCode::aborted_by_data_loader;
        CHECK(outcome->code == expected);
        CHECK(outcome->abort_confirmed);
        // The target saw the ABORT message with the code, and answered with a status file that
        // echoes it (5.4.5.2).
        REQUIRE_FALSE(rig.target.log().aborts_seen.empty());
        CHECK(rig.target.log().aborts_seen.front() == status);
        CHECK(rig.target.log().final_status == status);
        CHECK(rig.recorder.status_codes.back() == status);
        // And it did not carry on: the big file never finished.
        CHECK(rig.target.log().files_read.size() < load.item.reads.size());
        CHECK(rig.source->open_files() == 0);
        CHECK(rig.loader->idle());
        // The read that was open was ended with the message on its own transfer (6.3.5), not
        // only when the target next asked for a status file.
        std::size_t from_transfers = 0;
        for (const auto& packet : rig.network.captured()) {
            const std::string_view bytes{reinterpret_cast<const char*>(packet.payload.data()),
                                         packet.payload.size()};
            if (packet.from.address == Rig::loader_address &&
                packet.from.port != tftp_control_port &&
                bytes.find("ABORT:") != std::string_view::npos) {
                ++from_transfers;
            }
        }
        CHECK(from_transfers >= 1);
    }
}

TEST_CASE("an abort before the target has accepted needs nothing from it",
          "[engine][operation][abort]") {
    SimPlan plan;
    plan.init_wait_seconds = 20;  // the loader is waiting to ask again
    Rig rig(plan);
    AbortAt abort(Instant{seconds(3)}, StatusCode::aborted_by_operator);
    RunOptions options;
    options.controller = &abort;
    const auto outcome = rig.run(InformationOperation{}, options);
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::aborted_by_operator);
    CHECK_FALSE(outcome->abort_confirmed);
    CHECK(rig.elapsed < seconds(4));
    CHECK(rig.target.log().init_requests_at.size() == 1);
}

TEST_CASE("an abort the target never confirms ends after the data load protocol time-out",
          "[engine][operation][abort]") {
    const SampleLoad load = make_sample_load();
    SimPlan plan = upload_plan(load);
    plan.work_time = seconds(60);
    plan.silent_after_statuses = 1;
    Rig rig(plan);
    rig.source = std::make_unique<MediaSource>(load.files);
    std::vector<LoadReference> storage;
    AbortAt abort(Instant{seconds(6)}, StatusCode::aborted_by_operator);
    RunOptions options;
    options.controller = &abort;
    const auto outcome = rig.run(upload_of(load, storage), options);
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::aborted_by_operator);
    CHECK_FALSE(outcome->abort_confirmed);
    CHECK(rig.elapsed >= seconds(19));
    CHECK(rig.elapsed < seconds(19) + milliseconds(500));
}

TEST_CASE("a target's own abort ends the operation and the other transfers with ABORT 1003",
          "[engine][operation][abort]") {
    const SampleLoad load = make_sample_load(60'000);
    SimPlan plan = upload_plan(load);
    plan.request_block_size = 32;
    plan.abort_after_statuses = 2;
    Rig rig(plan);
    rig.network.set_profile(slow_link());
    rig.source = std::make_unique<MediaSource>(load.files);
    std::vector<LoadReference> storage;
    const auto outcome = rig.run(upload_of(load, storage));
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::aborted_by_target);
    CHECK(outcome->last_status == StatusCode::aborted_by_target);
    // The read still going when the status file said so is ended with ABORT:1003 (6.3.5).
    CHECK(packets_containing(rig.network.captured(), "ABORT:1003") >= 1);
    CHECK(rig.source->open_files() == 0);
    CHECK(rig.loader->idle());
}

TEST_CASE("a download is aborted by the operator and the file in flight is thrown away",
          "[engine][operation][abort]") {
    SimPlan plan;
    plan.files = {DownloadItem{"BIG.BIN", pattern(40'000, 3), "big"}};
    plan.request_block_size = 32;
    Rig rig(plan);
    rig.network.set_profile(slow_link());
    std::vector<FileRequest> storage;
    AbortAt abort(Instant{seconds(6)}, StatusCode::aborted_by_operator);
    RunOptions options;
    options.controller = &abort;
    const auto outcome = rig.run(MediaDownloadOperation{request_for(plan.files, storage)}, options);
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::aborted_by_operator);
    CHECK(outcome->abort_confirmed);
    CHECK(rig.sink.files.empty());
    CHECK(rig.sink.discarded == std::vector<std::string>{"BIG.BIN"});
    CHECK(rig.sink.open_files() == 0);
}

// ---- Protocol version
// -----------------------------------------------------------------------------------------

TEST_CASE("a protocol version that is not A4 is aborted at the first status file request",
          "[engine][operation][version]") {
    SimPlan plan;
    plan.init_version[0] = 'B';
    plan.init_version[1] = '5';
    Rig rig(plan);
    const auto outcome = rig.run(InformationOperation{});
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::unsupported_protocol_version);
    CHECK(rig.recorder.initialization.empty());  // its structure was not read
    REQUIRE(rig.target.log().aborts_seen.size() == 1);
    CHECK(rig.target.log().aborts_seen[0] == StatusCode::aborted_by_data_loader);
    CHECK(rig.elapsed < seconds(5));
}

TEST_CASE("a target on another version that sends no status file is waited for",
          "[engine][operation][version]") {
    SimPlan plan;
    plan.init_version[0] = 'B';
    plan.init_version[1] = '5';
    plan.silent_after_statuses = 0;
    Rig rig(plan);
    const auto outcome = rig.run(InformationOperation{});
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::unsupported_protocol_version);
    CHECK(rig.elapsed >= seconds(13));
}

// ---- A network that loses, repeats and reorders
// ------------------------------------------------------------

TEST_CASE("an upload over a network that loses, repeats and reorders datagrams",
          "[engine][operation][faults]") {
    const SampleLoad load = make_sample_load(20'000);
    unsigned completed = 0;
    unsigned failed = 0;
    for (std::uint64_t seed = 1; seed <= 24; ++seed) {
        CAPTURE(seed);
        SimPlan plan = upload_plan(load);
        plan.request_block_size = 256;
        Rig rig(plan, seed);
        adl::fake::LinkProfile profile;
        profile.latency = milliseconds(1);
        profile.jitter = milliseconds(30);
        profile.loss_per_million = 5'000;
        profile.duplicate_per_million = 60'000;
        rig.network.set_profile(profile);
        rig.source = std::make_unique<MediaSource>(load.files);
        std::vector<LoadReference> storage;
        const auto outcome = rig.run(upload_of(load, storage));
        REQUIRE(outcome);
        if (outcome->code == OutcomeCode::completed) {
            ++completed;
            // However the datagrams were treated, what the target read is what was there.
            CHECK(rig.target.log().files_corrupt.empty());
            CHECK(rig.target.log().files_read == names_of(load.item.reads));
        } else {
            ++failed;
            // With one retry a run of bad luck can end an operation, and then it says why.
            const bool acceptable = outcome->code == OutcomeCode::transfer_failed ||
                                    outcome->code == OutcomeCode::dlp_timed_out ||
                                    outcome->code == OutcomeCode::aborted_by_target;
            CHECK(acceptable);
            CHECK(rig.target.log().files_corrupt.empty());
        }
        CHECK(rig.loader->idle());
        CHECK(rig.source->open_files() == 0);
    }
    CHECK(completed >= 18);
    CHECK(completed + failed == 24);
}

TEST_CASE("a download over a network that loses, repeats and reorders datagrams",
          "[engine][operation][faults]") {
    unsigned completed = 0;
    for (std::uint64_t seed = 1; seed <= 24; ++seed) {
        CAPTURE(seed);
        SimPlan plan;
        plan.files = three_files();
        plan.request_block_size = 256;
        Rig rig(plan, seed);
        adl::fake::LinkProfile profile;
        profile.latency = milliseconds(1);
        profile.jitter = milliseconds(30);
        profile.loss_per_million = 5'000;
        profile.duplicate_per_million = 60'000;
        rig.network.set_profile(profile);
        std::vector<FileRequest> storage;
        const auto outcome = rig.run(MediaDownloadOperation{request_for(plan.files, storage)});
        REQUIRE(outcome);
        // Whatever arrived and was kept is exactly the file.
        for (const auto& [name, bytes] : rig.sink.files) {
            const auto item = std::ranges::find(plan.files, name, &DownloadItem::name);
            REQUIRE(item != plan.files.end());
            CHECK(bytes == item->content);
        }
        CHECK(rig.sink.open_files() == 0);
        CHECK(rig.loader->idle());
        if (outcome->code == OutcomeCode::completed) {
            ++completed;
            CHECK(outcome->files_received + (outcome->files_requested - outcome->files_received) ==
                  3);
        }
    }
    CHECK(completed >= 18);
}

TEST_CASE("a network that loses most datagrams ends the operation instead of hanging",
          "[engine][operation][faults]") {
    const SampleLoad load = make_sample_load();
    for (std::uint64_t seed = 1; seed <= 6; ++seed) {
        CAPTURE(seed);
        Rig rig(upload_plan(load), seed);
        adl::fake::LinkProfile profile;
        profile.loss_per_million = 450'000;
        rig.network.set_profile(profile);
        rig.source = std::make_unique<MediaSource>(load.files);
        std::vector<LoadReference> storage;
        const auto outcome = rig.run(upload_of(load, storage));
        REQUIRE(outcome);
        CHECK(outcome->code != OutcomeCode::running);
        CHECK(outcome->code != OutcomeCode::completed);
        CHECK(rig.elapsed < std::chrono::minutes(5));
        CHECK(rig.loader->idle());
    }
}

TEST_CASE("the same seed gives the same history", "[engine][operation][faults]") {
    const SampleLoad load = make_sample_load(20'000);
    std::vector<std::size_t> sent;
    std::vector<Duration> times;
    for (int run = 0; run < 2; ++run) {
        SimPlan plan = upload_plan(load);
        plan.request_block_size = 256;
        Rig rig(plan, 7);
        adl::fake::LinkProfile profile;
        profile.jitter = milliseconds(30);
        profile.loss_per_million = 10'000;
        profile.duplicate_per_million = 60'000;
        rig.network.set_profile(profile);
        rig.source = std::make_unique<MediaSource>(load.files);
        std::vector<LoadReference> storage;
        const auto outcome = rig.run(upload_of(load, storage));
        REQUIRE(outcome);
        sent.push_back(rig.network.counters().sent);
        times.push_back(rig.elapsed);
    }
    CHECK(sent[0] == sent[1]);
    CHECK(times[0] == times[1]);
}

// ---- The runner
// ------------------------------------------------------------------------------------------------------

TEST_CASE("the runner reports a control port that is taken", "[engine][runner]") {
    SimPlan plan;
    plan.fast_information = true;
    Rig rig(plan);
    adl::platform::UdpOptions options;
    options.local = adl::platform::Endpoint{adl::platform::Ipv4Address::any(), 59};
    auto holder = rig.loader_host.network().open_udp(options);
    REQUIRE(holder);
    const auto outcome = rig.run(InformationOperation{});
    REQUIRE_FALSE(outcome);
    CHECK(outcome.error().stage == RunError::Stage::control_socket);
    CHECK(outcome.error().platform == adl::platform::ErrorCode::address_in_use);
}

TEST_CASE("the runner reports a request the loader cannot take", "[engine][runner]") {
    Rig rig;
    rig.config.thw_id_pos = "";
    const auto outcome = rig.run(InformationOperation{});
    REQUIRE_FALSE(outcome);
    CHECK(outcome.error().stage == RunError::Stage::start);
    CHECK(outcome.error().engine == Error::invalid_value);
}

TEST_CASE("a loader with too small a workspace says so", "[engine][runner]") {
    Rig rig;
    std::vector<std::byte> small(100);
    LoaderServices services;
    DataLoader loader(rig.config, small, services);
    const auto started = loader.start(Instant{}, InformationOperation{});
    REQUIRE_FALSE(started);
    CHECK(started.error() == Error::buffer_too_small);
}

TEST_CASE("an operation is started once", "[engine][runner]") {
    SimPlan plan;
    plan.fast_information = true;
    Rig rig(plan);
    const auto outcome = rig.run(InformationOperation{});
    REQUIRE(outcome);
    const auto again = rig.loader->start(rig.now(), InformationOperation{});
    REQUIRE_FALSE(again);
}

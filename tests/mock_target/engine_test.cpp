// The mock target's engine, headless: the loader's real engine (DataLoader and Runner over a
// fake host) runs whole operations against the mock engine over the fake network, so the mock
// target is proven to be a valid 615A target for the loader. FIND, Information, Upload (byte
// for byte), the two Download modes, an operator abort with its confirmation, and the
// injection controls.

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <expected>
#include <string>
#include <vector>

#include "adl/arinc615a/directory_files.hpp"
#include "adl/arinc615a/find_client.hpp"
#include "adl/arinc615a/runner.hpp"
#include "fake_link.hpp"
#include "loader_rig.hpp"

using namespace adl::arinc615a;
using namespace adl::mock;
using namespace adl::test;

namespace {

// What the mock target reported, for the assertions.
struct EngineRecorder final : public EngineListener {
    std::vector<std::pair<int, std::string>> lines;
    std::vector<std::string> operations;
    std::vector<int> progress;
    std::vector<UploadRecord> uploads;

    void log(Severity severity, std::string_view text) override {
        lines.emplace_back(static_cast<int>(severity), std::string{text});
    }
    void operation_changed(std::string_view name) override { operations.emplace_back(name); }
    void progress_changed(int percent) override { progress.push_back(percent); }
    void upload_received(const UploadRecord& record) override { uploads.push_back(record); }

    [[nodiscard]] bool contains(std::string_view part) const {
        return std::ranges::any_of(lines,
                                   [part](const auto& line) { return line.second.contains(part); });
    }
};

// A mock target over the fake link, started, with its listener.
struct MockTarget {
    MockRig rig;
    EngineRecorder listener;
    MockEngine engine{&listener};
    FakeTargetLink link{rig.world, rig.network, rig.target_address};
    Config config{};

    explicit MockTarget(Config with = {}) : config(std::move(with)) {
        link.attach(engine);
        const auto error = engine.start(link, config);
        REQUIRE_FALSE(error);
        if (error) {
            FAIL("the engine could not start: " + *error);
        }
    }
};

TEST_CASE("FIND is answered with the configured identity", "[mock][find]") {
    MockTarget target;
    std::array<FindTarget, 8> answers{};
    Runner runner(target.rig.loader_host);
    const auto count = runner.find(FindConfig{}, answers);
    REQUIRE(count);
    REQUIRE(*count == 1);
    CHECK(answers[0].from.address == Address{{10, 0, 0, 2}});
    CHECK(answers[0].hardware_id.view() == "HNPFMS");
    CHECK(answers[0].type_name.view() == "FMS");
    CHECK(answers[0].position.view() == "L");
    CHECK(answers[0].literal_name.view() == "FMS LEFT");
    CHECK(answers[0].manufacturer_code.view() == "HNP");
    CHECK(target.listener.contains("answered FIND"));
}

TEST_CASE("an Information operation delivers the configuration", "[mock][information]") {
    Config config;
    config.information = {adl::mock::HardwareItem{
        "FMS LEFT",
        "SN-0001",
        {{"ACM47-1234-5678", "", "FMS 1 OPS"}, {"ACM47-8765-4321", "A", "FMS 1 DATA"}}}};
    MockTarget target(config);
    LoaderRecorder recorder;
    LoaderServices services;
    services.listener = &recorder;
    const auto outcome = target.rig.run(InformationOperation{}, services);
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::completed);
    CHECK(recorder.initialization == std::vector<StatusCode>{StatusCode::accepted});
    CHECK(recorder.configuration_lists == 1);
    CHECK(recorder.hardware_names == std::vector<std::string>{"FMS LEFT"});
    REQUIRE(recorder.part_numbers.size() == 2);
    CHECK(recorder.part_numbers[0] == "ACM47-1234-5678");
    CHECK(recorder.part_numbers[1] == "ACM47-8765-4321");
    REQUIRE_FALSE(recorder.status_codes.empty());
    CHECK(recorder.status_codes.back() == StatusCode::completed);
    CHECK(target.listener.contains("served HNPFMS_L.LCI"));
    CHECK(target.listener.contains("Information operation started"));
    CHECK(target.listener.contains("Information operation finished: completed"));
}

TEST_CASE("an Upload delivers every file byte for byte", "[mock][upload]") {
    MockTarget target;
    const adl::arinc665::Load load = make_load(30000, 7);
    put_load_files(target.rig.loader_host, load);
    DirectoryFileSource source(target.rig.loader_host.files(), "/load");
    const std::vector<LoadReference> references = upload_references(load);
    LoaderRecorder recorder;
    LoaderServices services;
    services.source = &source;
    services.listener = &recorder;
    const auto outcome = target.rig.run(UploadOperation{UploadRequest{references}}, services);
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::completed);
    CHECK(outcome->items_failed == 0);
    CHECK(outcome->files_served == 1 + load.data_files.size() + load.support_files.size());

    // Every file arrived at the target, byte for byte.
    CHECK(target.engine.stored().at(load.header_file_name) == load.header_bytes);
    for (const auto& file : load.data_files) {
        INFO(file.name);
        CHECK(target.engine.stored().at(file.name) == file.bytes);
    }
    for (const auto& file : load.support_files) {
        INFO(file.name);
        CHECK(target.engine.stored().at(file.name) == file.bytes);
    }

    // The completed upload was handed over for storage.
    REQUIRE(target.listener.uploads.size() == 1);
    CHECK(target.listener.uploads[0].load_part_number == adl::samples::sample_load_part_number);
    CHECK(target.listener.uploads[0].files.size() ==
          1 + load.data_files.size() + load.support_files.size());

    // Progress climbed to 100, and the log tells the story.
    CHECK(std::ranges::find(target.listener.progress, 100) != target.listener.progress.end());
    CHECK(target.listener.contains("read " + load.header_file_name + " ("));
    for (const auto& file : load.data_files) {
        CHECK(target.listener.contains("read " + file.name + " ("));
    }
    REQUIRE_FALSE(recorder.status_codes.empty());
    CHECK(recorder.status_codes.back() == StatusCode::completed);
}

TEST_CASE("a media defined Download writes the held files", "[mock][download]") {
    Config config;
    config.holds = {HeldFile{"F1.BIN", pattern(5000, 11), "file one"},
                    HeldFile{"F2.BIN", pattern(300, 12), "file two"}};
    MockTarget target(config);
    MemorySink sink;
    const std::vector<FileRequest> files = {FileRequest{"F1.BIN", {}}, FileRequest{"F2.BIN", {}}};
    LoaderRecorder recorder;
    LoaderServices services;
    services.sink = &sink;
    services.listener = &recorder;
    const auto outcome = target.rig.run(MediaDownloadOperation{DownloadRequest{files}}, services);
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::completed);
    CHECK(outcome->files_received == 2);
    REQUIRE(sink.files.size() == 2);
    CHECK(sink.files.at("F1.BIN") == config.holds[0].content);
    CHECK(sink.files.at("F2.BIN") == config.holds[1].content);
    CHECK(recorder.received_names == std::vector<std::string>{"F1.BIN", "F2.BIN"});
    CHECK(target.listener.contains("wrote F1.BIN"));
    CHECK(target.listener.contains("Download (media defined) operation finished: completed"));
}

TEST_CASE("an operator defined Download serves only the chosen files", "[mock][download]") {
    Config config;
    config.holds = {HeldFile{"A.BIN", pattern(100, 21), "a"},
                    HeldFile{"B.BIN", pattern(200, 22), "b"}};
    MockTarget target(config);
    MemorySink sink;
    LoaderRecorder recorder;
    recorder.choose = {"B.BIN"};
    LoaderServices services;
    services.sink = &sink;
    services.listener = &recorder;
    const auto outcome = target.rig.run(OperatorDownloadOperation{}, services);
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::completed);
    CHECK(recorder.listed_names == std::vector<std::string>{"A.BIN", "B.BIN"});
    REQUIRE(sink.files.size() == 1);
    CHECK(sink.files.contains("B.BIN"));
    CHECK(sink.files.at("B.BIN") == config.holds[1].content);
}

TEST_CASE("an operator abort in the middle of an Upload is confirmed", "[mock][abort]") {
    MockTarget target;
    const adl::arinc665::Load load = make_load(3'000'000, 9);
    put_load_files(target.rig.loader_host, load);
    DirectoryFileSource source(target.rig.loader_host.files(), "/load");
    const std::vector<LoadReference> references = upload_references(load);
    LoaderRecorder recorder;
    AbortWhenWanted controller;
    recorder.abort_on_first_status = &controller.wanted;
    LoaderServices services;
    services.source = &source;
    services.listener = &recorder;
    RunOptions options;
    options.controller = &controller;
    const auto outcome =
        target.rig.run(UploadOperation{UploadRequest{references}}, services, options);
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::aborted_by_operator);
    CHECK(outcome->abort_confirmed);
    CHECK(target.listener.contains("sees ABORT"));
    REQUIRE_FALSE(recorder.status_codes.empty());
    CHECK(recorder.status_codes.back() == StatusCode::aborted_by_operator);
}

TEST_CASE("a refused Upload is denied by the target", "[mock][injections]") {
    Config config;
    config.refusal =
        Refusal{adl::mock::OperationKind::upload, StatusCode::operation_denied, "not today"};
    MockTarget target(config);
    const adl::arinc665::Load load = make_load();
    const std::vector<LoadReference> references = upload_references(load);
    LoaderRecorder recorder;
    LoaderServices services;
    services.listener = &recorder;
    const auto outcome = target.rig.run(UploadOperation{UploadRequest{references}}, services);
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::denied);
    CHECK(recorder.initialization == std::vector<StatusCode>{StatusCode::operation_denied});
    CHECK(recorder.initialization_text == std::vector<std::string>{"not today"});
}

TEST_CASE("a silent target ends the operation with the DLP time-out", "[mock][injections]") {
    Config config;
    config.silent = true;
    MockTarget target(config);
    LoaderRecorder recorder;
    LoaderServices services;
    services.listener = &recorder;
    const auto outcome = target.rig.run(InformationOperation{}, services);
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::dlp_timed_out);
    CHECK(target.listener.contains("goes silent"));
}

TEST_CASE("a garbage initialization file is a protocol error", "[mock][injections]") {
    Config config;
    config.garbage_init = true;
    MockTarget target(config);
    LoaderRecorder recorder;
    LoaderServices services;
    services.listener = &recorder;
    const auto outcome = target.rig.run(InformationOperation{}, services);
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::protocol_error);
}

TEST_CASE("a wrong checksum option on a download is reported and kept", "[mock][injections]") {
    Config config;
    config.holds = {HeldFile{"F1.BIN", pattern(2000, 31), ""}};
    config.wrong_checksum = true;
    MockTarget target(config);
    MemorySink sink;
    const std::vector<FileRequest> files = {FileRequest{"F1.BIN", {}}};
    LoaderRecorder recorder;
    LoaderServices services;
    services.sink = &sink;
    services.listener = &recorder;
    const auto outcome = target.rig.run(MediaDownloadOperation{DownloadRequest{files}}, services);
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::completed);
    CHECK(recorder.saw(Notice::checksum_mismatch));
    CHECK(sink.files.contains("F1.BIN"));  // kept all the same (5.3.2.3.8.5)
}

TEST_CASE("a lost status file is reported as a gap by the counter", "[mock][injections]") {
    Config config;
    config.drop_status_every = 2;
    MockTarget target(config);
    // An upload sends a status file after every file it reads, so some are in progress ones
    // the injection can lose (the first and the final statuses are never lost).
    const adl::arinc665::Load load = make_load(3'000'000, 13);
    put_load_files(target.rig.loader_host, load);
    DirectoryFileSource source(target.rig.loader_host.files(), "/load");
    const std::vector<LoadReference> references = upload_references(load);
    LoaderRecorder recorder;
    LoaderServices services;
    services.source = &source;
    services.listener = &recorder;
    const auto outcome = target.rig.run(UploadOperation{UploadRequest{references}}, services);
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::completed);
    CHECK(recorder.saw(Notice::status_counter_gap));
    CHECK(target.listener.contains("lost (injection)"));
}

TEST_CASE("the target aborts the operation after N status files", "[mock][injections]") {
    Config config;
    config.abort_after_statuses = 2;
    MockTarget target(config);
    const adl::arinc665::Load load = make_load(30000, 15);
    put_load_files(target.rig.loader_host, load);
    DirectoryFileSource source(target.rig.loader_host.files(), "/load");
    const std::vector<LoadReference> references = upload_references(load);
    LoaderRecorder recorder;
    LoaderServices services;
    services.source = &source;
    services.listener = &recorder;
    const auto outcome = target.rig.run(UploadOperation{UploadRequest{references}}, services);
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::aborted_by_target);
    REQUIRE_FALSE(recorder.status_codes.empty());
    CHECK(recorder.status_codes.back() == StatusCode::aborted_by_target);
}

TEST_CASE("a WAIT answer makes the loader ask again", "[mock][injections]") {
    Config config;
    config.wait_first_seconds = 1;
    MockTarget target(config);
    LoaderRecorder recorder;
    LoaderServices services;
    services.listener = &recorder;
    const auto outcome = target.rig.run(InformationOperation{}, services);
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::completed);
    CHECK(recorder.saw(Notice::waiting));
    CHECK(target.listener.contains("answers the request with WAIT:1"));
}

}  // namespace

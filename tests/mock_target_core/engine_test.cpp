// SPDX-License-Identifier: GPL-3.0-or-later
// The mock target's engine, headless: the loader's real engine (DataLoader and Runner over a
// fake host) runs whole operations against the mock engine over the fake network, so the mock
// target is proven to be a valid 615A target for the loader. FIND, Information, Upload (byte
// for byte), the two Download modes, an operator abort with its confirmation, and the
// injection controls.

#include "adl/arinc615a/directory_files.hpp"
#include "adl/arinc615a/find_client.hpp"
#include "adl/arinc615a/runner.hpp"
#include "fake_link.hpp"
#include "loader_rig.hpp"
#include "scripted_loader.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <expected>
#include <optional>
#include <string>
#include <vector>

using namespace adl::arinc615a;
using namespace adl::mock;
using namespace adl::test;

namespace
{

// How an operation ended, as the engine said it.
struct Ended
{
    std::string name;
    std::optional<StatusCode> status;
    bool refused = false;

    friend bool operator==(const Ended&, const Ended&) = default;
};

// What the mock target reported, for the assertions.
struct EngineRecorder final : public EngineListener
{
    std::vector<std::pair<int, std::string>> lines;
    std::vector<std::string> operations;
    std::vector<int> progress;
    std::vector<UploadRecord> uploads;
    std::vector<Ended> ended;

    void log(Severity severity, std::string_view text) override
    {
        lines.emplace_back(static_cast<int>(severity), std::string{text});
    }
    void operation_changed(std::string_view name) override
    {
        operations.emplace_back(name);
    }
    void progress_changed(int percent) override
    {
        progress.push_back(percent);
    }
    void upload_received(const UploadRecord& record) override
    {
        uploads.push_back(record);
    }
    void operation_ended(std::string_view name, std::optional<StatusCode> status, bool refused) override
    {
        ended.push_back(Ended{std::string{name}, status, refused});
    }

    [[nodiscard]] bool contains(std::string_view part) const
    {
        return std::ranges::any_of(lines, [part](const auto& line) { return line.second.contains(part); });
    }
};

// A mock target over the fake link, started, with its listener.
struct MockTarget
{
    MockRig rig;
    EngineRecorder listener;
    MockEngine engine{&listener};
    FakeTargetLink link{rig.world, rig.network, rig.target_address};
    Config config{};

    explicit MockTarget(Config with = {}) : config(std::move(with))
    {
        link.attach(engine);
        const auto error = engine.start(link, config);
        REQUIRE_FALSE(error);
        if (error)
        {
            FAIL("the engine could not start: " + *error);
        }
    }
};

TEST_CASE("FIND is answered with the configured identity", "[mock][find]")
{
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

TEST_CASE("FIND is answered with an identity whose text is long and no longer held by the caller", "[mock][find]")
{
    Config config;
    {
        // Past the small-string buffer, so the text is on the heap and a view of it that outlived
        // these strings would read freed memory (which AddressSanitizer reports).
        const std::string hardware(15, 'H');
        const std::string type_name(8, 'T');
        const std::string position(8, 'P');
        const std::string literal_name(20, 'L');
        const std::string manufacturer(3, 'M');
        config.find_identity = FindIdentity{hardware, type_name, position, literal_name, manufacturer};
    }
    MockTarget target(config);
    std::array<FindTarget, 8> answers{};
    Runner runner(target.rig.loader_host);
    const auto count = runner.find(FindConfig{}, answers);
    REQUIRE(count);
    REQUIRE(*count == 1);
    CHECK(answers[0].hardware_id.view() == std::string(15, 'H'));
    CHECK(answers[0].type_name.view() == std::string(8, 'T'));
    CHECK(answers[0].position.view() == std::string(8, 'P'));
    CHECK(answers[0].literal_name.view() == std::string(20, 'L'));
    CHECK(answers[0].manufacturer_code.view() == std::string(3, 'M'));
}

TEST_CASE("a configuration list is encoded, or refused when it cannot be", "[mock][information]")
{
    const std::vector<adl::mock::HardwareItem> good = {
        adl::mock::HardwareItem{"FMS LEFT", "SN-0001", {{"ACM47-1234-5678", "", "FMS 1 OPS"}}}};
    const auto bytes = encode_configuration(good);
    REQUIRE(bytes);
    CHECK_FALSE(bytes->empty());

    auto no_serial = good;
    no_serial[0].serial_number.clear(); // a serial number is 1 to 254 characters
    CHECK_FALSE(encode_configuration(no_serial));

    auto too_long = good;
    too_long[0].parts[0].part_number = std::string(255, 'P');
    CHECK_FALSE(encode_configuration(too_long));

    auto too_many = good; // more than the file the engine writes can hold
    too_many[0].parts.assign(100, adl::mock::HardwarePart{std::string(60, 'P'), "", std::string(20, 'D')});
    CHECK_FALSE(encode_configuration(too_many));
}

TEST_CASE("an Information operation delivers the configuration", "[mock][information]")
{
    Config config;
    config.information = {adl::mock::HardwareItem{
        "FMS LEFT", "SN-0001", {{"ACM47-1234-5678", "", "FMS 1 OPS"}, {"ACM47-8765-4321", "A", "FMS 1 DATA"}}}};
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
    CHECK(target.listener.ended == std::vector<Ended>{Ended{"Information", StatusCode::completed, false}});
}

TEST_CASE("a mock target that is not on port 59 is reached by a loader on 59", "[mock][information][port]")
{
    // With no port option the mock writes what it starts to the port it listens on, where a loader on 59 is
    // not; the loader has to say where it is.
    constexpr std::uint16_t mock_port = 5059;
    Config config;
    config.tftp_port = mock_port;
    config.information = {adl::mock::HardwareItem{"FMS LEFT", "SN-0001", {{"ACM47-1234-5678", "", "FMS 1 OPS"}}}};
    MockTarget target(config);
    target.rig.target_port = mock_port;
    LoaderRecorder recorder;
    LoaderServices services;
    services.listener = &recorder;
    const auto outcome = target.rig.run(InformationOperation{}, services);
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::completed);
    CHECK(recorder.configuration_lists == 1);
    REQUIRE_FALSE(recorder.status_codes.empty());
    CHECK(recorder.status_codes.back() == StatusCode::completed);
    CHECK(std::ranges::none_of(recorder.notices,
                               [](const auto& notice) { return notice.first == Notice::port_option_ignored; }));
    CHECK(target.listener.contains("Information operation finished: completed"));
}

TEST_CASE("an Upload delivers every file byte for byte", "[mock][upload]")
{
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
    for (const auto& file : load.data_files)
    {
        INFO(file.name);
        CHECK(target.engine.stored().at(file.name) == file.bytes);
    }
    for (const auto& file : load.support_files)
    {
        INFO(file.name);
        CHECK(target.engine.stored().at(file.name) == file.bytes);
    }

    // The completed upload was handed over for storage.
    REQUIRE(target.listener.uploads.size() == 1);
    CHECK(target.listener.uploads[0].load_part_number == adl::samples::sample_load_part_number);
    CHECK(target.listener.uploads[0].files.size() == 1 + load.data_files.size() + load.support_files.size());

    // Progress climbed to 100, and the log tells the story.
    CHECK(std::ranges::find(target.listener.progress, 100) != target.listener.progress.end());
    CHECK(target.listener.contains("read " + load.header_file_name + " ("));
    for (const auto& file : load.data_files)
    {
        CHECK(target.listener.contains("read " + file.name + " ("));
    }
    REQUIRE_FALSE(recorder.status_codes.empty());
    CHECK(recorder.status_codes.back() == StatusCode::completed);
}

TEST_CASE("a media defined Download writes the held files", "[mock][download]")
{
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

TEST_CASE("an operator defined Download serves only the chosen files", "[mock][download]")
{
    Config config;
    config.holds = {HeldFile{"A.BIN", pattern(100, 21), "a"}, HeldFile{"B.BIN", pattern(200, 22), "b"}};
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

TEST_CASE("an operator abort in the middle of an Upload is confirmed", "[mock][abort]")
{
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
    const auto outcome = target.rig.run(UploadOperation{UploadRequest{references}}, services, options);
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::aborted_by_operator);
    CHECK(outcome->abort_confirmed);
    CHECK(target.listener.contains("sees ABORT"));
    REQUIRE_FALSE(recorder.status_codes.empty());
    CHECK(recorder.status_codes.back() == StatusCode::aborted_by_operator);
    CHECK(target.listener.ended == std::vector<Ended>{Ended{"Upload", StatusCode::aborted_by_operator, false}});
}

TEST_CASE("an operator abort the moment the first status file arrives is confirmed at once", "[mock][abort]")
{
    // The timing of the loopback test over real sockets, replayed on the fake network where
    // it is exact: the target is slow to start and then reports every second, and the abort
    // comes as soon as the first status file has been read, while that file's transfer is
    // still lingering on the loader's side. The target's next status file request is answered
    // with ABORT and the status file after it confirms the abort. Nothing in that may fail,
    // since a target that gives up on a status file does not send it again and the loader
    // would wait out the data load protocol time-out (13 s) for a confirmation that never
    // comes.
    Config config;
    config.think_time = std::chrono::seconds(2);
    config.status_period = std::chrono::seconds(1);
    MockTarget target(config);
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
    const auto started = target.rig.world.now();
    const auto outcome = target.rig.run(UploadOperation{UploadRequest{references}}, services, options);
    const auto took = target.rig.world.now() - started;
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::aborted_by_operator);
    CHECK(outcome->abort_confirmed);
    CHECK(target.listener.contains("sees ABORT"));
    CHECK_FALSE(target.listener.contains("failed"));
    REQUIRE_FALSE(recorder.status_codes.empty());
    CHECK(recorder.status_codes.back() == StatusCode::aborted_by_operator);
    // Two seconds of think time, a second until the next status file, and the confirmation. A
    // status file transfer that fails once costs the target a 4 second time-out before it
    // sends the file again, and one that fails twice costs the confirmation (13 seconds more).
    CHECK(took < std::chrono::seconds(5));
}

TEST_CASE("a refused Upload is denied by the target", "[mock][injections]")
{
    Config config;
    config.refusal = Refusal{adl::mock::OperationKind::upload, StatusCode::operation_denied, "not today"};
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
    CHECK(target.listener.ended == std::vector<Ended>{Ended{"Upload", StatusCode::operation_denied, true}});
}

TEST_CASE("an operation refused as not supported is reported with 0x1002", "[mock][injections]")
{
    Config config;
    config.refusal = Refusal{adl::mock::OperationKind::information, StatusCode::operation_not_supported, "not here"};
    MockTarget target(config);
    LoaderRecorder recorder;
    LoaderServices services;
    services.listener = &recorder;
    const auto outcome = target.rig.run(InformationOperation{}, services);
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::not_supported);
    CHECK(recorder.initialization == std::vector<StatusCode>{StatusCode::operation_not_supported});
    CHECK(recorder.initialization_text == std::vector<std::string>{"not here"});
    CHECK(target.listener.ended == std::vector<Ended>{Ended{"Information", StatusCode::operation_not_supported, true}});
}

TEST_CASE("a silent target ends the operation with the DLP time-out", "[mock][injections]")
{
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
    CHECK(target.listener.ended.empty()); // a silent operation never ends
}

TEST_CASE("a garbage initialization file is a protocol error", "[mock][injections]")
{
    Config config;
    config.garbage_init = true;
    MockTarget target(config);
    LoaderRecorder recorder;
    LoaderServices services;
    services.listener = &recorder;
    const auto outcome = target.rig.run(InformationOperation{}, services);
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::protocol_error);
    // Refused, with no status to give: the file has no valid structure.
    CHECK(target.listener.ended == std::vector<Ended>{Ended{"Information", std::nullopt, true}});
}

TEST_CASE("a wrong checksum option on a download is reported and kept", "[mock][injections]")
{
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
    CHECK(sink.files.contains("F1.BIN")); // kept all the same (5.3.2.3.8.5)
}

TEST_CASE("a lost status file is reported as a gap by the counter", "[mock][injections]")
{
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

TEST_CASE("the target aborts the operation after N status files", "[mock][injections]")
{
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
    CHECK(target.listener.ended == std::vector<Ended>{Ended{"Upload", StatusCode::aborted_by_target, false}});
}

TEST_CASE("a WAIT answer makes the loader ask again", "[mock][injections]")
{
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

// ---- What the engine does when the loader does not do its part -------------------------------------------

// A loader at 10.0.0.3 that is a script (tests/mock_target_core/scripted_loader.hpp), on the fake network: the
// loader's engine cannot be made to say ABORT with a code of its own, or to vanish.
class ScriptedLoader final : public adl::fake::Agent
{
public:
    static constexpr adl::platform::Ipv4Address address{{10, 0, 0, 3}};
    static constexpr std::uint16_t port = 49300;

    ScriptedLoader(MockRig& rig, LoaderScript::Then then, StatusCode abort_code = StatusCode::aborted_by_data_loader)
        : rig_(rig), script_(then, port, abort_code)
    {
        rig_.network.attach_agent(address, *this);
    }
    ~ScriptedLoader() override
    {
        rig_.network.detach_agent(address);
    }
    ScriptedLoader(const ScriptedLoader&) = delete;
    ScriptedLoader& operator=(const ScriptedLoader&) = delete;

    void ask_for(std::string_view file)
    {
        rig_.network.send(adl::platform::Endpoint{address, port},
                          adl::platform::Endpoint{MockRig::target_address, rig_.target_port}, script_.request(file));
    }

    void on_datagram(adl::platform::Instant, const adl::platform::Endpoint& from, const adl::platform::Endpoint& to,
                     std::span<const std::byte> payload) override
    {
        if (const auto reply = script_.react(payload))
        {
            rig_.network.send(to, from, *reply);
        }
    }

    [[nodiscard]] const LoaderScript& script() const noexcept
    {
        return script_;
    }

private:
    MockRig& rig_;
    LoaderScript script_;
};

// Whether a datagram is the packet `opcode` (3 data, 4 acknowledgement) of `block`.
[[nodiscard]] bool is_packet(std::span<const std::byte> payload, unsigned opcode, unsigned block)
{
    return payload.size() >= 4 && std::to_integer<unsigned>(payload[0]) == 0 &&
           std::to_integer<unsigned>(payload[1]) == opcode && std::to_integer<unsigned>(payload[2]) == 0 &&
           std::to_integer<unsigned>(payload[3]) == block;
}

// When the target sent block `block` of a file to `to` (its time on the fake clock), from the capture.
[[nodiscard]] std::vector<std::chrono::milliseconds> blocks_sent_to(MockRig& rig, const adl::platform::Ipv4Address& to,
                                                                    unsigned block)
{
    std::vector<std::chrono::milliseconds> times;
    for (const adl::fake::CapturedPacket& packet : rig.network.captured())
    {
        if (packet.from.address == MockRig::target_address && packet.to.address == to &&
            is_packet(packet.payload, 3, block))
        {
            times.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(packet.sent.since_origin));
        }
    }
    return times;
}

// Whether the target can serve an Information operation to the loader's engine now.
void require_next_operation_served(MockTarget& target)
{
    LoaderRecorder recorder;
    LoaderServices services;
    services.listener = &recorder;
    const auto outcome = target.rig.run(InformationOperation{}, services);
    REQUIRE(outcome);
    CHECK(outcome->code == OutcomeCode::completed);
    CHECK(recorder.configuration_lists == 1);
}

TEST_CASE("a lost acknowledgement of the initialization file is covered by the target's second try", "[mock][timeout]")
{
    // Attachment 4, 4-3.1 and 4-3.2: the target sends the last block again after two seconds. The file is one block,
    // so the acknowledgement that is lost is the one that tells the target it can start. The loader keeps the file
    // for its dally to answer the repeat; here, on the fake network, the repeat reaches it in time.
    MockTarget target;
    bool dropped = false;
    target.rig.network.set_filter(
        [&dropped](const adl::fake::PacketInfo& packet)
        {
            if (!dropped && packet.from.address == MockRig::loader_address && is_packet(packet.payload, 4, 1))
            {
                dropped = true;
                return adl::fake::Verdict::drop;
            }
            return adl::fake::Verdict::deliver;
        });
    LoaderRecorder recorder;
    LoaderServices services;
    services.listener = &recorder;
    const auto outcome = target.rig.run(InformationOperation{}, services);
    REQUIRE(outcome);
    CHECK(dropped);
    CHECK(outcome->code == OutcomeCode::completed);
    const auto sent = blocks_sent_to(target.rig, MockRig::loader_address, 1);
    // Block 1 of the initialization file went when the loader asked, and again two seconds later (the status files
    // are written with blocks of their own, from later in the run).
    REQUIRE(sent.size() >= 2);
    CHECK(sent[1] - sent[0] == std::chrono::seconds(2));
}

TEST_CASE("an initialization file that nobody acknowledges is sent twice, and the target is ready for the next request",
          "[mock][timeout]")
{
    MockTarget target;
    ScriptedLoader loader(target.rig, LoaderScript::Then::acknowledge_nothing);
    loader.ask_for("HNPFMS_L.LCI");
    target.rig.world.advance_by(std::chrono::seconds(30));
    const auto sent = blocks_sent_to(target.rig, ScriptedLoader::address, 1);
    REQUIRE(sent.size() == 2);
    CHECK(sent[1] - sent[0] == std::chrono::seconds(2)); // the time-out, and one retry (4-3.1, 4-3.2)
    CHECK_FALSE(target.engine.operation_running());
    CHECK(target.listener.ended.empty()); // it never started: the loader did not say it had the file
    require_next_operation_served(target);
}

TEST_CASE("a loader that says ABORT with a code of its own ends the operation, and the target is ready for the next",
          "[mock][abort]")
{
    // The standard gives a data loader three codes for an abort, and a status file can hold only those. The target
    // answers any other as an abort by the data loader, which is what it is, and the answer ends the operation.
    Config config;
    config.think_time = std::chrono::milliseconds(100);
    config.status_period = std::chrono::seconds(1);
    MockTarget target(config);
    ScriptedLoader loader(target.rig, LoaderScript::Then::abort_the_first_status_file, static_cast<StatusCode>(0x1FFF));
    loader.ask_for("HNPFMS_L.LUI");
    target.rig.world.advance_by(std::chrono::seconds(30));
    CHECK(target.listener.contains("sees ABORT"));
    CHECK(target.listener.contains("answers it as aborted by the data loader"));
    CHECK_FALSE(target.engine.operation_running());
    CHECK(target.listener.ended == std::vector<Ended>{Ended{"Upload", StatusCode::aborted_by_data_loader, false}});
    // What the loader was told: the one status file it got is the answer, and it says what the abort was.
    REQUIRE(loader.script().status_files().size() == 1);
    const auto status = decode_upload_status(loader.script().status_files()[0]);
    REQUIRE(status);
    CHECK(status->status == StatusCode::aborted_by_data_loader);
    require_next_operation_served(target);
}

TEST_CASE("a loader that goes silent after the initialization file ends the operation at the target's time-out",
          "[mock][timeout]")
{
    // The target writes its status files to the loader, each with the time-out and the one retry of Attachment 4,
    // twice over. When a status file cannot be delivered the loader has gone (it was killed, or it is silent for
    // longer than the target waits), and the target ends the exchange and is ready for the next request. Before,
    // it wrote a status file every period for ever and served nothing else.
    Config config;
    config.think_time = std::chrono::milliseconds(100);
    config.status_period = std::chrono::seconds(1);
    MockTarget target(config);
    ScriptedLoader loader(target.rig, LoaderScript::Then::say_nothing);
    const auto started = target.rig.world.now();
    loader.ask_for("HNPFMS_L.LUI");
    CHECK_FALSE(
        target.rig.world.wait_until([&target] { return !target.listener.ended.empty(); }, std::chrono::seconds(7)));
    CHECK(target.engine.operation_running()); // still trying: two seconds, and again, and then the whole file again
    REQUIRE(
        target.rig.world.wait_until([&target] { return !target.listener.ended.empty(); }, std::chrono::seconds(10)));
    const auto took = target.rig.world.now() - started;
    CHECK(took > std::chrono::seconds(8));
    CHECK(took < std::chrono::seconds(9));
    CHECK(target.listener.ended == std::vector<Ended>{Ended{"Upload", StatusCode::aborted_by_target, false}});
    CHECK(target.listener.contains("the loader does not answer"));
    CHECK_FALSE(target.engine.operation_running());
    CHECK(loader.script().status_files().empty());
    // And nothing is written afterwards: the heartbeat of the operation that ended is over.
    const auto requests = loader.script().status_requests();
    target.rig.world.advance_by(std::chrono::seconds(30));
    CHECK(loader.script().status_requests() == requests);
    require_next_operation_served(target);
}

} // namespace

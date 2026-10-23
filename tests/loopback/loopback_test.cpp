// The loader's real 615A engine against the mock target's engine, end to end over real UDP
// sockets on the loopback interface: adl::arinc615a::DataLoader driven by adl::arinc615a::Runner
// over the host adl::platform::make_host() returns, and adl::mock::MockEngine driven by run()
// over its RealTargetLink, both on 127.0.0.1. This is the first test in the repository that runs
// whole 615A operations through real sockets; the engine's tests run the same operations on the
// fake in-memory platform, and the conformance suite checks the platform's sockets. The mock
// target's engine is single-threaded, so each scenario runs it on a std::thread of its own and
// the loader's Runner (which blocks for the whole operation) on the test thread.
//
// Robustness, for the three CI systems: every port is a high one (nothing below 1024, which an
// unprivileged account may not be able to bind; port 59 and the FIND port 1001 are only named
// where the standard fixes them), the mock target's status heartbeat is one second (the data
// load protocol wants a status file at least every 13), and the only waits are the protocol's
// own time-outs, which are seconds long and tolerate a loaded machine. Each scenario builds its
// own target, loader host and directories, so a failure in one cannot leak into the next, and
// the fixed ports can be bound again once the previous scenario's sockets are gone.

#include "adl/arinc615a/data_loader.hpp"
#include "adl/arinc615a/directory_files.hpp"
#include "adl/arinc615a/runner.hpp"
#include "adl/arinc665/load.hpp"
#include "adl/mock/engine.hpp"
#include "adl/mock/link.hpp"
#include "adl/platform/host.hpp"
#include "adl/samples/scenarios.hpp"
#include "temp_dir.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

// The mock's types are qualified, so that HardwareItem and friends keep meaning the 615A codec's.
using namespace adl::arinc615a;

namespace
{

// ---- The session -------------------------------------------------------------------------------

// The ports of the session, all high ones. The loader's control port is what it binds and offers
// in the "port" option of its requests, which the mock target honors (it is how the target knows
// where to write its status files); the standard's port 59 would need privileges on CI hosts.
// They sit in the 30 thousands: above 1024, which an unprivileged account may not bind, and
// below the dynamic range Windows hands out (49152 and up), where Hyper-V and WSL carve their
// port exclusions out of and refuse the rest; a fixed port in there fails with WSAEACCES on a
// machine that has them.
constexpr std::uint16_t target_tftp_port = 30959;
constexpr std::uint16_t loader_control_port = 30901;
// FIND is the one thing that cannot move: the loader's FIND socket binds 1001 and sends to the
// destination's port 1001 (both fixed in the runner and the standard), so a target that wants
// FIND must listen on 1001. There is deliberately no FIND scenario here: on one machine both
// ends then hold port 1001, and the loader's own socket (bound with reuse, after the target's)
// receives its own unicast request, which FindClient ignores as a looped-back request, so the
// target never hears it (measured: 0 answers in 10 consecutive finds on Windows; Linux and
// macOS would hash the delivery between the two sockets with SO_REUSEPORT, no more reliably).
// FIND belongs on two machines, or behind a runner knob that this test may not add. The
// operations below do not use FIND, and binding 1001 (which another process on a shared CI
// machine may hold) buys them nothing, so they listen on a high port instead.
constexpr std::uint16_t target_find_port = 30961;
constexpr std::string_view target_thw = "HNPFMS_L";

// ---- Small helpers -----------------------------------------------------------------------------

// Deterministic pseudo-random bytes, so a wrong byte anywhere is found.
[[nodiscard]] std::vector<std::byte> pattern(std::size_t size, std::uint64_t seed)
{
    std::vector<std::byte> bytes(size);
    std::uint64_t state = seed + 0x9e3779b97f4a7c15ULL;
    for (std::byte& byte : bytes)
    {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        byte = std::byte{static_cast<std::uint8_t>(state >> 56)};
    }
    return bytes;
}

// Writes one file into `directory` through the platform's file system (the platform refuses
// backslash paths, so everything goes through its API).
void write_file(adl::platform::FileSystem& files, const std::string& directory, std::string_view name,
                std::span<const std::byte> bytes)
{
    const auto created = files.create(directory + "/" + std::string{name}, adl::platform::CreateMode::exclusive);
    REQUIRE(created.has_value());
    REQUIRE((*created)->write(bytes).has_value());
    REQUIRE((*created)->close().has_value());
}

// Reads a whole file back through the platform's file system.
[[nodiscard]] std::vector<std::byte> read_file(adl::platform::FileSystem& files, const std::string& directory,
                                               std::string_view name)
{
    const auto opened = files.open_read(directory + "/" + std::string{name});
    REQUIRE(opened.has_value());
    const auto size = (*opened)->size();
    REQUIRE(size.has_value());
    std::vector<std::byte> bytes(static_cast<std::size_t>(*size));
    const auto read = (*opened)->read_at(0, bytes);
    REQUIRE(read.has_value());
    REQUIRE(*read == bytes.size());
    return bytes;
}

// The names of the regular files in a directory.
[[nodiscard]] std::vector<std::string> list_names(adl::platform::FileSystem& files, const std::string& directory)
{
    std::vector<std::string> names;
    const auto listed = adl::platform::for_each_entry(files, directory,
                                                      [&names](std::string_view name, adl::platform::FileKind kind)
                                                      {
                                                          if (kind == adl::platform::FileKind::regular)
                                                          {
                                                              names.emplace_back(name);
                                                          }
                                                          return true;
                                                      });
    REQUIRE(listed.has_value());
    return names;
}

// A small load built like the sample: the sample's header file name and part number, one data
// file of `data_bytes`, no support files.
[[nodiscard]] adl::arinc665::Load make_load(std::size_t data_bytes = 4096, std::uint64_t seed = 5)
{
    adl::arinc665::LoadSpec spec = adl::samples::sample_load_spec();
    spec.data_files.resize(1);
    spec.data_files[0].content = pattern(data_bytes, seed);
    spec.support_files.clear();
    auto load = adl::arinc665::build_load(spec);
    REQUIRE(load.has_value());
    return std::move(*load);
}

// The upload operation of `load`, referenced by header name and part number.
[[nodiscard]] std::vector<LoadReference> references_of(const adl::arinc665::Load& load)
{
    return {LoadReference{load.header_file_name, adl::samples::sample_load_part_number}};
}

// ---- What each side reported -------------------------------------------------------------------

// What the loader's engine reported, copied (the engine hands out views that are valid during
// the call only). `choose` picks the files of an operator defined download (empty: everything),
// and `abort_on_first_status` points at the flag a RunController watches for the abort scenario.
struct Recorder final : public OperationListener
{
    std::vector<StatusCode> initialization;
    std::vector<StatusCode> status_codes;
    std::vector<std::string> status_texts;
    std::size_t configuration_lists = 0;
    std::vector<std::string> hardware_names;
    std::vector<std::string> part_numbers;
    std::vector<std::string> received_names;
    std::vector<std::string> sent_names;
    std::vector<std::string> listed_names;
    std::vector<std::string> choose;
    std::atomic<bool>* abort_on_first_status = nullptr;

    void on_initialization(const InitializationFile& file) noexcept override
    {
        initialization.push_back(file.status);
    }
    void on_configuration_status(const ConfigurationStatus& status) noexcept override
    {
        status_codes.push_back(status.status);
        status_texts.emplace_back(status.description);
    }
    void on_upload_status(const UploadStatus& status) noexcept override
    {
        if (abort_on_first_status != nullptr)
        {
            abort_on_first_status->store(true, std::memory_order_release);
        }
        status_codes.push_back(status.status);
        status_texts.emplace_back(status.description);
    }
    void on_download_status(const DownloadStatus& status) noexcept override
    {
        status_codes.push_back(status.status);
        status_texts.emplace_back(status.description);
    }
    void on_configuration_list(const ConfigurationList& list) noexcept override
    {
        ++configuration_lists;
        for (const HardwareItem& item : list.hardware)
        {
            hardware_names.emplace_back(item.literal_name);
            for (const HardwarePart& part : item.parts)
            {
                part_numbers.emplace_back(part.part_number);
            }
        }
    }
    SelectionDecision on_download_list(const DownloadList& list, FileSelection& selection) noexcept override
    {
        for (const FileListing& file : list.files)
        {
            listed_names.emplace_back(file.file_name);
        }
        for (const FileListing& file : list.files)
        {
            if (choose.empty() || std::ranges::find(choose, std::string{file.file_name}) != choose.end())
            {
                selection.select(file.file_name);
            }
        }
        return SelectionDecision::selected;
    }
    void on_file_received(std::string_view name, const ReceivedFile&) noexcept override
    {
        received_names.emplace_back(name);
    }
    void on_file_sent(std::string_view name, std::uint64_t) noexcept override
    {
        sent_names.emplace_back(name);
    }
};

// What the mock target's engine reported, copied for the assertions (its strings are valid
// during the call only, too).
struct TargetRecorder final : public adl::mock::EngineListener
{
    std::vector<std::string> lines;

    void log(adl::mock::Severity, std::string_view text) override
    {
        lines.emplace_back(text);
    }
    void operation_changed(std::string_view) override {}
    void progress_changed(int) override {}
    void upload_received(const adl::mock::UploadRecord&) override {}

    [[nodiscard]] bool contains(std::string_view part) const
    {
        return std::ranges::any_of(lines, [part](const auto& line) { return line.contains(part); });
    }
};

// The operator's abort, issued from the runner's thread once the recorder has seen the first
// upload status: the RunController the runner polls while an operation runs.
struct AbortWhenWanted final : public RunController
{
    std::atomic<bool> wanted{false};

    void on_poll(DataLoader& loader, Instant now) noexcept override
    {
        if (wanted.load(std::memory_order_acquire) && !issued_)
        {
            issued_ = true;
            loader.abort(now, StatusCode::aborted_by_operator);
        }
    }

private:
    bool issued_ = false;
};

// ---- The two sides -----------------------------------------------------------------------------

// The mock target of one scenario: a real host, the engine over its RealTargetLink, and the
// engine's run() loop on a thread. Stopping the target asks the link to stop (the engine polls
// it every 200 ms) and joins the thread.
struct MockTarget
{
    TargetRecorder listener;
    std::unique_ptr<adl::mock::RealTargetLink> link;
    adl::mock::MockEngine engine{&listener};
    std::thread thread;

    explicit MockTarget(adl::mock::Config config)
    {
        auto made = adl::platform::make_host();
        REQUIRE(made.has_value());
        link = std::make_unique<adl::mock::RealTargetLink>(std::move(*made));
        if (const auto error = engine.start(*link, std::move(config)))
        {
            INFO("the mock target could not start: " << *error);
            REQUIRE_FALSE(error.has_value());
            return;
        }
        thread = std::thread([this] { engine.run(); });
    }

    MockTarget(const MockTarget&) = delete;
    MockTarget& operator=(const MockTarget&) = delete;

    ~MockTarget()
    {
        if (link != nullptr)
        {
            link->request_stop();
        }
        if (thread.joinable())
        {
            thread.join();
        }
    }
};

// The configuration every scenario starts from: the identity, the ports, and a mock target that
// moves briskly (its 200 ms "think" is only the pause before it starts work) while its status
// heartbeat stays well inside the data load protocol's 13 seconds.
[[nodiscard]] adl::mock::Config mock_config()
{
    adl::mock::Config config;
    config.thw = std::string{target_thw};
    config.tftp_port = target_tftp_port;
    config.find_port = target_find_port;
    // Both ends on loopback: no socket listens on the network, so no firewall asks about the test
    // program (a prompt for an unfamiliar path holds the screen and stops unattended runs).
    config.bind_address = Address::loopback();
    config.think_time = std::chrono::milliseconds(200);
    config.status_period = std::chrono::seconds(1);
    return config;
}

// The loader's side of one scenario: the real host, a workspace, the directories the
// DirectoryFileSource reads the load from and the DirectoryFileSink writes the target's files
// into, and the recorder. `run` runs one operation to its end (the runner blocks for it).
struct LoaderRig
{
    std::unique_ptr<adl::platform::Host> host;
    adl::test::TempDir upload_directory;
    adl::test::TempDir download_directory;
    std::unique_ptr<DirectoryFileSource> source;
    std::unique_ptr<DirectoryFileSink> sink;
    Recorder recorder;
    LoaderConfig config{};
    std::vector<std::byte> workspace;

    LoaderRig()
    {
        auto made = adl::platform::make_host();
        REQUIRE(made.has_value());
        host = std::move(*made);
        source = std::make_unique<DirectoryFileSource>(host->files(), upload_directory.path().generic_string());
        sink = std::make_unique<DirectoryFileSink>(host->files(), download_directory.path().generic_string());
        config.target = Endpoint{Address{{127, 0, 0, 1}}, target_tftp_port};
        config.thw_id_pos = target_thw;
        config.control_port = loader_control_port;
    }

    [[nodiscard]] std::expected<Outcome, RunError> run(const Operation& operation, const RunOptions& options = {})
    {
        workspace.assign(DataLoader::workspace_bytes(config), std::byte{0});
        LoaderServices services;
        services.source = source.get();
        services.sink = sink.get();
        services.listener = &recorder;
        DataLoader loader(config, workspace, services);
        Runner runner(*host);
        RunOptions on_loopback = options;
        on_loopback.local_address = adl::platform::Ipv4Address::loopback();
        return runner.run(loader, operation, on_loopback);
    }
};

// Runs one operation and requires that it ran at all (as opposed to the control socket or the
// start failing), with a message that says which stage failed.
[[nodiscard]] Outcome must_run(LoaderRig& rig, const Operation& operation, const RunOptions& options = {})
{
    const auto outcome = rig.run(operation, options);
    if (!outcome.has_value())
    {
        FAIL("the operation could not run: stage " << static_cast<int>(outcome.error().stage) << ", platform error "
                                                   << static_cast<int>(outcome.error().platform));
    }
    return *outcome;
}

// Puts every file of `load` into the rig's upload directory, flat, for the DirectoryFileSource.
void put_load_files(LoaderRig& rig, const adl::arinc665::Load& load)
{
    auto& files = rig.host->files();
    const std::string directory = rig.upload_directory.path().generic_string();
    write_file(files, directory, load.header_file_name, load.header_bytes);
    for (const auto& file : load.data_files)
    {
        write_file(files, directory, file.name, file.bytes);
    }
    for (const auto& file : load.support_files)
    {
        write_file(files, directory, file.name, file.bytes);
    }
}

// ---- The scenarios -----------------------------------------------------------------------------

TEST_CASE("an Information operation over real sockets reads the configuration", "[loopback]")
{
    MockTarget target(mock_config());
    LoaderRig rig;

    const Outcome outcome = must_run(rig, InformationOperation{});
    CHECK(outcome.code == OutcomeCode::completed);
    CHECK(rig.recorder.initialization == std::vector<StatusCode>{StatusCode::accepted});
    CHECK(rig.recorder.configuration_lists == 1);
    CHECK(rig.recorder.hardware_names == std::vector<std::string>{"FMS LEFT"});
    REQUIRE(rig.recorder.part_numbers.size() == 2);
    CHECK(rig.recorder.part_numbers[0] == "ACM47-1234-5678");
    CHECK(rig.recorder.part_numbers[1] == "ACM47-8765-4321");
    REQUIRE_FALSE(rig.recorder.status_codes.empty());
    CHECK(rig.recorder.status_codes.back() == StatusCode::completed);
    CHECK(target.listener.contains("served HNPFMS_L.LCI"));
    CHECK(target.listener.contains("Information operation finished: completed"));
}

TEST_CASE("an Upload over real sockets delivers every file byte for byte", "[loopback]")
{
    const adl::arinc665::Load load = make_load(4096, 7);
    MockTarget target(mock_config());
    LoaderRig rig;
    put_load_files(rig, load);

    const std::vector<LoadReference> references = references_of(load);
    const Outcome outcome = must_run(rig, UploadOperation{UploadRequest{references}});
    CHECK(outcome.code == OutcomeCode::completed);
    CHECK(outcome.items_failed == 0);
    CHECK(outcome.files_served == 1 + load.data_files.size() + load.support_files.size());

    // Every file arrived at the target, byte for byte.
    CHECK(target.engine.stored().at(load.header_file_name) == load.header_bytes);
    for (const auto& file : load.data_files)
    {
        INFO(file.name);
        CHECK(target.engine.stored().at(file.name) == file.bytes);
    }
    REQUIRE_FALSE(rig.recorder.status_codes.empty());
    CHECK(rig.recorder.status_codes.back() == StatusCode::completed);
}

TEST_CASE("a media defined Download over real sockets writes the held files", "[loopback]")
{
    adl::mock::Config config = mock_config();
    config.holds = {adl::mock::HeldFile{"F1.BIN", pattern(5000, 11), "file one"},
                    adl::mock::HeldFile{"F2.BIN", pattern(300, 12), "file two"},
                    adl::mock::HeldFile{"F3.BIN", pattern(12345, 13), "file three"}};
    const std::vector<adl::mock::HeldFile> held = config.holds; // copied: the config moves on
    MockTarget target(std::move(config));
    LoaderRig rig;

    const std::vector<FileRequest> files = {FileRequest{"F1.BIN", {}}, FileRequest{"F2.BIN", {}},
                                            FileRequest{"F3.BIN", {}}};
    const Outcome outcome = must_run(rig, MediaDownloadOperation{DownloadRequest{files}});
    CHECK(outcome.code == OutcomeCode::completed);
    CHECK(outcome.files_requested == 3);
    CHECK(outcome.files_received == 3);
    CHECK(rig.recorder.received_names == std::vector<std::string>{"F1.BIN", "F2.BIN", "F3.BIN"});

    // The sink's directory holds each file, byte for byte.
    auto& filesystem = rig.host->files();
    const std::string directory = rig.download_directory.path().generic_string();
    CHECK(read_file(filesystem, directory, "F1.BIN") == held[0].content);
    CHECK(read_file(filesystem, directory, "F2.BIN") == held[1].content);
    CHECK(read_file(filesystem, directory, "F3.BIN") == held[2].content);
    CHECK(target.listener.contains("wrote F1.BIN"));
    CHECK(target.listener.contains("Download (media defined) operation finished: completed"));
}

TEST_CASE("an operator defined Download over real sockets serves only the chosen files", "[loopback]")
{
    adl::mock::Config config = mock_config();
    config.holds = {adl::mock::HeldFile{"A.BIN", pattern(1000, 21), "a"},
                    adl::mock::HeldFile{"B.BIN", pattern(2000, 22), "b"},
                    adl::mock::HeldFile{"C.BIN", pattern(3000, 23), "c"}};
    const std::vector<adl::mock::HeldFile> held = config.holds; // copied: the config moves on
    MockTarget target(std::move(config));
    LoaderRig rig;
    rig.recorder.choose = {"B.BIN", "C.BIN"};

    const Outcome outcome = must_run(rig, OperatorDownloadOperation{});
    CHECK(outcome.code == OutcomeCode::completed);
    CHECK(rig.recorder.listed_names == std::vector<std::string>{"A.BIN", "B.BIN", "C.BIN"});
    CHECK(rig.recorder.received_names == std::vector<std::string>{"B.BIN", "C.BIN"});

    // Only the chosen files are in the sink's directory, and byte for byte.
    auto& filesystem = rig.host->files();
    const std::string directory = rig.download_directory.path().generic_string();
    auto names = list_names(filesystem, directory);
    std::ranges::sort(names);
    CHECK(names == std::vector<std::string>{"B.BIN", "C.BIN"});
    CHECK(read_file(filesystem, directory, "B.BIN") == held[1].content);
    CHECK(read_file(filesystem, directory, "C.BIN") == held[2].content);
    CHECK(target.listener.contains("Download (operator defined) operation finished: completed"));
}

TEST_CASE("an operator abort in the middle of an Upload over real sockets is confirmed", "[loopback]")
{
    adl::mock::Config config = mock_config();
    config.think_time = std::chrono::seconds(2); // slow to start: the abort lands while it works
    MockTarget target(std::move(config));
    LoaderRig rig;
    // A big file keeps the upload going long enough for the abort to land in the middle of it,
    // not after the operation is already over.
    const adl::arinc665::Load load = make_load(3'000'000, 9);
    put_load_files(rig, load);

    AbortWhenWanted controller;
    rig.recorder.abort_on_first_status = &controller.wanted;
    const std::vector<LoadReference> references = references_of(load);
    RunOptions options;
    options.controller = &controller;
    const Outcome outcome = must_run(rig, UploadOperation{UploadRequest{references}}, options);
    CHECK(outcome.code == OutcomeCode::aborted_by_operator);
    CHECK(outcome.abort_confirmed);
    CHECK(target.listener.contains("sees ABORT"));
    REQUIRE_FALSE(rig.recorder.status_codes.empty());
    CHECK(rig.recorder.status_codes.back() == StatusCode::aborted_by_operator);
}

} // namespace

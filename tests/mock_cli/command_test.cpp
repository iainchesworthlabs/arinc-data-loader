// SPDX-License-Identifier: GPL-3.0-or-later
// adl_mock as a program: its own run function started in a thread, on 127.0.0.1, with the loader's real
// engine (DataLoader driven by Runner over the real platform) run against it over real sockets, the way
// tests/loopback does for the engine alone. What the program writes on its standard output is read back
// as the events it promises, and its exit codes are the ones it lists.
//
// FIND is fixed to port 1001 and two sockets on one port are not reliable on every platform, so the
// loader names the target by address and every port here is a high one, apart from the loopback test's.

#include "adl/arinc615a/data_loader.hpp"
#include "adl/arinc615a/directory_files.hpp"
#include "adl/arinc615a/runner.hpp"
#include "adl/arinc665/load.hpp"
#include "adl/mock/cli/command.hpp"
#include "adl/platform/host.hpp"
#include "adl/platform/paths.hpp"
#include "adl/samples/scenarios.hpp"
#include "json_read.hpp"
#include "temp_dir.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <sstream>
#include <streambuf>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace adl::arinc615a;
using adl::test::Json;
using adl::test::JsonReader;
namespace cli = adl::mock::cli;

namespace
{

constexpr std::uint16_t target_tftp_port = 31959;
constexpr std::uint16_t target_find_port = 31961;
constexpr std::uint16_t loader_control_port = 31901;
constexpr std::string_view target_thw = "HNPFMS_L";

// ---- What the program writes ---------------------------------------------------------------------

// A stream the program writes on its own thread and the test reads on another: the text so far, and a
// wait for a line that says the target is listening.
class CapturedStream final : public std::streambuf
{
public:
    [[nodiscard]] std::string text() const
    {
        const std::scoped_lock lock{mutex_};
        return text_;
    }

    [[nodiscard]] bool wait_for(std::string_view part, std::chrono::seconds limit) const
    {
        std::unique_lock lock{mutex_};
        return changed_.wait_for(lock, limit, [&] { return text_.contains(part); });
    }

protected:
    int_type overflow(int_type c) override
    {
        if (c != traits_type::eof())
        {
            const std::scoped_lock lock{mutex_};
            text_.push_back(static_cast<char>(c));
        }
        return traits_type::not_eof(c);
    }
    std::streamsize xsputn(const char* s, std::streamsize count) override
    {
        {
            const std::scoped_lock lock{mutex_};
            text_.append(s, static_cast<std::size_t>(count));
        }
        changed_.notify_all();
        return count;
    }
    int sync() override
    {
        changed_.notify_all();
        return 0;
    }

private:
    mutable std::mutex mutex_;
    mutable std::condition_variable changed_;
    std::string text_;
};

struct Output
{
    CapturedStream out_buffer;
    CapturedStream err_buffer;
    std::ostream out{&out_buffer};
    std::ostream err{&err_buffer};
};

// A folder as the program is given it on its command line: UTF-8, whatever the system's code page (path::string()
// is the code page on Windows).
[[nodiscard]] std::string utf8_of(const std::filesystem::path& path)
{
    return adl::platform::to_utf8(path.u8string());
}

[[nodiscard]] std::vector<std::string> lines_of(const std::string& text)
{
    std::vector<std::string> lines;
    std::istringstream stream(text);
    for (std::string line; std::getline(stream, line);)
    {
        lines.push_back(line);
    }
    return lines;
}

// The events the program wrote, each read as JSON (and the test fails if a line is not).
[[nodiscard]] std::vector<Json> events_of(const std::string& text)
{
    std::vector<Json> events;
    for (const std::string& line : lines_of(text))
    {
        auto json = JsonReader::read(line);
        INFO(line);
        REQUIRE(json);
        REQUIRE(json->is_object());
        REQUIRE(json->find("event") != nullptr);
        REQUIRE(json->find("t") != nullptr);
        events.push_back(std::move(*json));
    }
    return events;
}

[[nodiscard]] std::vector<std::string> names_of(const std::vector<Json>& events)
{
    std::vector<std::string> names;
    for (const Json& event : events)
    {
        names.push_back(event.at("event").string());
    }
    return names;
}

// ---- The program, in a thread ----------------------------------------------------------------------

// adl_mock started as the program starts: its run function, on a thread, with its options and the
// ports of this test. start() returns once the program has said it is listening (or has ended).
class Program
{
public:
    explicit Program(std::vector<std::string> options, bool with_ports = true)
    {
        if (with_ports)
        {
            options.insert(options.end(),
                           {"--tftp-port", std::to_string(target_tftp_port), "--find-port",
                            std::to_string(target_find_port), "--think-time-ms", "200", "--status-period", "1"});
        }
        arguments_ = std::move(options);
        thread_ = std::thread(
            [this]
            {
                const std::vector<std::string_view> views(arguments_.begin(), arguments_.end());
                exit_code_ = cli::run_command(views, output.out, output.err);
            });
    }

    Program(const Program&) = delete;
    Program& operator=(const Program&) = delete;
    ~Program()
    {
        if (thread_.joinable())
        {
            thread_.join();
        }
    }

    // Waits for the line that says the target is listening; false when the program ended without it.
    [[nodiscard]] bool listening()
    {
        return output.out_buffer.wait_for("started", std::chrono::seconds(20));
    }

    // Waits for the program to end, and returns its exit code.
    [[nodiscard]] int join()
    {
        thread_.join();
        return exit_code_;
    }

    Output output;

private:
    std::vector<std::string> arguments_;
    std::thread thread_;
    int exit_code_ = -1;
};

// ---- The loader ------------------------------------------------------------------------------------

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

void write_bytes(const std::filesystem::path& path, std::span<const std::byte> bytes)
{
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    REQUIRE(out.good());
}

[[nodiscard]] std::vector<std::byte> read_bytes(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    const std::vector<char> text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    std::vector<std::byte> bytes(text.size());
    std::ranges::transform(text, bytes.begin(), [](char c) { return std::byte{static_cast<std::uint8_t>(c)}; });
    return bytes;
}

[[nodiscard]] adl::arinc665::Load make_load(std::size_t data_bytes, std::uint64_t seed)
{
    adl::arinc665::LoadSpec spec = adl::samples::sample_load_spec();
    spec.data_files.resize(1);
    spec.data_files[0].content = pattern(data_bytes, seed);
    spec.support_files.clear();
    auto load = adl::arinc665::build_load(spec);
    REQUIRE(load.has_value());
    return std::move(*load);
}

// What the loader's engine reported.
struct Recorder final : public OperationListener
{
    std::vector<StatusCode> initialization;
    std::vector<StatusCode> status_codes;
    std::size_t configuration_lists = 0;
    std::vector<std::string> hardware_names;
    std::vector<std::string> received_names;
    std::vector<std::string> listed_names;
    std::vector<std::string> choose; // an operator defined download: the names to take (empty: all)

    void on_initialization(const InitializationFile& file) noexcept override
    {
        initialization.push_back(file.status);
    }
    void on_configuration_status(const ConfigurationStatus& status) noexcept override
    {
        status_codes.push_back(status.status);
    }
    void on_upload_status(const UploadStatus& status) noexcept override
    {
        status_codes.push_back(status.status);
    }
    void on_download_status(const DownloadStatus& status) noexcept override
    {
        status_codes.push_back(status.status);
    }
    void on_configuration_list(const ConfigurationList& list) noexcept override
    {
        ++configuration_lists;
        for (const HardwareItem& item : list.hardware)
        {
            hardware_names.emplace_back(item.literal_name);
        }
    }
    SelectionDecision on_download_list(const DownloadList& list, FileSelection& selection) noexcept override
    {
        for (const FileListing& file : list.files)
        {
            listed_names.emplace_back(file.file_name);
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
};

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
        source = std::make_unique<DirectoryFileSource>(host->files(),
                                                       adl::platform::to_platform_path(upload_directory.path()));
        sink = std::make_unique<DirectoryFileSink>(host->files(),
                                                   adl::platform::to_platform_path(download_directory.path()));
        config.target = Endpoint{Address{{127, 0, 0, 1}}, target_tftp_port};
        config.thw_id_pos = target_thw;
        config.control_port = loader_control_port;
    }

    [[nodiscard]] Outcome run(const Operation& operation)
    {
        workspace.assign(DataLoader::workspace_bytes(config), std::byte{0});
        LoaderServices services;
        services.source = source.get();
        services.sink = sink.get();
        services.listener = &recorder;
        DataLoader loader(config, workspace, services);
        Runner runner(*host);
        RunOptions options;
        options.local_address = adl::platform::Ipv4Address::loopback();
        const auto outcome = runner.run(loader, operation, options);
        if (!outcome.has_value())
        {
            FAIL("the operation could not run: stage " << static_cast<int>(outcome.error().stage));
        }
        return *outcome;
    }

    // Puts the files of `load` into the directory the loader reads an upload from, flat.
    void put_load(const adl::arinc665::Load& load) const
    {
        write_bytes(upload_directory.path() / load.header_file_name, load.header_bytes);
        for (const auto& file : load.data_files)
        {
            write_bytes(upload_directory.path() / file.name, file.bytes);
        }
        for (const auto& file : load.support_files)
        {
            write_bytes(upload_directory.path() / file.name, file.bytes);
        }
    }
};

[[nodiscard]] const Json* find_event(const std::vector<Json>& events, std::string_view name)
{
    const auto found = std::ranges::find_if(events, [name](const Json& e) { return e.at("event").string() == name; });
    return found == events.end() ? nullptr : &*found;
}

} // namespace

// ---- The events, from a whole operation -------------------------------------------------------------

TEST_CASE("an Information operation is served and written as events", "[mock_cli][command]")
{
    Program program({"--operations", "1", "--seconds", "60", "--json", "--part", "FMS LEFT:ACM47-1:A:FMS 1 OPS"});
    REQUIRE(program.listening());
    LoaderRig rig;
    const Outcome outcome = rig.run(InformationOperation{});
    CHECK(outcome.code == OutcomeCode::completed);
    CHECK(rig.recorder.configuration_lists == 1);
    CHECK(rig.recorder.hardware_names == std::vector<std::string>{"FMS LEFT"});
    CHECK(program.join() == cli::exit_ok);

    CHECK(program.output.err_buffer.text().empty());
    const std::vector<Json> events = events_of(program.output.out_buffer.text());
    CHECK(names_of(events).front() == "started");
    CHECK(names_of(events).back() == "stopped");
    const Json& started = events.front();
    CHECK(started.at("program").string() == "adl_mock");
    CHECK(started.at("events").number() == 1);
    CHECK_FALSE(started.at("version").string().empty());
    CHECK(started.at("thw_id_pos").string() == "HNPFMS_L");
    CHECK(started.at("address").string() == "127.0.0.1");
    CHECK(started.at("find_port").number() == target_find_port);
    CHECK(started.at("tftp_port").number() == target_tftp_port);
    CHECK(started.at("faults").items().empty());

    const Json* began = find_event(events, "operation_began");
    REQUIRE(began != nullptr);
    CHECK(began->at("operation").string() == "information");
    const Json* ended = find_event(events, "operation_ended");
    REQUIRE(ended != nullptr);
    CHECK(ended->at("operation").string() == "information");
    CHECK(ended->at("status").number() == 3);
    CHECK(ended->at("status_name").string() == "completed");
    CHECK_FALSE(ended->at("refused").boolean());
    const Json& stopped = events.back();
    CHECK(stopped.at("reason").string() == "operations");
    CHECK(stopped.at("operations").number() == 1);

    // The time never goes back, and starts near zero.
    double last = -1;
    for (const Json& event : events)
    {
        CHECK(event.at("t").number() >= last);
        last = event.at("t").number();
        CHECK(event.at("t").number_text.find('.') != std::string::npos);
        CHECK(event.at("t").number_text.size() - event.at("t").number_text.find('.') == 4); // three decimals
    }
    CHECK(events.front().at("t").number() < 5.0);
    // Progress was reported for this operation, and only while it ran.
    for (const Json& event : events)
    {
        if (event.at("event").string() == "progress")
        {
            CHECK(event.at("operation").string() == "information");
            CHECK(event.at("percent").number() >= 0);
            CHECK(event.at("percent").number() <= 100);
        }
    }
}

TEST_CASE("an Upload is stored byte for byte and written as an event with the part number and the names",
          "[mock_cli][command]")
{
    adl::test::TempDir storage;
    Program program({"--storage", utf8_of(storage.path()), "--operations", "1", "--seconds", "60", "--json"});
    REQUIRE(program.listening());
    LoaderRig rig;
    const adl::arinc665::Load load = make_load(30000, 7);
    rig.put_load(load);
    const std::vector<LoadReference> references = {
        LoadReference{load.header_file_name, adl::samples::sample_load_part_number}};
    const Outcome outcome = rig.run(UploadOperation{UploadRequest{references}});
    CHECK(outcome.code == OutcomeCode::completed);
    CHECK(program.join() == cli::exit_ok);

    const std::filesystem::path stored = storage.path() / std::string{adl::samples::sample_load_part_number};
    CHECK(read_bytes(stored / load.header_file_name) == load.header_bytes);
    for (const auto& file : load.data_files)
    {
        CHECK(read_bytes(stored / file.name) == file.bytes);
    }

    const std::vector<Json> events = events_of(program.output.out_buffer.text());
    const Json* received = find_event(events, "upload_received");
    REQUIRE(received != nullptr);
    CHECK(received->at("part_number").string() == std::string{adl::samples::sample_load_part_number});
    std::vector<std::string> names;
    for (const Json& name : received->at("files").items())
    {
        names.push_back(name.string());
    }
    CHECK(names.front() == load.header_file_name);
    CHECK(names.size() == 1 + load.data_files.size());
    CHECK(received->at("stored_in").string() == utf8_of(stored));
    // It came before the end of the operation.
    const auto order = names_of(events);
    CHECK(std::ranges::find(order, "upload_received") < std::ranges::find(order, "operation_ended"));
    CHECK(find_event(events, "operation_began")->at("operation").string() == "upload");
    CHECK(find_event(events, "operation_ended")->at("status_name").string() == "completed");
}

TEST_CASE("an Upload with no storage folder is served and not kept", "[mock_cli][command]")
{
    Program program({"--operations", "1", "--seconds", "60", "--json"});
    REQUIRE(program.listening());
    LoaderRig rig;
    const adl::arinc665::Load load = make_load(2000, 3);
    rig.put_load(load);
    const std::vector<LoadReference> references = {
        LoadReference{load.header_file_name, adl::samples::sample_load_part_number}};
    CHECK(rig.run(UploadOperation{UploadRequest{references}}).code == OutcomeCode::completed);
    CHECK(program.join() == cli::exit_ok);
    const std::vector<Json> events = events_of(program.output.out_buffer.text());
    const Json* received = find_event(events, "upload_received");
    REQUIRE(received != nullptr);
    CHECK(received->at("stored_in").is_null());
}

TEST_CASE("both Downloads serve the files of the folder it holds", "[mock_cli][command]")
{
    adl::test::TempDir holds;
    const std::vector<std::byte> first = pattern(5000, 11);
    const std::vector<std::byte> second = pattern(300, 12);
    const std::vector<std::byte> third = pattern(12345, 13);
    write_bytes(holds.path() / "F1.BIN", first);
    write_bytes(holds.path() / "F2.BIN", second);
    write_bytes(holds.path() / "F3.BIN", third);
    Program program({"--holds", utf8_of(holds.path()), "--operations", "2", "--seconds", "90", "--json"});
    REQUIRE(program.listening());

    LoaderRig media;
    const std::vector<FileRequest> files = {FileRequest{"F1.BIN", {}}, FileRequest{"F3.BIN", {}}};
    CHECK(media.run(MediaDownloadOperation{DownloadRequest{files}}).code == OutcomeCode::completed);
    CHECK(media.recorder.received_names == std::vector<std::string>{"F1.BIN", "F3.BIN"});
    CHECK(read_bytes(media.download_directory.path() / "F1.BIN") == first);
    CHECK(read_bytes(media.download_directory.path() / "F3.BIN") == third);

    LoaderRig chosen;
    chosen.recorder.choose = {"F2.BIN"};
    CHECK(chosen.run(OperatorDownloadOperation{}).code == OutcomeCode::completed);
    CHECK(chosen.recorder.listed_names == std::vector<std::string>{"F1.BIN", "F2.BIN", "F3.BIN"});
    CHECK(chosen.recorder.received_names == std::vector<std::string>{"F2.BIN"});
    CHECK(read_bytes(chosen.download_directory.path() / "F2.BIN") == second);

    CHECK(program.join() == cli::exit_ok);
    std::vector<std::string> operations;
    for (const Json& event : events_of(program.output.out_buffer.text()))
    {
        if (event.at("event").string() == "operation_began")
        {
            operations.push_back(event.at("operation").string());
        }
    }
    CHECK(operations == std::vector<std::string>{"download_media_defined", "download_operator_defined"});
}

TEST_CASE("a target with no folder to hold serves one demonstration file", "[mock_cli][command]")
{
    Program program({"--operations", "1", "--seconds", "60", "--json"});
    REQUIRE(program.listening());
    LoaderRig rig;
    CHECK(rig.run(OperatorDownloadOperation{}).code == OutcomeCode::completed);
    CHECK(rig.recorder.listed_names == std::vector<std::string>{"DEMO.TXT"});
    CHECK(program.join() == cli::exit_ok);
}

TEST_CASE("an operation the target refuses is counted, and is written as refused", "[mock_cli][command]")
{
    Program program({"--refuse", "information", "--operations", "1", "--seconds", "60", "--json"});
    REQUIRE(program.listening());
    LoaderRig rig;
    CHECK(rig.run(InformationOperation{}).code == OutcomeCode::denied);
    CHECK(rig.recorder.initialization == std::vector<StatusCode>{StatusCode::operation_denied});
    CHECK(program.join() == cli::exit_ok);

    const std::vector<Json> events = events_of(program.output.out_buffer.text());
    CHECK(names_of(events) == std::vector<std::string>{"started", "operation_ended", "stopped"});
    CHECK(events.front().at("faults").items().size() == 1);
    CHECK(events.front().at("faults").items()[0].string() == "--refuse");
    const Json& ended = events[1];
    CHECK(ended.at("operation").string() == "information");
    CHECK(ended.at("status").number() == 0x1000);
    CHECK(ended.at("status_name").string() == "operation_denied");
    CHECK(ended.at("refused").boolean());
    CHECK(events.back().at("reason").string() == "operations");
    CHECK(events.back().at("operations").number() == 1);
}

// ---- Stopping ------------------------------------------------------------------------------------

TEST_CASE("with --seconds the target stops by itself when the time is up", "[mock_cli][command]")
{
    const auto before = std::chrono::steady_clock::now();
    Program program({"--seconds", "1", "--json"});
    CHECK(program.join() == cli::exit_ok);
    const auto took = std::chrono::steady_clock::now() - before;
    CHECK(took >= std::chrono::milliseconds(900));
    CHECK(took < std::chrono::seconds(10));

    const std::vector<Json> events = events_of(program.output.out_buffer.text());
    CHECK(names_of(events) == std::vector<std::string>{"started", "stopped"});
    CHECK(events.back().at("reason").string() == "seconds");
    CHECK(events.back().at("operations").number() == 0);
    CHECK(events.back().at("t").number() >= 0.9);
    CHECK(program.output.err_buffer.text().empty());
}

TEST_CASE("the plain output is one line for each event and carries no JSON", "[mock_cli][command]")
{
    Program program({"--seconds", "1"});
    CHECK(program.join() == cli::exit_ok);
    const std::vector<std::string> lines = lines_of(program.output.out_buffer.text());
    REQUIRE(lines.size() == 2);
    CHECK(lines[0].find(" started program=adl_mock version=") != std::string::npos);
    CHECK(lines[0].ends_with(" faults=none"));
    CHECK(lines[1].find(" stopped reason=seconds operations=0") != std::string::npos);
    for (const std::string& line : lines)
    {
        CHECK(line.front() != '{');
    }
}

// ---- What it says about itself and the exit codes -------------------------------------------------------

TEST_CASE("--version and --help say what they are and exit 0", "[mock_cli][command]")
{
    Program version({"--version"}, false);
    CHECK(version.join() == cli::exit_ok);
    const std::string text = version.output.out_buffer.text();
    CHECK(text.starts_with("adl_mock "));
    CHECK(text.ends_with(" events 1\n"));
    CHECK(lines_of(text).size() == 1);
    CHECK(version.output.err_buffer.text().empty());

    Program help({"--help"}, false);
    CHECK(help.join() == cli::exit_ok);
    CHECK(help.output.out_buffer.text().starts_with("usage: adl_mock"));
    CHECK(help.output.err_buffer.text().empty());
}

TEST_CASE("wrong options exit 2, write no event and say why on the error stream", "[mock_cli][command]")
{
    for (const std::vector<std::string>& options : std::vector<std::vector<std::string>>{
             {"--no-such-option"}, {"--seconds", "x"}, {"--silent"}, {"--json", "--json"}, {"stray"}})
    {
        Program program(options, false);
        CHECK(program.join() == cli::exit_usage);
        CHECK(program.output.out_buffer.text().empty());
        CHECK(program.output.err_buffer.text().starts_with("adl_mock: "));
        CHECK(program.output.err_buffer.text().contains("--help"));
    }
    // --json does not put the error on the standard output
    Program json({"--json", "--no-such-option"}, false);
    CHECK(json.join() == cli::exit_usage);
    CHECK(json.output.out_buffer.text().empty());
}

TEST_CASE("parts that cannot be written into a configuration list exit 2", "[mock_cli][command]")
{
    std::vector<std::string> options;
    for (int i = 0; i < 100; ++i)
    {
        options.insert(options.end(), {"--part", "H" + std::to_string(i) + ":" + std::string(120, 'P')});
    }
    Program program(options, false);
    CHECK(program.join() == cli::exit_usage);
    CHECK(program.output.out_buffer.text().empty());
    CHECK(program.output.err_buffer.text().contains("--part"));
}

TEST_CASE("a folder that cannot be used exits 3 before anything is opened", "[mock_cli][command]")
{
    adl::test::TempDir directory;
    write_bytes(directory.path() / "a file", pattern(10, 1));

    Program missing_holds({"--holds", (directory.path() / "missing").string(), "--seconds", "5", "--json"});
    CHECK(missing_holds.join() == cli::exit_folder);
    CHECK(missing_holds.output.out_buffer.text().empty());
    CHECK(missing_holds.output.err_buffer.text().contains("--holds"));

    Program file_as_holds({"--holds", (directory.path() / "a file").string(), "--seconds", "5", "--json"});
    CHECK(file_as_holds.join() == cli::exit_folder);

    Program file_as_storage({"--storage", (directory.path() / "a file").string(), "--seconds", "5", "--json"});
    CHECK(file_as_storage.join() == cli::exit_folder);
    CHECK(file_as_storage.output.out_buffer.text().empty());
    CHECK(file_as_storage.output.err_buffer.text().contains("--storage"));

    // A storage folder that is not there yet is made.
    Program new_storage({"--storage", (directory.path() / "new" / "kept").string(), "--seconds", "1", "--json"});
    CHECK(new_storage.join() == cli::exit_ok);
    CHECK(std::filesystem::is_directory(directory.path() / "new" / "kept"));
}

TEST_CASE("a port that cannot be opened exits 4 and names the port", "[mock_cli][command]")
{
    // Another program holds the TFTP port, without sharing it.
    auto made = adl::platform::make_host();
    REQUIRE(made.has_value());
    adl::platform::UdpOptions holder_options;
    holder_options.local = adl::platform::Endpoint{adl::platform::Ipv4Address::loopback(), target_tftp_port};
    auto holder = (*made)->network().open_udp(holder_options);
    REQUIRE(holder.has_value());

    Program program({"--seconds", "5", "--json"});
    CHECK(program.join() == cli::exit_port);
    CHECK(program.output.out_buffer.text().empty());
    CHECK(program.output.err_buffer.text().contains(std::to_string(target_tftp_port)));
}

// ---- The client of the Unicode folders test -------------------------------------------------------------

// Not a test of its own: the other half of tools.adl_mock.unicode_folders (mock_unicode.cmake), which starts
// adl_mock as a process with --holds and --storage named in characters no Windows code page holds, and this
// program as the next command of the same pipeline, so that the events of the first arrive on this one's
// standard input. The tag begins with a dot, so it runs only when it is asked for by name. It waits for the
// "started" event, uploads a load, downloads F1.BIN (the script wrote it, with the text below), and echoes
// what the target writes, to the end, for the script to read.
TEST_CASE("the loader against adl_mock started as a process, with folders of Unicode names", "[.][unicode_client]")
{
    const auto echo = [](std::string line)
    {
        if (line.ends_with('\r'))
        {
            line.pop_back();
        }
        std::cout << line << '\n';
        std::cout.flush();
        return line;
    };

    bool started = false;
    for (std::string line; !started && std::getline(std::cin, line);)
    {
        started = echo(line).contains("\"event\":\"started\"");
    }
    REQUIRE(started);

    LoaderRig upload;
    const adl::arinc665::Load load = make_load(3000, 21);
    upload.put_load(load);
    const std::vector<LoadReference> references = {
        LoadReference{load.header_file_name, adl::samples::sample_load_part_number}};
    CHECK(upload.run(UploadOperation{UploadRequest{references}}).code == OutcomeCode::completed);

    LoaderRig download;
    const std::vector<FileRequest> files = {FileRequest{"F1.BIN", {}}};
    CHECK(download.run(MediaDownloadOperation{DownloadRequest{files}}).code == OutcomeCode::completed);
    CHECK(download.recorder.received_names == std::vector<std::string>{"F1.BIN"});
    const std::vector<std::byte> got = read_bytes(download.download_directory.path() / "F1.BIN");
    CHECK(std::string(reinterpret_cast<const char*>(got.data()), got.size()) ==
          "Held in a folder whose name no code page can hold."); // as mock_unicode.cmake writes it

    for (std::string line; std::getline(std::cin, line);)
    {
        static_cast<void>(echo(line));
    }
}

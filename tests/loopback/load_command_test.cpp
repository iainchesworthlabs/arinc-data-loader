// SPDX-License-Identifier: GPL-3.0-or-later
// adl_load's run function, the function main calls, on the real host over real UDP sockets on 127.0.0.1,
// against the mock target's engine (adl::mock::MockEngine over its RealTargetLink on a thread of its own), as
// tests/loopback/loopback_test.cpp does for the loader's engine. The whole command is in it: the options, the
// media set read from a real disk, the files written to a real disk, the events on a stream, the record in a
// real file.
//
// Every port is a high one: the mock target listens on 30959 of the loopback address, which the program is told
// with --target-port (the standard's 59 needs privileges on Linux), and the loader's own port is 30901
// (--control-port), so that an unprivileged account can run these as well as any other. The same commands are run
// on the fake platform by tests/load/*_test.cpp in every build. FIND is not here: on one computer its answers do
// not reliably find their way back (the loopback test says why), and the fake network is where it is tested.
//
// This test links the mock target's engine, which still links Qt, so it sits where the loopback test does,
// inside the part that builds with the applications. It can move out once the mock target's own command
// (adl_mock, pull request 88) is on develop and the engine is built without Qt.

#include "adl/arinc665/load.hpp"
#include "adl/load/run.hpp"
#include "adl/media_tree/media_tree.hpp"
#include "adl/mock/engine.hpp"
#include "adl/mock/link.hpp"
#include "adl/platform/arguments.hpp"
#include "adl/platform/host.hpp"
#include "adl/samples/scenarios.hpp"
#include "bytes.hpp"
#include "mini_json.hpp"
#include "temp_dir.hpp"
#include "utf8_path.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

using adl::test::Json;

namespace
{

// High ports below the dynamic range, as the loopback test chooses them: above 1024, which an unprivileged
// account may not bind, and below the 49152 and up that Windows hands out and where Hyper-V and WSL carve out
// exclusions. The target's is told to the program with --target-port, and the loader's own with --control-port.
constexpr std::uint16_t target_tftp_port = 30959;
constexpr std::uint16_t target_find_port = 30961;
constexpr std::uint16_t loader_control_port = 30901;
constexpr std::string_view loader_port_text = "30901";
constexpr std::string_view target_port_text = "30959";

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

[[nodiscard]] std::vector<std::byte> read_bytes(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.is_open());
    const std::string text{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    std::vector<std::byte> bytes;
    for (const char c : text)
    {
        bytes.push_back(static_cast<std::byte>(static_cast<unsigned char>(c)));
    }
    return bytes;
}

struct TargetRecorder final : public adl::mock::EngineListener
{
    void log(adl::mock::Severity, std::string_view) override {}
    void operation_changed(std::string_view) override {}
    void progress_changed(int) override {}
    void upload_received(const adl::mock::UploadRecord&) override {}
};

// The mock target of one scenario, on a thread of its own. `started` is false, with the reason, when the engine
// could not bind its ports.
struct MockTarget
{
    TargetRecorder listener;
    std::unique_ptr<adl::mock::RealTargetLink> link;
    adl::mock::MockEngine engine{&listener};
    std::thread thread;
    std::optional<std::string> not_started;

    explicit MockTarget(adl::mock::Config config)
    {
        auto made = adl::platform::make_host();
        REQUIRE(made.has_value());
        link = std::make_unique<adl::mock::RealTargetLink>(std::move(*made));
        not_started = engine.start(*link, std::move(config));
        if (!not_started)
        {
            thread = std::thread([this] { engine.run(); });
        }
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

[[nodiscard]] adl::mock::Config mock_config()
{
    adl::mock::Config config;
    config.thw = "HNPFMS_L";
    config.tftp_port = target_tftp_port;
    config.find_port = target_find_port;
    // Both ends on loopback: no socket listens on the network, so no firewall asks about the test program.
    config.bind_address = adl::arinc615a::Address::loopback();
    config.think_time = std::chrono::milliseconds(200);
    config.status_period = std::chrono::seconds(1);
    return config;
}

struct Run
{
    int exit_code = -1;
    std::string out;
    std::string err;

    [[nodiscard]] std::vector<Json> events() const
    {
        std::vector<Json> parsed = adl::test::parse_json_lines(out);
        for (const Json& event : parsed)
        {
            INFO("a line of standard output that is not a JSON object: " << event.text);
            REQUIRE(event.is(Json::Kind::object));
        }
        return parsed;
    }
    [[nodiscard]] std::vector<Json> events_named(std::string_view name) const
    {
        std::vector<Json> found;
        for (const Json& event : events())
        {
            if (event.at("event").text == name)
            {
                found.push_back(event);
            }
        }
        return found;
    }
    [[nodiscard]] Json finished() const
    {
        const std::vector<Json> all = events();
        REQUIRE_FALSE(all.empty());
        REQUIRE(all.back().at("event").text == "finished");
        return all.back();
    }
};

// One command on the real host.
[[nodiscard]] Run run_command(std::vector<std::string_view> arguments)
{
    auto host = adl::platform::make_host();
    REQUIRE(host.has_value());
    std::ostringstream out;
    std::ostringstream err;
    Run run;
    run.exit_code = adl::load::run(**host, arguments, out, err);
    run.out = out.str();
    run.err = err.str();
    INFO(run.out);
    INFO(run.err);
    return run;
}

} // namespace

TEST_CASE("an Information operation over real sockets, through the whole command", "[load][system]")
{
    MockTarget target(mock_config());
    INFO("the mock target could not start: " << target.not_started.value_or(""));
    REQUIRE_FALSE(target.not_started.has_value());
    const Run run = run_command({"information", "--target", "127.0.0.1", "--position", "HNPFMS_L", "--control-port",
                                 loader_port_text, "--target-port", target_port_text, "--json"});
    CHECK(run.exit_code == 0);
    CHECK(run.err.empty());
    const Json finished = run.finished();
    CHECK(finished.at("reason").text == "completed");
    CHECK(finished.at("status_code").as_number() == 3);
    const std::vector<Json> configuration = run.events_named("configuration");
    REQUIRE(configuration.size() == 1);
    CHECK(configuration[0].at("hardware").elements[0].at("literal_name").text == "FMS LEFT");
    CHECK(configuration[0].at("hardware").elements[0].at("parts").elements.size() == 2);
}

TEST_CASE("an upload over real sockets delivers every file of the media set byte for byte, and a record is written",
          "[load][system]")
{
    MockTarget target(mock_config());
    INFO("the mock target could not start: " << target.not_started.value_or(""));
    REQUIRE_FALSE(target.not_started.has_value());
    // The sample media set, with a data file big enough to take a few blocks, on a real disk.
    adl::arinc665::LoadSpec load_spec = adl::samples::sample_load_spec();
    load_spec.data_files[0].content = pattern(6000, 7);
    auto load = adl::arinc665::build_load(load_spec);
    REQUIRE(load.has_value());
    auto spec = adl::samples::sample_media_set_spec();
    REQUIRE(spec.has_value());
    spec->loads[0].load = *load;
    const auto members = adl::arinc665::build_media_set(*spec);
    REQUIRE(members.has_value());
    const adl::test::TempDir directory;
    const std::filesystem::path set = directory.path() / "set";
    REQUIRE(adl::media_tree::write_media_set_tree(set, *members).has_value());
    const std::string set_text = adl::test::utf8(set);
    const std::string record = adl::test::utf8(directory.path() / "record.json");

    const Run run = run_command({"upload", "--target", "127.0.0.1", "--position", "HNPFMS_L", "--media-set", set_text,
                                 "--all-loads", "--control-port", loader_port_text, "--target-port", target_port_text,
                                 "--record", record, "--json"});
    CHECK(run.exit_code == 0);
    CHECK(run.err.empty());
    const Json finished = run.finished();
    CHECK(finished.at("reason").text == "completed");
    CHECK(finished.at("record").text == record);
    CHECK(run.events_named("media_set_checked").size() == 1);

    CHECK(target.engine.stored().at(load->header_file_name) == load->header_bytes);
    for (const auto& file : load->data_files)
    {
        CHECK(target.engine.stored().at(file.name) == file.bytes);
    }

    // The record is in the file, valid, and says the same.
    const std::vector<std::byte> bytes = read_bytes(record);
    const std::string text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    const auto parsed = adl::test::parse_json(text);
    REQUIRE(parsed.has_value());
    CHECK(parsed->at("format").as_number() == 1);
    CHECK(parsed->at("kind").text == "adl-operation-record");
    CHECK(parsed->at("operation").text == "upload");
    CHECK(parsed->at("reason").text == "completed");
    CHECK(parsed->at("loads").elements.at(0).at("status").text == "completed");
    CHECK(parsed->at("media_set").at("part_number").text == adl::samples::sample_media_set_part_number);
    CHECK_FALSE(std::filesystem::exists(adl::platform::path_from_utf8(record + ".part")));
}

TEST_CASE("both kinds of download over real sockets write the files into a folder of their own", "[load][system]")
{
    adl::mock::Config config = mock_config();
    config.holds = {adl::mock::HeldFile{"F1.BIN", pattern(5000, 11), "file one"},
                    adl::mock::HeldFile{"F2.BIN", pattern(300, 12), "file two"},
                    adl::mock::HeldFile{"F3.BIN", pattern(1500, 13), "file three"}};
    const std::vector<adl::mock::HeldFile> held = config.holds;
    MockTarget target(std::move(config));
    INFO("the mock target could not start: " << target.not_started.value_or(""));
    REQUIRE_FALSE(target.not_started.has_value());
    const adl::test::TempDir directory;
    const std::string into = adl::test::utf8(directory.path());

    SECTION("the loader names the files")
    {
        const Run run = run_command({"download", "--target", "127.0.0.1", "--position", "HNPFMS_L", "--into", into,
                                     "--file", "F1.BIN", "--file", "F3.BIN", "--control-port", loader_port_text,
                                     "--target-port", target_port_text, "--json"});
        CHECK(run.exit_code == 0);
        const Json finished = run.finished();
        CHECK(finished.at("reason").text == "completed");
        const std::filesystem::path folder = adl::platform::path_from_utf8(finished.at("download_folder").text);
        CHECK(folder.filename() == "DNLD_DATA_HNPFMS_L_1");
        CHECK(read_bytes(folder / "F1.BIN") == held[0].content);
        CHECK(read_bytes(folder / "F3.BIN") == held[2].content);
        CHECK_FALSE(std::filesystem::exists(folder / "F2.BIN"));
        CHECK(run.events_named("file_received").size() == 2);
    }
    SECTION("the target lists its files and --pick takes one of them")
    {
        const Run run =
            run_command({"download", "--target", "127.0.0.1", "--position", "HNPFMS_L", "--into", into, "--pick",
                         "F2.BIN", "--control-port", loader_port_text, "--target-port", target_port_text, "--json"});
        CHECK(run.exit_code == 0);
        const std::vector<Json> listed = run.events_named("file_list");
        REQUIRE(listed.size() == 1);
        CHECK(listed[0].at("files").elements.size() == 3);
        const std::filesystem::path folder = adl::platform::path_from_utf8(run.finished().at("download_folder").text);
        CHECK(read_bytes(folder / "F2.BIN") == held[1].content);
        CHECK_FALSE(std::filesystem::exists(folder / "F1.BIN"));
    }
    SECTION("--pick-all takes all of them")
    {
        const Run run =
            run_command({"download", "--target", "127.0.0.1", "--position", "HNPFMS_L", "--into", into, "--pick-all",
                         "--control-port", loader_port_text, "--target-port", target_port_text, "--json"});
        CHECK(run.exit_code == 0);
        CHECK(run.events_named("file_received").size() == 3);
        const std::filesystem::path folder = adl::platform::path_from_utf8(run.finished().at("download_folder").text);
        for (const adl::mock::HeldFile& file : held)
        {
            CHECK(read_bytes(folder / file.name) == file.content);
        }
    }
}

TEST_CASE("a target that refuses, over real sockets, is a failure that carries the refusal's status code",
          "[load][system]")
{
    adl::mock::Config config = mock_config();
    config.refusal = adl::mock::Refusal{adl::mock::OperationKind::information,
                                        adl::arinc615a::StatusCode::operation_denied, "maintenance mode"};
    MockTarget target(std::move(config));
    INFO("the mock target could not start: " << target.not_started.value_or(""));
    REQUIRE_FALSE(target.not_started.has_value());
    const Run run = run_command({"information", "--target", "127.0.0.1", "--position", "HNPFMS_L", "--control-port",
                                 loader_port_text, "--target-port", target_port_text, "--json"});
    CHECK(run.exit_code == 1);
    const Json finished = run.finished();
    CHECK(finished.at("reason").text == "target_refused");
    CHECK(finished.at("status_code").as_number() == 0x1000);
    CHECK(finished.at("detail").text.find("maintenance mode") != std::string::npos);
}

TEST_CASE("a media set, a download folder and a record in a folder named with characters outside the code page",
          "[load][system]")
{
    adl::mock::Config config = mock_config();
    config.holds = {adl::mock::HeldFile{"F1.BIN", pattern(2000, 21), "file one"}};
    const std::vector<adl::mock::HeldFile> held = config.holds;
    MockTarget target(std::move(config));
    INFO("the mock target could not start: " << target.not_started.value_or(""));
    REQUIRE_FALSE(target.not_started.has_value());
    // An accented letter, Japanese and a character outside the Basic Multilingual Plane, as UTF-8: no Windows code
    // page holds all of them. The names inside the media set are ASCII (libs/media_tree still throws on Windows for
    // a file name inside a set that is outside the code page).
    const std::u8string leaf = u8"adl-load-é-日本語-\U0001F600 and spaces";
    const adl::test::TempDir directory;
    const std::filesystem::path folder = directory.path() / leaf;
    std::filesystem::create_directories(folder);

    auto spec = adl::samples::sample_media_set_spec();
    REQUIRE(spec.has_value());
    const auto members = adl::arinc665::build_media_set(*spec);
    REQUIRE(members.has_value());
    const std::filesystem::path set = folder / "set";
    REQUIRE(adl::media_tree::write_media_set_tree(set, *members).has_value());
    const std::string set_text = adl::test::utf8(set);
    const std::string record = adl::test::utf8(folder / "upload record.json");

    const Run upload = run_command({"upload", "--target", "127.0.0.1", "--position", "HNPFMS_L", "--media-set",
                                    set_text, "--all-loads", "--control-port", loader_port_text, "--target-port",
                                    target_port_text, "--record", record, "--json"});
    CHECK(upload.exit_code == 0);
    const std::vector<Json> checked = upload.events_named("media_set_checked");
    REQUIRE(checked.size() == 1);
    CHECK(checked[0].at("path").text == set_text);
    CHECK(upload.finished().at("record").text == record);
    const std::vector<std::byte> record_bytes = read_bytes(adl::platform::path_from_utf8(record));
    const auto written =
        adl::test::parse_json(std::string(reinterpret_cast<const char*>(record_bytes.data()), record_bytes.size()));
    REQUIRE(written.has_value());
    CHECK(written->at("media_set").at("path").text == set_text);

    const std::string into = adl::test::utf8(folder / "downloads");
    const Run download =
        run_command({"download", "--target", "127.0.0.1", "--position", "HNPFMS_L", "--into", into, "--pick-all",
                     "--control-port", loader_port_text, "--target-port", target_port_text, "--json"});
    CHECK(download.exit_code == 0);
    const std::string kept = download.finished().at("download_folder").text;
    CHECK(kept.starts_with(into));
    CHECK(read_bytes(adl::platform::path_from_utf8(kept) / "F1.BIN") == held[0].content);
    const std::vector<Json> received = download.events_named("file_received");
    REQUIRE(received.size() == 1);
    CHECK(adl::platform::path_from_utf8(received[0].at("path").text) == adl::platform::path_from_utf8(kept) / "F1.BIN");
}

TEST_CASE("the real host's wrong usage and --version come back as they do on the fake one", "[load][system]")
{
    const Run version = run_command({"--version"});
    CHECK(version.exit_code == 0);
    CHECK(version.out.ends_with(" events 1\n"));
    const Run usage = run_command({"upload", "--target", "127.0.0.1"});
    CHECK(usage.exit_code == 2);
    CHECK(usage.out.empty());
}

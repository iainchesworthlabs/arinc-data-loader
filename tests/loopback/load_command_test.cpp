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
#include "scripted_loader.hpp"
#include "temp_dir.hpp"
#include "utf8_path.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <istream>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <streambuf>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
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
    void operation_ended(std::string_view, std::optional<adl::arinc615a::StatusCode> status, bool) override
    {
        const std::lock_guard lock(mutex);
        ended.push_back(status);
        changed.notify_all();
    }

    // Whether an operation has ended within `timeout` (the engine runs on a thread of its own).
    [[nodiscard]] bool wait_for_end(std::chrono::seconds timeout)
    {
        std::unique_lock lock(mutex);
        return changed.wait_for(lock, timeout, [this] { return !ended.empty(); });
    }
    [[nodiscard]] std::vector<std::optional<adl::arinc615a::StatusCode>> ended_so_far()
    {
        const std::lock_guard lock(mutex);
        return ended;
    }

    std::mutex mutex;
    std::condition_variable changed;
    std::vector<std::optional<adl::arinc615a::StatusCode>> ended;
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

namespace
{

// Whether this account can bind the standard's port 59 on the loopback address: Windows lets it, and so does Linux
// to root; an unprivileged account on Linux or macOS may not, and the test that needs the port is then skipped with
// the reason. Nothing else holds it here unless another test is running, which the lock on these tests prevents.
[[nodiscard]] bool may_bind_port_59()
{
    auto host = adl::platform::make_host();
    if (!host)
    {
        return false;
    }
    adl::platform::UdpOptions options;
    options.local = adl::platform::Endpoint{adl::platform::Ipv4Address::loopback(), 59};
    return (*host)->network().open_udp(options).has_value();
}

} // namespace

TEST_CASE(
    "a target that is not on port 59 is reached with --target-port alone, the loader being on 59 as it is by default",
    "[load][system]")
{
    if (!may_bind_port_59())
    {
        SKIP("this account cannot bind port 59 (an account without privileges on Linux or macOS), or something holds "
             "it");
    }
    MockTarget target(mock_config());
    INFO("the mock target could not start: " << target.not_started.value_or(""));
    REQUIRE_FALSE(target.not_started.has_value());
    // No --control-port: the loader listens on 59, and it has to tell the target so in its request, because the target
    // writes to the loader's port 59 only when it is told; without the option it writes to its own port, 30959.
    const Run run = run_command({"information", "--target", "127.0.0.1", "--position", "HNPFMS_L", "--target-port",
                                 target_port_text, "--json"});
    CHECK(run.exit_code == 0);
    CHECK(run.err.empty());
    const Json finished = run.finished();
    CHECK(finished.at("reason").text == "completed");
    CHECK(finished.at("status_code").as_number() == 3);
    const std::vector<Json> configuration = run.events_named("configuration");
    REQUIRE(configuration.size() == 1);
    CHECK(configuration[0].at("hardware").elements[0].at("literal_name").text == "FMS LEFT");
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

// ---- adl_load serve: one loader, several targets, over real sockets ----------------------------------------------
//
// Each mock target is on a loopback address of its own (127.0.0.2, 127.0.0.3 and so on: all of 127.0.0.0/8 is this
// computer on Linux and Windows), with the same port on each, since two on one address would have their transfers
// collide. The loader's one control port is on 127.0.0.1. `--control-port` is given as well as `--target-port`: the
// loader offers the target its own port in the request only when that is not 59.

namespace
{

// Standard input for the program that a test writes to while it runs, and closes to end it.
class PipeBuffer final : public std::streambuf
{
public:
    void write(std::string_view text)
    {
        {
            const std::scoped_lock lock(mutex_);
            pending_.append(text);
        }
        arrived_.notify_all();
    }
    void close()
    {
        {
            const std::scoped_lock lock(mutex_);
            closed_ = true;
        }
        arrived_.notify_all();
    }

protected:
    int_type underflow() override
    {
        std::unique_lock lock(mutex_);
        arrived_.wait(lock, [this] { return !pending_.empty() || closed_; });
        if (pending_.empty())
        {
            return traits_type::eof();
        }
        taken_ = std::move(pending_);
        pending_.clear();
        setg(taken_.data(), taken_.data(), taken_.data() + taken_.size());
        return traits_type::to_int_type(taken_.front());
    }

private:
    std::mutex mutex_;
    std::condition_variable arrived_;
    std::string pending_;
    std::string taken_;
    bool closed_ = false;
};

// `adl_load serve` on the real host, in a thread of its own, taking its requests from the pipe.
class ServeSession
{
public:
    explicit ServeSession(std::vector<std::string> arguments) : arguments_(std::move(arguments)), input_(&pipe_)
    {
        worker_ = std::thread(
            [this]
            {
                auto host = adl::platform::make_host();
                if (!host)
                {
                    return;
                }
                const std::vector<std::string_view> views(arguments_.begin(), arguments_.end());
                exit_code_ = adl::load::run(**host, views, input_, out_, err_);
            });
    }
    ServeSession(const ServeSession&) = delete;
    ServeSession& operator=(const ServeSession&) = delete;
    ~ServeSession()
    {
        pipe_.close();
        if (worker_.joinable())
        {
            worker_.join();
        }
    }

    void request(const std::string& line)
    {
        pipe_.write(line + "\n");
    }
    // The end of the input: the program stops when its operations have, and this returns what it wrote.
    Run end_input()
    {
        pipe_.close();
        worker_.join();
        Run run;
        run.exit_code = exit_code_;
        run.out = out_.str();
        run.err = err_.str();
        INFO(run.out);
        INFO(run.err);
        return run;
    }

private:
    std::vector<std::string> arguments_;
    PipeBuffer pipe_;
    std::istream input_;
    std::ostringstream out_;
    std::ostringstream err_;
    int exit_code_ = -1;
    std::thread worker_;
};

[[nodiscard]] std::string loopback_address(unsigned last)
{
    return "127.0.0." + std::to_string(last);
}

// A mock target on 127.0.0.<last> with the ports the tests use.
[[nodiscard]] adl::mock::Config mock_config_on(unsigned last)
{
    adl::mock::Config config = mock_config();
    config.bind_address = adl::arinc615a::Address{{127, 0, 0, static_cast<std::uint8_t>(last)}};
    return config;
}

[[nodiscard]] std::string request_line(std::initializer_list<std::string_view> fields)
{
    std::string line;
    for (const std::string_view field : fields)
    {
        line += line.empty() ? "" : "\t";
        line += field;
    }
    return line;
}

[[nodiscard]] std::string information_line(std::string_view id, unsigned last)
{
    return request_line({"run", id, "information", "--target", loopback_address(last), "--position", "HNPFMS_L",
                         "--target-port", target_port_text});
}

// The operation of `id`: its events, in order.
[[nodiscard]] std::vector<Json> events_of(const Run& run, std::string_view id)
{
    std::vector<Json> own;
    for (const Json& event : run.events())
    {
        if (event.at("event").text != "request_refused" && event.find("id") != nullptr && event.at("id").text == id)
        {
            own.push_back(event);
        }
    }
    return own;
}

// Whether this computer has 127.0.0.<last> to bind: macOS has 127.0.0.1 only, until it is given an alias.
[[nodiscard]] bool loopback_has(unsigned last)
{
    auto host = adl::platform::make_host();
    if (!host)
    {
        return false;
    }
    adl::platform::UdpOptions options;
    options.local =
        adl::platform::Endpoint{adl::platform::Ipv4Address{{127, 0, 0, static_cast<std::uint8_t>(last)}}, 0};
    return (*host)->network().open_udp(options).has_value();
}

} // namespace

TEST_CASE("serve runs Information operations on eight targets at once from one loader port, over real sockets",
          "[load][system][serve]")
{
    if (!loopback_has(9))
    {
        SKIP(
            "this computer has no loopback address 127.0.0.9 to bind (macOS has 127.0.0.1 only, until it has aliases)");
    }
    std::vector<std::unique_ptr<MockTarget>> targets;
    for (unsigned last = 2; last <= 9; ++last)
    {
        targets.push_back(std::make_unique<MockTarget>(mock_config_on(last)));
        INFO("the mock target on " << loopback_address(last)
                                   << " could not start: " << targets.back()->not_started.value_or(""));
        REQUIRE_FALSE(targets.back()->not_started.has_value());
    }

    ServeSession session({"serve", "--local-address", "127.0.0.1", "--control-port", std::string{loader_port_text}});
    for (unsigned last = 2; last <= 9; ++last)
    {
        session.request(information_line("t" + std::to_string(last), last));
    }
    const Run run = session.end_input(); // the input ends with the eight running: they finish, then it stops
    CHECK(run.exit_code == 0);
    CHECK(run.err.empty());

    const std::vector<Json> events = run.events();
    REQUIRE(events.size() >= 10);
    CHECK(events.front().at("event").text == "started");
    CHECK(events.front().at("command").text == "serve");
    CHECK(events.front().at("address").text == "127.0.0.1");
    CHECK(events.front().at("control_port").as_number() == loader_control_port);
    CHECK(events.back().at("event").text == "stopped");
    CHECK(events.back().at("reason").text == "end_of_input");
    CHECK(events.back().at("operations").as_number() == 8);
    for (unsigned last = 2; last <= 9; ++last)
    {
        const std::string id = "t" + std::to_string(last);
        INFO("operation " << id);
        const std::vector<Json> own = events_of(run, id);
        REQUIRE_FALSE(own.empty());
        CHECK(own.front().at("event").text == "started");
        CHECK(own.front().at("target").text == loopback_address(last));
        CHECK(own.back().at("event").text == "finished");
        CHECK(own.back().at("reason").text == "completed");
        CHECK(own.back().at("exit_code").as_number() == 0);
        std::size_t configurations = 0;
        for (const Json& event : own)
        {
            if (event.at("event").text == "configuration")
            {
                ++configurations;
                CHECK(event.at("hardware").elements[0].at("literal_name").text == "FMS LEFT");
            }
        }
        CHECK(configurations == 1);
    }
}

TEST_CASE("serve uploads to two targets at once, keeps a record for each, and aborts one on a cancel",
          "[load][system][serve]")
{
    if (!loopback_has(3))
    {
        SKIP(
            "this computer has no loopback address 127.0.0.3 to bind (macOS has 127.0.0.1 only, until it has aliases)");
    }
    // The second target keeps at it long enough for the cancel to arrive while it is under way.
    adl::mock::Config slow = mock_config_on(3);
    slow.think_time = std::chrono::milliseconds(3000);
    MockTarget first(mock_config_on(2));
    MockTarget second(std::move(slow));
    INFO("the mock targets could not start: " << first.not_started.value_or("") << second.not_started.value_or(""));
    REQUIRE_FALSE(first.not_started.has_value());
    REQUIRE_FALSE(second.not_started.has_value());

    adl::arinc665::LoadSpec load_spec = adl::samples::sample_load_spec();
    load_spec.data_files[0].content = pattern(6000, 7);
    auto load = adl::arinc665::build_load(load_spec);
    REQUIRE(load.has_value());
    auto spec = adl::samples::sample_media_set_spec();
    REQUIRE(spec.has_value());
    spec->loads[0].load = *load;
    const auto members = adl::arinc665::build_media_set(*spec);
    REQUIRE(members.has_value());
    // A folder named with characters outside any one code page, for the set and for the records.
    const std::u8string leaf = u8"serve-é-日本語-\U0001F600 and spaces";
    const adl::test::TempDir directory;
    const std::filesystem::path folder = directory.path() / leaf;
    std::filesystem::create_directories(folder);
    const std::filesystem::path set = folder / "set";
    REQUIRE(adl::media_tree::write_media_set_tree(set, *members).has_value());
    const std::string set_text = adl::test::utf8(set);
    const std::string record_one = adl::test::utf8(folder / "one.json");
    const std::string record_two = adl::test::utf8(folder / "two.json");

    ServeSession session({"serve", "--local-address", "127.0.0.1", "--control-port", std::string{loader_port_text}});
    session.request(
        request_line({"run", "one", "upload", "--target", loopback_address(2), "--position", "HNPFMS_L", "--media-set",
                      set_text, "--all-loads", "--target-port", target_port_text, "--record", record_one}));
    session.request(
        request_line({"run", "two", "upload", "--target", loopback_address(3), "--position", "HNPFMS_L", "--media-set",
                      set_text, "--all-loads", "--target-port", target_port_text, "--record", record_two}));
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    session.request("cancel\ttwo");
    session.request("stop");
    const Run run = session.end_input();
    CHECK(run.exit_code == 0);
    CHECK(run.err.empty());

    const std::vector<Json> one = events_of(run, "one");
    REQUIRE_FALSE(one.empty());
    CHECK(one.back().at("reason").text == "completed");
    CHECK(one.back().at("record").text == record_one);
    CHECK(first.engine.stored().at(load->header_file_name) == load->header_bytes);
    for (const auto& file : load->data_files)
    {
        CHECK(first.engine.stored().at(file.name) == file.bytes);
    }
    const std::vector<Json> two = events_of(run, "two");
    REQUIRE_FALSE(two.empty());
    CHECK(two.back().at("reason").text == "aborted");
    CHECK(two.back().at("exit_code").as_number() == 1);
    CHECK(two.back().at("record").text == record_two);

    // A record of each, in the folder, valid, for the operation it belongs to.
    const std::vector<std::tuple<std::string, std::string, std::string>> records{
        {record_one, loopback_address(2), "completed"}, {record_two, loopback_address(3), "aborted"}};
    for (const auto& [path, address, reason] : records)
    {
        INFO(path);
        const std::vector<std::byte> bytes = read_bytes(adl::platform::path_from_utf8(path));
        const auto parsed =
            adl::test::parse_json(std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
        REQUIRE(parsed.has_value());
        CHECK(parsed->at("target").at("address").text == address);
        CHECK(parsed->at("reason").text == reason);
    }
    CHECK(run.events().back().at("reason").text == "stop");
}

TEST_CASE("serve with the loader on port 59 reaches a target on another port with --target-port alone",
          "[load][system][serve]")
{
    if (!may_bind_port_59())
    {
        SKIP("this account cannot bind port 59 (an account without privileges on Linux or macOS), or something holds "
             "it");
    }
    MockTarget target(mock_config());
    INFO("the mock target could not start: " << target.not_started.value_or(""));
    REQUIRE_FALSE(target.not_started.has_value());

    // No --control-port: the loader is on 59, and tells the target so, because the target is not.
    ServeSession session({"serve", "--local-address", "127.0.0.1"});
    session.request(request_line({"run", "alone", "information", "--target", "127.0.0.1", "--position", "HNPFMS_L",
                                  "--target-port", target_port_text}));
    const Run run = session.end_input();
    CHECK(run.exit_code == 0);
    CHECK(run.err.empty());
    const std::vector<Json> own = events_of(run, "alone");
    REQUIRE_FALSE(own.empty());
    CHECK(own.back().at("reason").text == "completed");
    CHECK(run.events().front().at("control_port").as_number() == 59);
}

namespace
{

// A loader written out as a script (tests/mock_target_core/scripted_loader.hpp) on a real socket of the loopback
// address: what the program adl_load cannot be made to do. It asks for an initialization file, naming its own port,
// and answers what the target writes to it the way the script says, until `done` is true or the time is up. When it
// goes out of scope its socket is closed, which is what a loader that was killed leaves.
class RawLoader
{
public:
    static constexpr std::uint16_t port = 30902;

    RawLoader(adl::test::LoaderScript::Then then,
              adl::arinc615a::StatusCode abort_code = adl::arinc615a::StatusCode::aborted_by_data_loader)
        : script_(then, port, abort_code)
    {
        auto made = adl::platform::make_host();
        REQUIRE(made.has_value());
        host_ = std::move(*made);
        adl::platform::UdpOptions options;
        options.local = adl::platform::Endpoint{adl::platform::Ipv4Address::loopback(), port};
        auto opened = host_->network().open_udp(options);
        REQUIRE(opened.has_value());
        socket_ = std::move(*opened);
    }

    // Asks for `file`, then reacts to the datagrams that come, until `done()` or `how_long`. True if `done()` was.
    template <typename Done>
    bool run(std::string_view file, std::chrono::seconds how_long, Done&& done)
    {
        const adl::platform::Endpoint target{adl::platform::Ipv4Address::loopback(), target_tftp_port};
        const adl::arinc615a_mini::Bytes request = script_.request(file);
        REQUIRE(socket_->send_to(target, request).has_value());
        const auto deadline = std::chrono::steady_clock::now() + how_long;
        std::array<std::byte, 4096> buffer{};
        while (std::chrono::steady_clock::now() < deadline)
        {
            if (done())
            {
                return true;
            }
            const auto got = socket_->receive_from(buffer, std::chrono::milliseconds(100));
            if (!got || got->truncated)
            {
                continue; // nothing came in that time
            }
            if (const auto reply = script_.react(std::span<const std::byte>{buffer}.first(got->size)))
            {
                REQUIRE(socket_->send_to(got->source, *reply).has_value());
            }
        }
        return done();
    }

    [[nodiscard]] const adl::test::LoaderScript& script() const noexcept
    {
        return script_;
    }

private:
    adl::test::LoaderScript script_;
    std::unique_ptr<adl::platform::Host> host_;
    adl::platform::Owned<adl::platform::UdpSocket> socket_;
};

} // namespace

TEST_CASE("after a loader says ABORT with a code of its own, the target is ready for the next operation",
          "[load][system][timeout]")
{
    MockTarget target(mock_config());
    INFO("the mock target could not start: " << target.not_started.value_or(""));
    REQUIRE_FALSE(target.not_started.has_value());
    {
        // The upload starts; the first status file is answered with ABORT:1FFF, a code that is not one of the three
        // of an abort. The target answers with a status file of its own and ends the operation.
        RawLoader loader(adl::test::LoaderScript::Then::abort_the_first_status_file,
                         static_cast<adl::arinc615a::StatusCode>(0x1FFF));
        REQUIRE(loader.run("HNPFMS_L.LUI", std::chrono::seconds(20),
                           [&target] { return target.listener.wait_for_end(std::chrono::seconds(0)); }));
        REQUIRE(loader.script().status_files().size() == 1);
    }
    // One copy of the list, so that the compiler sees the index in range after the size is checked (GCC -O3 does not
    // follow a second call, and warns of a null pointer).
    const auto ended = target.listener.ended_so_far();
    REQUIRE(ended.size() == 1);
    CHECK((!ended.empty() && ended.front() == adl::arinc615a::StatusCode::aborted_by_data_loader));
    // The program, right after: served, not left waiting for a target that has stopped answering.
    const Run run = run_command({"information", "--target", "127.0.0.1", "--position", "HNPFMS_L", "--control-port",
                                 loader_port_text, "--target-port", target_port_text, "--json"});
    CHECK(run.exit_code == 0);
    CHECK(run.finished().at("reason").text == "completed");
}

TEST_CASE("after a loader has gone away in the middle of an operation, the target ends it and is ready for the next",
          "[load][system][timeout]")
{
    MockTarget target(mock_config());
    INFO("the mock target could not start: " << target.not_started.value_or(""));
    REQUIRE_FALSE(target.not_started.has_value());
    {
        // The initialization file is read and acknowledged, and the loader is gone: its socket is closed, and nothing
        // answers the status files the target writes. (The target's time-out is two seconds with one retry, twice
        // over.)
        RawLoader loader(adl::test::LoaderScript::Then::say_nothing);
        static_cast<void>(loader.run("HNPFMS_L.LUI", std::chrono::seconds(1), [] { return false; }));
    }
    REQUIRE(target.listener.wait_for_end(std::chrono::seconds(30)));
    // One copy of the list, so that the compiler sees the index in range after the size is checked (GCC -O3 does not
    // follow a second call, and warns of a null pointer).
    const auto ended = target.listener.ended_so_far();
    REQUIRE(ended.size() == 1);
    CHECK((!ended.empty() && ended.front() == adl::arinc615a::StatusCode::aborted_by_target));
    const Run run = run_command({"information", "--target", "127.0.0.1", "--position", "HNPFMS_L", "--control-port",
                                 loader_port_text, "--target-port", target_port_text, "--json"});
    CHECK(run.exit_code == 0);
    CHECK(run.finished().at("reason").text == "completed");
}

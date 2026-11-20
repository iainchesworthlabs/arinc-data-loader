// SPDX-License-Identifier: GPL-3.0-or-later
// `adl_load --scenario` over real sockets: adl_load's run function, the function main calls, on the real host,
// uploading to the mock target's engine (adl::mock::MockEngine over its RealTargetLink on a thread of its own) on
// 127.0.0.1, with the loader doing something wrong on purpose, one action to a test. What is looked at is what the
// loader says it did (the `scenario_step` events), how the operation ended, and what the mock target made of it
// (what it logged and what it stored). The mock target is this project's reading of a target: these say what it does
// about each fault, and nothing about any other target.
//
// The ports are high ones, as tests/loopback/load_command_test.cpp has them, and the two files hold the same lock, so
// that only one of them has the sockets at a time. The same commands run on the fake platform, with no socket and a
// clock that does not move by itself, in tests/load/scenario_test.cpp.

#include "adl/arinc665/load.hpp"
#include "adl/arinc665/media_set.hpp"
#include "adl/load/run.hpp"
#include "adl/media_tree/media_tree.hpp"
#include "adl/mock/engine.hpp"
#include "adl/mock/link.hpp"
#include "adl/platform/arguments.hpp"
#include "adl/platform/host.hpp"
#include "adl/samples/scenarios.hpp"
#include "mini_json.hpp"
#include "temp_dir.hpp"
#include "utf8_path.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

using adl::test::Json;

namespace
{

constexpr std::uint16_t target_tftp_port = 30959;
constexpr std::uint16_t target_find_port = 30961;
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

// What the mock target says it does, as it says it.
class TargetLog final : public adl::mock::EngineListener
{
public:
    void log(adl::mock::Severity, std::string_view text) override
    {
        const std::scoped_lock lock(mutex_);
        lines_.emplace_back(text);
    }
    void operation_changed(std::string_view) override {}
    void progress_changed(int) override {}
    void upload_received(const adl::mock::UploadRecord&) override {}

    [[nodiscard]] bool contains(std::string_view part) const
    {
        const std::scoped_lock lock(mutex_);
        return std::ranges::any_of(lines_, [part](const std::string& line) { return line.contains(part); });
    }
    [[nodiscard]] std::string all() const
    {
        const std::scoped_lock lock(mutex_);
        std::string text;
        for (const std::string& line : lines_)
        {
            text += "\n  " + line;
        }
        return text;
    }

private:
    mutable std::mutex mutex_;
    std::vector<std::string> lines_;
};

// The mock target of one scenario, on a thread of its own.
struct MockTarget
{
    TargetLog listener;
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
    // What a scenario_step says of the fault and of the datagram, which is the same every time the scenario is run
    // against the same target: not `t`, and not `to`, whose port is one the target chose.
    [[nodiscard]] std::vector<std::string> steps_told() const
    {
        std::vector<std::string> told;
        for (const Json& step : events_named("scenario_step"))
        {
            std::string text;
            for (const std::string& key : step.keys())
            {
                if (key != "event" && key != "t" && key != "to")
                {
                    text += key + '=' + step.at(key).text + ' ';
                }
            }
            told.push_back(text);
        }
        return told;
    }
};

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

// A media set with one load whose data file is 4000 bytes, so that the mock target takes it in four blocks of 1024
// bytes or fewer, on a real disk, and the scenario files that go with it.
class Upload
{
public:
    Upload() : directory_()
    {
        adl::arinc665::LoadSpec load_spec = adl::samples::sample_load_spec();
        load_spec.data_files[0].content = pattern(4000, 7);
        auto built = adl::arinc665::build_load(load_spec);
        REQUIRE(built.has_value());
        load = *built;
        auto spec = adl::samples::sample_media_set_spec();
        REQUIRE(spec.has_value());
        spec->loads[0].load = *built;
        const auto members = adl::arinc665::build_media_set(*spec);
        REQUIRE(members.has_value());
        const std::filesystem::path set = directory_.path() / "set";
        REQUIRE(adl::media_tree::write_media_set_tree(set, *members).has_value());
        set_text_ = adl::test::utf8(set);
    }

    [[nodiscard]] const std::string& data_file() const
    {
        return load.data_files.front().name;
    }

    // The scenario file `steps` makes (the text inside the brackets of "steps") with `before` ahead of them, written
    // to a file of this test's folder. Returns its name as the program is given it.
    [[nodiscard]] std::string scenario(std::string_view steps, std::string_view before = "",
                                       std::u8string_view folder = u8"") const
    {
        std::filesystem::path directory = directory_.path();
        if (!folder.empty())
        {
            directory /= std::u8string{folder};
            std::filesystem::create_directories(directory);
        }
        const std::filesystem::path path = directory / "faults.json";
        std::ofstream out(path, std::ios::binary);
        out << R"({"kind":"adl-loader-scenario","format":1,)" << before << R"("steps":[)" << steps << "]}";
        return adl::test::utf8(path);
    }

    [[nodiscard]] std::string write(std::string_view text) const
    {
        const std::filesystem::path path = directory_.path() / "other.json";
        std::ofstream out(path, std::ios::binary);
        out << text;
        return adl::test::utf8(path);
    }

    // The upload of the load, with the scenario file `scenario_path`.
    [[nodiscard]] Run run(const std::string& scenario_path) const
    {
        return run_command({"upload", "--target", "127.0.0.1", "--position", "HNPFMS_L", "--media-set", set_text_,
                            "--all-loads", "--control-port", loader_port_text, "--target-port", target_port_text,
                            "--scenario", scenario_path, "--json"});
    }

    // Whether the mock target has every file of the load, as it was.
    [[nodiscard]] bool stored_whole(const MockTarget& target) const
    {
        const auto& stored = target.engine.stored();
        const auto same = [&stored](const std::string& name, const std::vector<std::byte>& bytes)
        {
            const auto found = stored.find(name);
            return found != stored.end() && found->second == bytes;
        };
        bool whole = same(load.header_file_name, load.header_bytes);
        for (const auto& file : load.data_files)
        {
            whole = whole && same(file.name, file.bytes);
        }
        for (const auto& file : load.support_files)
        {
            whole = whole && same(file.name, file.bytes);
        }
        return whole;
    }

    adl::arinc665::Load load;

private:
    adl::test::TempDir directory_;
    std::string set_text_;
};

// A step that acts on a block of the data file, for the actions that act on a datagram.
[[nodiscard]] std::string on_block(const Upload& upload, unsigned block, std::string_view action)
{
    return R"({"when":{"after_ms":0},"on":{"datagram":"data","file":")" + upload.data_file() + R"(","block":)" +
           std::to_string(block) + R"(},"do":{)" + std::string{action} + "}}";
}

// The `scenario_step` events of a run that acted once on a block of the data file.
void check_told_once(const Run& run, const Upload& upload, std::string_view action, unsigned block)
{
    const std::vector<Json> steps = run.events_named("scenario_step");
    REQUIRE(steps.size() == 1);
    CHECK(steps[0].at("step").as_number() == 1);
    CHECK(steps[0].at("operation").text == "upload");
    CHECK(steps[0].at("action").text == action);
    CHECK(steps[0].at("datagram").text == "data");
    CHECK(steps[0].at("file").text == upload.data_file());
    CHECK(steps[0].at("block").as_number() == block);
    CHECK(steps[0].at("to").text.starts_with("127.0.0.1:"));
    CHECK(run.finished().at("scenario_unused").elements.empty());
}

} // namespace

TEST_CASE("a data block that is dropped comes again, and the upload completes", "[load][system][scenario]")
{
    const Upload upload;
    MockTarget target(mock_config());
    INFO("the mock target could not start: " << target.not_started.value_or(""));
    REQUIRE_FALSE(target.not_started.has_value());
    const Run run = upload.run(upload.scenario(on_block(upload, 2, R"("action":"drop")")));
    INFO("the mock target said:" << target.listener.all());
    CHECK(run.exit_code == 0);
    CHECK(run.err.empty());
    CHECK(run.finished().at("reason").text == "completed");
    check_told_once(run, upload, "drop", 2);
    CHECK(upload.stored_whole(target));
    // The first line of the events says which file was read, and how many steps there are.
    CHECK(run.events().front().at("scenario_steps").as_number() == 1);
}

TEST_CASE("a data block sent three times is written once", "[load][system][scenario]")
{
    const Upload upload;
    MockTarget target(mock_config());
    REQUIRE_FALSE(target.not_started.has_value());
    const Run run = upload.run(upload.scenario(on_block(upload, 2, R"("action":"duplicate","copies":3)")));
    INFO("the mock target said:" << target.listener.all());
    CHECK(run.exit_code == 0);
    check_told_once(run, upload, "duplicate", 2);
    CHECK(run.events_named("scenario_step")[0].at("copies").as_number() == 3);
    CHECK(upload.stored_whole(target));
}

TEST_CASE("a data block that comes late, or after a pause, does no harm", "[load][system][scenario]")
{
    const Upload upload;
    MockTarget target(mock_config());
    REQUIRE_FALSE(target.not_started.has_value());

    SECTION("delay")
    {
        const Run run = upload.run(upload.scenario(on_block(upload, 2, R"("action":"delay","ms":300)")));
        INFO("the mock target said:" << target.listener.all());
        CHECK(run.exit_code == 0);
        check_told_once(run, upload, "delay", 2);
        CHECK(run.events_named("scenario_step")[0].at("ms").as_number() == 300);
        CHECK(upload.stored_whole(target));
    }
    SECTION("reorder")
    {
        const Run run = upload.run(upload.scenario(on_block(upload, 2, R"("action":"reorder","within_ms":400)")));
        INFO("the mock target said:" << target.listener.all());
        CHECK(run.exit_code == 0);
        check_told_once(run, upload, "reorder", 2);
        CHECK(upload.stored_whole(target));
    }
}

TEST_CASE("a data byte that is changed is found by the mock target, which keeps the file as it came",
          "[load][system][scenario]")
{
    const Upload upload;
    MockTarget target(mock_config());
    REQUIRE_FALSE(target.not_started.has_value());
    const Run run = upload.run(upload.scenario(on_block(upload, 1, R"("action":"corrupt","offset":4,"xor":1)")));
    INFO("the mock target said:" << target.listener.all());
    check_told_once(run, upload, "corrupt", 1);
    CHECK(target.listener.contains("checksum mismatch for " + upload.data_file()));
    CHECK_FALSE(upload.stored_whole(target));
    // One byte of the file is not what the loader's file has.
    const auto& stored = target.engine.stored().at(upload.data_file());
    const auto& sent = upload.load.data_files.front().bytes;
    REQUIRE(stored.size() == sent.size());
    CHECK(std::ranges::count_if(std::views::iota(std::size_t{0}, sent.size()),
                                [&](std::size_t i) { return stored[i] != sent[i]; }) == 1);
}

TEST_CASE("a check value that is changed in the acknowledgement of the options is told", "[load][system][scenario]")
{
    const Upload upload;
    MockTarget target(mock_config());
    REQUIRE_FALSE(target.not_started.has_value());
    const Run run = upload.run(
        upload.scenario(R"({"when":{"after_ms":0},"on":{"datagram":"option_ack"},"do":{"action":"check_value"}})"));
    INFO("the mock target said:" << target.listener.all());
    const std::vector<Json> steps = run.events_named("scenario_step");
    REQUIRE(steps.size() == 1);
    CHECK(steps[0].at("action").text == "check_value");
    CHECK(steps[0].at("datagram").text == "option_ack");
    CHECK(run.finished().at("scenario_unused").elements.empty());
}

TEST_CASE("a last block that is cut short is not taken for the whole file", "[load][system][scenario]")
{
    const Upload upload;
    MockTarget target(mock_config());
    REQUIRE_FALSE(target.not_started.has_value());
    // 4000 bytes at a thousand and twenty-four to a block is four blocks, the last one 928 bytes.
    const Run run = upload.run(upload.scenario(on_block(upload, 4, R"("action":"length","delta":-7)")));
    INFO("the mock target said:" << target.listener.all());
    check_told_once(run, upload, "length", 4);
    CHECK(run.events_named("scenario_step")[0].at("delta").text == "-7");
    CHECK_FALSE(upload.stored_whole(target));
}

TEST_CASE("an ERROR in place of a block ends that transfer, and the mock target takes the file again",
          "[load][system][scenario]")
{
    const Upload upload;
    MockTarget target(mock_config());
    REQUIRE_FALSE(target.not_started.has_value());
    const Run run = upload.run(upload.scenario(on_block(upload, 1, R"("action":"error","code":3,"text":"disk full")")));
    INFO("the mock target said:" << target.listener.all());
    check_told_once(run, upload, "error", 1);
    CHECK(run.events_named("scenario_step")[0].at("code").as_number() == 3);
    CHECK(run.exit_code == 0);
    CHECK(upload.stored_whole(target));
}

TEST_CASE("a WAIT in place of the answer to a file's request makes the mock target ask again after the wait",
          "[load][system][scenario]")
{
    const Upload upload;
    MockTarget target(mock_config());
    REQUIRE_FALSE(target.not_started.has_value());
    const Run run = upload.run(upload.scenario(R"({"when":{"after_ms":0},"on":{"datagram":"reply","file":")" +
                                               upload.data_file() + R"("},"do":{"action":"wait","seconds":2}})"));
    INFO("the mock target said:" << target.listener.all());
    const std::vector<Json> steps = run.events_named("scenario_step");
    REQUIRE(steps.size() == 1);
    CHECK(steps[0].at("action").text == "wait");
    CHECK(steps[0].at("seconds").as_number() == 2);
    CHECK(run.exit_code == 0);
    CHECK(target.listener.contains("the loader asked to wait 2s for " + upload.data_file()));
    CHECK(upload.stored_whole(target));
}

TEST_CASE("an ABORT in place of the answer to a status file ends the operation at the mock target",
          "[load][system][scenario]")
{
    const Upload upload;
    MockTarget target(mock_config());
    REQUIRE_FALSE(target.not_started.has_value());
    const Run run = upload.run(upload.scenario(
        R"({"when":{"status_file":2},"on":{"datagram":"reply","file":".LUS"},"do":{"action":"abort_message","status":"0x7777"}})"));
    INFO("the mock target said:" << target.listener.all());
    const std::vector<Json> steps = run.events_named("scenario_step");
    REQUIRE(steps.size() == 1);
    CHECK(steps[0].at("action").text == "abort_message");
    CHECK(steps[0].at("status").as_number() == 0x7777);
    CHECK(steps[0].at("trigger").text == "status_file");
    CHECK(steps[0].at("at").as_number() == 2);
    CHECK(target.listener.contains("sees ABORT"));
    CHECK(run.finished().at("result").text == "failed");
}

TEST_CASE("the loader's own abort at a chosen moment ends the operation as aborted", "[load][system][scenario]")
{
    const Upload upload;
    MockTarget target(mock_config());
    REQUIRE_FALSE(target.not_started.has_value());
    const Run run =
        upload.run(upload.scenario(R"({"when":{"status_file":1},"do":{"action":"abort","status":"0x1005"}})"));
    INFO("the mock target said:" << target.listener.all());
    const std::vector<Json> steps = run.events_named("scenario_step");
    REQUIRE(steps.size() == 1);
    CHECK(steps[0].at("action").text == "abort");
    CHECK(steps[0].at("status").as_number() == 0x1005);
    CHECK(steps[0].at("datagram").is(Json::Kind::null));
    CHECK(run.exit_code == 1);
    CHECK(run.finished().at("reason").text == "aborted");
    CHECK(target.listener.contains("sees ABORT"));
}

TEST_CASE("a loader that goes silent for a while is waited for, and the operation ends", "[load][system][scenario]")
{
    const Upload upload;
    MockTarget target(mock_config());
    REQUIRE_FALSE(target.not_started.has_value());
    const auto started = std::chrono::steady_clock::now();
    const Run run = upload.run(upload.scenario(R"({"when":{"after_ms":0},"do":{"action":"silent","for_ms":3000}})"));
    INFO("the mock target said:" << target.listener.all());
    const std::vector<Json> steps = run.events_named("scenario_step");
    REQUIRE(steps.size() == 1);
    CHECK(steps[0].at("action").text == "silent");
    CHECK(steps[0].at("for_ms").as_number() == 3000);
    CHECK(steps[0].at("datagram").is(Json::Kind::null));
    // The loader said nothing for three seconds, so the operation took longer than that, and it ended.
    CHECK(std::chrono::steady_clock::now() - started >= std::chrono::seconds(3));
    CHECK(run.finished().at("exit_code").as_number() == static_cast<std::uint64_t>(run.exit_code));
}

TEST_CASE("the same scenario gives the same steps every time", "[load][system][scenario]")
{
    const Upload upload;
    // A chance for each block, from the seed: about one block in three is delayed by a time between 20 and 120
    // milliseconds, and the seed says which.
    const std::string path = upload.scenario(
        R"({"when":{"after_ms":0},"on":{"datagram":"data"},"do":{"action":"delay","min_ms":20,"max_ms":120},)"
        R"("count":"all","per_mille":330})",
        R"("seed":11,)");
    std::vector<std::string> first;
    {
        MockTarget target(mock_config());
        REQUIRE_FALSE(target.not_started.has_value());
        const Run run = upload.run(path);
        CHECK(run.exit_code == 0);
        CHECK(upload.stored_whole(target));
        first = run.steps_told();
    }
    std::vector<std::string> second;
    {
        MockTarget target(mock_config());
        REQUIRE_FALSE(target.not_started.has_value());
        const Run run = upload.run(path);
        CHECK(run.exit_code == 0);
        second = run.steps_told();
    }
    CHECK_FALSE(first.empty());
    CHECK(first == second);
}

TEST_CASE("a scenario file in a folder named with characters outside the code page is read", "[load][system][scenario]")
{
    const Upload upload;
    MockTarget target(mock_config());
    REQUIRE_FALSE(target.not_started.has_value());
    // An accented letter, Japanese and a character outside the Basic Multilingual Plane, as UTF-8: no Windows code
    // page holds all of them.
    const std::string path = upload.scenario(on_block(upload, 2, R"("action":"delay","ms":50)"), "",
                                             u8"adl-load-é-日本語-\U0001F600 and spaces");
    const Run run = upload.run(path);
    INFO("the mock target said:" << target.listener.all());
    CHECK(run.exit_code == 0);
    CHECK(run.events().front().at("scenario").text == path);
    check_told_once(run, upload, "delay", 2);
}

TEST_CASE("a scenario file that is refused ends the program before it opens a port", "[load][system][scenario]")
{
    const Upload upload;
    // No mock target is listening and none is needed: the file is read first.
    const std::string bad = upload.write(R"({"kind":"adl-loader-scenario","format":1,"steps":[{"when":{"after_ms":0},)"
                                         R"("do":{"action":"delay"}}]})");
    const Run run = upload.run(bad);
    CHECK(run.exit_code == 5);
    CHECK(run.out.empty());
    CHECK(run.err.starts_with("adl_load: --scenario " + bad + ": step 1: "));
    CHECK(std::ranges::count(run.err, '\n') == 1);
}

// SPDX-License-Identifier: GPL-3.0-or-later
// The loader's faults on purpose against the mock target's engine, over the fake network: the loader's real engine
// with a Player in place runs an Upload, and what the mock target does about each fault is looked at (what it
// logged, what it stored, how the operation ended). The mock target is the project's reading of a target; these
// say what it does, and nothing about any other target.

#include "adl/arinc615a/directory_files.hpp"
#include "adl/loader_scenario/player.hpp"
#include "fake_link.hpp"
#include "loader_rig.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace
{

using namespace adl::arinc615a;
using namespace adl::loader_scenario;
using adl::mock::Config;
using adl::mock::EngineListener;
using adl::mock::MockEngine;
using adl::mock::Severity;
using adl::mock::UploadRecord;
using adl::test::FakeTargetLink;
using adl::test::LoaderRecorder;
using adl::test::MockRig;
using adl::test::put_load_files;
using adl::test::upload_references;
using std::chrono::seconds;

struct Reported
{
    std::size_t step = 0;
    Action action = Action::drop;
    std::optional<DatagramKind> datagram;
    std::string file;
    std::optional<std::uint16_t> block;
};

class Events final : public FaultListener
{
public:
    std::vector<Reported> list;
    void on_fault(const FaultEvent& event) noexcept override
    {
        list.push_back(Reported{event.step, event.action, event.datagram, std::string{event.file}, event.block});
    }
};

class Lines final : public EngineListener
{
public:
    std::vector<std::string> lines;
    std::vector<std::pair<std::string, std::optional<StatusCode>>> ended;

    void log(Severity, std::string_view text) override
    {
        lines.emplace_back(text);
    }
    void operation_changed(std::string_view) override {}
    void progress_changed(int) override {}
    void upload_received(const UploadRecord&) override {}
    void operation_ended(std::string_view name, std::optional<StatusCode> status, bool) override
    {
        ended.emplace_back(std::string{name}, status);
    }

    [[nodiscard]] bool contains(std::string_view part) const
    {
        return std::ranges::any_of(lines, [part](const std::string& line) { return line.contains(part); });
    }
};

[[nodiscard]] Step step_of(Action action, Selector on = {}, When when = {})
{
    Step step;
    step.when = when;
    step.on = std::move(on);
    step.fault.action = action;
    return step;
}

// An upload of the sample load to a mock target, with `steps` in effect.
struct Run
{
    MockRig rig;
    Lines listener;
    MockEngine engine{&listener};
    FakeTargetLink link{rig.world, rig.network, rig.target_address};
    adl::arinc665::Load load;
    Events events;
    std::unique_ptr<Player> player;
    LoaderRecorder recorder;
    std::expected<Outcome, RunError> outcome = std::unexpected(RunError{});
    adl::platform::Duration took{};

    explicit Run(std::vector<Step> steps, std::uint64_t seed = 1, std::size_t data_bytes = 30000, Config config = {})
        : load(adl::test::make_load(data_bytes, 7))
    {
        link.attach(engine);
        REQUIRE_FALSE(engine.start(link, config));
        put_load_files(rig.loader_host, load);
        DirectoryFileSource source(rig.loader_host.files(), "/load");
        const std::vector<LoadReference> references = upload_references(load);
        LoaderServices services;
        services.source = &source;
        services.listener = &recorder;
        if (!steps.empty())
        {
            player =
                std::make_unique<Player>(Scenario{"mock", "", seed, std::move(steps)}, OperationName::upload, &events);
            services.faults = player.get();
        }
        const auto began = rig.world.now();
        outcome = rig.run(UploadOperation{UploadRequest{references}}, services);
        took = rig.world.now() - began;
    }

    [[nodiscard]] Selector data_block(std::uint16_t block) const
    {
        Selector on;
        on.datagram = DatagramKind::data;
        on.file = load.data_files.front().name;
        on.block = block;
        return on;
    }

    [[nodiscard]] bool stored_whole() const
    {
        const auto& stored = engine.stored();
        if (stored.size() != 1 + load.data_files.size() + load.support_files.size())
        {
            return false;
        }
        bool whole = stored.at(load.header_file_name) == load.header_bytes;
        for (const auto& file : load.data_files)
        {
            whole = whole && stored.at(file.name) == file.bytes;
        }
        for (const auto& file : load.support_files)
        {
            whole = whole && stored.at(file.name) == file.bytes;
        }
        return whole;
    }
};

// What happened, for the report of a check that failed.
[[nodiscard]] std::string story(const Run& run)
{
    std::string text = run.outcome ? std::string{to_string(run.outcome->code)} : std::string{"did not run"};
    text += "; the target said:";
    for (const std::string& line : run.listener.lines)
    {
        text += " | " + line;
    }
    return text;
}

} // namespace

TEST_CASE("with no step the mock target is uploaded to whole, and the hook changes nothing", "[loader_scenario][mock]")
{
    const Run baseline({});
    REQUIRE(baseline.outcome);
    CHECK(baseline.outcome->code == OutcomeCode::completed);
    CHECK(baseline.stored_whole());

    // A hook with no step is the same run: same outcome, same time.
    Run quiet({step_of(Action::drop,
                       []
                       {
                           Selector on;
                           on.file = "NOTHING.BIN";
                           return on;
                       }())});
    REQUIRE(quiet.outcome);
    CHECK(quiet.outcome->code == OutcomeCode::completed);
    CHECK(quiet.stored_whole());
    CHECK(quiet.took == baseline.took);
    CHECK(quiet.events.list.empty());
    CHECK(quiet.player->unapplied_steps() == std::vector<std::size_t>{1});
}

TEST_CASE("a data block that is lost comes again, and the mock target waits for it", "[loader_scenario][mock]")
{
    const Run baseline({});
    Run probe({});
    const Run run({step_of(Action::drop, probe.data_block(1))});
    REQUIRE(run.outcome);
    CHECK(run.outcome->code == OutcomeCode::completed);
    CHECK(run.stored_whole());
    CHECK(run.events.list.size() == 1);
    // One time-out of the transfer, two seconds, lies between the two.
    CHECK(run.took >= baseline.took + seconds(2));
    CHECK(run.took < baseline.took + seconds(10));
}

TEST_CASE("a data block sent several times is written once", "[loader_scenario][mock]")
{
    Run probe({});
    Step step = step_of(Action::duplicate, probe.data_block(2));
    step.fault.copies = 4;
    const Run run({step});
    REQUIRE(run.outcome);
    CHECK(run.outcome->code == OutcomeCode::completed);
    CHECK(run.stored_whole());
    REQUIRE(run.events.list.size() == 1);
}

TEST_CASE("a request sent many times at once is answered once", "[loader_scenario][mock]")
{
    Selector on;
    on.datagram = DatagramKind::request;
    Step step = step_of(Action::duplicate, on);
    step.fault.copies = 8;
    const Run run({step});
    REQUIRE(run.outcome);
    CHECK(run.outcome->code == OutcomeCode::completed);
    CHECK(run.stored_whole());
}

TEST_CASE("a datagram that comes late is taken for a repeat", "[loader_scenario][mock]")
{
    Run probe({});
    Step step = step_of(Action::delay, probe.data_block(1));
    step.fault.milliseconds = 3000;
    step.fault.min_milliseconds = 3000;
    step.fault.max_milliseconds = 3000;
    const Run run({step});
    REQUIRE(run.outcome);
    CHECK(run.outcome->code == OutcomeCode::completed);
    CHECK(run.stored_whole());
}

TEST_CASE("a datagram held back until after the next one does no harm", "[loader_scenario][mock]")
{
    Selector on;
    on.datagram = DatagramKind::ack;
    Step step = step_of(Action::reorder, on);
    step.all = true;
    step.fault.milliseconds = 300;
    const Run run({step});
    REQUIRE(run.outcome);
    CHECK(run.outcome->code == OutcomeCode::completed);
    CHECK(run.stored_whole());
}

TEST_CASE("a data byte that is changed is found by the mock target, which keeps the file as it came",
          "[loader_scenario][mock]")
{
    Run probe({});
    const Run run({step_of(Action::corrupt, probe.data_block(1))});
    INFO(story(run));
    REQUIRE(run.outcome);
    CHECK(run.events.list.size() == 1);
    CHECK(run.listener.contains("checksum mismatch for " + run.load.data_files.front().name));
    // The mock target says so in its log and goes on: the upload ends as completed, with the damaged file stored.
    CHECK(run.outcome->code == OutcomeCode::completed);
    CHECK(!run.stored_whole());
    CHECK(run.engine.stored().at(run.load.data_files.front().name) != run.load.data_files.front().bytes);
}

TEST_CASE("a file that is cut short is not taken for the whole", "[loader_scenario][mock]")
{
    Run probe({});
    // The last block of the file, 30000 bytes at 1024 to a block, is block 30.
    Step step = step_of(Action::length, probe.data_block(30));
    step.fault.delta = -7;
    const Run run({step});
    REQUIRE(run.outcome);
    REQUIRE(run.events.list.size() == 1);
    CHECK(!run.stored_whole());
}

TEST_CASE("an ERROR in place of a block ends that transfer, and the mock target takes the file again",
          "[loader_scenario][mock]")
{
    Run probe({});
    Step step = step_of(Action::error, probe.data_block(1));
    step.fault.code = 3;
    step.fault.text = "disk full";
    const Run run({step});
    INFO(story(run));
    REQUIRE(run.outcome);
    REQUIRE(run.events.list.size() == 1);
    // A transfer that failed is attempted again (Attachment 4), and the file comes whole the second time.
    CHECK(run.outcome->code == OutcomeCode::completed);
    CHECK(run.stored_whole());
}

TEST_CASE("a WAIT in place of the answer to a file's request makes the mock target ask again after the wait",
          "[loader_scenario][mock]")
{
    Run probe({});
    Selector on;
    on.datagram = DatagramKind::reply;
    on.file = probe.load.data_files.front().name;
    Step step = step_of(Action::wait, on);
    step.fault.seconds = 2;
    const Run run({step});
    REQUIRE(run.outcome);
    CHECK(run.outcome->code == OutcomeCode::completed);
    CHECK(run.listener.contains("the loader asked to wait 2s for " + run.load.data_files.front().name));
    CHECK(run.stored_whole());
}

TEST_CASE("an ABORT in place of the answer to a status file ends the operation at the mock target",
          "[loader_scenario][mock]")
{
    Selector on;
    on.datagram = DatagramKind::reply;
    on.file = ".LUS";
    Step step = step_of(Action::abort_message, on, When{TriggerKind::status_file, 2});
    step.fault.status = 0x1004;
    const Run run({step});
    REQUIRE(run.outcome);
    REQUIRE(run.events.list.size() == 1);
    CHECK(run.listener.contains("sees ABORT"));
}

TEST_CASE("an ABORT with a code that is not the loader's is still an ABORT to the mock target",
          "[loader_scenario][mock]")
{
    Selector on;
    on.datagram = DatagramKind::reply;
    on.file = ".LUS";
    Step step = step_of(Action::abort_message, on, When{TriggerKind::status_file, 2});
    step.fault.status = 0x7777;
    const Run run({step});
    REQUIRE(run.outcome);
    REQUIRE(run.events.list.size() == 1);
    CHECK(run.listener.contains("sees ABORT"));
    CHECK(run.outcome->code != OutcomeCode::completed);
}

TEST_CASE("the loader's own abort at a chosen moment is confirmed by the mock target", "[loader_scenario][mock]")
{
    Step step = step_of(Action::abort, {}, When{TriggerKind::status_file, 2});
    step.fault.status = 0x1005;
    const Run run({step}, 1, 3'000'000);
    REQUIRE(run.outcome);
    CHECK(run.outcome->code == OutcomeCode::aborted_by_operator);
    CHECK(run.outcome->abort_confirmed);
    CHECK(run.listener.contains("sees ABORT"));
    REQUIRE(run.events.list.size() == 1);
    CHECK(run.events.list[0].action == Action::abort);
}

TEST_CASE("a loader that goes silent is timed out, and so is the mock target's side of it", "[loader_scenario][mock]")
{
    const Run run({step_of(Action::silent, {}, When{TriggerKind::request, 2})});
    REQUIRE(run.outcome);
    CHECK(run.outcome->code != OutcomeCode::completed);
    CHECK(run.outcome->code != OutcomeCode::running);
    CHECK(run.took < std::chrono::minutes(5));
    REQUIRE(run.events.list.size() == 1);
    CHECK(run.events.list[0].action == Action::silent);
}

TEST_CASE("the same scenario gives the same reports and the same end every time against the mock target",
          "[loader_scenario][mock]")
{
    const auto make = [](std::uint64_t seed)
    {
        Selector on;
        on.datagram = DatagramKind::data;
        Step step = step_of(Action::drop, on);
        step.per_mille = 150;
        step.count = 3;
        return Run({step}, seed);
    };
    const Run a = make(21);
    const Run b = make(21);
    REQUIRE(a.outcome);
    REQUIRE(b.outcome);
    REQUIRE(!a.events.list.empty());
    REQUIRE(a.events.list.size() == b.events.list.size());
    for (std::size_t i = 0; i < a.events.list.size(); ++i)
    {
        CHECK(a.events.list[i].file == b.events.list[i].file);
        CHECK(a.events.list[i].block == b.events.list[i].block);
    }
    CHECK(a.outcome->code == b.outcome->code);
    CHECK(a.took == b.took);
    CHECK(a.listener.lines == b.listener.lines);
}

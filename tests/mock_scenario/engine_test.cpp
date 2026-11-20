// SPDX-License-Identifier: GPL-3.0-or-later
// Three scenarios run against the loader's real engine (DataLoader and Runner over a fake host) on the fake network.
// The clock is the fake world's and the arithmetic of the draws is integer, so each run leaves exactly the same
// events, with the same times, on every run and on every system: what is written down here is what the mock target
// says happened, and what the loader made of it. Each scenario is run twice.

#include "adl/arinc615a/directory_files.hpp"
#include "adl/mock/scenario_file.hpp"
#include "adl/mock/scenario_player.hpp"
#include "fake_link.hpp"
#include "loader_rig.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using namespace adl::arinc615a;
using namespace adl::mock;
using namespace adl::test;

namespace
{

using Lines = std::vector<std::string>;

// A mock target on the fake network with a scenario, and what happened to it written down as lines of words with their
// times: the steps that fired (as the observer was told), the operations that ended, and how each operation ended for
// the loader.
class Session final : public EngineListener
{
public:
    explicit Session(std::string_view scenario_text, std::uint64_t world_seed = 1)
        : rig_(world_seed), link_(rig_.world, rig_.network, rig_.target_address)
    {
        auto scenario = read_scenario(scenario_text);
        if (!scenario)
        {
            FAIL(scenario.error().message);
        }
        player_ = std::make_shared<ScenarioPlayer>(std::move(*scenario),
                                                   [this](const StepFired& event) { note_step(event); });
        engine_ = std::make_unique<MockEngine>(this);
        link_.attach(*engine_);
        Config config;
        config.fault_plan = player_;
        const auto error = engine_->start(link_, config);
        if (error)
        {
            FAIL(*error);
        }
    }

    // The loader runs `operation` against the target, and its outcome is written down.
    void run(std::string_view name, const Operation& operation, LoaderServices services)
    {
        const auto outcome = rig_.run(operation, services);
        if (!outcome)
        {
            lines_.push_back(at() + " loader " + std::string{name} + " did not finish");
            return;
        }
        lines_.push_back(at() + " loader " + std::string{name} + " " + to_string(outcome->code));
    }

    [[nodiscard]] MockRig& rig() noexcept
    {
        return rig_;
    }
    [[nodiscard]] const Lines& lines() const noexcept
    {
        return lines_;
    }
    [[nodiscard]] const ScenarioPlayer& player() const noexcept
    {
        return *player_;
    }

    // EngineListener
    void log(Severity, std::string_view) override {}
    void operation_changed(std::string_view) override {}
    void progress_changed(int) override {}
    void upload_received(const UploadRecord&) override {}
    void operation_ended(std::string_view name, std::optional<StatusCode> status, bool refused) override
    {
        std::string line = at() + " target ended " + std::string{name} + " ";
        if (status)
        {
            char code[16];
            std::snprintf(code, sizeof code, "0x%04X", static_cast<unsigned>(*status));
            line += code;
        }
        else
        {
            line += "no status";
        }
        lines_.push_back(line + (refused ? " refused" : ""));
    }

private:
    [[nodiscard]] std::string at() const
    {
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(link_.now().since_origin).count();
        return std::to_string(ms) + " ms";
    }

    void note_step(const StepFired& event)
    {
        std::string line = at() + " step " + std::to_string(event.step) + " " +
                           std::string{adl::scenario::trigger_name(event.trigger)} + " " + std::to_string(event.at) +
                           " " + std::string{operation_word(event.operation)} + " " +
                           std::string{effect_word(event.effect)} + (event.applied ? " applied" : " not applied");
        if (event.value)
        {
            line += " " + std::to_string(*event.value);
        }
        lines_.push_back(line);
    }

    MockRig rig_;
    FakeTargetLink link_;
    std::shared_ptr<ScenarioPlayer> player_;
    std::unique_ptr<MockEngine> engine_;
    Lines lines_;
};

// An upload of the sample load, its first data file grown to `data_bytes`.
struct UploadJob
{
    adl::arinc665::Load load;
    std::vector<LoadReference> references;
    DirectoryFileSource source;

    UploadJob(Session& session, std::size_t data_bytes, std::uint64_t seed)
        : load(make_load(data_bytes, seed)), references(upload_references(load)),
          source((put_load_files(session.rig().loader_host, load), session.rig().loader_host.files()), "/load")
    {
    }
};

void upload(Session& session, std::string_view name, std::size_t data_bytes, std::uint64_t seed)
{
    UploadJob job(session, data_bytes, seed);
    LoaderRecorder recorder;
    LoaderServices services;
    services.source = &job.source;
    services.listener = &recorder;
    session.run(name, UploadOperation{UploadRequest{job.references}}, services);
}

void information(Session& session, std::string_view name)
{
    LoaderRecorder recorder;
    LoaderServices services;
    services.listener = &recorder;
    session.run(name, InformationOperation{}, services);
}

// ---- The three scenarios ---------------------------------------------------------------------

// 1. A slow start: the first request waits two seconds, and the second upload request is answered a little late.
constexpr std::string_view slow_start = R"({
  "kind": "adl-mock-scenario",
  "format": 1,
  "name": "Slow start",
  "seed": 11,
  "steps": [
    { "when": { "request": 1 }, "do": { "action": "wait", "seconds": 2 } },
    { "when": { "request": 2, "operation": "upload" }, "do": { "action": "delay", "ms": 300 } }
  ]
})";

Lines run_slow_start()
{
    Session session(slow_start);
    upload(session, "upload", 30000, 5);
    return session.lines();
}

// 2. Status files are lost by chance, from a seed, and the first answer is held back for a drawn time.
std::string lost_by_chance(std::string_view seed)
{
    return R"({
  "kind": "adl-mock-scenario",
  "format": 1,
  "seed": )" +
           std::string{seed} +
           R"(,
  "steps": [
    { "when": { "request": 1 }, "do": { "action": "delay", "min_ms": 100, "max_ms": 400 } },
    { "when": { "status_file": 2, "every": 1, "times": 8 }, "per_mille": 500, "do": { "action": "drop_status_file" } }
  ]
})";
}

Lines run_lost_by_chance(std::string_view seed)
{
    Session session(lost_by_chance(seed));
    upload(session, "upload", 3'000'000, 13);
    return session.lines();
}

// 3. An Information refused as not supported; an upload aborted by the target 2500 ms into it; and an upload whose
// third status file never comes, so that the loader's time-out ends it.
constexpr std::string_view refusal_abort_silence = R"({
  "kind": "adl-mock-scenario",
  "format": 1,
  "steps": [
    { "when": { "request": 1 }, "do": { "action": "refuse", "code": "0x1002" } },
    { "when": { "after_ms": 2500, "times": 1, "operation": "upload" }, "do": { "action": "abort" } },
    { "when": { "status_file": 3, "operation": "upload" }, "do": { "action": "silent" } }
  ]
})";

Lines run_refusal_abort_silence()
{
    Session session(refusal_abort_silence);
    information(session, "information");
    upload(session, "upload 1", 3'000'000, 15);
    upload(session, "upload 2", 30000, 16);
    return session.lines();
}

} // namespace

TEST_CASE("mock scenario engine: a slow start gives the same events every time", "[mock_scenario][engine]")
{
    const Lines first = run_slow_start();
    CHECK(run_slow_start() == first);
    // The first request is answered with WAIT:2 at once; the loader asks again two seconds later, and that is the
    // second upload request, whose answer is held back 300 ms. The target's work starts after its think time (1 s).
    CHECK(first == Lines{
                       "0 ms step 1 request 1 upload wait applied 2",
                       "2000 ms step 2 request 2 upload delay applied 300",
                       "3331 ms target ended Upload 0x0003",
                       "6331 ms loader upload completed",
                   });
}

TEST_CASE("mock scenario engine: status files lost by chance are the same for the same seed, and not for another",
          "[mock_scenario][engine]")
{
    const Lines first = run_lost_by_chance("20261120");
    CHECK(run_lost_by_chance("20261120") == first);
    CHECK(run_lost_by_chance("20261121") != first);
    // The first answer is held back 187 ms (a number drawn from the stream of step 1). Status files 2 to 6 of the
    // upload come from the stream of step 2: the chance says yes, yes, no, no, yes (the same as the draws of
    // tests/scenario_base for that seed and step). The sixth is the final one, which is never lost.
    CHECK(first == Lines{
                       "0 ms step 1 request 1 upload delay applied 187",
                       "3534 ms step 2 status_file 2 upload drop_status_file applied",
                       "3535 ms step 2 status_file 3 upload drop_status_file applied",
                       "3536 ms step 2 status_file 4 upload drop_status_file not applied",
                       "3536 ms step 2 status_file 5 upload drop_status_file not applied",
                       "3537 ms step 2 status_file 6 upload drop_status_file applied",
                       "3538 ms target ended Upload 0x0003",
                       "6538 ms loader upload completed",
                   });
}

TEST_CASE("mock scenario engine: a refusal, an abort and a silence give the same events every time",
          "[mock_scenario][engine]")
{
    const Lines first = run_refusal_abort_silence();
    CHECK(run_refusal_abort_silence() == first);
    // The Information is refused at the first request as not supported. The first upload is aborted 2500 ms after it
    // began (the target's status 0x1003, which the loader reports). The second upload goes silent at its third status
    // file, and the loader's data load protocol time-out (13 s) ends it.
    CHECK(first == Lines{
                       "0 ms step 1 request 1 information refuse applied 4098",
                       "1 ms target ended Information 0x1002 refused",
                       "3000 ms loader information not supported by the target",
                       "5501 ms step 2 after_ms 2500 upload abort applied",
                       "5502 ms target ended Upload 0x1003",
                       "6502 ms loader upload 1 aborted by the target",
                       "7530 ms step 3 status_file 3 upload silent applied",
                       "21530 ms loader upload 2 data load protocol time-out",
                   });
}

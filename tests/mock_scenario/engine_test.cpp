// SPDX-License-Identifier: GPL-3.0-or-later
// Scenarios run against the loader's real engine (DataLoader and Runner over a fake host) on the fake network.
// The clock is the fake world's and the arithmetic of the draws is integer, so each run leaves exactly the same
// events, with the same times, on every run and on every system: what is written down here is what the mock target
// says happened, and what the loader made of it. Each scenario is run twice.

#include "adl/arinc615a/directory_files.hpp"
#include "adl/mock/scenario_file.hpp"
#include "adl/mock/scenario_player.hpp"
#include "fake_link.hpp"
#include "loader_rig.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

using namespace adl::arinc615a;
using namespace adl::mock;
using namespace adl::test;

namespace
{

using Lines = std::vector<std::string>;

// The link of the fake network, writing down what the target sends: the time, and the kind of datagram with its block.
class RecordingLink final : public FakeTargetLink
{
public:
    using FakeTargetLink::FakeTargetLink;

    void send(std::uint16_t from_port, const adl::arinc615a::Endpoint& to, std::span<const std::byte> payload) override
    {
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now().since_origin).count();
        wire_.push_back(std::to_string(ms) + " ms " + describe(payload));
        FakeTargetLink::send(from_port, to, payload);
    }

    [[nodiscard]] const Lines& wire() const noexcept
    {
        return wire_;
    }

private:
    [[nodiscard]] static std::string describe(std::span<const std::byte> payload)
    {
        if (payload.size() < 2)
        {
            return "short";
        }
        const unsigned opcode = (std::to_integer<unsigned>(payload[0]) << 8U) | std::to_integer<unsigned>(payload[1]);
        const unsigned block =
            payload.size() >= 4 ? (std::to_integer<unsigned>(payload[2]) << 8U) | std::to_integer<unsigned>(payload[3])
                                : 0U;
        switch (opcode)
        {
        case 1:
            return "rrq";
        case 2:
            return "wrq";
        case 3:
            return "data " + std::to_string(block);
        case 4:
            return "ack " + std::to_string(block);
        case 5:
            return "error";
        case 6:
            return "oack";
        default:
            return "opcode " + std::to_string(opcode);
        }
    }

    Lines wire_;
};

// A mock target on the fake network with a scenario, and what happened to it written down as lines of words with their
// times: the steps that fired (as the observer was told), the operations that ended, and how each operation ended for
// the loader.
class Session final : public EngineListener
{
public:
    explicit Session(std::string_view scenario_text, std::uint64_t world_seed = 1, std::vector<HeldFile> holds = {})
        : rig_(world_seed), link_(rig_.world, rig_.network, rig_.target_address)
    {
        auto scenario = read_scenario(scenario_text);
        if (!scenario)
        {
            FAIL(scenario.error().message);
        }
        player_ = std::make_shared<ScenarioPlayer>(
            std::move(*scenario), [this](const StepFired& event) { note_step(event); },
            [this](const DatagramFaulted& hit) { note_datagram(hit); });
        engine_ = std::make_unique<MockEngine>(this);
        link_.attach(*engine_);
        Config config;
        config.fault_plan = player_;
        config.holds = std::move(holds);
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
    // The datagrams the target sent, as they left it.
    [[nodiscard]] const Lines& wire() const noexcept
    {
        return link_.wire();
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

    void note_datagram(const DatagramFaulted& hit)
    {
        std::string line = at() + " step " + std::to_string(hit.step) + " " + std::string{effect_word(hit.effect)} +
                           (hit.applied ? " applied" : " not applied") + " on " + std::string{datagram_word(hit.kind)};
        if (!hit.file.empty())
        {
            line += " " + hit.file;
        }
        if (hit.block)
        {
            line += " block " + std::to_string(*hit.block);
        }
        lines_.push_back(line);
    }

    MockRig rig_;
    RecordingLink link_;
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

// ---- The faults on a datagram -----------------------------------------------------------------

// The upload of a file that takes a few blocks, with the scenario's steps in `steps` (their text, in a file of seed
// 20261120).
Lines run_datagram_upload(std::string_view steps, std::size_t data_bytes = 3000, std::string_view seed = "20261120")
{
    Session session(R"({"kind":"adl-mock-scenario","format":1,"seed":)" + std::string{seed} + R"(,"steps":[)" +
                    std::string{steps} + "]}");
    upload(session, "upload", data_bytes, 21);
    return session.lines();
}

} // namespace

TEST_CASE("mock scenario engine: a slow start gives the same events every time", "[mock_scenario][engine]")
{
    const Lines first = run_slow_start();
    CHECK(run_slow_start() == first);
    // The first request is answered with WAIT:2 at once; the loader asks again two seconds later, and that is the
    // second upload request, whose answer (the option acknowledgement of the initialization file) is held back 300 ms.
    // The target's work starts after its think time (1 s).
    CHECK(first == Lines{
                       "0 ms step 1 request 1 upload wait applied 2",
                       "2000 ms step 2 request 2 upload delay applied 300",
                       "2000 ms step 2 delay applied on option_ack HNPFMS_L.LUI",
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
                       "0 ms step 1 delay applied on option_ack HNPFMS_L.LUI",
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

// ---- The faults on a datagram --------------------------------------------------------------------
//
// An upload of a small load on the fake network, with no scenario, ends at 1009 ms on the target's side and is
// completed for the loader at 4009 ms (the loader asks for the status three seconds after the target is done).

namespace
{

// The part of a run that tells how it ended: the lines about the operation, not about the steps.
Lines ending_of(const Lines& lines)
{
    Lines ending;
    for (const std::string& line : lines)
    {
        if (line.find(" step ") == std::string::npos)
        {
            ending.push_back(line);
        }
    }
    return ending;
}

// A fault by chance on each datagram that matches, as a string of "1" (it was applied) and "0" (it was not), in order.
std::string chance_pattern(const Lines& lines)
{
    std::string pattern;
    for (const std::string& line : lines)
    {
        if (line.find(" on ack ") != std::string::npos)
        {
            pattern += line.find(" not applied ") != std::string::npos ? '0' : '1';
        }
    }
    return pattern;
}

constexpr std::string_view drop_acks_by_chance =
    R"({"when":{"request":1},"on":{"datagram":"ack"},"per_mille":150,"count":65535,"do":{"action":"drop_datagram"}})";

} // namespace

TEST_CASE("mock scenario engine: a datagram that is lost is sent again, and the upload still completes",
          "[mock_scenario][engine][datagram]")
{
    // With no scenario, the upload ends for the target at 1009 ms and is completed for the loader at 4009 ms.
    CHECK(run_datagram_upload("") == Lines{"1009 ms target ended Upload 0x0003", "4009 ms loader upload completed"});

    // The acknowledgement of block 2 of APP.LUP is lost: the loader sends the block again after its time-out (2 s), and
    // the upload ends 2 s later than it would have.
    constexpr std::string_view lost_ack =
        R"({"when":{"request":1},"on":{"datagram":"ack","block":2},"do":{"action":"drop_datagram"}})";
    const Lines first = run_datagram_upload(lost_ack);
    CHECK(run_datagram_upload(lost_ack) == first);
    CHECK(first == Lines{
                       "0 ms step 1 request 1 upload drop_datagram applied",
                       "1004 ms step 1 drop_datagram applied on ack APP.LUP block 2",
                       "3009 ms target ended Upload 0x0003",
                       "6009 ms loader upload completed",
                   });

    // The first block of the status file is lost: the target sends it again after its own time-out.
    const Lines lost_status = run_datagram_upload(
        R"({"when":{"request":1},"on":{"datagram":"data","file":".LUS"},"do":{"action":"drop_datagram"}})");
    CHECK(lost_status == Lines{
                             "0 ms step 1 request 1 upload drop_datagram applied",
                             "1001 ms step 1 drop_datagram applied on data HNPFMS_L.LUS block 1",
                             "3009 ms target ended Upload 0x0003",
                             "6009 ms loader upload completed",
                         });
}

TEST_CASE("mock scenario engine: a datagram that is delayed or sent twice does not change how the upload ends",
          "[mock_scenario][engine][datagram]")
{
    // Three acknowledgements, the first one of each of three files, are 40 ms late: the upload ends 40 ms later.
    const Lines delayed = run_datagram_upload(
        R"({"when":{"request":1},"on":{"datagram":"ack"},"count":3,"do":{"action":"delay","ms":40}})");
    CHECK(delayed == Lines{
                         "0 ms step 1 request 1 upload delay applied 40",
                         "1002 ms step 1 delay applied on ack HNPFMS_L.LUR block 1",
                         "1002 ms step 1 delay applied on ack ACM47.LUH block 0",
                         "1043 ms step 1 delay applied on ack ACM47.LUH block 1",
                         "1049 ms target ended Upload 0x0003",
                         "4049 ms loader upload completed",
                     });

    // An acknowledgement sent three times: the loader takes it in its stride.
    const Lines twice = run_datagram_upload(
        R"({"when":{"request":1},"on":{"datagram":"ack","block":1},"do":{"action":"duplicate_datagram","copies":3}})");
    CHECK(twice == Lines{
                       "0 ms step 1 request 1 upload duplicate_datagram applied 3",
                       "1002 ms step 1 duplicate_datagram applied on ack HNPFMS_L.LUR block 1",
                       "1009 ms target ended Upload 0x0003",
                       "4009 ms loader upload completed",
                   });
}

TEST_CASE("mock scenario engine: a datagram that is held goes after the next one, or after the time it was given",
          "[mock_scenario][engine][datagram]")
{
    // The first status file's block is held for 400 ms and nothing follows it (the target waits for the loader's
    // acknowledgement), so it goes when the time is up. The second status file's block is held in the same way.
    const Lines held = run_datagram_upload(
        R"({"when":{"request":1},"on":{"datagram":"data","file":".LUS"},"count":2,"do":{"action":"reorder_datagram","within_ms":400}})");
    CHECK(held == Lines{
                      "0 ms step 1 request 1 upload reorder_datagram applied 400",
                      "1001 ms step 1 reorder_datagram applied on data HNPFMS_L.LUS block 1",
                      "1406 ms step 1 reorder_datagram applied on data HNPFMS_L.LUS block 1",
                      "1409 ms target ended Upload 0x0003",
                      "4409 ms loader upload completed",
                  });

    // The acknowledgement of the first block of a file, held while the acknowledgements of the next two are sent: they
    // go first, and then it goes. What the target sent, in order, as the engine passed it on:
    Session session(
        R"({"kind":"adl-mock-scenario","format":1,"steps":[
            {"when":{"request":1},"on":{"datagram":"ack","block":1},"do":{"action":"reorder_datagram","within_ms":5000}}]})");
    upload(session, "upload", 3000, 21);
    const Lines wire_held = session.wire();
    Session plain(R"({"kind":"adl-mock-scenario","format":1,"steps":[]})");
    upload(plain, "upload", 3000, 21);
    const Lines wire_plain = plain.wire();
    REQUIRE(wire_held.size() == wire_plain.size()); // nothing is lost, and nothing is made up
    CHECK(wire_held != wire_plain);                 // the order is not the same
    // At 1002 ms the target sends an acknowledgement, a request and another acknowledgement: held, the first goes
    // right after the request, which is the next datagram.
    const auto at_1002 = [](const Lines& wire)
    {
        Lines slice;
        std::ranges::copy_if(wire, std::back_inserter(slice),
                             [](const std::string& line) { return line.starts_with("1002 ms "); });
        return slice;
    };
    CHECK(at_1002(wire_plain) == Lines{"1002 ms ack 1", "1002 ms rrq", "1002 ms ack 0"});
    CHECK(at_1002(wire_held) == Lines{"1002 ms rrq", "1002 ms ack 1", "1002 ms ack 0"});
    auto sorted_without_times = [](Lines wire)
    {
        for (std::string& line : wire)
        {
            line.erase(0, line.find(' ') + 1);
            line.erase(0, line.find(' ') + 1);
        }
        std::ranges::sort(wire);
        return wire;
    };
    CHECK(sorted_without_times(wire_held) == sorted_without_times(wire_plain)); // the same datagrams
}

TEST_CASE("mock scenario engine: when every acknowledgement is lost the loader gives up",
          "[mock_scenario][engine][datagram]")
{
    // The acknowledgements of the first two files are lost every time they are sent, and sent again by the loader every
    // 2 s, until its retries are used up.
    constexpr std::string_view all_lost =
        R"({"when":{"request":1},"on":{"datagram":"ack"},"count":65535,"do":{"action":"drop_datagram"}})";
    const Lines first = run_datagram_upload(all_lost);
    CHECK(run_datagram_upload(all_lost) == first);
    CHECK(ending_of(first) == Lines{"10002 ms loader upload a file transfer failed"});
}

TEST_CASE("mock scenario engine: datagrams lost by chance are the same for the same seed, and not for another",
          "[mock_scenario][engine][datagram]")
{
    const Lines first = run_datagram_upload(drop_acks_by_chance, 30000);
    CHECK(run_datagram_upload(drop_acks_by_chance, 30000) == first);
    const Lines other = run_datagram_upload(drop_acks_by_chance, 30000, "20261121");
    CHECK(other != first);
    // The chance is drawn for each acknowledgement the target sends, from the stream of the step.
    CHECK(chance_pattern(first) == "000100001000101000000000000001100100010000000100111000000100000000000000000"
                                   "0000101000000100000010001001000110000000000000000010001");
    CHECK(chance_pattern(other) == "000000010000000000000000000100100000000000000000000000000100000100000100000"
                                   "0100");
    CHECK(ending_of(first) == Lines{"35044 ms target ended Upload 0x0003", "38044 ms loader upload completed"});
    CHECK(ending_of(other) == Lines{"13031 ms target ended Upload 0x0003", "16030 ms loader upload completed"});
}

TEST_CASE("mock scenario engine: a check value that does not match is reported by the loader and the file is kept",
          "[mock_scenario][engine][datagram]")
{
    // corrupt_served on the first status file of a media download: the file the target serves next has one bit of its
    // middle byte flipped, and the check value is that of the file as it was, so the loader sees a mismatch.
    constexpr std::string_view corrupt =
        R"({"kind":"adl-mock-scenario","format":1,"steps":[
            {"when":{"status_file":1,"operation":"download_media_defined"},"do":{"action":"corrupt_served"}}]})";
    const Bytes original = pattern(2000, 31);
    const auto run_download = [&](Session& session, MemorySink& sink, LoaderRecorder& recorder)
    {
        LoaderServices services;
        services.sink = &sink;
        services.listener = &recorder;
        const std::vector<FileRequest> files = {FileRequest{"F1.BIN", {}}};
        session.run("download", MediaDownloadOperation{DownloadRequest{files}}, services);
    };

    Session session(corrupt, 1, {HeldFile{"F1.BIN", original, ""}});
    MemorySink sink;
    LoaderRecorder recorder;
    run_download(session, sink, recorder);
    CHECK(session.lines() == Lines{
                                 "1001 ms step 1 status_file 1 download_media_defined corrupt_served applied",
                                 "1006 ms target ended Download (media defined) 0x0003",
                                 "4006 ms loader download completed",
                             });
    CHECK(std::ranges::count(recorder.notices, std::pair{Notice::checksum_mismatch, std::string{"F1.BIN"}}) == 1);
    REQUIRE(sink.files.contains("F1.BIN"));
    const Bytes& got = sink.files.at("F1.BIN"); // kept all the same (5.3.2.3.8.5)
    REQUIRE(got.size() == original.size());
    std::vector<std::size_t> differing;
    for (std::size_t i = 0; i < got.size(); ++i)
    {
        if (got[i] != original[i])
        {
            differing.push_back(i);
            CHECK((static_cast<unsigned>(got[i]) ^ static_cast<unsigned>(original[i])) == 0x01U);
        }
    }
    CHECK(differing == std::vector<std::size_t>{1000});

    // Without the step the same download is clean.
    Session clean(R"({"kind":"adl-mock-scenario","format":1,"steps":[]})", 1, {HeldFile{"F1.BIN", original, ""}});
    MemorySink clean_sink;
    LoaderRecorder clean_recorder;
    run_download(clean, clean_sink, clean_recorder);
    CHECK_FALSE(clean_recorder.saw(Notice::checksum_mismatch));
    CHECK(clean_sink.files.at("F1.BIN") == original);
}

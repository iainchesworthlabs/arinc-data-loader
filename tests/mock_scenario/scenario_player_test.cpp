// SPDX-License-Identifier: GPL-3.0-or-later
// The player of a scenario, without an engine: what counts as an occurrence of a step (the nth, then every kth, at most
// `times` of them, for the operation it names), what is drawn from the seed and in which order, and what is told to the
// observer. The engine's use of it is tested against the loader's own engine in engine_test.cpp.

#include "adl/mock/scenario_file.hpp"
#include "adl/mock/scenario_player.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>
#include <vector>

using namespace adl::mock;
using adl::scenario::Trigger;

namespace
{

// A scenario with a seed and the given steps (written as the steps of a file).
Scenario scenario_of(std::string_view steps, std::string_view seed = "20261120")
{
    const std::string text = R"({"kind":"adl-mock-scenario","format":1,"seed":)" + std::string{seed} + R"(,"steps":[)" +
                             std::string{steps} + "]}";
    auto scenario = read_scenario(text);
    if (!scenario)
    {
        FAIL(scenario.error().message);
    }
    return std::move(*scenario);
}

struct Observed
{
    std::vector<StepFired> fired;
    StepObserver observer()
    {
        return [this](const StepFired& event) { fired.push_back(event); };
    }
};

// Asks the player at each request, and says at which of them it gave an answer (1-based).
std::vector<int> requests_answered(ScenarioPlayer& player, int count, OperationKind kind = OperationKind::upload)
{
    std::vector<int> answered;
    for (int n = 1; n <= count; ++n)
    {
        if (!player.on_request(kind).empty())
        {
            answered.push_back(n);
        }
    }
    return answered;
}

} // namespace

TEST_CASE("mock player: a request step fires at its number, then every kth, at most `times` times",
          "[mock_scenario][player]")
{
    Observed seen;
    ScenarioPlayer once(scenario_of(R"({"when":{"request":3},"do":{"action":"wait","seconds":2}})"), seen.observer());
    CHECK(requests_answered(once, 8) == std::vector<int>{3});
    REQUIRE(seen.fired.size() == 1);
    CHECK(seen.fired[0] == StepFired{1, Trigger::request, 3, OperationKind::upload, Effect::wait, true, 2U});

    ScenarioPlayer every(scenario_of(R"({"when":{"request":2,"every":3},"do":{"action":"invalid_init_file"}})"));
    CHECK(requests_answered(every, 12) == std::vector<int>{2, 5, 8, 11});

    ScenarioPlayer limited(
        scenario_of(R"({"when":{"request":2,"every":3,"times":2},"do":{"action":"invalid_init_file"}})"));
    CHECK(requests_answered(limited, 12) == std::vector<int>{2, 5});

    ScenarioPlayer first_only(scenario_of(R"({"when":{"request":1,"times":1},"do":{"action":"invalid_init_file"}})"));
    CHECK(requests_answered(first_only, 5) == std::vector<int>{1});
}

TEST_CASE("mock player: a step for an operation counts only the requests for it", "[mock_scenario][player]")
{
    ScenarioPlayer player(
        scenario_of(R"({"when":{"request":2,"operation":"upload"},"do":{"action":"wait","seconds":1}})"));
    // Information, upload, information, information, upload: the second upload request is the fifth.
    CHECK(player.on_request(OperationKind::information).empty());
    CHECK(player.on_request(OperationKind::upload).empty());
    CHECK(player.on_request(OperationKind::information).empty());
    CHECK(player.on_request(OperationKind::information).empty());
    const std::vector<Action> fifth = player.on_request(OperationKind::upload);
    REQUIRE(fifth.size() == 1);
    CHECK(fifth[0] == Action{Effect::wait, 1});
    CHECK(player.on_request(OperationKind::upload).empty());
}

TEST_CASE("mock player: a status file step counts the files of an operation, whichever operation it is in",
          "[mock_scenario][player]")
{
    ScenarioPlayer player(scenario_of(R"({"when":{"status_file":2},"do":{"action":"drop_status_file"}})"));
    CHECK(player.on_status_file(OperationKind::upload, 1).empty());
    CHECK(player.on_status_file(OperationKind::upload, 2) == std::vector<Action>{Action{Effect::drop_status_file, 0}});
    CHECK(player.on_status_file(OperationKind::upload, 3).empty());
    // The next operation counts from its first file again, and the step fires again in it.
    CHECK(player.on_status_file(OperationKind::information, 1).empty());
    CHECK(player.on_status_file(OperationKind::information, 2).size() == 1);

    // `times` is for the whole run.
    ScenarioPlayer limited(scenario_of(R"({"when":{"status_file":1,"times":1},"do":{"action":"abort"}})"));
    CHECK(limited.on_status_file(OperationKind::upload, 1).size() == 1);
    CHECK(limited.on_status_file(OperationKind::upload, 1).empty());

    // For one operation only.
    ScenarioPlayer filtered(
        scenario_of(R"({"when":{"status_file":1,"operation":"download_media_defined"},"do":{"action":"abort"}})"));
    CHECK(filtered.on_status_file(OperationKind::upload, 1).empty());
    CHECK(filtered.on_status_file(OperationKind::download_media, 1).size() == 1);

    // every
    ScenarioPlayer periodic(scenario_of(R"({"when":{"status_file":2,"every":2},"do":{"action":"drop_status_file"}})"));
    std::vector<std::size_t> numbers;
    for (std::size_t n = 1; n <= 9; ++n)
    {
        if (!periodic.on_status_file(OperationKind::upload, n).empty())
        {
            numbers.push_back(n);
        }
    }
    CHECK(numbers == std::vector<std::size_t>{2, 4, 6, 8});
}

TEST_CASE("mock player: a time step is set for when the operation began, and again every k milliseconds",
          "[mock_scenario][player]")
{
    Observed seen;
    ScenarioPlayer player(scenario_of(R"({"when":{"status_file":1},"do":{"action":"abort"}},
                                         {"when":{"after_ms":1500,"every":500,"times":3},"do":{"action":"silent"}},
                                         {"when":{"after_ms":0,"operation":"download_operator_defined"},"do":{"action":"wrong_checksum"}})"),
                          seen.observer());
    const std::vector<TimedStep> began = player.on_operation_began(OperationKind::upload);
    REQUIRE(began.size() == 1); // the third step is for another operation
    CHECK(began[0].id == 1);
    CHECK(began[0].after == std::chrono::milliseconds(1500));

    TimeAnswer first = player.on_time(1, OperationKind::upload);
    CHECK(first.actions == std::vector<Action>{Action{Effect::silent, 0}});
    CHECK(first.again == std::chrono::milliseconds(500));
    TimeAnswer second = player.on_time(1, OperationKind::upload);
    CHECK(second.again == std::chrono::milliseconds(500));
    TimeAnswer third = player.on_time(1, OperationKind::upload);
    CHECK(third.actions.size() == 1);
    CHECK(!third.again); // three occurrences were all it was asked for
    CHECK(player.on_time(1, OperationKind::upload).actions.empty());

    REQUIRE(seen.fired.size() == 3);
    CHECK(seen.fired[0].at == 1500);
    CHECK(seen.fired[1].at == 2000);
    CHECK(seen.fired[2].at == 2500);
    CHECK(seen.fired[0].trigger == Trigger::after_ms);

    // A step for the operation that began is returned, at its time; an id that is not a step is answered with nothing.
    const std::vector<TimedStep> download = player.on_operation_began(OperationKind::download_operator);
    REQUIRE(download.size() == 1);
    CHECK(download[0].id == 2);
    CHECK(download[0].after == std::chrono::milliseconds(0));
    CHECK(player.on_time(99, OperationKind::upload).actions.empty());
}

TEST_CASE("mock player: a chance is drawn from the stream of its own step", "[mock_scenario][player]")
{
    // Step 3 with a chance of 500 per mille, in a file whose seed is 20261120: the pattern is the one that the
    // shared generator gives step 3 (tests/scenario_base), on every system.
    Observed seen;
    ScenarioPlayer player(scenario_of(R"({"when":{"request":1},"do":{"action":"wait","seconds":1}},
                                         {"when":{"request":1},"do":{"action":"wait","seconds":1}},
                                         {"when":{"request":1,"every":1},"per_mille":500,"do":{"action":"invalid_init_file"}})"),
                          seen.observer());
    for (int i = 0; i < 20; ++i)
    {
        static_cast<void>(player.on_request(OperationKind::upload));
    }
    std::string pattern;
    for (const StepFired& event : seen.fired)
    {
        if (event.step == 3)
        {
            pattern += event.applied ? '1' : '0';
        }
    }
    CHECK(pattern == "01011111001111001111");
    // A step whose chance says no (the first draw of step 1 for seed 1 is 159 of 1000, and the chance is 1) is told
    // to the observer and asks for nothing.
    Observed unlucky;
    ScenarioPlayer never_applied(
        scenario_of(R"({"when":{"request":1},"per_mille":1,"do":{"action":"invalid_init_file"}})", "1"),
        unlucky.observer());
    CHECK(never_applied.on_request(OperationKind::upload).empty());
    REQUIRE(unlucky.fired.size() == 1);
    CHECK(!unlucky.fired[0].applied);
}

TEST_CASE("mock player: a range of delays is drawn after the chance, and the draws do not move when it does not apply",
          "[mock_scenario][player]")
{
    // Step 2: a range with no chance. Its stream is the one of step 2.
    Observed seen;
    ScenarioPlayer ranged(scenario_of(R"({"when":{"request":9},"do":{"action":"wait","seconds":1}},
                                         {"when":{"request":1,"every":1},"do":{"action":"delay","min_ms":200,"max_ms":900}})"),
                          seen.observer());
    std::vector<std::uint32_t> delays;
    for (int i = 0; i < 4; ++i)
    {
        // A delay is armed by its trigger and held to the next datagram, whose fate is the delay that was drawn.
        CHECK(ranged.on_request(OperationKind::upload).empty());
        const DatagramFate fate = ranged.on_datagram(DatagramInfo{});
        REQUIRE(fate.kind == DatagramFate::Kind::delay);
        delays.push_back(fate.amount);
    }
    CHECK(delays == std::vector<std::uint32_t>{495, 790, 895, 517});
    REQUIRE(seen.fired.size() == 4);
    CHECK(seen.fired[0].value == 495U);
    CHECK(seen.fired[0].effect == Effect::delay);

    // Step 5 with both: the chance is drawn first and then the delay, and both are drawn whether or not the chance
    // says yes. The first occurrence applies with 112; the next three do not, and their delays would have been 143,
    // 106 and 174.
    Observed both;
    std::string steps;
    for (int i = 1; i <= 4; ++i)
    {
        steps += R"({"when":{"request":99},"do":{"action":"wait","seconds":1}},)";
    }
    steps += R"({"when":{"request":1,"every":1},"per_mille":500,"do":{"action":"delay","min_ms":100,"max_ms":200}})";
    ScenarioPlayer player(scenario_of(steps), both.observer());
    for (int i = 0; i < 4; ++i)
    {
        static_cast<void>(player.on_request(OperationKind::information));
    }
    REQUIRE(both.fired.size() == 4);
    CHECK(both.fired[0].applied);
    CHECK(both.fired[0].value == 112U);
    CHECK(!both.fired[1].applied);
    CHECK(both.fired[1].value == 143U);
    CHECK(both.fired[2].value == 106U);
    CHECK(both.fired[3].value == 174U);
}

TEST_CASE("mock player: the values the observer is told", "[mock_scenario][player]")
{
    Observed seen;
    ScenarioPlayer player(scenario_of(R"({"when":{"request":1},"do":{"action":"refuse","code":"0x1002"}},
                                         {"when":{"request":1},"do":{"action":"corrupt_served"}})"),
                          seen.observer());
    const std::vector<Action> actions = player.on_request(OperationKind::download_media);
    CHECK(actions == std::vector<Action>{Action{Effect::refuse, 0x1002}, Action{Effect::corrupt_served, 0}});
    REQUIRE(seen.fired.size() == 2);
    CHECK(seen.fired[0] ==
          StepFired{1, Trigger::request, 1, OperationKind::download_media, Effect::refuse, true, 0x1002U});
    CHECK(seen.fired[1] ==
          StepFired{2, Trigger::request, 1, OperationKind::download_media, Effect::corrupt_served, true, std::nullopt});
}

TEST_CASE("mock player: the steps that never fired are told at the end", "[mock_scenario][player]")
{
    ScenarioPlayer player(scenario_of(R"({"when":{"request":1},"do":{"action":"wait","seconds":1}},
                                         {"when":{"request":50},"do":{"action":"wait","seconds":1}},
                                         {"when":{"status_file":1},"do":{"action":"abort"}},
                                         {"when":{"after_ms":9},"do":{"action":"abort"}})"));
    CHECK(player.unused_steps() == std::vector<std::size_t>{1, 2, 3, 4});
    static_cast<void>(player.on_request(OperationKind::upload));
    static_cast<void>(player.on_status_file(OperationKind::upload, 1));
    CHECK(player.unused_steps() == std::vector<std::size_t>{2, 4});
    // A step whose chance said no did occur.
    ScenarioPlayer chance(
        scenario_of(R"({"when":{"request":1},"per_mille":1,"do":{"action":"invalid_init_file"}})", "3"));
    static_cast<void>(chance.on_request(OperationKind::upload));
    CHECK(chance.unused_steps().empty());
}

// ---- The faults on a datagram ------------------------------------------------------------------------------------

namespace
{

DatagramInfo datagram(DatagramKind kind, std::uint32_t block = 0, std::string_view file = {})
{
    DatagramInfo info;
    info.kind = kind;
    info.block = block;
    info.file = file;
    info.from_port = 40001;
    info.to = adl::arinc615a::Endpoint{adl::arinc615a::Address{{10, 0, 0, 1}}, 59};
    info.size = 20;
    return info;
}

constexpr DatagramFate sent{DatagramFate::Kind::send, 0};

struct ObservedDatagrams
{
    std::vector<DatagramFaulted> hits;
    DatagramObserver observer()
    {
        return [this](const DatagramFaulted& hit) { hits.push_back(hit); };
    }
};

} // namespace

TEST_CASE("mock player: a fault on a datagram is armed by its trigger and acts on a datagram that matches, once",
          "[mock_scenario][player][datagram]")
{
    Observed seen;
    ObservedDatagrams hits;
    ScenarioPlayer player(
        scenario_of(R"({"when":{"request":1},"on":{"datagram":"data","block":1},"do":{"action":"drop_datagram"}})"),
        seen.observer(), hits.observer());
    // Before the trigger it does nothing.
    CHECK(player.on_datagram(datagram(DatagramKind::data, 1)) == sent);
    CHECK(player.on_request(OperationKind::upload).empty()); // armed, and asks nothing of the engine
    REQUIRE(seen.fired.size() == 1);
    CHECK(seen.fired[0].effect == Effect::drop_datagram);
    CHECK(seen.fired[0].count == 1);
    REQUIRE(seen.fired[0].on.has_value());
    CHECK(seen.fired[0].on->datagram == DatagramKind::data);
    CHECK(seen.fired[0].on->block == 1U);
    // An acknowledgement of block 1, and a data datagram of block 2, are not it.
    CHECK(player.on_datagram(datagram(DatagramKind::ack, 1)) == sent);
    CHECK(player.on_datagram(datagram(DatagramKind::data, 2)) == sent);
    CHECK(hits.hits.empty());
    // Block 1 of data is, once.
    CHECK(player.on_datagram(datagram(DatagramKind::data, 1, "HNPFMS_L.LUS")) ==
          DatagramFate{DatagramFate::Kind::drop, 0});
    REQUIRE(hits.hits.size() == 1);
    CHECK(hits.hits[0].step == 1);
    CHECK(hits.hits[0].effect == Effect::drop_datagram);
    CHECK(hits.hits[0].applied);
    CHECK(hits.hits[0].kind == DatagramKind::data);
    CHECK(hits.hits[0].file == "HNPFMS_L.LUS");
    CHECK(hits.hits[0].block == 1U);
    CHECK(hits.hits[0].to == "10.0.0.1:59");
    CHECK(hits.hits[0].size == 20);
    CHECK(player.on_datagram(datagram(DatagramKind::data, 1)) == sent); // the count was 1
    CHECK(player.idle_steps().empty());
}

TEST_CASE("mock player: count is how many datagrams, and an armed step is armed again by its next occurrence",
          "[mock_scenario][player][datagram]")
{
    ScenarioPlayer player(
        scenario_of(R"({"when":{"status_file":1,"every":1},"count":2,"do":{"action":"drop_datagram"}})"));
    static_cast<void>(player.on_status_file(OperationKind::upload, 1));
    const DatagramFate drop{DatagramFate::Kind::drop, 0};
    CHECK(player.on_datagram(datagram(DatagramKind::data, 1)) == drop);
    CHECK(player.on_datagram(datagram(DatagramKind::ack, 7)) == drop);
    CHECK(player.on_datagram(datagram(DatagramKind::data, 1)) == sent);
    // The step occurs again, and starts its count again.
    static_cast<void>(player.on_status_file(OperationKind::upload, 2));
    CHECK(player.on_datagram(datagram(DatagramKind::data, 1)) == drop);
    CHECK(player.on_datagram(datagram(DatagramKind::data, 1)) == drop);
    CHECK(player.on_datagram(datagram(DatagramKind::data, 1)) == sent);
}

TEST_CASE("mock player: the fates of the four faults on a datagram", "[mock_scenario][player][datagram]")
{
    ScenarioPlayer player(scenario_of(R"({"when":{"request":1},"do":{"action":"duplicate_datagram","copies":3}},
                                          {"when":{"request":2},"do":{"action":"reorder_datagram","within_ms":250}},
                                          {"when":{"request":3},"do":{"action":"delay","ms":400}},
                                          {"when":{"request":4},"do":{"action":"duplicate_datagram"}},
                                          {"when":{"request":5},"do":{"action":"reorder_datagram"}})"));
    const auto next = [&]
    {
        static_cast<void>(player.on_request(OperationKind::upload));
        return player.on_datagram(datagram(DatagramKind::request));
    };
    CHECK(next() == DatagramFate{DatagramFate::Kind::duplicate, 3});
    CHECK(next() == DatagramFate{DatagramFate::Kind::hold, 250});
    CHECK(next() == DatagramFate{DatagramFate::Kind::delay, 400});
    CHECK(next() == DatagramFate{DatagramFate::Kind::duplicate, 2}); // the default of copies
    CHECK(next() == DatagramFate{DatagramFate::Kind::hold, 1000});   // the default of within_ms
}

TEST_CASE("mock player: a selector names a kind, a block, a file or an extension, and all that are given have to match",
          "[mock_scenario][player][datagram]")
{
    const auto fate = [](std::string_view on, const DatagramInfo& info)
    {
        ScenarioPlayer player(
            scenario_of(R"({"when":{"request":1},"on":)" + std::string{on} + R"(,"do":{"action":"drop_datagram"}})"));
        static_cast<void>(player.on_request(OperationKind::upload));
        return player.on_datagram(info).kind == DatagramFate::Kind::drop;
    };
    // A file by name, in any case, and by extension.
    CHECK(fate(R"({"file":"LOAD1.BIN"})", datagram(DatagramKind::data, 1, "load1.bin")));
    CHECK(!fate(R"({"file":"LOAD1.BIN"})", datagram(DatagramKind::data, 1, "LOAD2.BIN")));
    CHECK(!fate(R"({"file":"LOAD1.BIN"})", datagram(DatagramKind::data, 1)));
    CHECK(fate(R"({"file":".LUS"})", datagram(DatagramKind::request, 0, "HNPFMS_L.LUS")));
    CHECK(fate(R"({"file":".lus"})", datagram(DatagramKind::request, 0, "HNPFMS_L.LUS")));
    CHECK(!fate(R"({"file":".LUS"})", datagram(DatagramKind::request, 0, "HNPFMS_L.LUR")));
    CHECK(!fate(R"({"file":".LUS"})",
                datagram(DatagramKind::request, 0, ".LUS"))); // an extension of a name, not the name
    // A kind, and `any`.
    CHECK(fate(R"({"datagram":"option_ack"})", datagram(DatagramKind::option_ack)));
    CHECK(!fate(R"({"datagram":"option_ack"})", datagram(DatagramKind::ack)));
    CHECK(fate(R"({"datagram":"error"})", datagram(DatagramKind::error)));
    CHECK(fate(R"({"datagram":"any"})", datagram(DatagramKind::other)));
    CHECK(fate(R"({})", datagram(DatagramKind::other)));
    CHECK(!fate(R"({"datagram":"request"})", datagram(DatagramKind::other)));
    // A block is a data or an ack datagram with that number, with a kind or without one.
    CHECK(fate(R"({"block":3})", datagram(DatagramKind::data, 3)));
    CHECK(fate(R"({"block":3})", datagram(DatagramKind::ack, 3)));
    CHECK(!fate(R"({"block":3})", datagram(DatagramKind::ack, 4)));
    CHECK(!fate(R"({"block":0})", datagram(DatagramKind::option_ack)));
    CHECK(fate(R"({"block":0})", datagram(DatagramKind::ack, 0)));
    // All given have to match.
    CHECK(fate(R"({"datagram":"ack","block":1,"file":".LUR"})", datagram(DatagramKind::ack, 1, "X.LUR")));
    CHECK(!fate(R"({"datagram":"ack","block":1,"file":".LUR"})", datagram(DatagramKind::data, 1, "X.LUR")));
    CHECK(!fate(R"({"datagram":"ack","block":1,"file":".LUR"})", datagram(DatagramKind::ack, 1, "X.LUS")));
}

TEST_CASE("mock player: of several steps armed together, the first that matches acts, and one acts on a datagram",
          "[mock_scenario][player][datagram]")
{
    ScenarioPlayer player(
        scenario_of(R"({"when":{"request":1},"on":{"datagram":"ack"},"do":{"action":"duplicate_datagram","copies":4}},
                                          {"when":{"request":1},"do":{"action":"drop_datagram"}},
                                          {"when":{"request":1},"do":{"action":"delay","ms":9}})"));
    static_cast<void>(player.on_request(OperationKind::upload));
    // An ack: step 1 acts. Then a data datagram: step 1 does not match, step 2 acts. Then anything: step 3.
    CHECK(player.on_datagram(datagram(DatagramKind::ack, 1)) == DatagramFate{DatagramFate::Kind::duplicate, 4});
    CHECK(player.on_datagram(datagram(DatagramKind::data, 1)) == DatagramFate{DatagramFate::Kind::drop, 0});
    CHECK(player.on_datagram(datagram(DatagramKind::data, 2)) == DatagramFate{DatagramFate::Kind::delay, 9});
    CHECK(player.on_datagram(datagram(DatagramKind::data, 3)) == sent);
}

TEST_CASE(
    "mock player: the chance of a fault on a datagram is drawn for each datagram that matches, from its own stream",
    "[mock_scenario][player][datagram]")
{
    // Step 3 with a chance of 500 per mille in a file whose seed is 20261120: the same pattern as the shared generator
    // gives the step (tests/scenario_base), now one draw for each datagram, and none when the trigger occurs.
    ObservedDatagrams hits;
    ScenarioPlayer player(scenario_of(R"({"when":{"request":98},"do":{"action":"wait","seconds":1}},
                       {"when":{"request":99},"do":{"action":"wait","seconds":1}},
                       {"when":{"request":1},"per_mille":500,"count":65535,"do":{"action":"drop_datagram"}})"),
                          {}, hits.observer());
    Observed arming;
    static_cast<void>(player.on_request(OperationKind::upload));
    std::string pattern;
    for (int i = 0; i < 20; ++i)
    {
        pattern += player.on_datagram(datagram(DatagramKind::data, 1)).kind == DatagramFate::Kind::drop ? '1' : '0';
    }
    CHECK(pattern == "01011111001111001111");
    // Every datagram it met is told, with whether the chance applied; only the ones that applied counted.
    REQUIRE(hits.hits.size() == 20);
    std::string told;
    for (const DatagramFaulted& hit : hits.hits)
    {
        told += hit.applied ? '1' : '0';
    }
    CHECK(told == pattern);
}

TEST_CASE("mock player: a delay keeps its draw at the trigger, and a datagram fault is dropped when the operation ends",
          "[mock_scenario][player][datagram]")
{
    Observed seen;
    ScenarioPlayer player(scenario_of(R"({"when":{"request":9},"do":{"action":"wait","seconds":1}},
                                          {"when":{"request":1,"every":1},"do":{"action":"delay","min_ms":200,"max_ms":900},"count":2},
                                          {"when":{"request":1},"do":{"action":"drop_datagram"}})"),
                          seen.observer());
    static_cast<void>(player.on_request(OperationKind::upload));
    REQUIRE(seen.fired.size() == 2);
    CHECK(seen.fired[0].value == 495U); // drawn when the trigger occurred, as the stream of step 2 gives
    CHECK(seen.fired[0].count == 2);
    CHECK(!seen.fired[1].value.has_value());
    CHECK(player.on_datagram(datagram(DatagramKind::data, 1)) == DatagramFate{DatagramFate::Kind::delay, 495});
    // The end of an operation drops what is armed.
    player.on_operation_ended();
    CHECK(player.on_datagram(datagram(DatagramKind::data, 1)) == sent);
    // Armed again, the delay is what the trigger drew the second time.
    static_cast<void>(player.on_request(OperationKind::upload));
    REQUIRE(seen.fired.size() == 3);
    REQUIRE(seen.fired[2].value.has_value());
    CHECK(*seen.fired[2].value != *seen.fired[0].value);
    CHECK(*seen.fired[2].value >= 200U);
    CHECK(*seen.fired[2].value <= 900U);
    CHECK(player.on_datagram(datagram(DatagramKind::data, 1)) ==
          DatagramFate{DatagramFate::Kind::delay, *seen.fired[2].value});
}

TEST_CASE("mock player: a step on a datagram that was armed and met none is idle", "[mock_scenario][player][datagram]")
{
    ScenarioPlayer player(
        scenario_of(R"({"when":{"request":1},"on":{"file":"NOPE.BIN"},"do":{"action":"drop_datagram"}},
                                          {"when":{"request":1},"do":{"action":"drop_datagram"}},
                                          {"when":{"request":77},"do":{"action":"drop_datagram"}},
                                          {"when":{"request":1},"do":{"action":"wait","seconds":1}})"));
    static_cast<void>(player.on_request(OperationKind::upload));
    CHECK(player.idle_steps() == std::vector<std::size_t>{1, 2}); // step 3 never occurred: it is unused, not idle
    static_cast<void>(player.on_datagram(datagram(DatagramKind::data, 1)));
    CHECK(player.idle_steps() == std::vector<std::size_t>{1});
    CHECK(player.unused_steps() == std::vector<std::size_t>{3});
}

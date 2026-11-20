// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// What the fuzzer of the mock target's scenario file (libs/mock_scenario/fuzz) and the test that replays its seeds
// share: the bytes as a scenario file, and what must hold of whatever the reader makes of them. The input is the
// file itself.

#include "adl/mock/scenario_file.hpp"
#include "adl/mock/scenario_player.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace adl::test
{

// What must hold of a scenario that was read: every number is inside its limit, an action goes with its trigger, a
// step that draws has a seed, and the steps are numbered from 1.
[[nodiscard]] inline bool scenario_holds(const adl::mock::Scenario& scenario)
{
    using namespace adl::mock;
    using adl::scenario::Trigger;
    bool holds = scenario.steps.size() <= adl::scenario::max_steps;
    std::size_t number = 0;
    for (const ScenarioStep& step : scenario.steps)
    {
        ++number;
        const ScenarioAction& action = step.action;
        const bool for_request = action.effect == Effect::wait || action.effect == Effect::refuse ||
                                 action.effect == Effect::invalid_init_file;
        const bool for_later = action.effect == Effect::drop_status_file || action.effect == Effect::abort ||
                               action.effect == Effect::silent || action.effect == Effect::wrong_checksum;
        holds = holds && step.number == number && (!for_request || step.trigger == Trigger::request) &&
                (!for_later || step.trigger != Trigger::request) && (!step.per_mille || scenario.seed.has_value()) &&
                (!action.ranged || scenario.seed.has_value()) && action.min_ms <= action.max_ms &&
                action.max_ms <= 60000 &&
                (action.effect != Effect::wait || (action.seconds >= 1 && action.seconds <= 65535)) &&
                (action.effect != Effect::refuse || action.code == 0x1000U || action.code == 0x1002U) &&
                (action.effect != Effect::delay || action.max_ms >= 1 || action.ranged) &&
                (action.effect != Effect::duplicate_datagram || (action.copies >= 2 && action.copies <= 16)) &&
                (action.effect != Effect::reorder_datagram || (action.within_ms >= 1 && action.within_ms <= 60000)) &&
                (!step.count || (is_datagram_effect(action.effect) && *step.count >= 1 && *step.count <= 65535)) &&
                (!step.on || (is_datagram_effect(action.effect) && (!step.on->block || *step.on->block <= 65535) &&
                              (!step.on->block || !step.on->datagram || *step.on->datagram == DatagramKind::data ||
                               *step.on->datagram == DatagramKind::ack))) &&
                !effect_word(action.effect).empty();
    }
    return holds;
}

// Reads the input as a scenario file. A file that is refused says why in one line; one that is read holds what
// scenario_holds checks, is written and read again as the same scenario (and the written text again as the same
// text), and can be played: the player is asked at a few moments and does not fail.
[[nodiscard]] inline bool fuzz_mock_scenario(const std::uint8_t* data, std::size_t size)
{
    using namespace adl::mock;
    const std::string_view text{size == 0 ? "" : reinterpret_cast<const char*>(data), size};
    const auto scenario = read_scenario(text);
    if (!scenario)
    {
        return !scenario.error().message.empty() && scenario.error().message.find('\n') == std::string::npos;
    }
    bool holds = scenario_holds(*scenario);

    const std::string written = write_scenario(*scenario);
    const auto again = read_scenario(written);
    holds = holds && again.has_value() && *again == *scenario && write_scenario(*again) == written;

    std::size_t fired = 0;
    ScenarioPlayer player(*scenario, [&fired](const StepFired&) { ++fired; });
    for (const OperationKind kind : {OperationKind::information, OperationKind::upload, OperationKind::download_media})
    {
        for (int i = 0; i < 3; ++i)
        {
            static_cast<void>(player.on_request(kind));
        }
        for (std::size_t number = 1; number <= 3; ++number)
        {
            static_cast<void>(player.on_status_file(kind, number));
        }
        for (const DatagramKind datagram : {DatagramKind::request, DatagramKind::option_ack, DatagramKind::data,
                                            DatagramKind::ack, DatagramKind::error, DatagramKind::other})
        {
            DatagramInfo info;
            info.kind = datagram;
            info.block = 1;
            info.file = "HNPFMS_L.LUS";
            static_cast<void>(player.on_datagram(info));
        }
        for (const TimedStep& step : player.on_operation_began(kind))
        {
            for (int i = 0; i < 3; ++i)
            {
                static_cast<void>(player.on_time(step.id, kind));
            }
        }
    }
    player.on_operation_ended();
    holds = holds && player.unused_steps().size() <= scenario->steps.size() &&
            player.idle_steps().size() <= scenario->steps.size();
    return holds;
}

} // namespace adl::test

// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/mock/scenario_player.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <utility>

namespace adl::mock
{

namespace
{

using adl::scenario::Trigger;

// Whether the nth occurrence (counted from 1) of what the step counts is one of its own: the first, and then
// every `every` after that.
[[nodiscard]] bool is_occurrence(const ScenarioStep& step, std::int64_t n) noexcept
{
    if (n == step.at)
    {
        return true;
    }
    return step.every && n > step.at && (n - step.at) % *step.every == 0;
}

[[nodiscard]] adl::arinc615a::Duration milliseconds(std::int64_t count) noexcept
{
    return std::chrono::duration_cast<adl::arinc615a::Duration>(std::chrono::milliseconds(count));
}

[[nodiscard]] bool same_text_ignoring_case(std::string_view a, std::string_view b) noexcept
{
    return std::ranges::equal(
        a, b, [](char x, char y)
        { return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y)); });
}

// Whether a datagram is one that a selector names. A file given as ".LUS" (a leading dot) is any file of that
// extension.
[[nodiscard]] bool matches(const std::optional<DatagramSelector>& on, const DatagramInfo& datagram)
{
    if (!on)
    {
        return true;
    }
    if (on->datagram && *on->datagram != datagram.kind)
    {
        return false;
    }
    if (on->block &&
        !((datagram.kind == DatagramKind::data || datagram.kind == DatagramKind::ack) && datagram.block == *on->block))
    {
        return false;
    }
    if (!on->file.empty())
    {
        if (datagram.file.empty())
        {
            return false;
        }
        if (on->file.front() == '.')
        {
            return datagram.file.size() > on->file.size() &&
                   same_text_ignoring_case(datagram.file.substr(datagram.file.size() - on->file.size()), on->file);
        }
        return same_text_ignoring_case(datagram.file, on->file);
    }
    return true;
}

// "10.0.0.2:40001".
[[nodiscard]] std::string endpoint_text(const adl::arinc615a::Endpoint& endpoint)
{
    std::string text;
    for (std::size_t i = 0; i < endpoint.address.octets.size(); ++i)
    {
        text += (i == 0 ? "" : ".") + std::to_string(endpoint.address.octets[i]);
    }
    return text + ":" + std::to_string(endpoint.port);
}

} // namespace

ScenarioPlayer::ScenarioPlayer(Scenario scenario, StepObserver observer, DatagramObserver datagram_observer)
    : scenario_(std::move(scenario)), observer_(std::move(observer)), datagram_observer_(std::move(datagram_observer))
{
    // Step s (from 1) has the stream that starts from the s-th output of the file's own generator. A file with no
    // seed has no step that draws, so what its steps start from does not matter.
    const std::uint64_t seed = scenario_.seed.value_or(0);
    states_.reserve(scenario_.steps.size());
    for (const ScenarioStep& step : scenario_.steps)
    {
        states_.push_back(State{adl::scenario::step_stream(seed, step.number), 0, 0, 0, 0, 0, 0});
    }
}

bool ScenarioPlayer::spent(const ScenarioStep& step, const State& state) const noexcept
{
    return step.times && state.occurrences >= *step.times;
}

bool ScenarioPlayer::takes_part(const ScenarioStep& step, OperationKind kind) const noexcept
{
    return !step.operation || *step.operation == kind;
}

std::vector<Action> ScenarioPlayer::fire(std::size_t index, OperationKind kind, std::int64_t at)
{
    const ScenarioStep& step = scenario_.steps[index];
    State& state = states_[index];
    ++state.occurrences;

    // The draws are made whether or not they are used, in the same order, so that the numbers of a step depend on
    // how many times it has occurred and on nothing else.
    // (For a fault on a datagram, other than a delay, the chance is drawn for each datagram that matches.)
    const bool on_datagram = is_datagram_effect(step.action.effect);
    const bool per_datagram = on_datagram && step.action.effect != Effect::delay;
    const bool applied = !step.per_mille || per_datagram || state.stream.chance(static_cast<unsigned>(*step.per_mille));
    std::uint32_t delay = step.action.min_ms;
    if (step.action.ranged)
    {
        delay = static_cast<std::uint32_t>(state.stream.between(step.action.min_ms, step.action.max_ms));
    }

    Action action{step.action.effect, 0};
    std::optional<std::uint32_t> value;
    switch (step.action.effect)
    {
    case Effect::wait:
        action.value = value.emplace(step.action.seconds);
        break;
    case Effect::refuse:
        action.value = value.emplace(step.action.code);
        break;
    case Effect::delay:
        value.emplace(delay);
        break;
    case Effect::duplicate_datagram:
        value.emplace(step.action.copies);
        break;
    case Effect::reorder_datagram:
        value.emplace(step.action.within_ms);
        break;
    default:
        break;
    }
    if (observer_)
    {
        StepFired fired{step.number, step.trigger, at, kind, step.action.effect, applied, value};
        if (on_datagram)
        {
            fired.count = step.count.value_or(1);
            fired.on = step.on;
        }
        observer_(fired);
    }
    if (on_datagram)
    {
        // Armed: it acts on the datagrams that follow. Armed again, it starts its count again.
        if (applied)
        {
            state.armed = step.count.value_or(1);
            state.delay_ms = delay;
        }
        return {};
    }
    return applied ? std::vector<Action>{action} : std::vector<Action>{};
}

std::vector<Action> ScenarioPlayer::on_request(OperationKind kind)
{
    std::vector<Action> actions;
    for (std::size_t i = 0; i < scenario_.steps.size(); ++i)
    {
        const ScenarioStep& step = scenario_.steps[i];
        if (step.trigger != Trigger::request || !takes_part(step, kind))
        {
            continue;
        }
        State& state = states_[i];
        ++state.seen;
        if (!spent(step, state) && is_occurrence(step, state.seen))
        {
            for (const Action& action : fire(i, kind, state.seen))
            {
                actions.push_back(action);
            }
        }
    }
    return actions;
}

std::vector<Action> ScenarioPlayer::on_status_file(OperationKind kind, std::size_t number)
{
    std::vector<Action> actions;
    for (std::size_t i = 0; i < scenario_.steps.size(); ++i)
    {
        const ScenarioStep& step = scenario_.steps[i];
        if (step.trigger != Trigger::status_file || !takes_part(step, kind) || spent(step, states_[i]) ||
            !is_occurrence(step, static_cast<std::int64_t>(number)))
        {
            continue;
        }
        for (const Action& action : fire(i, kind, static_cast<std::int64_t>(number)))
        {
            actions.push_back(action);
        }
    }
    return actions;
}

std::vector<TimedStep> ScenarioPlayer::on_operation_began(OperationKind kind)
{
    std::vector<TimedStep> timed;
    for (std::size_t i = 0; i < scenario_.steps.size(); ++i)
    {
        const ScenarioStep& step = scenario_.steps[i];
        if (step.trigger != Trigger::after_ms || !takes_part(step, kind) || spent(step, states_[i]))
        {
            continue;
        }
        states_[i].scheduled = step.at;
        timed.push_back(TimedStep{i, milliseconds(step.at)});
    }
    return timed;
}

TimeAnswer ScenarioPlayer::on_time(std::size_t id, OperationKind kind)
{
    TimeAnswer answer;
    if (id >= scenario_.steps.size())
    {
        return answer;
    }
    const ScenarioStep& step = scenario_.steps[id];
    State& state = states_[id];
    if (spent(step, state))
    {
        return answer;
    }
    answer.actions = fire(id, kind, state.scheduled);
    if (step.every && !spent(step, state))
    {
        state.scheduled += *step.every;
        answer.again = milliseconds(*step.every);
    }
    return answer;
}

DatagramFate ScenarioPlayer::on_datagram(const DatagramInfo& datagram)
{
    for (std::size_t i = 0; i < scenario_.steps.size(); ++i)
    {
        const ScenarioStep& step = scenario_.steps[i];
        State& state = states_[i];
        if (state.armed <= 0 || !is_datagram_effect(step.action.effect) || !matches(step.on, datagram))
        {
            continue;
        }
        ++state.matched;
        // The chance of every fault on a datagram but a delay is drawn here, for each datagram that matches.
        const bool applied = !step.per_mille || step.action.effect == Effect::delay ||
                             state.stream.chance(static_cast<unsigned>(*step.per_mille));
        if (datagram_observer_)
        {
            DatagramFaulted faulted;
            faulted.step = step.number;
            faulted.effect = step.action.effect;
            faulted.applied = applied;
            faulted.kind = datagram.kind;
            faulted.file = std::string{datagram.file};
            if (datagram.kind == DatagramKind::data || datagram.kind == DatagramKind::ack)
            {
                faulted.block = datagram.block;
            }
            faulted.to = endpoint_text(datagram.to);
            faulted.size = datagram.size;
            datagram_observer_(faulted);
        }
        if (!applied)
        {
            continue; // this step does not act on this datagram, and a later one may
        }
        --state.armed;
        switch (step.action.effect)
        {
        case Effect::drop_datagram:
            return DatagramFate{DatagramFate::Kind::drop, 0};
        case Effect::duplicate_datagram:
            return DatagramFate{DatagramFate::Kind::duplicate, step.action.copies};
        case Effect::reorder_datagram:
            return DatagramFate{DatagramFate::Kind::hold, step.action.within_ms};
        default:
            return DatagramFate{DatagramFate::Kind::delay, state.delay_ms};
        }
    }
    return {};
}

void ScenarioPlayer::on_operation_ended()
{
    for (State& state : states_)
    {
        state.armed = 0;
    }
}

std::vector<std::size_t> ScenarioPlayer::idle_steps() const
{
    std::vector<std::size_t> idle;
    for (std::size_t i = 0; i < scenario_.steps.size(); ++i)
    {
        if (is_datagram_effect(scenario_.steps[i].action.effect) && states_[i].occurrences > 0 &&
            states_[i].matched == 0)
        {
            idle.push_back(scenario_.steps[i].number);
        }
    }
    return idle;
}

std::vector<std::size_t> ScenarioPlayer::unused_steps() const
{
    std::vector<std::size_t> unused;
    for (std::size_t i = 0; i < scenario_.steps.size(); ++i)
    {
        if (states_[i].occurrences == 0)
        {
            unused.push_back(scenario_.steps[i].number);
        }
    }
    return unused;
}

} // namespace adl::mock

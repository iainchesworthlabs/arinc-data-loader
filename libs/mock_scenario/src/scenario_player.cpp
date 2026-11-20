// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/mock/scenario_player.hpp"

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

} // namespace

ScenarioPlayer::ScenarioPlayer(Scenario scenario, StepObserver observer)
    : scenario_(std::move(scenario)), observer_(std::move(observer))
{
    // Step s (from 1) has the stream that starts from the s-th output of the file's own generator. A file with no
    // seed has no step that draws, so what its steps start from does not matter.
    const std::uint64_t seed = scenario_.seed.value_or(0);
    states_.reserve(scenario_.steps.size());
    for (const ScenarioStep& step : scenario_.steps)
    {
        states_.push_back(State{adl::scenario::step_stream(seed, step.number), 0, 0, 0});
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
    const bool applied = !step.per_mille || state.stream.chance(static_cast<unsigned>(*step.per_mille));
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
        action.value = value.emplace(delay);
        break;
    default:
        break;
    }
    if (observer_)
    {
        observer_(StepFired{step.number, step.trigger, at, kind, step.action.effect, applied, value});
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

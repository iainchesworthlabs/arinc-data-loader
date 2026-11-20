// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Plays a scenario for the engine: the FaultPlan that decides, at each request, each status file and each time that
// comes, which steps fire and what they ask of the engine. It counts the occurrences of each step, draws from the
// scenario's seed for a chance or a range of delays (one stream for each step, so that what one step draws never
// depends on another), and tells an observer about every occurrence, in the order they happen, so that a run with
// faults leaves a record that is the same when it is repeated.

#include "adl/mock/scenario.hpp"
#include "adl/scenario/random.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace adl::mock
{

// A step whose trigger occurred. `at` is the number of the request or status file, or the milliseconds since the
// operation began (as the step was set for). `applied` is false when the step's chance said no. `value` is the
// seconds of a wait, the status code of a refusal or the milliseconds of a delay that was drawn.
struct StepFired
{
    std::size_t step = 0; // from 1
    adl::scenario::Trigger trigger = adl::scenario::Trigger::request;
    std::int64_t at = 0;
    OperationKind operation = OperationKind::information;
    Effect effect = Effect::wait;
    bool applied = true;
    std::optional<std::uint32_t> value;

    friend bool operator==(const StepFired&, const StepFired&) = default;
};

using StepObserver = std::function<void(const StepFired&)>;

class ScenarioPlayer final : public FaultPlan
{
public:
    // `observer` is called at each occurrence, before the effect is applied, on the thread that runs the engine.
    explicit ScenarioPlayer(Scenario scenario, StepObserver observer = {});

    std::vector<Action> on_request(OperationKind kind) override;
    std::vector<Action> on_status_file(OperationKind kind, std::size_t number) override;
    std::vector<TimedStep> on_operation_began(OperationKind kind) override;
    TimeAnswer on_time(std::size_t id, OperationKind kind) override;

    [[nodiscard]] const Scenario& scenario() const noexcept
    {
        return scenario_;
    }

    // The numbers (from 1) of the steps whose trigger never occurred, in the order of the file.
    [[nodiscard]] std::vector<std::size_t> unused_steps() const;

private:
    struct State
    {
        adl::scenario::SplitMix64 stream;
        std::int64_t seen = 0;        // the requests that count for the step so far
        std::int64_t occurrences = 0; // the times its trigger occurred, in the whole run
        std::int64_t scheduled = 0;   // a time step: the milliseconds it is next set for in this operation
    };

    [[nodiscard]] bool spent(const ScenarioStep& step, const State& state) const noexcept;
    [[nodiscard]] bool takes_part(const ScenarioStep& step, OperationKind kind) const noexcept;
    // The step's trigger occurred at `at`: counts it, draws, tells the observer, and returns what it asks for.
    [[nodiscard]] std::vector<Action> fire(std::size_t index, OperationKind kind, std::int64_t at);

    Scenario scenario_;
    StepObserver observer_;
    std::vector<State> states_;
};

} // namespace adl::mock

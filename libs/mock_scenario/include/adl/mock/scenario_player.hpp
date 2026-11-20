// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Plays a scenario for the engine: the FaultPlan that decides, at each request, each status file, each time that
// comes and each datagram the target is about to send, which steps fire and what they ask of the engine. It counts the
// occurrences of each step, draws from the scenario's seed for a chance or a range of delays (one stream for each
// step, so that what one step draws never depends on another), and tells observers about every occurrence, in the
// order they happen, so that a run with faults leaves a record that is the same when it is repeated.
//
// The faults on a datagram (delay, drop_datagram, duplicate_datagram, reorder_datagram) are armed by the trigger of
// their step and then act on the datagrams that follow and match the step's `on`, as many as its `count`.

#include "adl/mock/scenario.hpp"
#include "adl/scenario/random.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace adl::mock
{

// A step whose trigger occurred. `at` is the number of the request or status file, or the milliseconds since the
// operation began (as the step was set for). `applied` is false when the step's chance said no. `value` is the
// seconds of a wait, the status code of a refusal, the milliseconds of a delay that was drawn, the copies of a
// duplicate or the milliseconds a reordered datagram waits at most. For a fault on a datagram, `count` is how many
// datagrams it is armed for, and `on` which.
struct StepFired
{
    std::size_t step = 0; // from 1
    adl::scenario::Trigger trigger = adl::scenario::Trigger::request;
    std::int64_t at = 0;
    OperationKind operation = OperationKind::information;
    Effect effect = Effect::wait;
    bool applied = true;
    std::optional<std::uint32_t> value;
    std::optional<std::int64_t> count = std::nullopt;
    std::optional<DatagramSelector> on = std::nullopt;

    friend bool operator==(const StepFired&, const StepFired&) = default;
};

// A fault on a datagram met a datagram it matches (`applied` is false when the step's chance said no for this one).
struct DatagramFaulted
{
    std::size_t step = 0; // from 1
    Effect effect = Effect::drop_datagram;
    bool applied = true;
    DatagramKind kind = DatagramKind::other;
    std::string file;                   // empty: none
    std::optional<std::uint32_t> block; // of a data or an ack datagram
    std::string to;                     // "127.0.0.1:40001"
    std::size_t size = 0;

    friend bool operator==(const DatagramFaulted&, const DatagramFaulted&) = default;
};

using StepObserver = std::function<void(const StepFired&)>;
using DatagramObserver = std::function<void(const DatagramFaulted&)>;

class ScenarioPlayer final : public FaultPlan
{
public:
    // The observers are called at each occurrence, before the effect is applied, on the thread that runs the engine.
    explicit ScenarioPlayer(Scenario scenario, StepObserver observer = {}, DatagramObserver datagram_observer = {});

    std::vector<Action> on_request(OperationKind kind) override;
    std::vector<Action> on_status_file(OperationKind kind, std::size_t number) override;
    std::vector<TimedStep> on_operation_began(OperationKind kind) override;
    TimeAnswer on_time(std::size_t id, OperationKind kind) override;
    DatagramFate on_datagram(const DatagramInfo& datagram) override;
    void on_operation_ended() override;

    [[nodiscard]] const Scenario& scenario() const noexcept
    {
        return scenario_;
    }

    // The numbers (from 1) of the steps whose trigger never occurred, in the order of the file.
    [[nodiscard]] std::vector<std::size_t> unused_steps() const;

    // The numbers of the steps on a datagram whose trigger occurred and that then met no datagram they match.
    [[nodiscard]] std::vector<std::size_t> idle_steps() const;

private:
    struct State
    {
        adl::scenario::SplitMix64 stream;
        std::int64_t seen = 0;        // the requests that count for the step so far
        std::int64_t occurrences = 0; // the times its trigger occurred, in the whole run
        std::int64_t scheduled = 0;   // a time step: the milliseconds it is next set for in this operation
        std::int64_t armed = 0;       // a fault on a datagram: the datagrams it may still act on
        std::uint32_t delay_ms = 0;   // a delay: what was drawn when it was armed
        std::int64_t matched = 0;     // the datagrams it matched, whether or not its chance applied
    };

    [[nodiscard]] bool spent(const ScenarioStep& step, const State& state) const noexcept;
    [[nodiscard]] bool takes_part(const ScenarioStep& step, OperationKind kind) const noexcept;
    // The step's trigger occurred at `at`: counts it, draws, tells the observer, and returns what it asks for (a fault
    // on a datagram asks for nothing now: it is armed).
    [[nodiscard]] std::vector<Action> fire(std::size_t index, OperationKind kind, std::int64_t at);

    Scenario scenario_;
    StepObserver observer_;
    DatagramObserver datagram_observer_;
    std::vector<State> states_;
};

} // namespace adl::mock

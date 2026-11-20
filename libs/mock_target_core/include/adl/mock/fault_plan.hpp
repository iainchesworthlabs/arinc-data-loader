// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// What a scenario can ask of the mock target's engine, in the engine's own terms. A scenario file (libs/mock_scenario)
// says which fault happens when; its player decides, from what the engine tells it happened (a request, a status
// file, an operation that began, a time that passed), which of the faults the engine already has are to be applied
// now. The engine knows nothing of files, steps, seeds or chances: it asks a FaultPlan and does what it is told.
//
// A plan is one object for one run, called on the thread that runs the engine, and it keeps its own counters. With no
// plan (the default) the engine behaves as it always did.

#include "adl/arinc615a/time.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace adl::mock
{

// Which of the four operations.
enum class OperationKind : std::uint8_t
{
    information,
    upload,
    download_media,
    download_operator,
};

// The faults the engine has, one for each thing a plan can ask for.
enum class Effect : std::uint8_t
{
    wait,              // answer this request with WAIT:value seconds (a request only)
    refuse,            // refuse the operation this request asks for, with the status code `value` (a request only)
    invalid_init_file, // serve an initialization file of an invalid structure (a request only)
    drop_status_file,  // lose a status file: this one, or the next
    abort,             // the target aborts (0x1003): in place of this status file, or now
    silent,            // say nothing more in this operation: from this status file, or from now
    corrupt_served,    // flip a bit in a file the target serves: this request's, or the next one served
    wrong_checksum,    // write a wrong check value option: the next one
    delay,             // hold back the next datagram the target sends by `value` milliseconds
};

struct Action
{
    Effect effect = Effect::wait;
    std::uint32_t value = 0; // seconds, a status code or milliseconds, as the effect says; otherwise 0

    friend bool operator==(const Action&, const Action&) = default;
};

// A step of a plan that fires some time after an operation began.
struct TimedStep
{
    std::size_t id = 0;               // the plan's own number for it, given back when the time comes
    adl::arinc615a::Duration after{}; // since the operation began
};

// What a step does when its time comes, and when it comes again.
struct TimeAnswer
{
    std::vector<Action> actions;
    std::optional<adl::arinc615a::Duration> again; // from now; none: it is done
};

class FaultPlan
{
public:
    virtual ~FaultPlan() = default;

    // The engine begins to answer a request for an initialization file (including one it answers with WAIT, and
    // one it answers again after a retransmission). What is returned applies to that answer.
    virtual std::vector<Action> on_request(OperationKind kind) = 0;

    // The engine is about to send the status file that is number `number` of the operation, counting from 1 and
    // counting the ones that are lost. What is returned applies to that file, or to what follows it.
    virtual std::vector<Action> on_status_file(OperationKind kind, std::size_t number) = 0;

    // An operation began (its initialization file has been served and accepted). The steps that fire some time
    // after it did are returned; the engine calls on_time for each when the time comes, unless the operation ended.
    virtual std::vector<TimedStep> on_operation_began(OperationKind kind) = 0;
    virtual TimeAnswer on_time(std::size_t id, OperationKind kind) = 0;
};

} // namespace adl::mock

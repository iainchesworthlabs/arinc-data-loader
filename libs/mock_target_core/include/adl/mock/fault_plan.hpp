// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// What a scenario can ask of the mock target's engine, in the engine's own terms. A scenario file (libs/mock_scenario)
// says which fault happens when; its player decides, from what the engine tells it happened (a request, a status
// file, an operation that began, a time that passed, a datagram about to be sent), which of the faults the engine
// already has are to be applied now. The engine knows nothing of files, steps, seeds or chances: it asks a FaultPlan
// and does what it is told.
//
// A plan is one object for one run, called on the thread that runs the engine, and it keeps its own counters. With no
// plan (the default) the engine behaves as it always did.

#include "adl/arinc615a/endpoint.hpp"
#include "adl/arinc615a/time.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
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

// The things a scenario step can do. The first nine the engine does when a plan answers with them (an Action); the
// last four are faults on a datagram, which the plan keeps armed and the engine meets only as a DatagramFate.
enum class Effect : std::uint8_t
{
    wait,               // answer this request with WAIT:value seconds (a request only)
    refuse,             // refuse the operation this request asks for, with the status code `value` (a request only)
    invalid_init_file,  // serve an initialization file of an invalid structure (a request only)
    drop_status_file,   // lose a status file: this one, or the next
    abort,              // the target aborts (0x1003): in place of this status file, or now
    silent,             // say nothing more in this operation: from this status file, or from now
    corrupt_served,     // flip a bit in a file the target serves: this request's, or the next one served
    wrong_checksum,     // write a wrong check value option: the next one
    delay,              // hold back a datagram (a DatagramFate)
    drop_datagram,      // do not send a datagram (a DatagramFate)
    duplicate_datagram, // send a datagram more than once (a DatagramFate)
    reorder_datagram,   // send a datagram after the next one (a DatagramFate)
};

struct Action
{
    Effect effect = Effect::wait;
    std::uint32_t value = 0; // seconds or a status code, as the effect says; otherwise 0

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

// What the engine knows of a datagram it is about to send.
enum class DatagramKind : std::uint8_t
{
    request,    // a read or write request (the engine's own: for a status file, a list, a file of an upload)
    option_ack, // an option acknowledgement
    ack,
    data,
    error, // an error packet, which is also what WAIT and ABORT are
    other, // anything else (a FIND answer)
};

struct DatagramInfo
{
    DatagramKind kind = DatagramKind::other;
    std::uint32_t block = 0; // the block of a data or an ack datagram, otherwise 0
    std::string_view file;   // the file of the datagram's transfer, or empty when it has none
    std::uint16_t from_port = 0;
    adl::arinc615a::Endpoint to{};
    std::size_t size = 0;
};

// What to do with that datagram. `amount` is the number of copies of a duplicate, or the milliseconds that a delayed
// datagram waits, or that a held one waits at most for the next datagram to go first.
struct DatagramFate
{
    enum class Kind : std::uint8_t
    {
        send,      // as it is
        drop,      // not at all
        duplicate, // `amount` times in a row
        delay,     // after `amount` milliseconds
        hold,      // after the next datagram that is sent, or after `amount` milliseconds
    };
    Kind kind = Kind::send;
    std::uint32_t amount = 0;

    friend bool operator==(const DatagramFate&, const DatagramFate&) = default;
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

    // The engine is about to send a datagram, whatever it is: it says what the datagram is and the plan says what to
    // do with it. The counterpart, on the target's side, of the loader's hook on the datagrams it means to send.
    virtual DatagramFate on_datagram(const DatagramInfo& datagram) = 0;

    // An operation ended (or the target woke from silence): what a plan kept armed for it is dropped.
    virtual void on_operation_ended() {}
};

} // namespace adl::mock

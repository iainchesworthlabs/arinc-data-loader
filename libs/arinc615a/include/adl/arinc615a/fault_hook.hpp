// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The one place a fault is let into the data loader. A FaultHook is given each datagram that arrives and each
// command the loader means to carry out, and says what is to be carried out in its place: nothing, the same,
// a changed one, several, or one held back until later. It can also ask for the loader's own abort at a chosen
// moment. The loader knows nothing about what a hook does: to the loader a datagram that was dropped or
// damaged here is one that was dropped or damaged on the network, and its time-outs and retries go on as they
// do then. A loader given no hook (the default) takes no other path than it did before this header existed.
//
// The hook is supplied by whoever runs the loader (`LoaderServices::faults`), for a test or for the conformance
// work of a supplier who wants to see how a target reacts to a loader that does something wrong on purpose.
// Everything here is `noexcept` and nothing allocates: an implementation allocates when it is made, not in a
// call. The bytes of a command a hook writes are the hook's, and have to stay valid until the next call into
// the loader (the next begin_call), as the loader's own do.

#include "adl/arinc615a/command.hpp"
#include "adl/arinc615a/endpoint.hpp"
#include "adl/arinc615a/status_code.hpp"
#include "adl/arinc615a/time.hpp"

#include <cstddef>
#include <optional>
#include <span>

namespace adl::arinc615a
{

class FaultHook
{
public:
    virtual ~FaultHook() = default;

    // A call into the loader has begun (a datagram, the timer, the start of the operation, an abort or a choice
    // of files). The commands the hook wrote in answer to an earlier call have been carried out by now, so the
    // storage of their bytes is the hook's again.
    virtual void begin_call(Instant now) noexcept = 0;

    // A datagram has arrived and the loader is about to handle it: on the socket of `slot`, or on the control
    // socket when `slot` is empty. Only the loader's own targets and transfers are reported (a datagram it
    // ignores for being from another host, or for a socket that is not open, is not). The hook cannot change
    // what the loader does with it.
    virtual void on_arrival(Instant now, std::optional<std::size_t> slot, const Endpoint& from,
                            std::span<const std::byte> bytes) noexcept = 0;

    // The loader has a command to carry out. The hook writes into `out`, the room that is left in the loader's
    // list of commands (at most DataLoader::max_commands), what is to be carried out instead, in order, and
    // returns how many it wrote, never more than `out.size()`. Writing the command itself carries it out as it
    // is; writing nothing drops it. A release of a slot's socket is a command too, and is passed on unless the
    // hook has a reason not to.
    [[nodiscard]] virtual std::size_t on_command(Instant now, const Command& command,
                                                 std::span<Command> out) noexcept = 0;

    // Time has passed. Commands that were held back and are due go into `out`, as above, and the count is
    // returned.
    [[nodiscard]] virtual std::size_t on_timer(Instant now, std::span<Command> out) noexcept = 0;

    // When on_timer() should next be called, or `never`.
    [[nodiscard]] virtual Instant next_wake() const noexcept = 0;

    // The loader's own abort (DataLoader::abort), asked for now: the status is aborted_by_operator or
    // aborted_by_data_loader. Asked once at the end of every call into the loader; an answer is taken once.
    [[nodiscard]] virtual std::optional<StatusCode> take_abort(Instant now) noexcept = 0;
};

} // namespace adl::arinc615a

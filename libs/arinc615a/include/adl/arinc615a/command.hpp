// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// What the data loader asks of whatever drives it: send these bytes, close that socket. The loader holds no
// socket, so everything it says to a target leaves it as a Command, which the runner (or the operation pool,
// or a test's network) carries out. FaultHook (fault_hook.hpp) sees each one before it is carried out.

#include "adl/arinc615a/endpoint.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace adl::arinc615a
{

// What the runner is to do after each call, in order.
struct Command
{
    enum class Kind : std::uint8_t
    {
        send_from_slot,    // send `bytes` to `to` from the socket of slot `slot`
        send_from_control, // send `bytes` to `to` from the control socket
        release_slot,      // close the socket of slot `slot`
    };
    Kind kind = Kind::release_slot;
    std::uint8_t slot = 0;
    Endpoint to;
    std::span<const std::byte> bytes;
};

} // namespace adl::arinc615a

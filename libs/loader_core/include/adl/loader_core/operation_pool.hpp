// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Several operations at once from one control port. run_operation (run.hpp) holds the loader's control port
// for one operation, so two of them on one computer cannot both have port 59 on one address, and a real target
// writes to 59. The pool holds the port once and runs one operation per target on it: each operation is its own
// DataLoader with its own workspace and its own transfer sockets, and every datagram that arrives on the control
// port is given to the operation whose target sent it. The engine does not change; what the pool adds is that
// choice of operation and the loop around it.
//
// One address has one operation at a time. That is what tells the operations apart: the engine already ignores a
// control datagram whose source is not its target, so a datagram from an address that has no operation is dropped
// here as it is there (and counted, unrouted_datagrams()). The target's replies to the loader's own requests come
// back to the operation's transfer sockets, whose ports are the operation's alone.
//
// It runs on the thread that calls step(); nothing in it is thread safe, and nothing in it blocks for longer
// than the time it is given. A caller that has other things to do (read requests, write events) calls step()
// with a short time between them. What an operation reads, writes and is told by (RunServices) is the caller's
// and must stay alive until the operation's completion has been called. What happens to a finished operation is
// the same as for run_operation: the completion gets its OperationResult, built by the same function.
//
// An operation ends when its engine is idle. The pool does not end one on its own account; abort() asks the
// engine to abort (the operator's abort, which the target confirms), and the result is then Reason::aborted.

#include "adl/arinc615a/data_loader.hpp"
#include "adl/arinc615a/endpoint.hpp"
#include "adl/arinc615a/limits.hpp"
#include "adl/loader_core/run.hpp"
#include "adl/platform/error.hpp"
#include "adl/platform/host.hpp"
#include "adl/platform/network.hpp"
#include "adl/platform/owned.hpp"
#include "adl/platform/time.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace adl::loader_core
{

struct PoolConfig
{
    // The port the loader listens on, for every operation. 0 takes any free port (control_port() says which).
    std::uint16_t control_port = adl::arinc615a::tftp_control_port;
    // The interface the control socket and every transfer socket are bound to. 0.0.0.0, every interface, is the
    // default: the targets are not known when the port is opened, so the window's rule of binding to loopback
    // for a target on this computer cannot be made. A pool of targets on this computer asks for 127.0.0.1.
    adl::platform::Ipv4Address local_address = adl::platform::Ipv4Address::any();
    // How many operations may run at once.
    std::size_t max_operations = 8;
    // How many sockets one wait on the network takes at a time, at most the platform's own limit. The control
    // socket is always one of them. Operations with more sockets than that between them have the rest looked at
    // without waiting, once for each pass of step(), which is slower to notice a datagram and never misses one.
    // It is a setting so that a test can make the case happen with a few operations.
    std::size_t wait_window = adl::platform::max_wait_sockets;
};

// Where one operation goes.
struct PoolTarget
{
    adl::arinc615a::Address address;
    // As adl::arinc615a::is_valid_thw_id_pos takes it. The pool keeps its own copy.
    std::string thw_id_pos;
    // The port the target takes the request that starts the operation on: the standard's 59, unless the target is
    // set up to listen elsewhere. Not 0: the operation then ends at once with local_error (stage start), as
    // run_operation's does.
    std::uint16_t port = adl::arinc615a::tftp_control_port;
};

// Why start() did not take an operation. What the engine says about the request itself (a THW_ID_POS it cannot
// use) is not here: that operation ends with Reason::local_error, as it does in run_operation.
enum class PoolRefusal : std::uint8_t
{
    not_open,           // open() has not been called, or it failed
    address_busy,       // that address has an operation that has not ended
    too_many_operations // PoolConfig::max_operations operations are running
};

[[nodiscard]] constexpr std::string_view to_string(PoolRefusal refusal) noexcept
{
    switch (refusal)
    {
    case PoolRefusal::not_open:
        return "not_open";
    case PoolRefusal::address_busy:
        return "address_busy";
    case PoolRefusal::too_many_operations:
        return "too_many_operations";
    }
    return "not_open";
}

class OperationPool
{
public:
    // Called from step() on the thread that calls it, after the operation has been taken out of the pool: the
    // address is free again, and the completion may start another operation on it.
    using Completion = std::function<void(OperationResult&&)>;

    OperationPool(adl::platform::Host& host, const PoolConfig& config);
    OperationPool(const OperationPool&) = delete;
    OperationPool& operator=(const OperationPool&) = delete;
    // An operation that is still running is abandoned, as killing the program would: its completion is not
    // called, and the target times out by itself.
    ~OperationPool();

    // Opens the control port. The error is the platform's: address_in_use when something holds the port.
    [[nodiscard]] adl::platform::Result<void> open();
    [[nodiscard]] bool is_open() const noexcept;
    // The port the control socket has (what PoolConfig asked for, or the free one it was given for 0).
    [[nodiscard]] std::uint16_t control_port() const noexcept;

    // Starts `operation` on the target. The pool does not wait: the operation runs inside step(), and
    // `completion` is called from there once. `services` follow run_operation's rules, with the lifetime above;
    // its download folder is copied. `control_port` and `local_address` of the target are the pool's.
    [[nodiscard]] std::expected<void, PoolRefusal> start(const PoolTarget& target,
                                                         const adl::arinc615a::Operation& operation,
                                                         const RunServices& services, Completion completion);

    // Asks the operation on `target` to abort, from this side. False if the address has no operation. The abort
    // is made on the next step(); the operation ends when the target has confirmed it, or after the DLP time-out.
    [[nodiscard]] bool abort(const adl::arinc615a::Address& target);

    [[nodiscard]] bool is_running(const adl::arinc615a::Address& target) const noexcept;
    // How many operations have not ended (an operation that could not be started and is waiting to be
    // completed does not count).
    [[nodiscard]] std::size_t running() const noexcept;
    [[nodiscard]] std::size_t max_operations() const noexcept
    {
        return config_.max_operations;
    }

    // Waits at most `max_wait` for a datagram, gives it to its operation, does what is due for every operation
    // (the controller's poll, the engine's timer) and completes those that have ended. Returns early when
    // something was done. Without operations it is a wait of `max_wait` on the control port, where a datagram
    // that arrives is counted and dropped.
    void step(adl::platform::Duration max_wait);

    // Datagrams on the control port from an address that had no operation, and ones larger than any block.
    [[nodiscard]] std::uint64_t unrouted_datagrams() const noexcept
    {
        return unrouted_;
    }

private:
    struct Running;
    struct Early;
    struct Entry;

    void execute(Running& operation);
    void complete_ended();
    void fail_all(adl::platform::ErrorCode code);

    adl::platform::Host& host_;
    PoolConfig config_;
    adl::platform::Owned<adl::platform::UdpSocket> control_;
    std::uint16_t control_port_ = 0;
    std::vector<std::unique_ptr<Running>> running_;
    std::vector<std::unique_ptr<Early>> early_;
    std::vector<std::byte> buffer_;
    std::uint64_t unrouted_ = 0;
};

} // namespace adl::loader_core

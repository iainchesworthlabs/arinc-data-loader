#pragma once

// The runner: the one piece of the 615A library that calls the platform. It owns no protocol.
// It opens the sockets the engine asks for (the control port, and one per transfer), waits on
// all of them from one thread with Network::wait_readable, hands each datagram and the time to
// the DataLoader or FindClient, and carries out what they answer with: send this, close that.
// The clock it reads is the platform's monotonic one.
//
// It is blocking and single-threaded. An operator's abort, or a deferred choice of files, reaches
// the loader through a RunController, which is polled between waits (the interval is in
// RunOptions) from the same thread.

#include <cstddef>
#include <optional>
#include <span>

#include "adl/arinc615a/data_loader.hpp"
#include "adl/arinc615a/find_client.hpp"
#include "adl/platform/error.hpp"
#include "adl/platform/host.hpp"

namespace adl::arinc615a {

// Polled by the runner between waits, on its thread. It may make at most one call to the loader
// each time it is polled (abort() or provide_selection()): the runner carries out the
// commands that call left, and a second call would replace them.
class RunController {
public:
    virtual ~RunController() = default;
    virtual void on_poll(DataLoader& loader, Instant now) noexcept = 0;
};

struct RunOptions {
    RunController* controller = nullptr;
    // How often a controller is polled while nothing else is happening.
    Duration poll_interval = std::chrono::milliseconds(100);
};

// Why run() could not run the operation at all (as opposed to the operation failing, which is
// an Outcome).
struct RunError {
    enum class Stage : std::uint8_t {
        start,           // the loader refused the request: `engine` says why
        control_socket,  // the control port could not be opened: `platform` says why
        wait,            // waiting on the sockets failed
    };
    Stage stage = Stage::start;
    Error engine = Error::invalid_value;
    adl::platform::ErrorCode platform = adl::platform::ErrorCode::other;
};

class Runner {
public:
    explicit Runner(adl::platform::Host& host) noexcept : host_(host) {}

    // Runs `operation` against the target `loader` was configured for, until it has an outcome
    // and nothing is left open. The socket on the loader's control port is held for the
    // duration.
    [[nodiscard]] std::expected<Outcome, RunError> run(DataLoader& loader,
                                                       const Operation& operation,
                                                       const RunOptions& options = {}) noexcept;

    // FIND: broadcasts the request (or sends it to each configured address), collects answers
    // for the window and copies up to out.size() of the targets that answered into `out`.
    // Returns how many. The socket is bound to port 1001, which the standard has the answer
    // come back to.
    [[nodiscard]] adl::platform::Result<std::size_t> find(
        const FindConfig& config, std::span<FindTarget> out,
        std::uint16_t port = find_port) noexcept;

private:
    adl::platform::Host& host_;
};

[[nodiscard]] Instant to_instant(adl::platform::Instant at) noexcept;

}  // namespace adl::arinc615a

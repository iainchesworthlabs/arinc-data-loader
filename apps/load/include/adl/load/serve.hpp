// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// `adl_load serve`: one loader that runs operations on several targets at once from one control port
// (docs/load-command.md). It holds the port once, reads requests from a LineSource (standard input in the
// program), starts each `run` on the loader library's OperationPool, and writes every event as one JSON line,
// whole and flushed, from the one thread that calls run_serve: the events of different operations are never
// mixed. The source is an interface so that a test can give the lines at chosen moments of the fake clock.

#include "adl/load/options.hpp"
#include "adl/load/preparer.hpp"
#include "adl/load/request.hpp"
#include "adl/platform/error.hpp"
#include "adl/platform/host.hpp"
#include "adl/platform/time.hpp"

#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

namespace adl::load
{

// A line that came in, or what became of one that could not be taken whole.
struct InputLine
{
    LineEnd end = LineEnd::line; // line, too_long or unfinished
    std::string text;            // for `line`
};

// Where the requests come from. run_serve calls it from its own thread only.
class LineSource
{
public:
    virtual ~LineSource() = default;

    // Starts reading. Called once, after the control port is held, so that nothing is read from a program that
    // could not run. The error is the platform's.
    [[nodiscard]] virtual adl::platform::Result<void> start() = 0;

    // Adds the lines that have arrived since the last call to `lines`, without waiting. `now` is the host's clock.
    virtual void poll(adl::platform::Instant now, std::vector<InputLine>& lines) = 0;

    // The input has ended and every line of it has been given by poll().
    [[nodiscard]] virtual bool ended() const = 0;
};

// A LineSource over a stream, read on a thread of its own because reading blocks: the program's standard input.
class StreamLineSource final : public LineSource
{
public:
    // `host` makes the thread and the lock. The stream has to outlive the source.
    StreamLineSource(adl::platform::Host& host, std::istream& in);
    StreamLineSource(const StreamLineSource&) = delete;
    StreamLineSource& operator=(const StreamLineSource&) = delete;
    // Joins the thread if the input has ended. If it has not, the thread is blocked in a read that nothing can
    // end, and it is left to the end of the program, which takes it with it.
    ~StreamLineSource() override;

    [[nodiscard]] adl::platform::Result<void> start() override;
    void poll(adl::platform::Instant now, std::vector<InputLine>& lines) override;
    [[nodiscard]] bool ended() const override;

private:
    struct State;
    adl::platform::Host& host_;
    std::istream& in_;
    std::unique_ptr<State> state_;
};

// Runs `serve` with the options that were accepted (`local_address`, `control_port`, `max_operations`) until it
// is told to stop, or its input ends, and its operations have finished. The requests come from `input`, and what an
// upload has to read before it can start (its media set) is done by `preparer`, away from the loop, so that starting
// an operation does not hold up the ones that are running. `start` is the moment `t` counts from. Returns the exit
// code: 0 it stopped as asked, whatever happened to the operations; 5 the control port could not be opened, the
// input could not be read or the preparer started, or standard output could not be written to.
[[nodiscard]] int run_serve(adl::platform::Host& host, const Options& options, LineSource& input, Preparer& preparer,
                            std::ostream& out, std::ostream& err, adl::platform::Instant start);

} // namespace adl::load

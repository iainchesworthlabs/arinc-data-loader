// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// What an operation of `adl_load serve` needs done before it can start, done away from the loop that serves the
// targets. An upload reads and checks its media set, and a set of a gigabyte takes longer than a target waits (about
// 17 seconds on the machine the program was measured on, against the 13 seconds of the data load protocol time-out),
// so a loop that read the set itself would fail every operation that was running beside it. The Preparer takes the
// work of one request, runs it on a thread of its own, one request at a time, and gives the result back to the loop
// when the loop asks for it, without the loop ever waiting.
//
// The work is a function of what the request holds and of the disk, never of the loop's state, and it is run on
// another thread: it may not touch what the loop is using. It returns nothing when all is well, and the sentence of
// wrong usage when the choice does not fit what it found (a part number the set does not hold).
//
// An interface, as the source of the requests is, so that a test gives the preparations by the fake clock and the
// program gives a thread.

#include "adl/platform/error.hpp"
#include "adl/platform/host.hpp"
#include "adl/platform/time.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace adl::load
{

// The sentence of wrong usage, or nothing.
using PreparationResult = std::optional<std::string>;
using PreparationWork = std::function<PreparationResult()>;

// A preparation that has ended: the ticket it was submitted with, and what it found.
struct Preparation
{
    std::uint64_t ticket = 0;
    PreparationResult problem;
};

class Preparer
{
public:
    virtual ~Preparer() = default;

    // Gets ready to take work. Called once, after the control port is held. The error is the platform's.
    [[nodiscard]] virtual adl::platform::Result<void> start() = 0;

    // Takes the work of one request and returns at once. The work is run, away from the caller's thread, after the
    // ones submitted before it have ended.
    virtual void submit(std::uint64_t ticket, PreparationWork work) = 0;

    // Adds the preparations that have ended since the last call to `done`, in the order they were submitted, without
    // waiting. `now` is the host's clock.
    virtual void poll(adl::platform::Instant now, std::vector<Preparation>& done) = 0;
};

// A thread of its own that runs the work one request at a time.
class ThreadPreparer final : public Preparer
{
public:
    // `host` makes the thread, the lock and the signal.
    explicit ThreadPreparer(adl::platform::Host& host);
    ThreadPreparer(const ThreadPreparer&) = delete;
    ThreadPreparer& operator=(const ThreadPreparer&) = delete;
    // Ends the thread: the work that is running is waited for (it cannot be interrupted), and what has not begun is
    // dropped.
    ~ThreadPreparer() override;

    [[nodiscard]] adl::platform::Result<void> start() override;
    void submit(std::uint64_t ticket, PreparationWork work) override;
    void poll(adl::platform::Instant now, std::vector<Preparation>& done) override;

private:
    struct State;
    adl::platform::Host& host_;
    std::unique_ptr<State> state_;
};

} // namespace adl::load

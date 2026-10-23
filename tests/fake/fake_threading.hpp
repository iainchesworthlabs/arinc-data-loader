// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Threads, locks and semaphores without a second thread. A fake "thread" does
// not run when it is spawned: it runs, on the caller's stack, when it is joined
// (or destroyed, or when the test calls run_pending), which is what keeps a test
// deterministic. The consequence is that the fake cannot show two threads
// racing; that is what the conformance tests that only a real platform runs, and
// the loopback tests, are for. A fake semaphore that is empty lets the world
// run (so a simulated target can act) until the time-out passes.

#include "adl/platform/threads.hpp"
#include "fake_world.hpp"

#include <memory>
#include <vector>

namespace adl::fake
{

class FakeThread;

class FakeThreading final : public platform::Threading
{
public:
    explicit FakeThreading(FakeWorld& world) noexcept : world_(world) {}

    platform::Result<platform::Owned<platform::Thread>> spawn(const platform::ThreadSpec& spec) noexcept override;
    platform::Result<platform::Owned<platform::Mutex>> create_mutex() noexcept override;
    platform::Result<platform::Owned<platform::Semaphore>> create_semaphore(unsigned initial,
                                                                            unsigned ceiling) noexcept override;

    // Runs every spawned thread that has not run yet, in the order spawned.
    // Returns how many ran.
    std::size_t run_pending();

    [[nodiscard]] std::size_t spawned() const noexcept
    {
        return spawned_;
    }

private:
    friend class FakeThread;
    FakeWorld& world_;
    std::vector<FakeThread*> pending_;
    std::size_t spawned_ = 0;
};

} // namespace adl::fake

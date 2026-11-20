// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/load/preparer.hpp"

#include "adl/platform/threads.hpp"

#include <chrono>
#include <deque>
#include <utility>

namespace adl::load
{

namespace
{

// The most that can wait for the worker at once: a request for each operation the program may run, and some more.
constexpr unsigned most_waiting = 4096;

} // namespace

// What the loop and the worker share.
struct ThreadPreparer::State
{
    struct Job
    {
        std::uint64_t ticket = 0;
        PreparationWork work;
    };

    // The thread's entry.
    void run() noexcept
    {
        for (;;)
        {
            // One signal for each job, and one for the end: a time-out is only the wait going round again.
            static_cast<void>(signal->wait(std::chrono::hours(1)));
            Job job;
            {
                const adl::platform::LockGuard guard(*mutex);
                if (jobs.empty())
                {
                    if (stopping)
                    {
                        return;
                    }
                    continue;
                }
                if (stopping)
                {
                    return; // what has not begun is dropped
                }
                job = std::move(jobs.front());
                jobs.pop_front();
            }
            PreparationResult problem = job.work();
            const adl::platform::LockGuard guard(*mutex);
            ended.push_back(Preparation{job.ticket, std::move(problem)});
        }
    }

    adl::platform::Owned<adl::platform::Mutex> mutex;
    adl::platform::Owned<adl::platform::Semaphore> signal;
    adl::platform::Owned<adl::platform::Thread> thread; // after the others: it is joined before they go
    std::deque<Job> jobs;                               // guarded by the mutex
    std::vector<Preparation> ended;                     // guarded by the mutex
    bool stopping = false;                              // guarded by the mutex
};

ThreadPreparer::ThreadPreparer(adl::platform::Host& host) : host_(host) {}

ThreadPreparer::~ThreadPreparer()
{
    if (!state_ || !state_->thread)
    {
        return;
    }
    {
        const adl::platform::LockGuard guard(*state_->mutex);
        state_->stopping = true;
    }
    static_cast<void>(state_->signal->post());
    state_.reset(); // joins the thread: the work it is running ends first
}

adl::platform::Result<void> ThreadPreparer::start()
{
    state_ = std::make_unique<State>();
    auto mutex = host_.threads().create_mutex();
    if (!mutex)
    {
        return std::unexpected(mutex.error());
    }
    state_->mutex = std::move(*mutex);
    auto signal = host_.threads().create_semaphore(0, most_waiting);
    if (!signal)
    {
        return std::unexpected(signal.error());
    }
    state_->signal = std::move(*signal);
    adl::platform::ThreadSpec spec;
    spec.name = "adl_load_prepare";
    auto thread = adl::platform::spawn_member(host_.threads(), spec, *state_);
    if (!thread)
    {
        return std::unexpected(thread.error());
    }
    state_->thread = std::move(*thread);
    return {};
}

void ThreadPreparer::submit(std::uint64_t ticket, PreparationWork work)
{
    if (!state_ || !state_->thread)
    {
        return;
    }
    {
        const adl::platform::LockGuard guard(*state_->mutex);
        state_->jobs.push_back(State::Job{ticket, std::move(work)});
    }
    static_cast<void>(state_->signal->post());
}

void ThreadPreparer::poll(adl::platform::Instant /*now*/, std::vector<Preparation>& done)
{
    if (!state_ || !state_->mutex)
    {
        return;
    }
    const adl::platform::LockGuard guard(*state_->mutex);
    for (Preparation& preparation : state_->ended)
    {
        done.push_back(std::move(preparation));
    }
    state_->ended.clear();
}

} // namespace adl::load

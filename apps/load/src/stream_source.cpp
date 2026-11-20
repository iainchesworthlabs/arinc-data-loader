// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/load/serve.hpp"
#include "adl/platform/threads.hpp"

#include <deque>
#include <istream>
#include <utility>

namespace adl::load
{

// What the reader thread and the loop share: the lines that have been read and whether the input has ended.
struct StreamLineSource::State
{
    explicit State(std::istream& stream) : in(stream) {}

    // The thread's entry.
    void run() noexcept
    {
        std::string text;
        for (;;)
        {
            const LineEnd end = read_line(in, text);
            adl::platform::LockGuard guard(*mutex);
            if (end == LineEnd::end)
            {
                done = true;
                return;
            }
            lines.push_back(InputLine{end, std::move(text)});
            text.clear();
            if (end == LineEnd::unfinished)
            {
                done = true; // the end of the input is what unfinished a line
                return;
            }
        }
    }

    std::istream& in;
    adl::platform::Owned<adl::platform::Mutex> mutex;
    std::deque<InputLine> lines; // guarded by the mutex
    bool done = false;           // guarded by the mutex
    // Last, so that it is destroyed first, and joined, before anything the thread uses is.
    adl::platform::Owned<adl::platform::Thread> thread;
};

StreamLineSource::StreamLineSource(adl::platform::Host& host, std::istream& in) : host_(host), in_(in) {}

StreamLineSource::~StreamLineSource()
{
    if (!state_)
    {
        return;
    }
    bool finished = !state_->thread;
    if (state_->mutex)
    {
        const adl::platform::LockGuard guard(*state_->mutex);
        finished = finished || state_->done;
    }
    if (finished)
    {
        state_.reset(); // joins the thread, which has returned or is about to
        return;
    }
    // The thread is blocked reading an input that has not ended, and nothing can wake it: joining would wait for
    // the end of the program. It and what it uses are left, reachable, for the program's exit to take.
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory): never destroyed, on purpose
    static auto* const abandoned = new std::vector<std::unique_ptr<State>>();
    abandoned->push_back(std::move(state_));
}

adl::platform::Result<void> StreamLineSource::start()
{
    state_ = std::make_unique<State>(in_);
    auto mutex = host_.threads().create_mutex();
    if (!mutex)
    {
        return std::unexpected(mutex.error());
    }
    state_->mutex = std::move(*mutex);
    adl::platform::ThreadSpec spec;
    spec.name = "adl_load_input";
    auto thread = adl::platform::spawn_member(host_.threads(), spec, *state_);
    if (!thread)
    {
        return std::unexpected(thread.error());
    }
    state_->thread = std::move(*thread);
    return {};
}

void StreamLineSource::poll(adl::platform::Instant /*now*/, std::vector<InputLine>& lines)
{
    if (!state_ || !state_->mutex)
    {
        return;
    }
    const adl::platform::LockGuard guard(*state_->mutex);
    for (InputLine& line : state_->lines)
    {
        lines.push_back(std::move(line));
    }
    state_->lines.clear();
}

bool StreamLineSource::ended() const
{
    if (!state_ || !state_->mutex)
    {
        return false;
    }
    const adl::platform::LockGuard guard(*state_->mutex);
    return state_->done && state_->lines.empty();
}

} // namespace adl::load

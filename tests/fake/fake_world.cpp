#include "fake_world.hpp"

#include <chrono>

namespace adl::fake
{

void FakeWorld::schedule(platform::Instant at, Callback callback)
{
    events_.push(Event{at, sequence_++, std::move(callback)});
}

std::optional<platform::Instant> FakeWorld::next_event_time() const
{
    if (events_.empty())
    {
        return std::nullopt;
    }
    return events_.top().at;
}

bool FakeWorld::step()
{
    if (events_.empty())
    {
        return false;
    }
    // Copy out before running: the callback may schedule more events.
    Event event = events_.top();
    events_.pop();
    if (event.at > now_)
    {
        now_ = event.at;
    }
    event.callback();
    return true;
}

void FakeWorld::advance_to(platform::Instant at)
{
    while (!events_.empty() && events_.top().at <= at)
    {
        step();
    }
    if (at > now_)
    {
        now_ = at;
    }
}

void FakeClock::sleep_for(platform::Duration d) noexcept
{
    if (d > platform::Duration::zero())
    {
        world_.advance_by(d);
    }
}

platform::Result<std::int64_t> FakeClock::wall_time_seconds() noexcept
{
    return world_.wall_origin() + std::chrono::duration_cast<std::chrono::seconds>(world_.now().since_origin).count();
}

} // namespace adl::fake

#pragma once

// The fake platform's sense of time. A FakeWorld is a clock that only moves when
// told to and a queue of things that are due at given instants (a datagram
// arriving, a simulated target's timer). Nothing here sleeps: a program that
// "waits 13 seconds" advances the clock by 13 seconds and returns at once, and
// every event that was due on the way runs, in time order, on the calling
// thread. So a whole transfer, time-outs and retries included, runs in
// milliseconds of real time and gives the same result every time.

#include "adl/platform/time.hpp"

#include <cstdint>
#include <functional>
#include <optional>
#include <queue>
#include <vector>

namespace adl::fake
{

// SplitMix64: a small generator that is the same on every compiler and library,
// unlike the distributions of <random>, so a seed names one history everywhere.
class Random
{
public:
    explicit Random(std::uint64_t seed = 1) noexcept : state_(seed) {}

    [[nodiscard]] std::uint64_t next() noexcept
    {
        std::uint64_t z = (state_ += 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }
    // Uniform enough in [0, bound) for fault injection; `bound` > 0.
    [[nodiscard]] std::uint64_t below(std::uint64_t bound) noexcept
    {
        return next() % bound;
    }
    // True `parts_per_million` times in a million.
    [[nodiscard]] bool chance(std::uint32_t parts_per_million) noexcept
    {
        return below(1'000'000) < parts_per_million;
    }

private:
    std::uint64_t state_;
};

class FakeWorld
{
public:
    explicit FakeWorld(std::uint64_t seed = 1) : random_(seed) {}

    [[nodiscard]] platform::Instant now() const noexcept
    {
        return now_;
    }
    [[nodiscard]] Random& random() noexcept
    {
        return random_;
    }

    using Callback = std::function<void()>;

    // Runs `callback` when the clock reaches `at` (at once, at the next step, if
    // that has passed). Events at the same instant run in the order scheduled.
    void schedule(platform::Instant at, Callback callback);
    void schedule_after(platform::Duration delay, Callback callback)
    {
        schedule(now_ + delay, std::move(callback));
    }

    [[nodiscard]] std::optional<platform::Instant> next_event_time() const;

    // Runs the earliest event, moving the clock to its time first (never back).
    // False if there is none.
    bool step();

    // Runs every event due up to and including `at`, then sets the clock to `at`.
    void advance_to(platform::Instant at);
    void advance_by(platform::Duration d)
    {
        advance_to(now_ + d);
    }

    // Runs events until `done()` is true, or `timeout` has passed on the clock.
    // Returns done() at the end. With `forever` and nothing left to happen it
    // returns false rather than wait for what cannot come, which a real
    // program would do by hanging.
    template <class Done>
    bool wait_until(Done&& done, platform::Duration timeout)
    {
        const platform::Instant deadline = now_ + timeout;
        for (;;)
        {
            if (done())
            {
                return true;
            }
            if (timeout <= platform::Duration::zero())
            {
                return false;
            }
            const auto next = next_event_time();
            if (next && *next <= deadline)
            {
                step();
                // Everything else due at the same instant has happened too, as it would
                // have: two datagrams that arrive together are both there when the first is seen.
                while (const auto also = next_event_time())
                {
                    if (*also > now_)
                    {
                        break;
                    }
                    step();
                }
                continue;
            }
            if (timeout != platform::forever)
            {
                advance_to(deadline);
            }
            return done();
        }
    }

    // Seconds since the Unix epoch at the clock's origin, for wall_time_seconds().
    void set_wall_origin(std::int64_t unix_seconds) noexcept
    {
        wall_origin_ = unix_seconds;
    }
    [[nodiscard]] std::int64_t wall_origin() const noexcept
    {
        return wall_origin_;
    }

private:
    struct Event
    {
        platform::Instant at;
        std::uint64_t sequence;
        Callback callback;
    };
    struct Later
    {
        bool operator()(const Event& a, const Event& b) const noexcept
        {
            return a.at != b.at ? a.at > b.at : a.sequence > b.sequence;
        }
    };

    platform::Instant now_{};
    std::uint64_t sequence_ = 0;
    std::int64_t wall_origin_ = 1'700'000'000; // 2023-11-14, so "plausible" means after 2020
    Random random_;
    std::priority_queue<Event, std::vector<Event>, Later> events_;
};

class FakeClock final : public platform::Clock
{
public:
    explicit FakeClock(FakeWorld& world) noexcept : world_(world) {}

    platform::Instant now() noexcept override
    {
        return world_.now();
    }
    void sleep_for(platform::Duration d) noexcept override;
    platform::Result<std::int64_t> wall_time_seconds() noexcept override;

private:
    FakeWorld& world_;
};

} // namespace adl::fake

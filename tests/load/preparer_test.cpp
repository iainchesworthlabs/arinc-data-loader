// SPDX-License-Identifier: GPL-3.0-or-later
// The thread preparer of `adl_load serve` on the real host: its thread, the order of its work, a poll that never
// waits, and what dropping it does. The fake platform runs a thread only when it is joined, so none of this can be
// shown there; the loop's own behaviour with preparations is tested with a scripted preparer (serve_test.cpp).

#include "adl/load/preparer.hpp"
#include "adl/platform/host.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

TEST_CASE(
    "the preparer runs the work of each request on a thread of its own, one at a time, and poll never waits for it",
    "[load][integration][serve][prepare]")
{
    auto host = adl::platform::make_host();
    REQUIRE(host.has_value());
    adl::load::ThreadPreparer preparer(**host);
    REQUIRE(preparer.start().has_value());

    std::atomic<bool> release{false};
    std::mutex mutex;
    std::vector<std::thread::id> threads;
    std::vector<std::uint64_t> order;
    const auto work = [&](std::uint64_t ticket, std::optional<std::string> problem, bool wait)
    {
        return [&, ticket, problem, wait]() -> adl::load::PreparationResult
        {
            while (wait && !release.load())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            const std::scoped_lock lock(mutex);
            threads.push_back(std::this_thread::get_id());
            order.push_back(ticket);
            return problem;
        };
    };
    preparer.submit(1, work(1, std::nullopt, true)); // held until the test lets it go
    preparer.submit(2, work(2, std::string{"the set has no such load"}, false));
    preparer.submit(3, work(3, std::nullopt, false));

    std::vector<adl::load::Preparation> done;
    preparer.poll((**host).clock().now(), done);
    CHECK(done.empty()); // the first is running, and poll did not wait for it, nor did the second run beside it
    release = true;

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (done.size() < 3 && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        preparer.poll((**host).clock().now(), done);
    }
    REQUIRE(done.size() == 3);
    CHECK(done[0].ticket == 1);
    CHECK_FALSE(done[0].problem.has_value());
    CHECK(done[1].ticket == 2);
    CHECK(done[1].problem == std::optional<std::string>{"the set has no such load"});
    CHECK(done[2].ticket == 3);
    CHECK_FALSE(done[2].problem.has_value());
    const std::scoped_lock lock(mutex);
    CHECK(order == std::vector<std::uint64_t>{1, 2, 3});
    REQUIRE(threads.size() == 3);
    CHECK(threads[0] != std::this_thread::get_id());
    CHECK(threads[0] == threads[1]);
    CHECK(threads[1] == threads[2]);
}

TEST_CASE("a preparer that is dropped waits for the work that is running and drops what has not begun",
          "[load][integration][serve][prepare]")
{
    auto host = adl::platform::make_host();
    REQUIRE(host.has_value());
    std::atomic<bool> release{false};
    std::atomic<int> began{0};
    std::atomic<int> second_ran{0};
    {
        adl::load::ThreadPreparer preparer(**host);
        REQUIRE(preparer.start().has_value());
        preparer.submit(1,
                        [&]() -> adl::load::PreparationResult
                        {
                            ++began;
                            while (!release.load())
                            {
                                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                            }
                            return std::nullopt;
                        });
        preparer.submit(2,
                        [&]() -> adl::load::PreparationResult
                        {
                            ++second_ran;
                            return std::nullopt;
                        });
        while (began.load() == 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        // The first is running and cannot be interrupted: the destructor waits for it, so the test lets it go from
        // here.
        std::thread releaser(
            [&]
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                release = true;
            });
        releaser.detach();
    } // ~ThreadPreparer
    CHECK(release.load()); // it waited for the work that was running
    CHECK(second_ran.load() == 0);

    // One that was never started, and one that was and never given any work, can be dropped.
    adl::load::ThreadPreparer never_started(**host);
    never_started.submit(1, [] { return adl::load::PreparationResult{}; }); // ignored: there is no thread
    std::vector<adl::load::Preparation> nothing;
    never_started.poll((**host).clock().now(), nothing);
    CHECK(nothing.empty());
    {
        adl::load::ThreadPreparer idle(**host);
        REQUIRE(idle.start().has_value());
    }
}

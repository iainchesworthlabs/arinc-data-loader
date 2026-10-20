// A test only Zephyr can run: a thread on a stack the caller made. The conformance suite spawns
// threads with the platform's stacks (dynamic); a platform that must not allocate gives the thread
// its own, as K_KERNEL_STACK_DEFINE makes it, and the port has to use it as it is.

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>

#include "adl/platform/host.hpp"

#include <zephyr/kernel.h>

using namespace adl::platform;

namespace {

K_KERNEL_STACK_DEFINE(caller_stack, 4096);

void count_run(void* argument) noexcept {
    static_cast<std::atomic<int>*>(argument)->fetch_add(1);
}

}  // namespace

TEST_CASE("a thread runs on a stack the caller supplies, again and again",
          "[platform][zephyr][threads]") {
    auto host = make_host();
    REQUIRE(host.has_value());

    std::atomic<int> runs{0};
    ThreadSpec spec;
    spec.entry = &count_run;
    spec.argument = &runs;
    spec.stack_storage = {reinterpret_cast<std::byte*>(caller_stack),
                          K_KERNEL_STACK_SIZEOF(caller_stack)};

    // The same storage is used by thread after thread, which only works if a finished thread
    // has really let go of it.
    for (int round = 1; round <= 3; ++round) {
        auto thread = (*host)->threads().spawn(spec);
        REQUIRE(thread.has_value());
        REQUIRE((*thread)->join().has_value());
        CHECK(runs.load() == round);
    }
}

TEST_CASE("storage that is not aligned for a stack is refused", "[platform][zephyr][threads]") {
    auto host = make_host();
    REQUIRE(host.has_value());

    std::atomic<int> runs{0};
    ThreadSpec spec;
    spec.entry = &count_run;
    spec.argument = &runs;
    spec.stack_storage = std::span<std::byte>{reinterpret_cast<std::byte*>(caller_stack),
                                              K_KERNEL_STACK_SIZEOF(caller_stack)}
                             .subspan(1);

    auto thread = (*host)->threads().spawn(spec);
    REQUIRE_FALSE(thread.has_value());
    CHECK(thread.error().code == ErrorCode::invalid_argument);
    CHECK(runs.load() == 0);
}

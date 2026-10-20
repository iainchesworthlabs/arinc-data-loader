// A test only FreeRTOS can run: a thread on a stack the caller made. The conformance suite spawns
// threads with the kernel's heap; a platform that must not allocate gives the task its own stack
// (xTaskCreateStatic), and the port has to use it as it is and let go of it when the task is gone.

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <span>

#include "adl/platform/host.hpp"

#include <FreeRTOS.h>

using namespace adl::platform;

namespace {

constexpr std::size_t stack_words = 4096;
alignas(8) StackType_t caller_stack[stack_words];

void count_run(void* argument) noexcept {
    static_cast<std::atomic<int>*>(argument)->fetch_add(1);
}

[[nodiscard]] std::span<std::byte> whole_stack() {
    return {reinterpret_cast<std::byte*>(caller_stack), sizeof caller_stack};
}

}  // namespace

TEST_CASE("a thread runs on a stack the caller supplies, again and again",
          "[platform][freertos][threads]") {
    auto host = make_host();
    REQUIRE(host.has_value());

    std::atomic<int> runs{0};
    ThreadSpec spec;
    spec.entry = &count_run;
    spec.argument = &runs;
    spec.stack_storage = whole_stack();

    // The same storage is used by task after task, which only works if a finished task has
    // really let go of it.
    for (int round = 1; round <= 3; ++round) {
        auto thread = (*host)->threads().spawn(spec);
        REQUIRE(thread.has_value());
        REQUIRE((*thread)->join().has_value());
        CHECK(runs.load() == round);
    }
}

TEST_CASE("storage that is misaligned or too small for a stack is refused",
          "[platform][freertos][threads]") {
    auto host = make_host();
    REQUIRE(host.has_value());

    std::atomic<int> runs{0};
    ThreadSpec spec;
    spec.entry = &count_run;
    spec.argument = &runs;

    spec.stack_storage = whole_stack().subspan(1);
    auto misaligned = (*host)->threads().spawn(spec);
    REQUIRE_FALSE(misaligned.has_value());
    CHECK(misaligned.error().code == ErrorCode::invalid_argument);

    spec.stack_storage = whole_stack().first(16);
    auto small = (*host)->threads().spawn(spec);
    REQUIRE_FALSE(small.has_value());
    CHECK(small.error().code == ErrorCode::invalid_argument);
    CHECK(runs.load() == 0);
}

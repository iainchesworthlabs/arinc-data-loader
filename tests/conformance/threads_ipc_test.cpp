// Conformance: threads, locks, semaphores and message queues. The checks that need
// real concurrency run on the real host only; the fake runs a thread when it is
// joined, so it cannot show two of them at once.

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cstring>
#include <vector>

#include "host_under_test.hpp"

using namespace adl::platform;
using namespace adl::test;
using namespace std::chrono_literals;

namespace {

struct Counter {
    std::atomic<int> value{0};
};

void bump(void* argument) noexcept {
    static_cast<Counter*>(argument)->value.fetch_add(1);
}

[[nodiscard]] std::span<const std::byte> bytes_of(const std::string& text) {
    return {reinterpret_cast<const std::byte*>(text.data()), text.size()};
}

[[nodiscard]] std::string text_of(std::span<const std::byte> bytes) {
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

}  // namespace

TEST_CASE("a thread runs its entry function and join waits for it",
          "[platform][conformance][threads]") {
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    Threading& threads = under.host().threads();

    Counter counter;
    ThreadSpec spec;
    spec.name = "conformance";
    spec.entry = &bump;
    spec.argument = &counter;
    auto thread = threads.spawn(spec);
    REQUIRE(thread.has_value());
    REQUIRE((*thread)->join().has_value());
    CHECK(counter.value.load() == 1);
    // A second join is harmless.
    CHECK((*thread)->join().has_value());
    CHECK(counter.value.load() == 1);
}

TEST_CASE("destroying a thread that has not been joined waits for it",
          "[platform][conformance][threads]") {
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    Counter counter;
    {
        ThreadSpec spec;
        spec.entry = &bump;
        spec.argument = &counter;
        auto thread = under.host().threads().spawn(spec);
        REQUIRE(thread.has_value());
    }
    CHECK(counter.value.load() == 1);
}

TEST_CASE("a thread needs an entry function", "[platform][conformance][threads]") {
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    auto thread = under.host().threads().spawn(ThreadSpec{});
    REQUIRE_FALSE(thread.has_value());
    CHECK(thread.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("a thread can be started at each priority and says what happened",
          "[platform][conformance][threads]") {
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);

    for (const Priority level :
         {Priority::lowest, Priority::low, Priority::normal, Priority::high, Priority::highest}) {
        CAPTURE(static_cast<int>(level));
        Counter counter;
        ThreadSpec spec;
        spec.priority = level;
        spec.entry = &bump;
        spec.argument = &counter;
        auto thread = under.host().threads().spawn(spec);
        // Whatever the platform thinks of the priority, the thread must run.
        REQUIRE(thread.has_value());
        const PriorityOutcome outcome = (*thread)->priority_outcome();
        CHECK((outcome == PriorityOutcome::applied || outcome == PriorityOutcome::not_permitted ||
               outcome == PriorityOutcome::unsupported));
        REQUIRE((*thread)->join().has_value());
        CHECK(counter.value.load() == 1);
        if (level == Priority::normal && !under.is_fake()) {
            // The default level is always available.
            CHECK(outcome == PriorityOutcome::applied);
        }
    }
}

TEST_CASE("a mutex excludes, and try_lock says whether it got the lock",
          "[platform][conformance][threads]") {
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    auto mutex = under.host().threads().create_mutex();
    REQUIRE(mutex.has_value());

    (*mutex)->lock();
    CHECK_FALSE((*mutex)->try_lock());
    (*mutex)->unlock();
    CHECK((*mutex)->try_lock());
    (*mutex)->unlock();
    {
        LockGuard guard(**mutex);
        CHECK_FALSE((*mutex)->try_lock());
    }
    CHECK((*mutex)->try_lock());
    (*mutex)->unlock();
}

TEST_CASE("a semaphore counts, times out, and has a ceiling", "[platform][conformance][threads]") {
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    Clock& clock = under.host().clock();
    auto semaphore = under.host().threads().create_semaphore(1, 2);
    REQUIRE(semaphore.has_value());

    // One to start with.
    CHECK((*semaphore)->wait(Duration::zero()).has_value());
    auto none = (*semaphore)->wait(Duration::zero());
    REQUIRE_FALSE(none.has_value());
    CHECK(none.error().code == ErrorCode::timeout);

    // A wait with a time limit takes at least that.
    const Instant before = clock.now();
    auto timed = (*semaphore)->wait(40ms);
    const Duration took = clock.now() - before;
    REQUIRE_FALSE(timed.has_value());
    CHECK(timed.error().code == ErrorCode::timeout);
    CHECK(took >= 40ms);
    CHECK(took < 40ms + 750ms);

    // Two posts fit; a third does not, and does not disturb the count.
    CHECK((*semaphore)->post().has_value());
    CHECK((*semaphore)->post().has_value());
    auto third = (*semaphore)->post();
    REQUIRE_FALSE(third.has_value());
    CHECK(third.error().code == ErrorCode::resource_exhausted);
    CHECK((*semaphore)->wait(Duration::zero()).has_value());
    CHECK((*semaphore)->wait(Duration::zero()).has_value());
    CHECK_FALSE((*semaphore)->wait(Duration::zero()).has_value());
}

TEST_CASE("a semaphore with no ceiling or a start above it is refused",
          "[platform][conformance][threads]") {
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    CHECK(under.host().threads().create_semaphore(0, 0).error().code ==
          ErrorCode::invalid_argument);
    CHECK(under.host().threads().create_semaphore(3, 2).error().code ==
          ErrorCode::invalid_argument);
}

// ---- message queues -------------------------------------------------------

TEST_CASE("a queue delivers messages whole and in order", "[platform][conformance][ipc]") {
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    auto queue = under.host().ipc().create_queue(QueueSpec{16, 4, {}});
    REQUIRE(queue.has_value());
    CHECK((*queue)->capacity() == 4);
    CHECK((*queue)->message_bytes() == 16);
    CHECK((*queue)->size() == 0);

    const std::vector<std::string> messages{"a", "", "sixteen bytes!!!", "four"};
    for (const auto& message : messages) {
        REQUIRE((*queue)->send(bytes_of(message), Duration::zero()).has_value());
    }
    CHECK((*queue)->size() == 4);

    SECTION("a full queue refuses another and says so") {
        const std::string extra = "x";
        auto refused = (*queue)->send(bytes_of(extra), Duration::zero());
        REQUIRE_FALSE(refused.has_value());
        CHECK(refused.error().code == ErrorCode::timeout);

        const Instant before = under.host().clock().now();
        auto waited = (*queue)->send(bytes_of(extra), 30ms);
        CHECK_FALSE(waited.has_value());
        CHECK(under.host().clock().now() - before >= 30ms);
        CHECK((*queue)->size() == 4);
    }

    SECTION("receiving gives the messages back as they were sent") {
        std::array<std::byte, 16> out{};
        for (const auto& message : messages) {
            const auto got = (*queue)->receive(out, Duration::zero());
            REQUIRE(got.has_value());
            CHECK(text_of({out.data(), *got}) == message);
        }
        CHECK((*queue)->size() == 0);
        auto empty = (*queue)->receive(out, Duration::zero());
        REQUIRE_FALSE(empty.has_value());
        CHECK(empty.error().code == ErrorCode::timeout);
    }

    SECTION("the ring wraps") {
        std::array<std::byte, 16> out{};
        for (int round = 0; round < 20; ++round) {
            const std::string message = "round " + std::to_string(round);
            REQUIRE((*queue)->receive(out, Duration::zero()).has_value());
            REQUIRE((*queue)->send(bytes_of(message), Duration::zero()).has_value());
        }
        // The four from the start are gone; the last four rounds are in, in order.
        for (const int round : {16, 17, 18, 19}) {
            const auto got = (*queue)->receive(out, Duration::zero());
            REQUIRE(got.has_value());
            CHECK(text_of({out.data(), *got}) == "round " + std::to_string(round));
        }
        CHECK((*queue)->size() == 0);
    }
}

TEST_CASE("a queue refuses a message too long and a buffer too short",
          "[platform][conformance][ipc]") {
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    auto queue = under.host().ipc().create_queue(QueueSpec{8, 2, {}});
    REQUIRE(queue.has_value());

    const std::string too_long = "123456789";
    CHECK((*queue)->send(bytes_of(too_long), Duration::zero()).error().code ==
          ErrorCode::invalid_argument);
    CHECK((*queue)->size() == 0);

    const std::string fits = "12345678";
    REQUIRE((*queue)->send(bytes_of(fits), Duration::zero()).has_value());
    std::array<std::byte, 7> short_buffer{};
    CHECK((*queue)->receive(short_buffer, Duration::zero()).error().code ==
          ErrorCode::invalid_argument);
    // The message is still there.
    CHECK((*queue)->size() == 1);
}

TEST_CASE("a queue is made from a valid description only", "[platform][conformance][ipc]") {
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    Ipc& ipc = under.host().ipc();

    CHECK(ipc.create_queue(QueueSpec{0, 4, {}}).error().code == ErrorCode::invalid_argument);
    CHECK(ipc.create_queue(QueueSpec{8, 0, {}}).error().code == ErrorCode::invalid_argument);

    SECTION("with storage from the caller") {
        std::vector<std::byte> storage(QueueSpec::storage_required(8, 3));
        auto queue = ipc.create_queue(QueueSpec{8, 3, storage});
        REQUIRE(queue.has_value());
        const std::string message = "stored";
        REQUIRE((*queue)->send(bytes_of(message), Duration::zero()).has_value());
        std::array<std::byte, 8> out{};
        const auto got = (*queue)->receive(out, Duration::zero());
        REQUIRE(got.has_value());
        CHECK(text_of({out.data(), *got}) == message);

        std::vector<std::byte> too_small(8);
        CHECK(ipc.create_queue(QueueSpec{8, 3, too_small}).error().code ==
              ErrorCode::invalid_argument);
    }
}

// ---- concurrency: real host only -------------------------------------------

namespace {

struct Producer {
    MessageQueue* queue = nullptr;
    int count = 0;
    void run() noexcept {
        for (int i = 0; i < count; ++i) {
            std::array<std::byte, 4> message{};
            std::memcpy(message.data(), &i, sizeof i);
            if (!queue->send(message, 5s).has_value()) {
                failed = true;
                return;
            }
        }
    }
    std::atomic<bool> failed{false};
};

struct Incrementer {
    Mutex* mutex = nullptr;
    long* shared = nullptr;
    int times = 0;
    void run() noexcept {
        for (int i = 0; i < times; ++i) {
            LockGuard guard(*mutex);
            ++*shared;
        }
    }
};

struct Waiter {
    Semaphore* semaphore = nullptr;
    std::atomic<bool> got{false};
    void run() noexcept { got = semaphore->wait(5s).has_value(); }
};

}  // namespace

TEST_CASE("a queue passes messages between threads without loss or reordering",
          "[platform][conformance][ipc][real]") {
    HostUnderTest under(HostKind::real);
    // Room for 3 only, so that the producer has to wait for the consumer.
    auto queue = under.host().ipc().create_queue(QueueSpec{4, 3, {}});
    REQUIRE(queue.has_value());

    Producer producer;
    producer.queue = queue->get();
    producer.count = 2000;
    auto thread = spawn_member(under.host().threads(), ThreadSpec{}, producer);
    REQUIRE(thread.has_value());

    std::array<std::byte, 4> out{};
    for (int expected = 0; expected < producer.count; ++expected) {
        const auto got = (*queue)->receive(out, 5s);
        REQUIRE(got.has_value());
        REQUIRE(*got == 4);
        int value = -1;
        std::memcpy(&value, out.data(), sizeof value);
        REQUIRE(value == expected);
    }
    REQUIRE((*thread)->join().has_value());
    CHECK_FALSE(producer.failed.load());
}

TEST_CASE("a mutex keeps the count right under contention",
          "[platform][conformance][threads][real]") {
    HostUnderTest under(HostKind::real);
    auto mutex = under.host().threads().create_mutex();
    REQUIRE(mutex.has_value());

    long shared = 0;
    constexpr int workers = 4;
    constexpr int times = 20'000;
    std::vector<Incrementer> jobs(workers);
    std::vector<Owned<Thread>> threads;
    for (auto& job : jobs) {
        job.mutex = mutex->get();
        job.shared = &shared;
        job.times = times;
        auto thread = spawn_member(under.host().threads(), ThreadSpec{}, job);
        REQUIRE(thread.has_value());
        threads.push_back(std::move(*thread));
    }
    for (auto& thread : threads) {
        REQUIRE(thread->join().has_value());
    }
    CHECK(shared == static_cast<long>(workers) * times);
}

TEST_CASE("a semaphore wakes a thread that is waiting on it",
          "[platform][conformance][threads][real]") {
    HostUnderTest under(HostKind::real);
    auto semaphore = under.host().threads().create_semaphore(0, 1);
    REQUIRE(semaphore.has_value());

    Waiter waiter;
    waiter.semaphore = semaphore->get();
    auto thread = spawn_member(under.host().threads(), ThreadSpec{}, waiter);
    REQUIRE(thread.has_value());

    under.host().clock().sleep_for(50ms);
    CHECK_FALSE(waiter.got.load());
    REQUIRE((*semaphore)->post().has_value());
    REQUIRE((*thread)->join().has_value());
    CHECK(waiter.got.load());
}

TEST_CASE("a wait that is not signalled gives up when its time is up, whatever else runs",
          "[platform][conformance][threads][real]") {
    HostUnderTest under(HostKind::real);
    auto semaphore = under.host().threads().create_semaphore(0, 1);
    REQUIRE(semaphore.has_value());
    Clock& clock = under.host().clock();

    // Another thread posting to a different semaphore must not end this wait early.
    auto other = under.host().threads().create_semaphore(0, 1);
    REQUIRE(other.has_value());
    Waiter busy;
    busy.semaphore = other->get();
    auto thread = spawn_member(under.host().threads(), ThreadSpec{}, busy);
    REQUIRE(thread.has_value());

    const Instant before = clock.now();
    auto result = (*semaphore)->wait(100ms);
    const Duration took = clock.now() - before;
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == ErrorCode::timeout);
    CHECK(took >= 100ms);

    REQUIRE((*other)->post().has_value());
    REQUIRE((*thread)->join().has_value());
}

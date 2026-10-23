// SPDX-License-Identifier: GPL-3.0-or-later
// A smoke test of the Zephyr platform layer on a board built with the Zephyr SDK's own toolchain
// (GCC, picolibc and its libstdc++, no exceptions): a Cortex-M3 under QEMU. It is not the
// conformance suite, which needs Catch2 and a file system and runs on native_sim
// (tests/platform/zephyr); it is the few things that show the platform layer compiles, links and
// runs on a real architecture, a real C library and a kernel configured for a small target: the
// clock, memory, a thread at every priority, a mutex, a semaphore, a message queue and UDP over
// Zephyr's loopback interface. It prints one "SMOKE:" line per check and ends with the count of
// failures, which tools/ci/zephyr_qemu_smoke.sh reads.

#include "adl/platform/host.hpp"

#include <zephyr/kernel.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>

using namespace adl::platform;
using namespace std::chrono_literals;

namespace
{

std::atomic<int> threads_run{0};
int failures = 0;

void check(bool ok, const char* what)
{
    std::printf("SMOKE: %s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok)
    {
        ++failures;
    }
}

void worker(void* argument) noexcept
{
    threads_run.fetch_add(1);
    (void)static_cast<Semaphore*>(argument)->post();
}

} // namespace

int main()
{
    auto made = make_host();
    if (!made)
    {
        std::printf("SMOKE: FAIL no host\n");
        std::printf("SMOKE: 1 failure(s)\n");
        return 1;
    }
    Host& host = **made;

    const Instant before = host.clock().now();
    host.clock().sleep_for(30ms);
    check(host.clock().now() - before >= 30ms, "sleep_for(30 ms) takes at least 30 ms");

    auto region = host.memory().acquire(1000, 64, MemoryUse::general);
    check(region.has_value() && reinterpret_cast<std::uintptr_t>(region->data()) % 64 == 0,
          "memory is 64 byte aligned");

    auto semaphore = host.threads().create_semaphore(0, 8);
    check(semaphore.has_value(), "a semaphore is made");
    if (semaphore)
    {
        for (const Priority level :
             {Priority::lowest, Priority::low, Priority::normal, Priority::high, Priority::highest})
        {
            ThreadSpec spec;
            spec.priority = level;
            spec.entry = &worker;
            spec.argument = semaphore->get();
            auto thread = host.threads().spawn(spec);
            check(thread.has_value() && (*thread)->priority_outcome() == PriorityOutcome::applied &&
                      (*semaphore)->wait(2s).has_value() && (*thread)->join().has_value(),
                  "a thread runs at a priority and is joined");
        }
        check(threads_run.load() == 5, "five threads ran");
    }

    auto mutex = host.threads().create_mutex();
    check(mutex.has_value(), "a mutex is made");
    if (mutex)
    {
        (*mutex)->lock();
        check(!(*mutex)->try_lock(), "try_lock by the holder fails");
        (*mutex)->unlock();
        check((*mutex)->try_lock(), "try_lock of a free mutex works");
        (*mutex)->unlock();
    }

    auto queue = host.ipc().create_queue(QueueSpec{16, 4, {}});
    const std::array<std::byte, 3> message{std::byte{1}, std::byte{2}, std::byte{3}};
    std::array<std::byte, 16> out{};
    check(queue.has_value() && (*queue)->send(message, 0ms).has_value() &&
              (*queue)->receive(out, 0ms).value_or(0) == 3 && out[2] == std::byte{3},
          "a message goes through a queue whole");

    UdpOptions options;
    options.local = Endpoint{Ipv4Address::loopback(), 0};
    auto first = host.network().open_udp(options);
    auto second = host.network().open_udp(options);
    check(first.has_value() && second.has_value(), "two UDP sockets are opened");
    if (first && second)
    {
        std::array<std::byte, 1400> payload{};
        payload[0] = std::byte{42};
        payload[1399] = std::byte{7};
        check((*first)->send_to((*second)->local_endpoint(), payload).has_value(), "a 1400 byte datagram is sent");
        std::array<std::byte, 2000> in{};
        const auto got = (*second)->receive_from(in, 2s);
        check(got.has_value() && got->size == 1400 && !got->truncated && in[0] == std::byte{42} &&
                  in[1399] == std::byte{7},
              "it arrives whole");
        const auto none = (*second)->receive_from(in, 50ms);
        check(!none.has_value() && none.error().code == ErrorCode::timeout, "a receive with nothing to read times out");
    }

    std::printf("SMOKE: %d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}

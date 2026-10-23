#pragma once

// A Host made of the fakes: a fake clock, network, files, memory, threads and
// queues, all driven by one FakeWorld. The engine, handed this Host, runs a whole
// transfer with no socket and no real second.
//
// Two ways to have one:
//   FakeSystem   - a world, a network and one Host at 127.0.0.1, all in one object.
//                  What most tests want, and what the conformance tests use.
//   FakeHost     - one Host on a world and network that are shared with others, to
//                  put a simulated loader and a simulated target on the same
//                  network at different addresses.

#include "adl/platform/host.hpp"
#include "adl/platform/ring_ipc.hpp"
#include "fake_network.hpp"
#include "fake_storage.hpp"
#include "fake_threading.hpp"
#include "fake_world.hpp"

#include <memory>

namespace adl::fake
{

class FakeHost final : public platform::Host
{
public:
    FakeHost(FakeWorld& world, FakeNetwork& network, platform::Ipv4Address address);

    platform::HostInfo info() const noexcept override
    {
        return {"fake", false};
    }
    platform::Clock& clock() noexcept override
    {
        return clock_;
    }
    platform::MemoryProvider& memory() noexcept override
    {
        return memory_;
    }
    platform::FileSystem& files() noexcept override
    {
        return files_;
    }
    platform::Threading& threads() noexcept override
    {
        return threads_;
    }
    platform::Network& network() noexcept override
    {
        return network_;
    }
    platform::Ipc& ipc() noexcept override
    {
        return ipc_;
    }

    // The concrete fakes, for the knobs a test turns.
    [[nodiscard]] FakeClock& fake_clock() noexcept
    {
        return clock_;
    }
    [[nodiscard]] FakeMemory& fake_memory() noexcept
    {
        return memory_;
    }
    [[nodiscard]] MemoryFileSystem& fake_files() noexcept
    {
        return files_;
    }
    [[nodiscard]] FakeThreading& fake_threads() noexcept
    {
        return threads_;
    }
    [[nodiscard]] FakeNetworkNode& fake_network() noexcept
    {
        return network_;
    }

private:
    FakeClock clock_;
    FakeMemory memory_;
    MemoryFileSystem files_;
    FakeThreading threads_;
    FakeNetworkNode network_;
    platform::RingIpc ipc_;
};

class FakeSystem
{
public:
    explicit FakeSystem(std::uint64_t seed = 1, platform::Ipv4Address address = platform::Ipv4Address::loopback())
        : world_(seed), network_(world_), host_(world_, network_, address)
    {
    }

    [[nodiscard]] FakeWorld& world() noexcept
    {
        return world_;
    }
    [[nodiscard]] FakeNetwork& network() noexcept
    {
        return network_;
    }
    [[nodiscard]] FakeHost& host() noexcept
    {
        return host_;
    }

private:
    FakeWorld world_;
    FakeNetwork network_;
    FakeHost host_;
};

} // namespace adl::fake

#pragma once

// A TargetLink over the fake in-memory network: an Agent at the target's address, so the mock
// engine reacts to every datagram as it arrives inside the world's events (as the simulated
// target of the engine tests does), and timers run on the fake world's clock. The engine's
// run() loop works over it too: each wait() advances the world, which delivers the datagrams
// and fires the timers straight into the engine.

#include "adl/mock/engine.hpp"
#include "fake_network.hpp"
#include "fake_world.hpp"

#include <atomic>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace adl::test
{

class FakeTargetLink final : public adl::mock::TargetLink, public adl::fake::Agent
{
public:
    FakeTargetLink(adl::fake::FakeWorld& world, adl::fake::FakeNetwork& network, adl::platform::Ipv4Address address)
        : world_(world), network_(network), address_(address)
    {
        network_.attach_agent(address_, *this);
    }
    ~FakeTargetLink() override
    {
        network_.detach_agent(address_);
    }
    FakeTargetLink(const FakeTargetLink&) = delete;
    FakeTargetLink& operator=(const FakeTargetLink&) = delete;

    // Agent: hands every datagram to the engine at once, inside the world's event.
    void on_datagram(adl::platform::Instant, const adl::platform::Endpoint& from, const adl::platform::Endpoint& to,
                     std::span<const std::byte> payload) override
    {
        if (engine_ != nullptr)
        {
            engine_->on_datagram(to.port, convert(from), payload);
        }
    }

    void attach(adl::mock::MockEngine& engine) noexcept override
    {
        engine_ = &engine;
    }

    void send(std::uint16_t from_port, const adl::arinc615a::Endpoint& to, std::span<const std::byte> payload) override
    {
        network_.send(adl::platform::Endpoint{address_, from_port}, convert(to), payload);
    }
    void after(adl::arinc615a::Duration delay, std::function<void()> action) override
    {
        world_.schedule_after(delay, std::move(action));
    }
    bool bind(std::uint16_t port, std::string&) override
    {
        // The agent takes everything addressed to the target, so nothing needs a socket.
        ports_.push_back(port);
        return true;
    }
    void set_local_address(const adl::arinc615a::Address& address) noexcept override
    {
        local_address_ = address;
    }
    // The interface the engine asked for (0.0.0.0 until it asks): the fake network has none.
    [[nodiscard]] const adl::arinc615a::Address& local_address() const noexcept
    {
        return local_address_;
    }
    [[nodiscard]] std::vector<std::uint16_t> ports() const override
    {
        return ports_;
    }
    std::int32_t wait(std::uint32_t timeout_ms) override
    {
        world_.advance_by(std::chrono::milliseconds(timeout_ms));
        return 0;
    }
    bool receive(std::uint16_t, adl::arinc615a::Endpoint&, std::span<std::byte>, std::size_t&) override
    {
        return false; // the agent delivered everything as it arrived
    }
    [[nodiscard]] adl::arinc615a::Instant now() const noexcept override
    {
        return adl::arinc615a::Instant{world_.now().since_origin};
    }
    void request_stop() noexcept override
    {
        stop_.store(true, std::memory_order_release);
    }
    [[nodiscard]] bool stop_requested() const noexcept override
    {
        return stop_.load(std::memory_order_acquire);
    }

private:
    [[nodiscard]] static adl::arinc615a::Endpoint convert(const adl::platform::Endpoint& endpoint)
    {
        return adl::arinc615a::Endpoint{adl::arinc615a::Address{endpoint.address.octets}, endpoint.port};
    }
    [[nodiscard]] static adl::platform::Endpoint convert(const adl::arinc615a::Endpoint& endpoint)
    {
        return adl::platform::Endpoint{adl::platform::Ipv4Address{endpoint.address.octets}, endpoint.port};
    }

    adl::fake::FakeWorld& world_;
    adl::fake::FakeNetwork& network_;
    adl::platform::Ipv4Address address_;
    adl::mock::MockEngine* engine_ = nullptr;
    std::vector<std::uint16_t> ports_;
    adl::arinc615a::Address local_address_{};
    std::atomic<bool> stop_{false};
};

} // namespace adl::test

#pragma once

// The seam the mock target's engine runs over: a small interface of datagrams and timers that
// has one implementation over adl::platform's real sockets (RealTargetLink) and one over the
// fake in-memory network (apps/mock_target/tests's FakeTargetLink), so the same engine serves
// demonstrations on real UDP sockets and is tested headless. It is deliberately minimal and
// single-threaded: the engine calls it from its one thread, and nothing here starts a thread.

#include "adl/arinc615a/endpoint.hpp"
#include "adl/arinc615a/time.hpp"
#include "adl/platform/error.hpp"
#include "adl/platform/host.hpp"
#include "adl/platform/network.hpp"
#include "adl/platform/owned.hpp"
#include "adl/platform/time.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace adl::mock
{

class MockEngine;

class TargetLink
{
public:
    virtual ~TargetLink() = default;

    // Sends one datagram from `from_port` (a port this link serves) to `to`. A datagram that
    // cannot be sent is dropped, as UDP does.
    virtual void send(std::uint16_t from_port, const adl::arinc615a::Endpoint& to,
                      std::span<const std::byte> payload) = 0;

    // Runs `action` after `delay`, on the engine's thread. The engine guards the callback, so
    // the link need not know about its lifetime.
    virtual void after(adl::arinc615a::Duration delay, std::function<void()> action) = 0;

    // Pre-binds a listening port (the FIND and TFTP ports). False, with `error` for the
    // operator, when the port cannot be bound.
    virtual bool bind(std::uint16_t port, std::string& error) = 0;

    // The interface the link's sockets are bound to, from the next bind() or send() on:
    // 0.0.0.0 (the default) listens on every interface; 127.0.0.1 only answers a loader on
    // this machine, and is the one binding an operating system firewall does not ask about.
    // The fake network has no interfaces and ignores it.
    virtual void set_local_address(const adl::arinc615a::Address& /*address*/) noexcept {}

    // The ports with a socket now (pre-bound ones, and the transfer ports opened by sends).
    [[nodiscard]] virtual std::vector<std::uint16_t> ports() const = 0;

    // Waits until a datagram arrives on one of the ports, or `timeout_ms` passes; callbacks
    // scheduled with after() run as they fall due. Returns how many ports have a datagram
    // (0 is a time-out), or -1 when waiting failed.
    [[nodiscard]] virtual std::int32_t wait(std::uint32_t timeout_ms) = 0;

    // Receives one datagram queued for `port` without waiting, if there is one. False when
    // there is none (or the datagram was larger than `out` and has been dropped).
    virtual bool receive(std::uint16_t port, adl::arinc615a::Endpoint& from, std::span<std::byte> out,
                         std::size_t& size) = 0;

    // The clock of the link: the platform's monotonic clock, or the fake world's.
    [[nodiscard]] virtual adl::arinc615a::Instant now() const noexcept = 0;

    // The model asked the engine to stop. Thread safe; the engine polls it in its loop.
    virtual void request_stop() noexcept = 0;
    [[nodiscard]] virtual bool stop_requested() const noexcept = 0;

    // Gives the engine the link was made for, so a link that dispatches datagrams itself
    // (the fake, whose agent runs inside the world's events) knows where to hand them. The
    // real link does nothing: the engine drains its sockets in its own loop.
    virtual void attach(MockEngine& /*engine*/) noexcept {}
};

// The link over adl::platform: lazily opens one UDP socket per port (a send opens the socket
// on demand; bind() pre-binds the listening ports with reuse_address and a receive buffer),
// waits on all of them with Network::wait_readable, and keeps the after() callbacks in a
// min-heap of deadlines that wait() fires as they fall due. It owns the Host, which the
// factory made on the engine's thread (once per session, like the application's).
class RealTargetLink final : public TargetLink
{
public:
    explicit RealTargetLink(std::unique_ptr<adl::platform::Host> host) : host_(std::move(host)) {}

    void send(std::uint16_t from_port, const adl::arinc615a::Endpoint& to, std::span<const std::byte> payload) override;
    void after(adl::arinc615a::Duration delay, std::function<void()> action) override;
    bool bind(std::uint16_t port, std::string& error) override;
    void set_local_address(const adl::arinc615a::Address& address) noexcept override
    {
        local_ = adl::platform::Ipv4Address{address.octets};
    }
    [[nodiscard]] std::vector<std::uint16_t> ports() const override;
    [[nodiscard]] std::int32_t wait(std::uint32_t timeout_ms) override;
    bool receive(std::uint16_t port, adl::arinc615a::Endpoint& from, std::span<std::byte> out,
                 std::size_t& size) override;
    [[nodiscard]] adl::arinc615a::Instant now() const noexcept override;
    void request_stop() noexcept override
    {
        stop_.store(true, std::memory_order_release);
    }
    [[nodiscard]] bool stop_requested() const noexcept override
    {
        return stop_.load(std::memory_order_acquire);
    }

private:
    void fire_due(adl::arinc615a::Instant now);

    std::unique_ptr<adl::platform::Host> host_;
    adl::platform::Ipv4Address local_ = adl::platform::Ipv4Address::any();
    std::map<std::uint16_t, adl::platform::Owned<adl::platform::UdpSocket>> sockets_;
    // The scheduled callbacks, by deadline (equal deadlines run in the order scheduled).
    std::multimap<adl::arinc615a::Instant, std::function<void()>> timers_;
    std::atomic<bool> stop_{false};
};

} // namespace adl::mock

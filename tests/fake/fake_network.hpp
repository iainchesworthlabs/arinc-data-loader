// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// A network in memory. Datagrams sent through it are delivered by the
// FakeWorld's event queue after a latency, and may on the way be lost,
// duplicated, corrupted or reordered, either at random (from a seed, so the same
// seed is the same history) or by a rule the test writes ("drop the third
// DATA packet"). A simulated target can be attached at an address as an Agent,
// which is handed each datagram sent to it and replies through the network in
// reply.

#include "adl/platform/network.hpp"
#include "fake_world.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <span>
#include <vector>

namespace adl::fake
{

struct LinkProfile
{
    platform::Duration latency = std::chrono::microseconds(200);
    // Each datagram is delayed by a further random time up to this. Anything
    // above zero can reorder datagrams that were sent close together.
    platform::Duration jitter{};
    std::uint32_t loss_per_million = 0;
    std::uint32_t duplicate_per_million = 0;
    std::uint32_t corrupt_per_million = 0; // one bit of the payload flipped
};

enum class Verdict : std::uint8_t
{
    deliver, // as the profile decides
    drop,
    duplicate, // two copies
    corrupt,   // one bit flipped
};

struct PacketInfo
{
    platform::Endpoint from;
    platform::Endpoint to;
    platform::Instant sent;
    std::span<const std::byte> payload;
    std::uint64_t sequence = 0; // 0 for the first datagram sent through the network
};

// A datagram as it was sent, kept for the test to look at.
struct CapturedPacket
{
    platform::Endpoint from;
    platform::Endpoint to;
    platform::Instant sent;
    std::vector<std::byte> payload;
    Verdict verdict = Verdict::deliver;
    bool lost = false;
};

// Something at an address that is not a Host: a simulated target.
class Agent
{
public:
    virtual ~Agent() = default;
    virtual void on_datagram(platform::Instant at, const platform::Endpoint& from, const platform::Endpoint& to,
                             std::span<const std::byte> payload) = 0;
};

class FakeNetwork
{
public:
    explicit FakeNetwork(FakeWorld& world) noexcept : world_(world) {}
    FakeNetwork(const FakeNetwork&) = delete;
    FakeNetwork& operator=(const FakeNetwork&) = delete;

    void set_profile(const LinkProfile& profile)
    {
        profile_ = profile;
    }
    [[nodiscard]] const LinkProfile& profile() const noexcept
    {
        return profile_;
    }

    // Decides the fate of each datagram before the profile does; return `deliver`
    // to leave it to the profile. Called once per datagram, in send order.
    void set_filter(std::function<Verdict(const PacketInfo&)> filter)
    {
        filter_ = std::move(filter);
    }

    void attach_agent(const platform::Ipv4Address& address, Agent& agent);
    void detach_agent(const platform::Ipv4Address& address);

    // Sends a datagram from `from` to `to` as a Host's socket would; an Agent
    // uses it to reply.
    void send(const platform::Endpoint& from, const platform::Endpoint& to, std::span<const std::byte> payload);

    [[nodiscard]] const std::vector<CapturedPacket>& captured() const noexcept
    {
        return captured_;
    }
    void clear_capture()
    {
        captured_.clear();
    }

    struct Counters
    {
        std::uint64_t sent = 0;
        std::uint64_t delivered = 0; // copies that reached a socket or an agent
        std::uint64_t dropped = 0;   // lost by the profile, a filter, or no one listening
        std::uint64_t duplicated = 0;
        std::uint64_t corrupted = 0;
        std::uint64_t overflowed = 0; // dropped because a socket's buffer was full
    };
    [[nodiscard]] const Counters& counters() const noexcept
    {
        return counters_;
    }

    [[nodiscard]] FakeWorld& world() noexcept
    {
        return world_;
    }

    // ---- for FakeNetworkNode ------------------------------------------------

    struct SocketState
    {
        platform::Ipv4Address host; // the Host it belongs to
        platform::Endpoint local;   // what it is bound to
        bool allow_broadcast = false;
        bool reuse_address = false;
        std::size_t capacity_bytes = 0; // receive buffer
        std::size_t queued_bytes = 0;
        struct Queued
        {
            platform::Endpoint source;
            std::vector<std::byte> data;
        };
        std::deque<Queued> queue;
    };
    [[nodiscard]] std::shared_ptr<SocketState> bind(const platform::Ipv4Address& host,
                                                    const platform::UdpOptions& options, platform::ErrorCode& error);
    void unbind(const std::shared_ptr<SocketState>& socket);

private:
    void deliver(const platform::Endpoint& from, const platform::Endpoint& to, const std::vector<std::byte>& payload);
    void deliver_to_socket(SocketState& socket, const platform::Endpoint& from, const std::vector<std::byte>& payload);

    FakeWorld& world_;
    LinkProfile profile_;
    std::function<Verdict(const PacketInfo&)> filter_;
    std::map<std::array<std::uint8_t, 4>, Agent*> agents_;
    std::vector<std::shared_ptr<SocketState>> sockets_;
    std::vector<CapturedPacket> captured_;
    Counters counters_;
    std::uint64_t sequence_ = 0;
    std::uint16_t next_ephemeral_ = 49152;
};

// The Network of one fake Host: sockets opened through it belong to `address`.
class FakeNetworkNode final : public platform::Network
{
public:
    FakeNetworkNode(FakeNetwork& network, platform::Ipv4Address address) noexcept : network_(network), address_(address)
    {
    }

    [[nodiscard]] platform::Ipv4Address address() const noexcept
    {
        return address_;
    }

    platform::Result<platform::Owned<platform::UdpSocket>>
    open_udp(const platform::UdpOptions& options) noexcept override;
    platform::Result<std::size_t> wait_readable(std::span<platform::UdpSocket* const> sockets,
                                                platform::Duration timeout, std::span<bool> ready) noexcept override;
    std::size_t max_datagram_bytes() const noexcept override
    {
        return 65507;
    }

private:
    FakeNetwork& network_;
    platform::Ipv4Address address_;
};

} // namespace adl::fake

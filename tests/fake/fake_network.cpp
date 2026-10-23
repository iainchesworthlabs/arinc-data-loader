#include "fake_network.hpp"

#include <algorithm>
#include <cstring>

namespace adl::fake
{
namespace
{

using platform::Endpoint;
using platform::ErrorCode;
using platform::Ipv4Address;

constexpr std::size_t default_receive_buffer = 64 * 1024;

[[nodiscard]] bool is_loopback(const Ipv4Address& address) noexcept
{
    return address.octets[0] == 127;
}

[[nodiscard]] bool overlaps(const Ipv4Address& a, const Ipv4Address& b) noexcept
{
    return a.is_any() || b.is_any() || a == b;
}

class FakeUdpSocket final : public platform::UdpSocket
{
public:
    FakeUdpSocket(FakeNetwork& network, std::shared_ptr<FakeNetwork::SocketState> state) noexcept
        : network_(network), state_(std::move(state))
    {
    }
    ~FakeUdpSocket() override
    {
        network_.unbind(state_);
    }

    [[nodiscard]] bool readable() const noexcept
    {
        return !state_->queue.empty();
    }

    Endpoint local_endpoint() const noexcept override
    {
        return state_->local;
    }

    platform::Result<void> send_to(const Endpoint& destination, std::span<const std::byte> data) noexcept override
    {
        if (data.size() > 65507)
        {
            return platform::fail(ErrorCode::message_too_large);
        }
        if (destination.address.is_broadcast() && !state_->allow_broadcast)
        {
            return platform::fail(ErrorCode::permission_denied);
        }
        // A datagram to the loopback address stays on the host and shows it as the
        // sender; any other carries the host's address.
        Endpoint source{is_loopback(destination.address) ? destination.address : state_->host, state_->local.port};
        network_.send(source, destination, data);
        return {};
    }

    platform::Result<platform::Datagram> receive_from(std::span<std::byte> buffer,
                                                      platform::Duration timeout) noexcept override
    {
        if (!network_.world().wait_until([this] { return readable(); }, timeout))
        {
            return platform::fail(ErrorCode::timeout);
        }
        auto message = std::move(state_->queue.front());
        state_->queue.pop_front();
        state_->queued_bytes -= message.data.size();
        platform::Datagram datagram;
        datagram.size = std::min(buffer.size(), message.data.size());
        datagram.truncated = message.data.size() > buffer.size();
        datagram.source = message.source;
        if (datagram.size != 0)
        {
            std::memcpy(buffer.data(), message.data.data(), datagram.size);
        }
        return datagram;
    }

private:
    FakeNetwork& network_;
    std::shared_ptr<FakeNetwork::SocketState> state_;
};

} // namespace

void FakeNetwork::attach_agent(const Ipv4Address& address, Agent& agent)
{
    agents_[address.octets] = &agent;
}

void FakeNetwork::detach_agent(const Ipv4Address& address)
{
    agents_.erase(address.octets);
}

std::shared_ptr<FakeNetwork::SocketState> FakeNetwork::bind(const Ipv4Address& host,
                                                            const platform::UdpOptions& options, ErrorCode& error)
{
    const Ipv4Address wanted = options.local.address;
    if (!wanted.is_any() && !is_loopback(wanted) && wanted != host)
    {
        error = ErrorCode::address_unavailable;
        return nullptr;
    }
    std::uint16_t port = options.local.port;
    const auto in_use = [&](std::uint16_t candidate, bool strict)
    {
        return std::any_of(sockets_.begin(), sockets_.end(),
                           [&](const auto& other)
                           {
                               return other->host == host && other->local.port == candidate &&
                                      overlaps(other->local.address, wanted) &&
                                      (strict || !(other->reuse_address && options.reuse_address));
                           });
    };
    if (port == 0)
    {
        // The next unused port of the ephemeral range, as an OS would hand out.
        for (std::uint32_t tries = 0; tries < 16384; ++tries)
        {
            const std::uint16_t candidate = next_ephemeral_;
            next_ephemeral_ =
                next_ephemeral_ == 65535 ? std::uint16_t{49152} : static_cast<std::uint16_t>(next_ephemeral_ + 1);
            if (!in_use(candidate, true))
            {
                port = candidate;
                break;
            }
        }
        if (port == 0)
        {
            error = ErrorCode::resource_exhausted;
            return nullptr;
        }
    }
    else if (in_use(port, false))
    {
        error = ErrorCode::address_in_use;
        return nullptr;
    }
    auto state = std::make_shared<SocketState>();
    state->host = host;
    state->local = Endpoint{wanted, port};
    state->allow_broadcast = options.allow_broadcast;
    state->reuse_address = options.reuse_address;
    state->capacity_bytes = options.receive_buffer_bytes != 0 ? options.receive_buffer_bytes : default_receive_buffer;
    sockets_.push_back(state);
    return state;
}

void FakeNetwork::unbind(const std::shared_ptr<SocketState>& socket)
{
    sockets_.erase(std::remove(sockets_.begin(), sockets_.end(), socket), sockets_.end());
}

void FakeNetwork::send(const Endpoint& from, const Endpoint& to, std::span<const std::byte> payload)
{
    const std::uint64_t sequence = sequence_++;
    ++counters_.sent;
    const PacketInfo info{from, to, world_.now(), payload, sequence};

    Verdict verdict = filter_ ? filter_(info) : Verdict::deliver;
    if (verdict == Verdict::deliver)
    {
        if (world_.random().chance(profile_.loss_per_million))
        {
            verdict = Verdict::drop;
        }
        else if (world_.random().chance(profile_.duplicate_per_million))
        {
            verdict = Verdict::duplicate;
        }
        else if (world_.random().chance(profile_.corrupt_per_million))
        {
            verdict = Verdict::corrupt;
        }
    }

    CapturedPacket capture{from, to, world_.now(), {payload.begin(), payload.end()}, verdict, false};
    if (verdict == Verdict::drop)
    {
        ++counters_.dropped;
        capture.lost = true;
        captured_.push_back(std::move(capture));
        return;
    }
    captured_.push_back(capture);

    std::vector<std::byte> bytes(payload.begin(), payload.end());
    if (verdict == Verdict::corrupt && !bytes.empty())
    {
        ++counters_.corrupted;
        const std::uint64_t bit = world_.random().below(bytes.size() * 8);
        bytes[bit / 8] ^= static_cast<std::byte>(1U << (bit % 8));
    }
    const int copies = verdict == Verdict::duplicate ? 2 : 1;
    if (copies == 2)
    {
        ++counters_.duplicated;
    }
    for (int copy = 0; copy < copies; ++copy)
    {
        platform::Duration delay = profile_.latency;
        if (profile_.jitter > platform::Duration::zero())
        {
            delay += platform::Duration(static_cast<std::int64_t>(
                world_.random().below(static_cast<std::uint64_t>(profile_.jitter.count()) + 1)));
        }
        world_.schedule_after(delay, [this, from, to, bytes] { deliver(from, to, bytes); });
    }
}

void FakeNetwork::deliver_to_socket(SocketState& socket, const Endpoint& from, const std::vector<std::byte>& payload)
{
    if (socket.queued_bytes + payload.size() > socket.capacity_bytes)
    {
        ++counters_.overflowed;
        ++counters_.dropped;
        return;
    }
    socket.queued_bytes += payload.size();
    socket.queue.push_back(SocketState::Queued{from, payload});
    ++counters_.delivered;
}

void FakeNetwork::deliver(const Endpoint& from, const Endpoint& to, const std::vector<std::byte>& payload)
{
    bool taken = false;
    if (to.address.is_broadcast())
    {
        for (const auto& socket : sockets_)
        {
            const bool sender = socket->host == from.address && socket->local.port == from.port;
            if (socket->local.port == to.port && !sender)
            {
                deliver_to_socket(*socket, from, payload);
                taken = true;
            }
        }
        for (const auto& [octets, agent] : agents_)
        {
            if (octets != from.address.octets)
            {
                agent->on_datagram(world_.now(), from, to, payload);
                ++counters_.delivered;
                taken = true;
            }
        }
    }
    else
    {
        const Ipv4Address destination_host = is_loopback(to.address) ? from.address : to.address;
        for (const auto& socket : sockets_)
        {
            if (socket->host == destination_host && socket->local.port == to.port &&
                (socket->local.address.is_any() || socket->local.address == to.address))
            {
                deliver_to_socket(*socket, from, payload);
                taken = true;
                break;
            }
        }
        if (!taken)
        {
            if (const auto agent = agents_.find(to.address.octets); agent != agents_.end())
            {
                agent->second->on_datagram(world_.now(), from, to, payload);
                ++counters_.delivered;
                taken = true;
            }
        }
    }
    if (!taken)
    {
        ++counters_.dropped; // nobody was listening: UDP does not say so
    }
}

platform::Result<platform::Owned<platform::UdpSocket>>
FakeNetworkNode::open_udp(const platform::UdpOptions& options) noexcept
{
    ErrorCode error = ErrorCode::other;
    auto state = network_.bind(address_, options, error);
    if (!state)
    {
        return platform::fail(error);
    }
    return platform::adopt<platform::UdpSocket>(std::make_unique<FakeUdpSocket>(network_, std::move(state)));
}

platform::Result<std::size_t> FakeNetworkNode::wait_readable(std::span<platform::UdpSocket* const> sockets,
                                                             platform::Duration timeout, std::span<bool> ready) noexcept
{
    if (sockets.size() > platform::max_wait_sockets || ready.size() != sockets.size())
    {
        return platform::fail(ErrorCode::invalid_argument);
    }
    const auto any_readable = [&]
    {
        return std::any_of(sockets.begin(), sockets.end(),
                           [](platform::UdpSocket* socket) { return static_cast<FakeUdpSocket*>(socket)->readable(); });
    };
    network_.world().wait_until(any_readable, timeout);
    std::size_t count = 0;
    for (std::size_t i = 0; i < sockets.size(); ++i)
    {
        ready[i] = static_cast<FakeUdpSocket*>(sockets[i])->readable();
        count += ready[i] ? 1U : 0U;
    }
    return count;
}

} // namespace adl::fake

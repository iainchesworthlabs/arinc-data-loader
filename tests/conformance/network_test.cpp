// SPDX-License-Identifier: GPL-3.0-or-later
// Conformance: UDP networking. Every check runs against the fake network and,
// over the loopback interface, against the real one. The loopback tests at the
// end (tagged [integration]) are the real host only: they use real threads.

#include "host_under_test.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <array>
#include <atomic>
#include <cstring>
#include <numeric>
#include <vector>

using namespace adl::platform;
using namespace adl::test;
using namespace std::chrono_literals;

namespace
{

[[nodiscard]] std::vector<std::byte> pattern(std::size_t size, unsigned seed = 1)
{
    std::vector<std::byte> bytes(size);
    unsigned state = seed;
    for (auto& byte : bytes)
    {
        state = state * 1664525U + 1013904223U;
        byte = static_cast<std::byte>(state >> 24);
    }
    return bytes;
}

[[nodiscard]] UdpOptions loopback_options()
{
    UdpOptions options;
    options.local = Endpoint{Ipv4Address::loopback(), 0};
    return options;
}

[[nodiscard]] Owned<UdpSocket> open(Network& network, const UdpOptions& options = loopback_options())
{
    auto socket = network.open_udp(options);
    REQUIRE(socket.has_value());
    return std::move(*socket);
}

} // namespace

TEST_CASE("a datagram arrives whole with the address it came from", "[platform][conformance][network]")
{
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    Network& network = under.host().network();
    auto receiver = open(network);
    auto sender = open(network);
    REQUIRE(receiver->local_endpoint().port != 0);
    REQUIRE(sender->local_endpoint().port != 0);
    CHECK(receiver->local_endpoint().port != sender->local_endpoint().port);
    CHECK(receiver->local_endpoint().address == Ipv4Address::loopback());

    // The sizes that matter to TFTP: nothing, one byte, the default block plus its
    // header, a block with the checksum option's headroom, and the largest RFC 2348 block.
    for (const std::size_t size :
         {std::size_t{0}, std::size_t{1}, std::size_t{516}, std::size_t{1472}, std::size_t{8196}, std::size_t{65468}})
    {
        CAPTURE(size);
        const auto payload = pattern(size, static_cast<unsigned>(size));
        REQUIRE(sender->send_to(receiver->local_endpoint(), payload).has_value());

        std::vector<std::byte> buffer(70'000);
        const auto got = receiver->receive_from(buffer, 2s);
        REQUIRE(got.has_value());
        CHECK(got->size == size);
        CHECK_FALSE(got->truncated);
        CHECK(got->source.port == sender->local_endpoint().port);
        CHECK(got->source.address == Ipv4Address::loopback());
        CHECK(std::equal(payload.begin(), payload.end(), buffer.begin()));
    }
}

TEST_CASE("datagrams arrive in the order sent on one path", "[platform][conformance][network]")
{
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    Network& network = under.host().network();
    auto receiver = open(network);
    auto sender = open(network);

    constexpr int count = 50;
    for (int i = 0; i < count; ++i)
    {
        std::array<std::byte, 4> message{};
        std::memcpy(message.data(), &i, sizeof i);
        REQUIRE(sender->send_to(receiver->local_endpoint(), message).has_value());
    }
    for (int i = 0; i < count; ++i)
    {
        std::array<std::byte, 16> buffer{};
        const auto got = receiver->receive_from(buffer, 2s);
        REQUIRE(got.has_value());
        int value = -1;
        std::memcpy(&value, buffer.data(), sizeof value);
        REQUIRE(value == i);
    }
}

TEST_CASE("receiving gives up at the time limit", "[platform][conformance][network]")
{
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    auto socket = open(under.host().network());
    Clock& clock = under.host().clock();
    std::array<std::byte, 64> buffer{};

    const Instant before = clock.now();
    const auto none = socket->receive_from(buffer, 60ms);
    const Duration took = clock.now() - before;
    REQUIRE_FALSE(none.has_value());
    CHECK(none.error().code == ErrorCode::timeout);
    CHECK(took >= 60ms);
    CHECK(took < 60ms + 750ms);

    // A poll does not wait.
    const Instant poll_start = clock.now();
    CHECK(socket->receive_from(buffer, Duration::zero()).error().code == ErrorCode::timeout);
    CHECK(clock.now() - poll_start < 250ms);
}

TEST_CASE("a datagram that is already queued is returned without waiting out the limit",
          "[platform][conformance][network]")
{
    // The case where the datagram arrives during the wait is the last test of this file,
    // on the real host: it needs a second thread.
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    Network& network = under.host().network();
    auto receiver = open(network);
    auto sender = open(network);

    const auto payload = pattern(10);
    REQUIRE(sender->send_to(receiver->local_endpoint(), payload).has_value());
    Clock& clock = under.host().clock();
    std::array<std::byte, 64> buffer{};
    const Instant before = clock.now();
    const auto got = receiver->receive_from(buffer, 10s);
    REQUIRE(got.has_value());
    CHECK(clock.now() - before < 5s);
}

TEST_CASE("a port can be bound once, and a second bind says so", "[platform][conformance][network]")
{
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    Network& network = under.host().network();
    auto first = open(network);
    const Endpoint taken = first->local_endpoint();

    UdpOptions again = loopback_options();
    again.local = taken;
    auto second = network.open_udp(again);
    REQUIRE_FALSE(second.has_value());
    CHECK(second.error().code == ErrorCode::address_in_use);

    SECTION("the port is free again once the first socket is closed")
    {
        first.reset();
        auto third = network.open_udp(again);
        CHECK(third.has_value());
    }
    SECTION("two sockets that both allow sharing may bind the same port")
    {
        first.reset();
        UdpOptions share = again;
        share.reuse_address = true;
        auto a = network.open_udp(share);
        REQUIRE(a.has_value());
        auto b = network.open_udp(share);
        CHECK(b.has_value());
    }
}

TEST_CASE("an address that is not the host's cannot be bound", "[platform][conformance][network]")
{
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    UdpOptions options;
    // TEST-NET-1 (RFC 5737): reserved for documentation, never a local address.
    options.local = Endpoint{Ipv4Address{{192, 0, 2, 77}}, 0};
    auto socket = under.host().network().open_udp(options);
    REQUIRE_FALSE(socket.has_value());
    CHECK(socket.error().code == ErrorCode::address_unavailable);
}

TEST_CASE("waiting on several sockets reports which have something", "[platform][conformance][network]")
{
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    Network& network = under.host().network();
    Clock& clock = under.host().clock();
    auto a = open(network);
    auto b = open(network);
    auto c = open(network);
    auto sender = open(network);
    std::array<UdpSocket*, 3> sockets{a.get(), b.get(), c.get()};
    std::array<bool, 3> ready{};

    SECTION("nothing to read is a time-out, not an error")
    {
        const Instant before = clock.now();
        const auto count = network.wait_readable(sockets, 50ms, ready);
        REQUIRE(count.has_value());
        CHECK(*count == 0);
        CHECK(clock.now() - before >= 50ms);
        CHECK(ready == std::array<bool, 3>{false, false, false});
    }
    SECTION("one with a datagram")
    {
        const auto payload = pattern(5);
        REQUIRE(sender->send_to(b->local_endpoint(), payload).has_value());
        const auto count = network.wait_readable(sockets, 2s, ready);
        REQUIRE(count.has_value());
        CHECK(*count == 1);
        CHECK(ready == std::array<bool, 3>{false, true, false});
        // It stays ready until read.
        CHECK(*network.wait_readable(sockets, Duration::zero(), ready) == 1);
        std::array<std::byte, 16> buffer{};
        REQUIRE(b->receive_from(buffer, Duration::zero()).has_value());
        CHECK(*network.wait_readable(sockets, Duration::zero(), ready) == 0);
    }
    SECTION("more than one")
    {
        const auto payload = pattern(5);
        REQUIRE(sender->send_to(a->local_endpoint(), payload).has_value());
        REQUIRE(sender->send_to(c->local_endpoint(), payload).has_value());
        // Both are there by the time a wait for both has seen them.
        std::size_t seen = 0;
        for (int tries = 0; tries < 50 && seen < 2; ++tries)
        {
            const auto count = network.wait_readable(sockets, 100ms, ready);
            REQUIRE(count.has_value());
            seen = *count;
        }
        CHECK(seen == 2);
        CHECK(ready == std::array<bool, 3>{true, false, true});
    }
    SECTION("the spans have to match, and there is a limit")
    {
        std::array<bool, 2> short_ready{};
        CHECK(network.wait_readable(sockets, Duration::zero(), short_ready).error().code ==
              ErrorCode::invalid_argument);

        std::vector<UdpSocket*> many(max_wait_sockets + 1, a.get());
        const auto flags = std::make_unique<bool[]>(many.size());
        CHECK(network.wait_readable(many, Duration::zero(), {flags.get(), many.size()}).error().code ==
              ErrorCode::invalid_argument);
    }
}

TEST_CASE("a datagram larger than the buffer is cut and says so", "[platform][conformance][network]")
{
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    Network& network = under.host().network();
    auto receiver = open(network);
    auto sender = open(network);

    const auto payload = pattern(100);
    REQUIRE(sender->send_to(receiver->local_endpoint(), payload).has_value());
    std::array<std::byte, 40> buffer{};
    const auto got = receiver->receive_from(buffer, 2s);
    REQUIRE(got.has_value());
    CHECK(got->truncated);
    CHECK(got->size == 40);
    CHECK(std::equal(buffer.begin(), buffer.end(), payload.begin()));

    // The rest is gone, not left for the next read.
    CHECK(receiver->receive_from(buffer, 30ms).error().code == ErrorCode::timeout);
}

TEST_CASE("a datagram over the maximum is refused", "[platform][conformance][network]")
{
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    Network& network = under.host().network();
    CHECK(network.max_datagram_bytes() == 65507);
    auto socket = open(network);

    const std::vector<std::byte> too_big(network.max_datagram_bytes() + 1);
    const auto result = socket->send_to(socket->local_endpoint(), too_big);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().code == ErrorCode::message_too_large);
}

TEST_CASE("sending to a port nobody is listening on does not upset the sender", "[platform][conformance][network]")
{
    // Windows reports the ICMP "port unreachable" from such a send as an error of the
    // next receive on the same socket; the interface promises it never does.
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    Network& network = under.host().network();
    auto sender = open(network);
    Endpoint nobody;
    {
        auto gone = open(network);
        nobody = gone->local_endpoint();
    }
    const auto payload = pattern(20);
    for (int i = 0; i < 3; ++i)
    {
        REQUIRE(sender->send_to(nobody, payload).has_value());
    }
    std::array<std::byte, 64> buffer{};
    const auto none = sender->receive_from(buffer, 100ms);
    REQUIRE_FALSE(none.has_value());
    CHECK(none.error().code == ErrorCode::timeout);

    // And the socket still works.
    auto friend_socket = open(network);
    REQUIRE(friend_socket->send_to(sender->local_endpoint(), payload).has_value());
    CHECK(sender->receive_from(buffer, 2s).has_value());
}

TEST_CASE("broadcast needs the socket to ask for it", "[platform][conformance][network]")
{
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    Network& network = under.host().network();
    const Endpoint everyone{Ipv4Address::broadcast(), 1001};
    const auto payload = pattern(2);

    SECTION("without permission")
    {
        auto socket = open(network);
        const auto refused = socket->send_to(everyone, payload);
        REQUIRE_FALSE(refused.has_value());
        // Permission is what is wrong; a system that finds there is no route first
        // (a machine with no network) says that instead.
        const ErrorCode code = refused.error().code;
        CHECK((code == ErrorCode::permission_denied || code == ErrorCode::unreachable ||
               code == ErrorCode::address_unavailable));
    }
    SECTION("with it the socket is made, and the send works or says there is no route")
    {
        UdpOptions options = loopback_options();
        options.local = Endpoint{Ipv4Address::any(), 0};
        options.allow_broadcast = true;
        auto socket = open(network, options);
        const auto result = socket->send_to(everyone, payload);
        if (!result.has_value())
        {
            // A machine with no network (a CI container) has nowhere to broadcast to.
            const ErrorCode code = result.error().code;
            CHECK((code == ErrorCode::unreachable || code == ErrorCode::address_unavailable ||
                   code == ErrorCode::permission_denied));
        }
    }
}

TEST_CASE("a socket asked for with a receive buffer size is made", "[platform][conformance][network]")
{
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    UdpOptions options = loopback_options();
    options.receive_buffer_bytes = 256 * 1024;
    CHECK(under.host().network().open_udp(options).has_value());
}

// ---- loopback integration over real UDP -------------------------------------

namespace
{

// Echoes datagrams until it receives one that is a single zero byte.
struct EchoServer
{
    Network* network = nullptr;
    UdpSocket* socket = nullptr;
    std::atomic<int> echoed{0};
    std::atomic<bool> failed{false};

    void run() noexcept
    {
        std::array<std::byte, 2048> buffer{};
        for (;;)
        {
            const auto got = socket->receive_from(buffer, 5s);
            if (!got.has_value())
            {
                failed = true;
                return;
            }
            if (got->size == 1 && buffer[0] == std::byte{0})
            {
                return;
            }
            if (!socket->send_to(got->source, {buffer.data(), got->size}).has_value())
            {
                failed = true;
                return;
            }
            ++echoed;
        }
    }
};

} // namespace

TEST_CASE("a thousand datagrams make the round trip over real loopback UDP",
          "[platform][conformance][network][integration][real]")
{
    HostUnderTest under(HostKind::real);
    Network& network = under.host().network();
    auto server_socket = open(network);
    auto client = open(network);

    EchoServer server;
    server.network = &network;
    server.socket = server_socket.get();
    auto thread = spawn_member(under.host().threads(), ThreadSpec{"echo", Priority::high}, server);
    REQUIRE(thread.has_value());

    constexpr int rounds = 1000;
    std::array<std::byte, 2048> buffer{};
    for (int i = 0; i < rounds; ++i)
    {
        const auto message = pattern(static_cast<std::size_t>(1 + i % 1400), static_cast<unsigned>(i));
        REQUIRE(client->send_to(server_socket->local_endpoint(), message).has_value());
        const auto back = client->receive_from(buffer, 5s);
        REQUIRE(back.has_value());
        REQUIRE(back->size == message.size());
        REQUIRE(std::equal(message.begin(), message.end(), buffer.begin()));
        REQUIRE(back->source == server_socket->local_endpoint());
    }
    const std::array<std::byte, 1> stop{std::byte{0}};
    REQUIRE(client->send_to(server_socket->local_endpoint(), stop).has_value());
    REQUIRE((*thread)->join().has_value());
    CHECK(server.echoed.load() == rounds);
    CHECK_FALSE(server.failed.load());
}

TEST_CASE("one thread serves several sockets with wait_readable over real loopback UDP",
          "[platform][conformance][network][integration][real]")
{
    // The shape of the loader's runner: a control socket and several transfer sockets
    // in one wait set, each datagram answered on the socket it came in on.
    HostUnderTest under(HostKind::real);
    Network& network = under.host().network();
    std::vector<Owned<UdpSocket>> servers;
    std::vector<UdpSocket*> wait_set;
    for (int i = 0; i < 5; ++i)
    {
        servers.push_back(open(network));
        wait_set.push_back(servers.back().get());
    }
    auto client = open(network);

    struct Runner
    {
        Network* network;
        std::vector<UdpSocket*>* sockets;
        std::atomic<int> served{0};
        void run() noexcept
        {
            std::array<bool, 5> ready{};
            std::array<std::byte, 64> buffer{};
            while (served.load() < 50)
            {
                const auto count = network->wait_readable(*sockets, 5s, ready);
                if (!count.has_value() || *count == 0)
                {
                    return;
                }
                for (std::size_t i = 0; i < sockets->size(); ++i)
                {
                    if (!ready[i])
                    {
                        continue;
                    }
                    const auto got = (*sockets)[i]->receive_from(buffer, Duration::zero());
                    if (got.has_value())
                    {
                        // Say which socket answered.
                        buffer[0] = static_cast<std::byte>(i);
                        (void)(*sockets)[i]->send_to(got->source, {buffer.data(), got->size});
                        ++served;
                    }
                }
            }
        }
    } runner{&network, &wait_set};
    auto thread = spawn_member(under.host().threads(), ThreadSpec{}, runner);
    REQUIRE(thread.has_value());

    std::array<std::byte, 64> buffer{};
    for (int i = 0; i < 50; ++i)
    {
        const std::size_t target = static_cast<std::size_t>(i) % servers.size();
        const std::array<std::byte, 8> message{};
        REQUIRE(client->send_to(servers[target]->local_endpoint(), message).has_value());
        const auto back = client->receive_from(buffer, 5s);
        REQUIRE(back.has_value());
        REQUIRE(back->source == servers[target]->local_endpoint());
        REQUIRE(buffer[0] == static_cast<std::byte>(target));
    }
    REQUIRE((*thread)->join().has_value());
    CHECK(runner.served.load() == 50);
}

TEST_CASE("a datagram sent from another thread wakes a receive that is waiting",
          "[platform][conformance][network][integration][real]")
{
    HostUnderTest under(HostKind::real);
    Network& network = under.host().network();
    auto receiver = open(network);
    auto sender = open(network);

    struct Delayed
    {
        Clock* clock;
        UdpSocket* from;
        Endpoint to;
        void run() noexcept
        {
            clock->sleep_for(150ms);
            const std::array<std::byte, 3> message{};
            (void)from->send_to(to, message);
        }
    } delayed{&under.host().clock(), sender.get(), receiver->local_endpoint()};
    auto thread = spawn_member(under.host().threads(), ThreadSpec{}, delayed);
    REQUIRE(thread.has_value());

    std::array<std::byte, 16> buffer{};
    const Instant before = under.host().clock().now();
    const auto got = receiver->receive_from(buffer, 10s);
    const Duration took = under.host().clock().now() - before;
    REQUIRE(got.has_value());
    CHECK(got->size == 3);
    CHECK(took >= 100ms);
    CHECK(took < 5s);
    REQUIRE((*thread)->join().has_value());
}

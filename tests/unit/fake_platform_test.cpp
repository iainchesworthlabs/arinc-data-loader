// SPDX-License-Identifier: GPL-3.0-or-later
// The fake platform itself: its clock, its event queue, its network with faults,
// its storage and threads. The conformance tests show that it behaves like a real
// host; these show what it adds, which is what the engine's tests are built on.

#include "fake_host.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <string>
#include <vector>

using namespace adl::fake;
using namespace adl::platform;
using namespace std::chrono_literals;

namespace
{

[[nodiscard]] std::vector<std::byte> bytes_of(std::string_view text)
{
    std::vector<std::byte> bytes(text.size());
    if (!text.empty())
    {
        std::memcpy(bytes.data(), text.data(), text.size());
    }
    return bytes;
}

[[nodiscard]] std::string text_of(std::span<const std::byte> bytes)
{
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

[[nodiscard]] Owned<UdpSocket> bind(Network& network, Endpoint local)
{
    UdpOptions options;
    options.local = local;
    auto socket = network.open_udp(options);
    REQUIRE(socket.has_value());
    return std::move(*socket);
}

// Replies to each datagram with the same bytes, from its own address, a little later.
class EchoAgent final : public Agent
{
public:
    EchoAgent(FakeNetwork& network, Ipv4Address self, Duration think) : network_(network), self_(self), think_(think) {}
    void on_datagram(Instant, const Endpoint& from, const Endpoint& to, std::span<const std::byte> payload) override
    {
        ++received;
        std::vector<std::byte> copy(payload.begin(), payload.end());
        const Endpoint reply_from{self_, to.port};
        network_.world().schedule_after(think_,
                                        [this, from, reply_from, copy] { network_.send(reply_from, from, copy); });
    }
    int received = 0;

private:
    FakeNetwork& network_;
    Ipv4Address self_;
    Duration think_;
};

} // namespace

TEST_CASE("the fake clock moves only when told to", "[platform][fake]")
{
    FakeWorld world;
    FakeClock clock(world);
    const Instant start = clock.now();
    CHECK(clock.now() == start);
    clock.sleep_for(13s);
    CHECK(clock.now() - start == 13s);
    clock.sleep_for(-5s);
    CHECK(clock.now() - start == 13s);
    const auto wall = clock.wall_time_seconds();
    REQUIRE(wall.has_value());
    CHECK(*wall == world.wall_origin() + 13);
}

TEST_CASE("events run in time order, and those at one instant in the order scheduled", "[platform][fake]")
{
    FakeWorld world;
    std::vector<int> order;
    world.schedule_after(30ms, [&] { order.push_back(3); });
    world.schedule_after(10ms, [&] { order.push_back(1); });
    world.schedule_after(10ms, [&] { order.push_back(2); });
    world.schedule_after(50ms, [&] { order.push_back(5); });
    world.schedule_after(30ms,
                         [&]
                         {
                             order.push_back(4);
                             // An event can schedule another, for now or later.
                             world.schedule_after(Duration::zero(), [&] { order.push_back(40); });
                         });

    world.advance_by(30ms);
    CHECK(order == std::vector<int>{1, 2, 3, 4, 40});
    CHECK(world.now().since_origin == 30ms);
    CHECK(world.next_event_time()->since_origin == 50ms);
    world.advance_by(1s);
    CHECK(order.back() == 5);
    CHECK_FALSE(world.next_event_time().has_value());
    CHECK_FALSE(world.step());
    CHECK(world.now().since_origin == 1030ms);
}

TEST_CASE("waiting runs the events that fall inside the limit and stops at it", "[platform][fake]")
{
    FakeWorld world;
    int fired = 0;
    world.schedule_after(100ms, [&] { ++fired; });
    world.schedule_after(900ms, [&] { ++fired; });

    CHECK_FALSE(world.wait_until([&] { return fired >= 2; }, 500ms));
    CHECK(fired == 1);
    CHECK(world.now().since_origin == 500ms);
    CHECK(world.wait_until([&] { return fired >= 2; }, 500ms));
    // It stopped as soon as the condition held, not at the limit.
    CHECK(world.now().since_origin == 900ms);

    SECTION("with no limit and nothing left to happen it does not hang")
    {
        CHECK_FALSE(world.wait_until([] { return false; }, forever));
    }
}

TEST_CASE("a datagram arrives after the latency, not before", "[platform][fake][network]")
{
    FakeSystem system;
    system.network().set_profile(LinkProfile{50ms, {}, 0, 0, 0});
    Network& network = system.host().network();
    auto a = bind(network, {Ipv4Address::loopback(), 0});
    auto b = bind(network, {Ipv4Address::loopback(), 0});
    std::array<std::byte, 16> buffer{};

    REQUIRE(a->send_to(b->local_endpoint(), bytes_of("hello")).has_value());
    CHECK(b->receive_from(buffer, 49ms).error().code == ErrorCode::timeout);
    const auto got = b->receive_from(buffer, 5ms);
    REQUIRE(got.has_value());
    CHECK(text_of({buffer.data(), got->size}) == "hello");
    // The wait ended when the datagram arrived, not at the end of its limit.
    CHECK(system.world().now().since_origin == 50ms);
}

TEST_CASE("a loss profile loses about that share, the same way for the same seed", "[platform][fake][network]")
{
    const auto run = [](std::uint64_t seed)
    {
        FakeSystem system(seed);
        LinkProfile profile;
        profile.loss_per_million = 100'000; // 10 %
        system.network().set_profile(profile);
        Network& network = system.host().network();
        auto a = bind(network, {Ipv4Address::loopback(), 0});
        auto b = bind(network, {Ipv4Address::loopback(), 0});
        std::vector<bool> lost;
        for (int i = 0; i < 5000; ++i)
        {
            REQUIRE(a->send_to(b->local_endpoint(), bytes_of("x")).has_value());
        }
        for (const auto& packet : system.network().captured())
        {
            lost.push_back(packet.lost);
        }
        return lost;
    };

    const auto first = run(7);
    const auto count = std::count(first.begin(), first.end(), true);
    CHECK(count > 350);
    CHECK(count < 650);
    CHECK(run(7) == first);
    CHECK(run(8) != first);
}

TEST_CASE("a filter can drop, duplicate or corrupt chosen datagrams", "[platform][fake][network]")
{
    FakeSystem system;
    Network& network = system.host().network();
    auto a = bind(network, {Ipv4Address::loopback(), 0});
    auto b = bind(network, {Ipv4Address::loopback(), 0});
    system.network().set_filter(
        [](const PacketInfo& packet)
        {
            switch (packet.sequence)
            {
            case 1:
                return Verdict::drop;
            case 2:
                return Verdict::duplicate;
            case 3:
                return Verdict::corrupt;
            default:
                return Verdict::deliver;
            }
        });
    for (const char* text : {"zero", "one", "two", "three", "four"})
    {
        REQUIRE(a->send_to(b->local_endpoint(), bytes_of(text)).has_value());
    }
    std::vector<std::string> received;
    std::array<std::byte, 16> buffer{};
    for (;;)
    {
        const auto got = b->receive_from(buffer, 10ms);
        if (!got.has_value())
        {
            break;
        }
        received.push_back(text_of({buffer.data(), got->size}));
    }
    // "one" was dropped, "two" came twice, "three" has one bit changed.
    REQUIRE(received.size() == 5);
    CHECK(received[0] == "zero");
    CHECK(received[1] == "two");
    CHECK(received[2] == "two");
    CHECK(received[3] != "three");
    CHECK(received[3].size() == 5);
    int differing_bits = 0;
    for (std::size_t i = 0; i < 5; ++i)
    {
        differing_bits += std::popcount(
            static_cast<unsigned>(static_cast<unsigned char>(received[3][i]) ^ static_cast<unsigned char>("three"[i])));
    }
    CHECK(differing_bits == 1);
    CHECK(received[4] == "four");

    const auto& counters = system.network().counters();
    CHECK(counters.sent == 5);
    CHECK(counters.dropped == 1);
    CHECK(counters.duplicated == 1);
    CHECK(counters.corrupted == 1);
    REQUIRE(system.network().captured().size() == 5);
    CHECK(system.network().captured()[1].lost);
    CHECK(system.network().captured()[2].verdict == Verdict::duplicate);
}

TEST_CASE("jitter reorders datagrams sent close together, and loses none", "[platform][fake][network]")
{
    FakeSystem system(3);
    LinkProfile profile;
    profile.latency = 1ms;
    profile.jitter = 40ms;
    system.network().set_profile(profile);
    Network& network = system.host().network();
    auto a = bind(network, {Ipv4Address::loopback(), 0});
    auto b = bind(network, {Ipv4Address::loopback(), 0});

    constexpr int count = 200;
    for (int i = 0; i < count; ++i)
    {
        std::array<std::byte, 4> message{};
        std::memcpy(message.data(), &i, sizeof i);
        REQUIRE(a->send_to(b->local_endpoint(), message).has_value());
    }
    std::vector<int> order;
    std::array<std::byte, 16> buffer{};
    while (const auto got = b->receive_from(buffer, 100ms))
    {
        int value = -1;
        std::memcpy(&value, buffer.data(), sizeof value);
        order.push_back(value);
    }
    REQUIRE(order.size() == count);
    CHECK_FALSE(std::is_sorted(order.begin(), order.end()));
    std::sort(order.begin(), order.end());
    std::vector<int> expected(count);
    for (int i = 0; i < count; ++i)
    {
        expected[static_cast<std::size_t>(i)] = i;
    }
    CHECK(order == expected);
}

TEST_CASE("an agent at an address answers datagrams sent to it", "[platform][fake][network]")
{
    // Not the loopback address, which the fake treats as "this host" and nothing else.
    FakeSystem system(1, *Ipv4Address::parse("10.0.0.1"));
    const Ipv4Address target = *Ipv4Address::parse("10.0.0.50");
    EchoAgent agent(system.network(), target, 300ms);
    system.network().attach_agent(target, agent);

    Network& network = system.host().network();
    auto socket = bind(network, {Ipv4Address::any(), 0});
    REQUIRE(socket->send_to({target, 59}, bytes_of("RRQ")).has_value());

    std::array<std::byte, 16> buffer{};
    const Instant before = system.world().now();
    const auto got = socket->receive_from(buffer, 2s);
    REQUIRE(got.has_value());
    CHECK(text_of({buffer.data(), got->size}) == "RRQ");
    CHECK(got->source == Endpoint{target, 59});
    CHECK(agent.received == 1);
    // Out, the agent's thinking time, and back.
    CHECK(system.world().now() - before >= 300ms);
}

TEST_CASE("two hosts on one network reach each other at their own addresses", "[platform][fake][network]")
{
    FakeWorld world;
    FakeNetwork net(world);
    FakeHost loader(world, net, *Ipv4Address::parse("10.0.0.1"));
    FakeHost target(world, net, *Ipv4Address::parse("10.0.0.2"));

    auto control = bind(target.network(), {Ipv4Address::any(), 59});
    auto client = bind(loader.network(), {Ipv4Address::any(), 0});
    REQUIRE(client->send_to({*Ipv4Address::parse("10.0.0.2"), 59}, bytes_of("WRQ")).has_value());

    std::array<std::byte, 16> buffer{};
    const auto got = control->receive_from(buffer, 1s);
    REQUIRE(got.has_value());
    // The sender is seen at its own address, not at 127.0.0.1.
    CHECK(got->source.address == *Ipv4Address::parse("10.0.0.1"));
    CHECK(got->source.port == client->local_endpoint().port);

    // Loopback stays on the host: the target's port is not reachable that way from the loader.
    REQUIRE(client->send_to({Ipv4Address::loopback(), 59}, bytes_of("x")).has_value());
    CHECK(control->receive_from(buffer, 100ms).error().code == ErrorCode::timeout);

    // And the same port on the two hosts is two ports.
    CHECK(bind(loader.network(), {Ipv4Address::any(), 59}) != nullptr);
}

TEST_CASE("a broadcast reaches the others on that port and not the sender", "[platform][fake][network]")
{
    FakeWorld world;
    FakeNetwork net(world);
    FakeHost loader(world, net, *Ipv4Address::parse("10.0.0.1"));
    FakeHost target(world, net, *Ipv4Address::parse("10.0.0.2"));
    const Ipv4Address third = *Ipv4Address::parse("10.0.0.3");
    EchoAgent agent(net, third, 100ms);
    net.attach_agent(third, agent);

    UdpOptions find;
    find.local = {Ipv4Address::any(), 1001};
    find.allow_broadcast = true;
    find.reuse_address = true;
    auto asker = loader.network().open_udp(find);
    auto listener = target.network().open_udp(find);
    REQUIRE(asker.has_value());
    REQUIRE(listener.has_value());

    REQUIRE((*asker)->send_to({Ipv4Address::broadcast(), 1001}, bytes_of("IRQ")).has_value());

    std::array<std::byte, 16> buffer{};
    const auto heard = (*listener)->receive_from(buffer, 1s);
    REQUIRE(heard.has_value());
    CHECK(text_of({buffer.data(), heard->size}) == "IRQ");
    CHECK(agent.received == 1);
    // The asker did not hear its own question, but does hear the agent's answer.
    const auto answer = (*asker)->receive_from(buffer, 1s);
    REQUIRE(answer.has_value());
    CHECK(answer->source.address == *Ipv4Address::parse("10.0.0.3"));
}

TEST_CASE("a full receive buffer drops datagrams as UDP does", "[platform][fake][network]")
{
    FakeSystem system;
    Network& network = system.host().network();
    UdpOptions small;
    small.local = {Ipv4Address::loopback(), 0};
    small.receive_buffer_bytes = 300;
    auto receiver = network.open_udp(small);
    REQUIRE(receiver.has_value());
    auto sender = bind(network, {Ipv4Address::loopback(), 0});

    const std::vector<std::byte> block(100);
    for (int i = 0; i < 6; ++i)
    {
        REQUIRE(sender->send_to((*receiver)->local_endpoint(), block).has_value());
    }
    int received = 0;
    std::array<std::byte, 128> buffer{};
    while ((*receiver)->receive_from(buffer, 10ms).has_value())
    {
        ++received;
    }
    CHECK(received == 3);
    CHECK(system.network().counters().overflowed == 3);
}

TEST_CASE("the in-memory file system can be filled and loaded from a test", "[platform][fake][files]")
{
    MemoryFileSystem files;
    files.put("/media/MEDIA_1/LOADS.LUM", "loads");
    files.put("/media/MEDIA_1/LOAD/PART.LUP", std::vector<std::byte>(10, std::byte{9}));

    CHECK(files.stat("/media/MEDIA_1")->kind == FileKind::directory);
    CHECK(files.stat("/media")->kind == FileKind::directory);
    REQUIRE(files.get("/media/MEDIA_1/LOADS.LUM").has_value());
    CHECK(text_of(*files.get("/media/MEDIA_1/LOADS.LUM")) == "loads");
    CHECK(files.get("/media/MEDIA_1/LOAD/PART.LUP")->size() == 10);
    CHECK_FALSE(files.get("/media/missing").has_value());
    CHECK(files.used_bytes() == 15);

    SECTION("a one-shot write failure")
    {
        auto out = files.create("/media/out", CreateMode::exclusive);
        REQUIRE(out.has_value());
        files.fail_next_write(ErrorCode::io_error);
        const auto data = bytes_of("abc");
        CHECK((*out)->write(data).error().code == ErrorCode::io_error);
        CHECK((*out)->write(data).has_value());
        CHECK(files.get("/media/out")->size() == 3);
    }
    SECTION("a directory moves with what is in it")
    {
        REQUIRE(files.rename("/media/MEDIA_1", "/media/MEDIA_2").has_value());
        CHECK(files.get("/media/MEDIA_2/LOAD/PART.LUP").has_value());
        CHECK_FALSE(files.get("/media/MEDIA_1/LOADS.LUM").has_value());
    }
    SECTION("dot-dot is resolved, and cannot climb out of the root")
    {
        CHECK(files.stat("/media/MEDIA_1/LOAD/../LOADS.LUM")->size == 5);
        CHECK(files.stat("/../x").error().code == ErrorCode::invalid_argument);
    }
}

TEST_CASE("the fake memory provider has a budget", "[platform][fake][memory]")
{
    FakeMemory memory;
    memory.set_budget(1000);
    auto a = memory.acquire(600, 16, MemoryUse::general);
    REQUIRE(a.has_value());
    CHECK(memory.outstanding_bytes() == 600);
    CHECK(memory.acquire(600, 16, MemoryUse::general).error().code == ErrorCode::resource_exhausted);
    auto b = memory.acquire(400, 16, MemoryUse::general);
    REQUIRE(b.has_value());
    a->reset();
    CHECK(memory.outstanding_bytes() == 400);
    CHECK(memory.acquire(600, 16, MemoryUse::general).has_value());

    memory.deny_resident(true);
    CHECK(memory.acquire(8, 8, MemoryUse::resident).error().code == ErrorCode::resource_exhausted);
}

TEST_CASE("fake threads run when joined, in order, and queues and semaphores work between them",
          "[platform][fake][threads]")
{
    FakeSystem system;
    Threading& threads = system.host().threads();
    std::vector<int> order;
    struct Job
    {
        std::vector<int>* order;
        int id;
        void run() noexcept
        {
            order->push_back(id);
        }
    };
    Job one{&order, 1};
    Job two{&order, 2};
    Job three{&order, 3};

    auto t1 = spawn_member(threads, ThreadSpec{}, one);
    auto t2 = spawn_member(threads, ThreadSpec{}, two);
    auto t3 = spawn_member(threads, ThreadSpec{}, three);
    REQUIRE((t1.has_value() && t2.has_value() && t3.has_value()));
    CHECK(order.empty());
    CHECK(system.host().fake_threads().spawned() == 3);

    // Joining the second runs it alone; the rest run together on request.
    REQUIRE((*t2)->join().has_value());
    CHECK(order == std::vector<int>{2});
    CHECK(system.host().fake_threads().run_pending() == 2);
    CHECK(order == std::vector<int>{2, 1, 3});

    SECTION("a semaphore that nothing will post times out on the fake clock")
    {
        auto semaphore = threads.create_semaphore(0, 1);
        REQUIRE(semaphore.has_value());
        const Instant before = system.world().now();
        CHECK((*semaphore)->wait(13s).error().code == ErrorCode::timeout);
        CHECK(system.world().now() - before == 13s);
        // Waiting for ever for something that cannot come does not hang the test.
        CHECK((*semaphore)->wait(forever).error().code == ErrorCode::timeout);
    }
    SECTION("an event in the world can post a semaphore that is being waited on")
    {
        auto semaphore = threads.create_semaphore(0, 1);
        REQUIRE(semaphore.has_value());
        Semaphore* raw = semaphore->get();
        system.world().schedule_after(2s, [raw] { (void)raw->post(); });
        CHECK((*semaphore)->wait(13s).has_value());
        CHECK(system.world().now().since_origin == 2s);
    }
}

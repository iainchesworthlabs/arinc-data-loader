// FIND (Attachment 3), the data loader's side: the state machine on its own, and the runner
// broadcasting over the fake network to simulated targets.

#include <catch2/catch_test_macros.hpp>

#include "adl/arinc615a/find.hpp"
#include "adl/arinc615a/find_client.hpp"
#include "rig.hpp"

using namespace adl::arinc615a;
using namespace adl::test;

namespace {

using std::chrono::milliseconds;
using std::chrono::seconds;

constexpr Endpoint target_a{Address{{10, 0, 0, 2}}, 1001};
constexpr Endpoint target_b{Address{{10, 0, 0, 3}}, 1001};

Bytes answer_bytes(const FindAnswer& answer) {
    Bytes bytes(find_max_answer_size);
    const auto size = encode_find_answer(answer, bytes);
    bytes.resize(size.value_or(0));
    return bytes;
}

const FindAnswer left{"HNPFMS", "FMS", "L", "FMS LEFT", "HNP"};
const FindAnswer right{"HNPFMS", "FMS", "R", "FMS RIGHT", "HNP"};

}  // namespace

TEST_CASE("the FIND request is the opcode, a null and 0x10", "[engine][find]") {
    FindClient client{FindConfig{}};
    client.start(Instant{});
    const auto request = client.request();
    REQUIRE(request.size() == 4);
    CHECK(std::to_integer<int>(request[0]) == 0);
    CHECK(std::to_integer<int>(request[1]) == 1);
    CHECK(std::to_integer<int>(request[2]) == 0);
    CHECK(std::to_integer<int>(request[3]) == 0x10);
    // It goes to the limited broadcast address unless told otherwise.
    REQUIRE(client.destinations().size() == 1);
    CHECK(client.destinations()[0] == Address::broadcast());
}

TEST_CASE("valid answers are registered for three seconds and then the window closes",
          "[engine][find]") {
    FindClient client{FindConfig{}};
    client.start(Instant{});
    CHECK(client.next_wake() == Instant{seconds(3)});
    client.on_datagram(Instant{milliseconds(100)}, target_a, answer_bytes(left));
    client.on_datagram(Instant{milliseconds(2900)}, target_b, answer_bytes(right));
    CHECK_FALSE(client.done());
    client.on_timer(Instant{seconds(3) - Duration(1)});
    CHECK_FALSE(client.done());
    client.on_timer(Instant{seconds(3)});
    CHECK(client.done());
    CHECK(client.next_wake() == never);
    REQUIRE(client.targets().size() == 2);
    const FindTarget& first = client.targets()[0];
    CHECK(first.from == target_a);
    CHECK(first.hardware_id.view() == "HNPFMS");
    CHECK(first.type_name.view() == "FMS");
    CHECK(first.position.view() == "L");
    CHECK(first.literal_name.view() == "FMS LEFT");
    CHECK(first.manufacturer_code.view() == "HNP");
    CHECK(client.targets()[1].position.view() == "R");
}

TEST_CASE("an answer after the window is not registered", "[engine][find]") {
    FindClient client{FindConfig{}};
    client.start(Instant{});
    client.on_datagram(Instant{seconds(3) + Duration(1)}, target_a, answer_bytes(left));
    CHECK(client.done());
    CHECK(client.targets().empty());
    client.on_datagram(Instant{seconds(4)}, target_a, answer_bytes(left));
    CHECK(client.targets().empty());
}

TEST_CASE("what is not a valid answer is ignored", "[engine][find]") {
    FindClient client{FindConfig{}};
    client.start(Instant{});
    const Bytes good = answer_bytes(left);

    Bytes request(8);
    request.resize(encode_find_request(request).value());  // our own request, looped back
    client.on_datagram(Instant{}, target_a, request);

    Bytes cut(good.begin(), good.end() - 1);  // no packet terminator
    client.on_datagram(Instant{}, target_a, cut);
    Bytes trailing = good;
    trailing.push_back(std::byte{0});  // bytes after the terminator
    client.on_datagram(Instant{}, target_a, trailing);
    Bytes opcode = good;
    opcode[1] = std::byte{9};
    client.on_datagram(Instant{}, target_a, opcode);
    client.on_datagram(Instant{}, target_a, {});
    // An answer that names no hardware identifier names nothing to load.
    client.on_datagram(Instant{}, target_a, answer_bytes(FindAnswer{"", "FMS", "L", "x", "HNP"}));
    // A manufacturer code of four characters, as the standard's own example prints it.
    Bytes four = good;
    const std::string_view text = "7HNP";
    // (HNP\0 is at the end before the terminator; make it four characters)
    four.insert(four.end() - 2, std::byte{'7'});
    static_cast<void>(text);
    client.on_datagram(Instant{}, target_a, four);

    CHECK(client.targets().empty());
    CHECK(client.ignored() == 7);

    client.on_datagram(Instant{}, target_a, good);
    CHECK(client.targets().size() == 1);
}

TEST_CASE("an answer repeated is registered once, and a second identity from one host is kept",
          "[engine][find]") {
    FindClient client{FindConfig{}};
    client.start(Instant{});
    client.on_datagram(Instant{}, target_a, answer_bytes(left));
    client.on_datagram(Instant{}, target_a, answer_bytes(left));  // the same answer again
    CHECK(client.targets().size() == 1);
    CHECK(client.ignored() == 1);
    // A concentrator answering for another unit (3-5, the commentary on proxies).
    client.on_datagram(Instant{}, target_a, answer_bytes(right));
    CHECK(client.targets().size() == 2);
    // The same identity from another address is another target.
    client.on_datagram(Instant{}, target_b, answer_bytes(left));
    CHECK(client.targets().size() == 3);
}

TEST_CASE("there is room for a limited number of targets", "[engine][find]") {
    FindClient client{FindConfig{}};
    client.start(Instant{});
    for (std::size_t i = 0; i < FindClient::max_targets + 5; ++i) {
        const Endpoint from{Address{{10, 1, static_cast<std::uint8_t>(i >> 8),
                                     static_cast<std::uint8_t>(i & 0xFF)}},
                            1001};
        client.on_datagram(Instant{}, from, answer_bytes(left));
    }
    CHECK(client.targets().size() == FindClient::max_targets);
    CHECK(client.ignored() == 5);
}

TEST_CASE("starting again forgets the last registration", "[engine][find]") {
    FindClient client{FindConfig{}};
    client.start(Instant{});
    client.on_datagram(Instant{}, target_a, answer_bytes(left));
    client.on_timer(Instant{seconds(3)});
    REQUIRE(client.done());
    client.start(Instant{seconds(10)});
    CHECK_FALSE(client.done());
    CHECK(client.targets().empty());
    CHECK(client.next_wake() == Instant{seconds(13)});
}

TEST_CASE("the runner finds the targets on the network", "[engine][find][runner]") {
    adl::fake::FakeWorld world(3);
    adl::fake::FakeNetwork network(world);
    adl::fake::FakeHost loader(world, network, adl::platform::Ipv4Address{{10, 0, 0, 1}});
    SimPlan plan_a;
    plan_a.find_answer = left;
    SimPlan plan_b;
    plan_b.find_answer = right;
    plan_b.find_answer_copies = 2;  // a target that answers twice
    SimPlan plan_late;
    plan_late.find_answer = FindAnswer{"LATE", "X", "1", "TOO LATE", "ACM"};
    plan_late.find_delay = milliseconds(3500);  // after the window
    SimPlan plan_silent;                        // answers nothing
    SimTarget a(world, network, adl::platform::Ipv4Address{{10, 0, 0, 2}}, plan_a);
    SimTarget b(world, network, adl::platform::Ipv4Address{{10, 0, 0, 3}}, plan_b);
    SimTarget late(world, network, adl::platform::Ipv4Address{{10, 0, 0, 4}}, plan_late);
    SimTarget silent(world, network, adl::platform::Ipv4Address{{10, 0, 0, 5}}, plan_silent);

    Runner runner(loader);
    std::array<FindTarget, 16> found{};
    const Instant started = to_instant(world.now());
    const auto count = runner.find(FindConfig{}, found);
    REQUIRE(count);
    CHECK(*count == 2);
    CHECK(to_instant(world.now()) - started == seconds(3));
    CHECK(found[0].from.address == Address{{10, 0, 0, 2}});
    CHECK(found[0].literal_name.view() == "FMS LEFT");
    CHECK(found[1].from.address == Address{{10, 0, 0, 3}});
    CHECK(found[1].literal_name.view() == "FMS RIGHT");
}

TEST_CASE("FIND to listed addresses reaches only those", "[engine][find][runner]") {
    adl::fake::FakeWorld world(4);
    adl::fake::FakeNetwork network(world);
    adl::fake::FakeHost loader(world, network, adl::platform::Ipv4Address{{10, 0, 0, 1}});
    SimPlan plan_a;
    plan_a.find_answer = left;
    SimPlan plan_b;
    plan_b.find_answer = right;
    SimTarget a(world, network, adl::platform::Ipv4Address{{10, 0, 0, 2}}, plan_a);
    SimTarget b(world, network, adl::platform::Ipv4Address{{10, 0, 0, 3}}, plan_b);

    // Appendix E: over AFDX there is no broadcast, so each target is asked by its own address.
    const std::array<Address, 1> destinations{Address{{10, 0, 0, 3}}};
    FindConfig config;
    config.destinations = destinations;
    Runner runner(loader);
    std::array<FindTarget, 4> found{};
    const auto count = runner.find(config, found);
    REQUIRE(count);
    REQUIRE(*count == 1);
    CHECK(found[0].literal_name.view() == "FMS RIGHT");
}

TEST_CASE("a lossy network still finds what gets through", "[engine][find][runner]") {
    unsigned total = 0;
    for (std::uint64_t seed = 1; seed <= 20; ++seed) {
        adl::fake::FakeWorld world(seed);
        adl::fake::FakeNetwork network(world);
        adl::fake::LinkProfile profile;
        profile.loss_per_million = 300'000;
        profile.duplicate_per_million = 300'000;
        network.set_profile(profile);
        adl::fake::FakeHost loader(world, network, adl::platform::Ipv4Address{{10, 0, 0, 1}});
        SimPlan plan;
        plan.find_answer = left;
        SimTarget target(world, network, adl::platform::Ipv4Address{{10, 0, 0, 2}}, plan);
        Runner runner(loader);
        std::array<FindTarget, 4> found{};
        const auto count = runner.find(FindConfig{}, found);
        REQUIRE(count);
        CHECK(*count <= 1);  // never registered twice, whatever was duplicated
        total += static_cast<unsigned>(*count);
    }
    CHECK(total > 5);   // some got through...
    CHECK(total < 20);  // ...and some did not: FIND has no retry
}

TEST_CASE("the runner reports a FIND port that is taken", "[engine][find][runner]") {
    adl::fake::FakeWorld world(5);
    adl::fake::FakeNetwork network(world);
    adl::fake::FakeHost loader(world, network, adl::platform::Ipv4Address{{10, 0, 0, 1}});
    adl::platform::UdpOptions options;
    options.local = adl::platform::Endpoint{adl::platform::Ipv4Address::any(), 1001};
    auto holder = loader.network().open_udp(options);  // no reuse
    REQUIRE(holder);
    Runner runner(loader);
    std::array<FindTarget, 1> found{};
    const auto count = runner.find(FindConfig{}, found);
    REQUIRE_FALSE(count);
    CHECK(count.error().code == adl::platform::ErrorCode::address_in_use);
}

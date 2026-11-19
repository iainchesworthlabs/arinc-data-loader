// SPDX-License-Identifier: GPL-3.0-or-later
// find, the whole command, on the fake platform: FIND over real sockets on one computer is not reliable (the
// loopback test says why), and on the fake network it is exact. The targets are simulated ones at other
// addresses of the same network.

#include "command_rig.hpp"
#include "faulty_host.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using adl::arinc615a::FindAnswer;
using adl::test::CommandRig;
using adl::test::CommandRun;
using adl::test::Json;
using adl::test::SimPlan;
using adl::test::SimTarget;

namespace
{

SimPlan answering(FindAnswer answer)
{
    SimPlan plan;
    plan.find_answer = answer;
    return plan;
}

constexpr FindAnswer left{"HNPFMS", "FMS", "L", "FMS LEFT", "HNP"};
constexpr FindAnswer right{"HNPFMS", "FMS", "R", "FMS RIGHT", "HNP"};

// The seconds in a `t`, as a number.
double seconds_of(const Json& event)
{
    return std::stod(event.at("t").text);
}

// The loader's host, except that the first wait on the network is cut short by a signal.
class InterruptedOnceHost final : public adl::platform::Host
{
public:
    explicit InterruptedOnceHost(adl::fake::FakeHost& inner) : inner_(inner), network_(inner.network(), *this) {}

    bool interrupted = false;

    adl::platform::HostInfo info() const noexcept override
    {
        return inner_.info();
    }
    adl::platform::Clock& clock() noexcept override
    {
        return inner_.clock();
    }
    adl::platform::MemoryProvider& memory() noexcept override
    {
        return inner_.memory();
    }
    adl::platform::FileSystem& files() noexcept override
    {
        return inner_.files();
    }
    adl::platform::Threading& threads() noexcept override
    {
        return inner_.threads();
    }
    adl::platform::Network& network() noexcept override
    {
        return network_;
    }
    adl::platform::Ipc& ipc() noexcept override
    {
        return inner_.ipc();
    }

private:
    class Network final : public adl::platform::Network
    {
    public:
        Network(adl::platform::Network& inner, InterruptedOnceHost& host) : inner_(inner), host_(host) {}

        adl::platform::Result<adl::platform::Owned<adl::platform::UdpSocket>>
        open_udp(const adl::platform::UdpOptions& options) noexcept override
        {
            return inner_.open_udp(options);
        }
        adl::platform::Result<std::size_t> wait_readable(std::span<adl::platform::UdpSocket* const> sockets,
                                                         adl::platform::Duration timeout,
                                                         std::span<bool> ready) noexcept override
        {
            if (!host_.interrupted)
            {
                host_.interrupted = true;
                return adl::platform::fail(adl::platform::ErrorCode::interrupted);
            }
            return inner_.wait_readable(sockets, timeout, ready);
        }
        std::size_t max_datagram_bytes() const noexcept override
        {
            return inner_.max_datagram_bytes();
        }

    private:
        adl::platform::Network& inner_;
        InterruptedOnceHost& host_;
    };

    adl::fake::FakeHost& inner_;
    Network network_;
};

} // namespace

TEST_CASE("find reports each target that answers, and how many", "[load][integration][find]")
{
    CommandRig rig(answering(left));
    const CommandRun run = rig.run({"find", "--json"});
    CHECK(run.exit_code == 0);
    CHECK(run.err.empty());
    adl::test::check_stream(run);

    const std::vector<Json> events = run.events();
    CHECK(events.front().at("command").text == "find");
    // A find has no target and no position.
    CHECK(events.front().keys() == std::vector<std::string>{"event", "t", "program", "version", "events", "command"});

    const std::vector<Json> found = run.events_named("target_found");
    REQUIRE(found.size() == 1);
    CHECK(found[0].keys() == std::vector<std::string>{"event", "t", "address", "hardware_id", "type_name", "position",
                                                      "literal_name", "manufacturer_code", "thw_id_pos"});
    CHECK(found[0].at("address").text == "10.0.0.2");
    CHECK(found[0].at("hardware_id").text == "HNPFMS");
    CHECK(found[0].at("type_name").text == "FMS");
    CHECK(found[0].at("position").text == "L");
    CHECK(found[0].at("literal_name").text == "FMS LEFT");
    CHECK(found[0].at("manufacturer_code").text == "HNP");
    CHECK(found[0].at("thw_id_pos").text == "HNPFMS_L");
    // It is written when the answer comes, not when the window has closed.
    CHECK(seconds_of(found[0]) < 1.0);

    const Json finished = run.finished();
    CHECK(finished.keys() ==
          std::vector<std::string>{"event", "t", "result", "reason", "status_code", "detail", "exit_code", "count"});
    CHECK(finished.at("result").text == "completed");
    CHECK(finished.at("reason").text == "completed");
    CHECK(finished.at("status_code").is(Json::Kind::null));
    CHECK(finished.at("detail").text == "1 target answered.");
    CHECK(finished.at("count").as_number() == 1);
    CHECK(finished.at("exit_code").as_number() == 0);
    // The window's three seconds.
    CHECK(seconds_of(finished) >= 3.0);
    CHECK(seconds_of(finished) < 3.5);
}

TEST_CASE("find with no answer ran, and the count is 0", "[load][integration][find]")
{
    CommandRig rig;
    const CommandRun run = rig.run({"find", "--json"});
    CHECK(run.exit_code == 0);
    adl::test::check_stream(run);
    CHECK(run.events_named("target_found").empty());
    const Json finished = run.finished();
    CHECK(finished.at("result").text == "completed");
    CHECK(finished.at("count").as_number() == 0);
    CHECK(finished.at("detail").text == "No target answered.");
}

TEST_CASE("find hears every target on the network, once each, in the order they answer", "[load][integration][find]")
{
    CommandRig rig(answering(left));
    SimPlan other = answering(right);
    other.find_answer_copies = 2; // a target that answers twice
    other.find_delay = std::chrono::milliseconds(300);
    SimTarget second(rig.rig.world, rig.rig.network, adl::platform::Ipv4Address{{10, 0, 0, 3}}, other);
    const CommandRun run = rig.run({"find", "--json"});
    CHECK(run.exit_code == 0);
    const std::vector<Json> found = run.events_named("target_found");
    REQUIRE(found.size() == 2);
    CHECK(found[0].at("address").text == "10.0.0.2");
    CHECK(found[0].at("thw_id_pos").text == "HNPFMS_L");
    CHECK(found[1].at("address").text == "10.0.0.3");
    CHECK(found[1].at("thw_id_pos").text == "HNPFMS_R");
    CHECK(found[1].at("literal_name").text == "FMS RIGHT");
    CHECK(run.finished().at("count").as_number() == 2);
    CHECK(run.finished().at("detail").text == "2 targets answered.");
}

TEST_CASE("--seconds is how long find listens", "[load][integration][find]")
{
    SECTION("longer, and a target that answers after the three seconds is heard")
    {
        SimPlan late = answering(left);
        late.find_delay = std::chrono::seconds(4);
        CommandRig rig(late);
        const CommandRun run = rig.run({"find", "--seconds", "6", "--json"});
        CHECK(run.exit_code == 0);
        CHECK(run.events_named("target_found").size() == 1);
        CHECK(seconds_of(run.finished()) >= 6.0);
        CHECK(seconds_of(run.finished()) < 6.5);
    }
    SECTION("the default does not hear it")
    {
        SimPlan late = answering(left);
        late.find_delay = std::chrono::seconds(4);
        CommandRig rig(late);
        const CommandRun run = rig.run({"find", "--json"});
        CHECK(run.events_named("target_found").empty());
        CHECK(run.finished().at("count").as_number() == 0);
    }
    SECTION("shorter")
    {
        CommandRig rig;
        const CommandRun run = rig.run({"find", "--seconds", "1", "--json"});
        CHECK(seconds_of(run.finished()) >= 1.0);
        CHECK(seconds_of(run.finished()) < 1.5);
    }
}

TEST_CASE("--local-address is the interface the FIND socket is bound to", "[load][integration][find]")
{
    SECTION("an address of this computer")
    {
        CommandRig rig(answering(left));
        const CommandRun run = rig.run({"find", "--local-address", "10.0.0.1", "--json"});
        CHECK(run.exit_code == 0);
        CHECK(run.events_named("target_found").size() == 1);
    }
    SECTION("an address that is not one of this computer's cannot be bound")
    {
        CommandRig rig(answering(left));
        const CommandRun run = rig.run({"find", "--local-address", "10.9.9.9", "--json"});
        INFO(run.out);
        CHECK(run.exit_code == 5);
        CHECK(run.finished().at("reason").text == "local_error");
    }
}

TEST_CASE("a find that cannot run is a local error, exit code 5", "[load][integration][find]")
{
    SECTION("the port cannot be opened")
    {
        CommandRig rig(answering(left));
        adl::test::FaultyHost faulty(rig.rig.loader_host);
        faulty.faulty_network().open_fails = adl::platform::ErrorCode::address_in_use;
        const CommandRun run = rig.run_on(faulty, {"find", "--json"});
        CHECK(run.exit_code == 5);
        adl::test::check_stream(run);
        const Json finished = run.finished();
        CHECK(finished.at("result").text == "failed");
        CHECK(finished.at("reason").text == "local_error");
        CHECK(finished.at("detail").text == "FIND could not run: the port could not be opened (address in use).");
        CHECK_FALSE(finished.find("count") != nullptr);
    }
    SECTION("the network cannot be waited on")
    {
        CommandRig rig(answering(left));
        adl::test::FaultyHost faulty(rig.rig.loader_host);
        faulty.faulty_network().wait_fails = adl::platform::ErrorCode::other;
        const CommandRun run = rig.run_on(faulty, {"find", "--json"});
        CHECK(run.exit_code == 5);
        CHECK(run.finished().at("detail").text == "FIND could not run: waiting for the network failed (other error).");
    }
    SECTION("a wait that was interrupted is waited on again")
    {
        CommandRig rig(answering(left));
        InterruptedOnceHost interrupted(rig.rig.loader_host);
        const CommandRun run = rig.run_on(interrupted, {"find", "--json"});
        CHECK(interrupted.interrupted);
        CHECK(run.exit_code == 0);
        CHECK(run.events_named("target_found").size() == 1);
    }
}

TEST_CASE("without --json find writes a line for each target and one for the end", "[load][integration][find]")
{
    CommandRig rig(answering(left));
    const CommandRun run = rig.run({"find"});
    CHECK(run.exit_code == 0);
    CHECK(run.out.find("Found 10.0.0.2: HNPFMS_L (type FMS, \"FMS LEFT\", manufacturer HNP)\n") != std::string::npos);
    CHECK(run.out.ends_with("Finished: completed (exit code 0): 1 target answered. [1 target]\n"));
}

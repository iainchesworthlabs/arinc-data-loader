// SPDX-License-Identifier: GPL-3.0-or-later
// The player of a scenario (libs/loader_scenario): each action done to the bytes the loader sends, the
// triggers and selectors, the order steps take datagrams in, the generator, and then the whole of an
// operation against the simulated target with the engine's FaultHook in place.

#include "adl/arinc615a/data_loader.hpp"
#include "adl/arinc615a/tftp.hpp"
#include "adl/loader_scenario/player.hpp"
#include "rig.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace
{

using namespace adl::arinc615a;
using namespace adl::loader_scenario;
using adl::test::Rig;
using std::chrono::milliseconds;
using std::chrono::seconds;

using Wire = std::vector<std::byte>;

const Endpoint target_control{Address{{10, 0, 0, 2}}, 59};
const Endpoint target_port{Address{{10, 0, 0, 2}}, 50001};
const Endpoint other_port{Address{{10, 0, 0, 2}}, 50002};

[[nodiscard]] Instant at(std::int64_t ms)
{
    return Instant{std::chrono::duration_cast<Duration>(milliseconds{ms})};
}

// What a fault was reported as, copied.
struct Reported
{
    std::size_t step = 0; // from 1
    Action action = Action::drop;
    std::optional<DatagramKind> datagram;
    std::string file;
    std::optional<std::uint16_t> block;
    std::int64_t amount = 0;
    std::int64_t at_ms = 0;
};

class Events final : public FaultListener
{
public:
    std::vector<Reported> list;
    void on_fault(const FaultEvent& event) noexcept override
    {
        list.push_back(Reported{event.step, event.action, event.datagram, std::string{event.file}, event.block,
                                event.amount,
                                std::chrono::duration_cast<milliseconds>(event.time.since_origin).count()});
    }
};

[[nodiscard]] Wire bytes_of(std::span<const std::byte> bytes)
{
    return Wire{bytes.begin(), bytes.end()};
}

[[nodiscard]] Wire data_packet(std::uint16_t block, std::size_t size, std::byte fill = std::byte{0x41})
{
    const std::vector<std::byte> body(size, fill);
    std::array<std::byte, 600> buffer{};
    const auto written = encode_data(TftpData{block, body}, buffer);
    REQUIRE(written);
    return bytes_of(std::span<const std::byte>{buffer}.first(*written));
}

[[nodiscard]] Wire ack_packet(std::uint16_t block)
{
    std::array<std::byte, 8> buffer{};
    const auto written = encode_ack(TftpAck{block}, buffer);
    REQUIRE(written);
    return bytes_of(std::span<const std::byte>{buffer}.first(*written));
}

[[nodiscard]] Wire request_packet(TftpRequest::Kind kind, std::string_view name, OptionList options = {})
{
    std::array<std::byte, 300> buffer{};
    const auto written = encode_request(TftpRequest{kind, name, options}, buffer);
    REQUIRE(written);
    return bytes_of(std::span<const std::byte>{buffer}.first(*written));
}

[[nodiscard]] Wire oack_with_checksum()
{
    OptionBuilder options;
    REQUIRE(options.add_block_size(512));
    ChecksumOption checksum;
    checksum.type = adl::arinc665::CheckValueType::crc16;
    checksum.value[0] = std::byte{0xAB};
    checksum.value[1] = std::byte{0xCD};
    checksum.length = 2;
    REQUIRE(options.add_checksum(checksum));
    std::array<std::byte, 200> buffer{};
    const auto written = encode_option_ack(TftpOptionAck{options.list()}, buffer);
    REQUIRE(written);
    return bytes_of(std::span<const std::byte>{buffer}.first(*written));
}

[[nodiscard]] Command send(const Wire& bytes, std::uint8_t slot = 0, Endpoint to = target_port)
{
    Command command;
    command.kind = Command::Kind::send_from_slot;
    command.slot = slot;
    command.to = to;
    command.bytes = bytes;
    return command;
}

[[nodiscard]] Step step_of(Action action, Selector on = {}, When when = {})
{
    Step step;
    step.when = when;
    step.on = std::move(on);
    step.fault.action = action;
    return step;
}

// Drives a player as the engine does: a call begins, then the player is given what the call has.
class Driver
{
public:
    explicit Driver(std::vector<Step> steps, std::uint64_t seed = 1, OperationName operation = OperationName::any)
        : player(Scenario{"test", "", seed, std::move(steps)}, operation, &events)
    {
    }

    // The commands the player makes of one command the loader had, copied.
    std::vector<Wire> command(std::int64_t ms, const Command& command, bool new_call = true)
    {
        if (new_call)
        {
            player.begin_call(at(ms));
        }
        const std::size_t count = player.on_command(at(ms), command, out);
        std::vector<Wire> result;
        for (std::size_t i = 0; i < count; ++i)
        {
            result.push_back(bytes_of(out[i].bytes));
        }
        return result;
    }

    std::vector<Wire> timer(std::int64_t ms)
    {
        player.begin_call(at(ms));
        const std::size_t count = player.on_timer(at(ms), out);
        std::vector<Wire> result;
        for (std::size_t i = 0; i < count; ++i)
        {
            result.push_back(bytes_of(out[i].bytes));
        }
        return result;
    }

    void arrive_request(std::int64_t ms, TftpRequest::Kind kind, std::string_view name, Endpoint from = target_port)
    {
        player.begin_call(at(ms));
        const Wire bytes = request_packet(kind, name);
        player.on_arrival(at(ms), std::nullopt, from, bytes);
    }

    Events events;
    Player player;
    std::array<Command, 64> out{};
};

} // namespace

// ---- Each action, to the bytes ------------------------------------------------------------------------------------

TEST_CASE("a player with no steps sends every command as it was", "[loader_scenario][player]")
{
    Driver driver({});
    const Wire data = data_packet(1, 100);
    const auto sent = driver.command(0, send(data));
    REQUIRE(sent.size() == 1);
    CHECK(sent[0] == data);
    CHECK(driver.events.list.empty());
    CHECK(driver.player.next_wake() == never);
    CHECK(!driver.player.take_abort(at(1)));
    CHECK(driver.player.unapplied_steps().empty());
}

TEST_CASE("drop sends nothing and says so", "[loader_scenario][player]")
{
    Selector on;
    on.datagram = DatagramKind::data;
    on.block = 2;
    Driver driver({step_of(Action::drop, on)});
    CHECK(driver.command(0, send(data_packet(1, 10))).size() == 1);
    CHECK(driver.command(1, send(ack_packet(2))).size() == 1);
    CHECK(driver.command(2, send(data_packet(2, 10))).empty());
    // The step was for one datagram.
    CHECK(driver.command(3, send(data_packet(2, 10))).size() == 1);
    REQUIRE(driver.events.list.size() == 1);
    CHECK(driver.events.list[0].action == Action::drop);
    CHECK(driver.events.list[0].datagram == DatagramKind::data);
    CHECK(driver.events.list[0].block == 2);
    CHECK(driver.player.applied() == 1);
}

TEST_CASE("count says how many datagrams a step is applied to, and all says every one", "[loader_scenario][player]")
{
    Selector on;
    on.datagram = DatagramKind::data;
    Step twice = step_of(Action::drop, on);
    twice.count = 2;
    Driver driver({twice});
    CHECK(driver.command(0, send(data_packet(1, 10))).empty());
    CHECK(driver.command(1, send(data_packet(2, 10))).empty());
    CHECK(driver.command(2, send(data_packet(3, 10))).size() == 1);

    Step always = step_of(Action::drop, on);
    always.all = true;
    Driver every({always});
    for (std::uint16_t block = 1; block <= 20; ++block)
    {
        CHECK(every.command(block, send(data_packet(block, 10))).empty());
    }
    CHECK(every.events.list.size() == 20);
}

TEST_CASE("duplicate sends a datagram several times in a row", "[loader_scenario][player]")
{
    Step step = step_of(Action::duplicate);
    step.fault.copies = 4;
    Driver driver({step});
    const Wire data = data_packet(1, 30);
    const auto sent = driver.command(0, send(data));
    REQUIRE(sent.size() == 4);
    for (const Wire& copy : sent)
    {
        CHECK(copy == data);
    }
    CHECK(driver.events.list.at(0).amount == 4);
}

TEST_CASE("delay sends a datagram later, from the timer", "[loader_scenario][player]")
{
    Step step = step_of(Action::delay);
    step.fault.milliseconds = 3000;
    Driver driver({step});
    const Wire data = data_packet(1, 30);
    CHECK(driver.command(100, send(data, 3, other_port)).empty());
    CHECK(driver.player.held() == 1);
    CHECK(driver.player.next_wake() == at(3100));
    CHECK(driver.timer(3099).empty());
    Command released;
    driver.player.begin_call(at(3100));
    REQUIRE(driver.player.on_timer(at(3100), driver.out) == 1);
    released = driver.out[0];
    CHECK(bytes_of(released.bytes) == data);
    CHECK(released.slot == 3);
    CHECK(released.to == other_port);
    CHECK(driver.player.held() == 0);
    CHECK(driver.player.next_wake() == never);
}

TEST_CASE("reorder sends a datagram after the next one the loader sends", "[loader_scenario][player]")
{
    Selector on;
    on.datagram = DatagramKind::data;
    on.block = 1;
    Driver driver({step_of(Action::reorder, on)});
    const Wire first = data_packet(1, 30);
    const Wire second = ack_packet(7);
    CHECK(driver.command(0, send(first)).empty());
    const auto sent = driver.command(10, send(second));
    REQUIRE(sent.size() == 2);
    CHECK(sent[0] == second);
    CHECK(sent[1] == first);
    CHECK(driver.player.held() == 0);
}

TEST_CASE("reorder with nothing after it sends the datagram when its time is up", "[loader_scenario][player]")
{
    Step step = step_of(Action::reorder);
    step.fault.milliseconds = 500;
    Driver driver({step});
    const Wire data = data_packet(1, 30);
    CHECK(driver.command(0, send(data)).empty());
    CHECK(driver.timer(499).empty());
    const auto sent = driver.timer(500);
    REQUIRE(sent.size() == 1);
    CHECK(sent[0] == data);
}

TEST_CASE("corrupt changes one byte of the datagram and nothing else", "[loader_scenario][player]")
{
    Step step = step_of(Action::corrupt);
    step.fault.offset = 6;
    step.fault.xor_mask = 0x80;
    Driver driver({step});
    const Wire data = data_packet(1, 30);
    const auto sent = driver.command(0, send(data));
    REQUIRE(sent.size() == 1);
    REQUIRE(sent[0].size() == data.size());
    for (std::size_t i = 0; i < data.size(); ++i)
    {
        if (i == 6)
        {
            CHECK(sent[0][i] == (data[i] ^ std::byte{0x80}));
        }
        else
        {
            CHECK(sent[0][i] == data[i]);
        }
    }
}

TEST_CASE("corrupt past the end changes the last byte", "[loader_scenario][player]")
{
    Step step = step_of(Action::corrupt);
    step.fault.offset = 5000;
    Driver driver({step});
    const Wire data = data_packet(1, 10);
    const auto sent = driver.command(0, send(data));
    REQUIRE(sent.size() == 1);
    CHECK(sent[0].back() == (data.back() ^ std::byte{1}));
    CHECK(sent[0].front() == data.front());
}

TEST_CASE("check_value changes the first digit of the check value of the next option acknowledgement",
          "[loader_scenario][player]")
{
    Driver driver({step_of(Action::check_value)});
    // An acknowledgement of block 1 has no check value: the step waits for one that has.
    CHECK(driver.command(0, send(ack_packet(1))).size() == 1);
    CHECK(driver.events.list.empty());
    const Wire oack = oack_with_checksum();
    const auto sent = driver.command(1, send(oack));
    REQUIRE(sent.size() == 1);
    REQUIRE(sent[0].size() == oack.size());
    const auto before = decode_packet(oack);
    const auto after = decode_packet(sent[0]);
    REQUIRE(before);
    REQUIRE(after);
    const auto& oack_before = std::get<TftpOptionAck>(*before);
    const auto& oack_after = std::get<TftpOptionAck>(*after);
    const auto value_of = [](const TftpOptionAck& packet)
    {
        std::string value;
        for (const TftpOption option : packet.options)
        {
            if (option.name.starts_with("checksum_"))
            {
                value = std::string{option.value};
            }
        }
        return value;
    };
    const std::string original = value_of(oack_before);
    const std::string changed = value_of(oack_after);
    REQUIRE(original.size() == changed.size());
    REQUIRE(!original.empty());
    CHECK(original != changed);
    CHECK(original.substr(1) == changed.substr(1));
    CHECK(driver.events.list.at(0).datagram == DatagramKind::option_ack);
}

TEST_CASE("length makes a datagram longer with zeros or shorter by cutting it", "[loader_scenario][player]")
{
    Step longer = step_of(Action::length);
    longer.fault.delta = 3;
    Driver add({longer});
    const Wire data = data_packet(1, 20);
    const auto grown = add.command(0, send(data));
    REQUIRE(grown.size() == 1);
    REQUIRE(grown[0].size() == data.size() + 3);
    CHECK(std::equal(data.begin(), data.end(), grown[0].begin()));
    CHECK(grown[0][data.size()] == std::byte{0});

    Step shorter = step_of(Action::length);
    shorter.fault.delta = -5;
    Driver cut({shorter});
    const auto cut_down = cut.command(0, send(data));
    REQUIRE(cut_down.size() == 1);
    REQUIRE(cut_down[0].size() == data.size() - 5);
    CHECK(std::equal(cut_down[0].begin(), cut_down[0].end(), data.begin()));

    Step gone = step_of(Action::length);
    gone.fault.delta = -1000;
    Driver all({gone});
    const auto nothing = all.command(0, send(data));
    REQUIRE(nothing.size() == 1);
    CHECK(nothing[0].empty());
}

TEST_CASE("error, wait and abort_message replace the datagram with a packet that says so", "[loader_scenario][player]")
{
    Step error = step_of(Action::error);
    error.fault.code = 2;
    error.fault.text = "no thank you";
    Driver errors({error});
    const auto sent = errors.command(0, send(data_packet(1, 10)));
    REQUIRE(sent.size() == 1);
    const auto packet = decode_packet(sent[0]);
    REQUIRE(packet);
    const auto& decoded = std::get<TftpError>(*packet);
    CHECK(decoded.code == TftpErrorCode::access_violation);
    CHECK(decoded.message == "no thank you");

    Step wait = step_of(Action::wait);
    wait.fault.seconds = 17;
    Driver waits({wait});
    const auto waited = waits.command(0, send(data_packet(1, 10)));
    REQUIRE(waited.size() == 1);
    const auto waited_packet = decode_packet(waited[0]);
    REQUIRE(waited_packet);
    const auto meaning = interpret_error(std::get<TftpError>(*waited_packet));
    REQUIRE(meaning);
    CHECK(std::get<WaitMessage>(*meaning) == WaitMessage{17});

    Step abort = step_of(Action::abort_message);
    abort.fault.status = 0x1004;
    Driver aborts({abort});
    const auto aborted = aborts.command(0, send(data_packet(1, 10)));
    REQUIRE(aborted.size() == 1);
    const auto aborted_packet = decode_packet(aborted[0]);
    REQUIRE(aborted_packet);
    const auto abort_meaning = interpret_error(std::get<TftpError>(*aborted_packet));
    REQUIRE(abort_meaning);
    CHECK(std::get<AbortMessage>(*abort_meaning).status == StatusCode::aborted_by_data_loader);
}

TEST_CASE("an abort message may carry a code that is not one of the loader's", "[loader_scenario][player]")
{
    Step step = step_of(Action::abort_message);
    step.fault.status = 0x7777;
    Driver driver({step});
    const auto sent = driver.command(0, send(data_packet(1, 10)));
    REQUIRE(sent.size() == 1);
    const auto packet = decode_packet(sent[0]);
    REQUIRE(packet);
    const auto& error = std::get<TftpError>(*packet);
    CHECK(error.code == TftpErrorCode::not_defined);
    CHECK(error.message == "ABORT:7777");
}

TEST_CASE("silence sends nothing until it ends, and the commands that close sockets still go",
          "[loader_scenario][player]")
{
    Step step = step_of(Action::silent, {}, When{TriggerKind::after_ms, 1000});
    step.fault.milliseconds = 4000;
    Driver driver({step});
    CHECK(driver.command(0, send(data_packet(1, 10))).size() == 1);
    CHECK(driver.player.next_wake() == at(1000));
    CHECK(driver.command(999, send(data_packet(2, 10))).size() == 1);
    CHECK(driver.command(1000, send(data_packet(3, 10))).empty());
    Command release;
    release.kind = Command::Kind::release_slot;
    release.slot = 2;
    CHECK(driver.command(1500, release).size() == 1);
    CHECK(driver.command(4999, send(data_packet(4, 10))).empty());
    CHECK(driver.command(5000, send(data_packet(5, 10))).size() == 1);
    REQUIRE(driver.events.list.size() == 1);
    CHECK(driver.events.list[0].action == Action::silent);
    CHECK(!driver.events.list[0].datagram);
    CHECK(driver.events.list[0].amount == 4000);
}

TEST_CASE("silence without a length lasts for the rest of the operation", "[loader_scenario][player]")
{
    Driver driver({step_of(Action::silent)});
    for (std::int64_t t = 0; t < 100'000; t += 10'000)
    {
        CHECK(driver.command(t, send(data_packet(1, 10))).empty());
    }
}

TEST_CASE("abort is asked for once, when its trigger is reached", "[loader_scenario][player]")
{
    Step step = step_of(Action::abort, {}, When{TriggerKind::status_file, 2});
    step.fault.status = 0x1004;
    Driver driver({step});
    driver.arrive_request(0, TftpRequest::Kind::write, "HNPFMS_L.LUS");
    CHECK(!driver.player.take_abort(at(0)));
    driver.arrive_request(10, TftpRequest::Kind::write, "HNPFMS_L.LUS");
    const auto status = driver.player.take_abort(at(10));
    REQUIRE(status);
    CHECK(*status == StatusCode::aborted_by_data_loader);
    CHECK(!driver.player.take_abort(at(11)));
    REQUIRE(driver.events.list.size() == 1);
    CHECK(driver.events.list[0].action == Action::abort);
}

// ---- Triggers and selectors -----------------------------------------------------------------------------------------

TEST_CASE("the nth request of the target puts a step in effect", "[loader_scenario][player][trigger]")
{
    Driver driver({step_of(Action::drop, {}, When{TriggerKind::request, 3})});
    driver.arrive_request(0, TftpRequest::Kind::write, "HNPFMS_L.LCS");
    driver.arrive_request(1, TftpRequest::Kind::read, "A.BIN");
    CHECK(driver.command(2, send(ack_packet(0))).size() == 1);
    driver.arrive_request(3, TftpRequest::Kind::read, "B.BIN");
    CHECK(driver.command(4, send(ack_packet(0))).empty());
}

TEST_CASE("only a write request for a status file counts as a status file", "[loader_scenario][player][trigger]")
{
    Driver driver({step_of(Action::drop, {}, When{TriggerKind::status_file, 2})});
    driver.arrive_request(0, TftpRequest::Kind::write, "HNPFMS_L.LCS");
    driver.arrive_request(1, TftpRequest::Kind::write, "HNPFMS_L.LCL"); // a list, not a status file
    driver.arrive_request(2, TftpRequest::Kind::read, "HNPFMS_L.LCS");  // a read
    CHECK(driver.command(3, send(ack_packet(0))).size() == 1);
    driver.arrive_request(4, TftpRequest::Kind::write, "HNPFMS_L.LCS");
    CHECK(driver.command(5, send(ack_packet(0))).empty());
}

TEST_CASE("a time trigger counts from the first command", "[loader_scenario][player][trigger]")
{
    Driver driver({step_of(Action::drop, {}, When{TriggerKind::after_ms, 2000})});
    CHECK(driver.command(50'000, send(ack_packet(0))).size() == 1); // the operation began here
    CHECK(driver.command(51'999, send(ack_packet(0))).size() == 1);
    CHECK(driver.command(52'000, send(ack_packet(0))).empty());
}

TEST_CASE("the reply selector takes the first datagram sent to the peer that sent a request",
          "[loader_scenario][player][selector]")
{
    Selector on;
    on.datagram = DatagramKind::reply;
    Driver driver({step_of(Action::drop, on)});
    driver.arrive_request(0, TftpRequest::Kind::write, "HNPFMS_L.LUS", target_port);
    // A datagram to someone else is not a reply.
    CHECK(driver.command(1, send(ack_packet(0), 0, other_port)).size() == 1);
    // The first one to the peer is.
    CHECK(driver.command(2, send(ack_packet(0), 1, target_port)).empty());
    // Not the second.
    CHECK(driver.command(3, send(ack_packet(1), 1, target_port)).size() == 1);
}

TEST_CASE("the file selector takes the transfer's file by name, or by extension", "[loader_scenario][player][selector]")
{
    Selector by_name;
    by_name.file = "load1.bin";
    Selector by_extension;
    by_extension.file = ".LUS";
    Driver driver({step_of(Action::drop, by_name), step_of(Action::drop, by_extension)});

    // The loader's own request names its file in the datagram.
    const Wire read = request_packet(TftpRequest::Kind::read, "HNPFMS_L.LUI");
    CHECK(driver.command(0, send(read, 0, target_control)).size() == 1);

    // The target's requests name the file, and the replies after them are for it.
    driver.arrive_request(1, TftpRequest::Kind::read, "LOAD1.BIN", other_port);
    CHECK(driver.command(2, send(ack_packet(0), 1, other_port)).empty()); // by name, case apart
    driver.arrive_request(3, TftpRequest::Kind::write, "HNPFMS_L.LUS", target_port);
    CHECK(driver.command(4, send(ack_packet(0), 2, target_port)).empty()); // by extension
    CHECK(driver.events.list.size() == 2);
    CHECK(driver.events.list[0].file == "LOAD1.BIN");
    CHECK(driver.events.list[1].file == "HNPFMS_L.LUS");

    // Later datagrams of the transfer are still for that file, and a closed slot forgets it.
    CHECK(driver.command(5, send(ack_packet(1), 1, other_port)).size() == 1); // spent
    Command release;
    release.kind = Command::Kind::release_slot;
    release.slot = 1;
    CHECK(driver.command(6, release).size() == 1);
}

TEST_CASE("the first step that is in effect and matches takes the datagram", "[loader_scenario][player][selector]")
{
    Selector data;
    data.datagram = DatagramKind::data;
    Step first = step_of(Action::drop, data);
    Step second = step_of(Action::corrupt, data);
    Driver driver({first, second});
    CHECK(driver.command(0, send(data_packet(1, 10))).empty()); // the first step
    const Wire again = data_packet(2, 10);
    const auto sent = driver.command(1, send(again)); // the first is spent: the second
    REQUIRE(sent.size() == 1);
    CHECK(sent[0] != again);
    REQUIRE(driver.events.list.size() == 2);
    CHECK(driver.events.list[0].step == 1);
    CHECK(driver.events.list[1].step == 2);
}

TEST_CASE("a step that was never applied is told afterwards", "[loader_scenario][player]")
{
    Selector never_there;
    never_there.file = "NOTHING.BIN";
    Driver driver({step_of(Action::drop, never_there), step_of(Action::drop)});
    CHECK(driver.command(0, send(ack_packet(0))).empty());
    CHECK(driver.player.unapplied_steps() == std::vector<std::size_t>{1});
}

TEST_CASE("the chance of a step is drawn from the seed, in the order the datagrams go",
          "[loader_scenario][player][random]")
{
    const auto history = [](std::uint64_t seed)
    {
        Step step = step_of(Action::drop);
        step.per_mille = 300;
        step.all = true;
        Driver driver({step}, seed);
        std::vector<bool> dropped;
        for (std::int64_t i = 0; i < 400; ++i)
        {
            dropped.push_back(driver.command(i, send(ack_packet(1))).empty());
        }
        return dropped;
    };
    const auto first = history(7);
    CHECK(first == history(7));
    CHECK(first != history(8));
    const auto count = std::count(first.begin(), first.end(), true);
    CHECK(count > 80); // 30 per cent of 400, give or take
    CHECK(count < 160);
}

TEST_CASE("a datagram too large to hold goes as it was", "[loader_scenario][player]")
{
    Step step = step_of(Action::delay);
    step.fault.milliseconds = 1000;
    Driver driver({step});
    const Wire huge(Player::datagram_capacity + 1, std::byte{1});
    const auto sent = driver.command(0, send(huge));
    REQUIRE(sent.size() == 1);
    CHECK(sent[0].size() == huge.size());
    CHECK(driver.events.list.empty());
    CHECK(driver.player.applied() == 0);
}

TEST_CASE("every puts a step in effect again, and times says how many times in all",
          "[loader_scenario][player][trigger]")
{
    When when{TriggerKind::request, 2};
    when.every = 3;
    when.times = 2;
    Driver driver({step_of(Action::drop, {}, when)});
    driver.arrive_request(0, TftpRequest::Kind::read, "A.BIN");
    CHECK(driver.command(1, send(ack_packet(0))).size() == 1);  // the 1st request: not yet
    driver.arrive_request(2, TftpRequest::Kind::read, "A.BIN"); // the 2nd: the first occurrence
    CHECK(driver.command(3, send(ack_packet(0))).empty());
    CHECK(driver.command(4, send(ack_packet(0))).size() == 1); // one datagram for each occurrence
    driver.arrive_request(5, TftpRequest::Kind::read, "A.BIN");
    driver.arrive_request(6, TftpRequest::Kind::read, "A.BIN");
    CHECK(driver.command(7, send(ack_packet(0))).size() == 1);
    driver.arrive_request(8, TftpRequest::Kind::read, "A.BIN"); // the 5th: the second occurrence
    CHECK(driver.command(9, send(ack_packet(0))).empty());
    for (int i = 0; i < 6; ++i)
    {
        driver.arrive_request(10 + i, TftpRequest::Kind::read, "A.BIN");
    }
    CHECK(driver.command(20, send(ack_packet(0))).size() == 1); // times: that was the last
    CHECK(driver.player.applied(1) == 2);
}

TEST_CASE("every with after_ms puts a step in effect each so many milliseconds", "[loader_scenario][player][trigger]")
{
    When when{TriggerKind::after_ms, 100};
    when.every = 100;
    Driver driver({step_of(Action::drop, {}, when)});
    CHECK(driver.command(0, send(ack_packet(0))).size() == 1);
    CHECK(driver.command(100, send(ack_packet(0))).empty());
    CHECK(driver.command(150, send(ack_packet(0))).size() == 1);
    CHECK(driver.command(250, send(ack_packet(0))).empty());
    CHECK(driver.command(260, send(ack_packet(0))).size() == 1);
}

TEST_CASE("a step for another operation is not in effect and is not reported as unused", "[loader_scenario][player]")
{
    When for_download;
    for_download.operation = OperationName::download_media_defined;
    When for_upload;
    for_upload.operation = OperationName::upload;
    Driver driver({step_of(Action::drop, {}, for_download), step_of(Action::drop, {}, for_upload),
                   step_of(Action::drop, Selector{DatagramKind::request, "", std::nullopt})},
                  1, OperationName::upload);
    CHECK(driver.command(0, send(ack_packet(0))).empty()); // the upload's step
    CHECK(driver.events.list.at(0).step == 2);
    CHECK(driver.player.unapplied_steps() == std::vector<std::size_t>{3});
}

TEST_CASE("each step draws from a stream of its own, SplitMix64 from the seed's nth output",
          "[loader_scenario][player][random]")
{
    // An independent SplitMix64, written out here, for the documented rule: the seed starts a first generator, its
    // nth output seeds step n's stream, and a chance is a draw in [1, 1000] by rejection that must be at most the
    // step's per_mille.
    const auto mix = [](std::uint64_t& state)
    {
        state += 0x9E3779B97F4A7C15ULL;
        std::uint64_t z = state;
        z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27U)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31U);
    };
    std::uint64_t zero = 0;
    CHECK(mix(zero) == 0xE220A8397B1DCDAFULL);
    CHECK(mix(zero) == 0x6E789E6AA1B965F4ULL);
    CHECK(mix(zero) == 0x06C45D188009454FULL);

    std::uint64_t first = 12345;
    const std::uint64_t seed_of_step_1 = mix(first);
    const std::uint64_t seed_of_step_2 = mix(first);
    const auto expected = [&](std::uint64_t stream_seed, unsigned per_mille, int count)
    {
        std::uint64_t state = stream_seed;
        std::vector<bool> applied;
        for (int i = 0; i < count; ++i)
        {
            for (;;)
            {
                const std::uint64_t output = mix(state);
                const std::uint64_t range = 1000;
                const std::uint64_t remainder = (0 - range) % range;
                if (output < 0 - remainder)
                {
                    applied.push_back(1 + output % range <= per_mille);
                    break;
                }
            }
        }
        return applied;
    };

    Selector data;
    data.datagram = DatagramKind::data;
    Selector acks;
    acks.datagram = DatagramKind::ack;
    Step first_step = step_of(Action::drop, data);
    first_step.per_mille = 300;
    first_step.all = true;
    Step second_step = step_of(Action::drop, acks);
    second_step.per_mille = 600;
    second_step.all = true;
    Driver driver({first_step, second_step}, 12345);
    std::vector<bool> data_dropped;
    std::vector<bool> ack_dropped;
    for (int i = 0; i < 60; ++i)
    {
        data_dropped.push_back(driver.command(i, send(data_packet(1, 10))).empty());
        ack_dropped.push_back(driver.command(i, send(ack_packet(1))).empty());
    }
    CHECK(data_dropped == expected(seed_of_step_1, 300, 60));
    CHECK(ack_dropped == expected(seed_of_step_2, 600, 60));
}

TEST_CASE("a delay that is a range is drawn from the step's stream between its ends",
          "[loader_scenario][player][random]")
{
    Step step = step_of(Action::delay);
    step.fault.min_milliseconds = 200;
    step.fault.max_milliseconds = 900;
    step.all = true;
    Driver driver({step}, 5);
    std::vector<std::int64_t> waits;
    for (int i = 0; i < 40; ++i)
    {
        CHECK(driver.command(i, send(ack_packet(1))).empty());
        waits.push_back(driver.events.list.back().amount);
    }
    for (const std::int64_t wait : waits)
    {
        CHECK(wait >= 200);
        CHECK(wait <= 900);
    }
    CHECK(std::ranges::adjacent_find(waits, std::not_equal_to<>{}) != waits.end()); // they are not all alike
    Driver again({step}, 5);
    for (int i = 0; i < 40; ++i)
    {
        CHECK(again.command(i, send(ack_packet(1))).empty());
        CHECK(again.events.list.back().amount == waits[static_cast<std::size_t>(i)]);
    }
}

// ---- The engine with a hook
// ------------------------------------------------------------------------------------------

namespace
{

using adl::test::make_sample_load;
using adl::test::SampleLoad;
using adl::test::SimPlan;
using adl::test::upload_of;

SimPlan upload_plan(const SampleLoad& load)
{
    SimPlan plan;
    plan.loads = {load.item};
    return plan;
}

struct Run
{
    std::unique_ptr<Rig> rig;
    std::unique_ptr<Events> events;
    std::unique_ptr<Player> player;
    std::expected<Outcome, RunError> outcome = std::unexpected(RunError{});
    SampleLoad load;
};

// An upload of the sample load, with `steps` in effect.
Run upload(std::vector<Step> steps, std::uint64_t seed = 1, std::size_t data_bytes = 0)
{
    Run run;
    run.load = make_sample_load(data_bytes);
    run.rig = std::make_unique<Rig>(upload_plan(run.load));
    run.rig->source = std::make_unique<adl::test::MediaSource>(run.load.files);
    run.events = std::make_unique<Events>();
    if (!steps.empty())
    {
        run.player = std::make_unique<Player>(Scenario{"upload", "", seed, std::move(steps)}, OperationName::upload,
                                              run.events.get());
        run.rig->faults = run.player.get();
    }
    std::vector<LoadReference> storage;
    run.outcome = run.rig->run(upload_of(run.load, storage));
    return run;
}

[[nodiscard]] std::vector<std::pair<std::string, Wire>> loader_datagrams(const Rig& rig)
{
    std::vector<std::pair<std::string, Wire>> sent;
    for (const auto& packet : rig.network.captured())
    {
        if (packet.from.address == Rig::loader_address && !packet.lost)
        {
            sent.emplace_back(std::to_string(packet.from.port) + ">" + std::to_string(packet.to.port) + "@" +
                                  std::to_string(packet.sent.since_origin.count()),
                              packet.payload);
        }
    }
    return sent;
}

} // namespace

TEST_CASE("a player with no steps leaves what the loader sends exactly as it is", "[loader_scenario][engine]")
{
    Run without = upload({});
    // A hook with nothing to do.
    Run with;
    with.load = make_sample_load();
    with.rig = std::make_unique<Rig>(upload_plan(with.load));
    with.rig->source = std::make_unique<adl::test::MediaSource>(with.load.files);
    Player idle{Scenario{}, OperationName::upload};
    with.rig->faults = &idle;
    std::vector<LoadReference> storage;
    with.outcome = with.rig->run(upload_of(with.load, storage));

    REQUIRE(without.outcome);
    REQUIRE(with.outcome);
    CHECK(without.outcome->code == OutcomeCode::completed);
    CHECK(with.outcome->code == OutcomeCode::completed);
    CHECK(loader_datagrams(*without.rig) == loader_datagrams(*with.rig));
    CHECK(without.rig->elapsed == with.rig->elapsed);
}

TEST_CASE("a data block that is dropped comes again after the time-out and the upload completes",
          "[loader_scenario][engine]")
{
    const Run baseline = upload({});
    Run probe;
    probe.load = make_sample_load();
    const std::string name = probe.load.load.data_files.front().name;
    Selector on;
    on.datagram = DatagramKind::data;
    on.file = name;
    on.block = 1;
    const Run run = upload({step_of(Action::drop, on)});
    REQUIRE(run.outcome);
    CHECK(run.outcome->code == OutcomeCode::completed);
    REQUIRE(run.events->list.size() == 1);
    CHECK(run.events->list[0].action == Action::drop);
    CHECK(run.events->list[0].file == name);
    CHECK(run.events->list[0].block == 1);
    CHECK(run.rig->target.log().files_corrupt.empty());
    CHECK(run.rig->target.log().files_read == baseline.rig->target.log().files_read);
    // The target waited for its time-out, and the loader sent the block again.
    CHECK(run.rig->elapsed >= baseline.rig->elapsed + seconds(2));
}

TEST_CASE("a data block that is sent three times is written once", "[loader_scenario][engine]")
{
    const Run baseline = upload({});
    Run probe;
    probe.load = make_sample_load();
    Selector on;
    on.datagram = DatagramKind::data;
    on.file = probe.load.load.data_files.front().name;
    on.block = 1;
    Step step = step_of(Action::duplicate, on);
    step.fault.copies = 3;
    const Run run = upload({step});
    REQUIRE(run.outcome);
    CHECK(run.outcome->code == OutcomeCode::completed);
    REQUIRE(run.events->list.size() == 1);
    CHECK(run.events->list[0].amount == 3);
    CHECK(run.rig->target.log().files_corrupt.empty());
    CHECK(run.rig->target.log().files_read == baseline.rig->target.log().files_read);
}

TEST_CASE("a data block that is delayed past the time-out is a duplicate when it comes", "[loader_scenario][engine]")
{
    Run probe;
    probe.load = make_sample_load();
    Selector on;
    on.datagram = DatagramKind::data;
    on.file = probe.load.load.data_files.front().name;
    on.block = 1;
    Step step = step_of(Action::delay, on);
    step.fault.milliseconds = 3000;
    const Run run = upload({step});
    REQUIRE(run.outcome);
    CHECK(run.outcome->code == OutcomeCode::completed);
    REQUIRE(run.events->list.size() == 1);
    CHECK(run.rig->target.log().files_corrupt.empty());
    CHECK(run.player->held() == 0);
}

TEST_CASE("a data byte that is changed is a file the target finds damaged", "[loader_scenario][engine]")
{
    Run probe;
    probe.load = make_sample_load();
    const std::string name = probe.load.load.data_files.front().name;
    Selector on;
    on.datagram = DatagramKind::data;
    on.file = name;
    on.block = 1;
    const Run run = upload({step_of(Action::corrupt, on)});
    REQUIRE(run.outcome);
    REQUIRE(run.events->list.size() == 1);
    const auto& corrupt = run.rig->target.log().files_corrupt;
    CHECK(std::ranges::find(corrupt, name) != corrupt.end());
}

TEST_CASE("a status file answered with WAIT is asked for again after the wait", "[loader_scenario][engine]")
{
    Selector on;
    on.datagram = DatagramKind::reply;
    on.file = ".LUS";
    Step step = step_of(Action::wait, on, When{TriggerKind::status_file, 2});
    step.fault.seconds = 2;
    const Run run = upload({step});
    REQUIRE(run.outcome);
    CHECK(run.outcome->code == OutcomeCode::completed);
    REQUIRE(run.events->list.size() == 1);
    CHECK(run.events->list[0].action == Action::wait);
    CHECK(run.events->list[0].file.ends_with(".LUS"));
    CHECK(run.rig->target.log().waits_received >= 1);
}

TEST_CASE("the loader's abort at a chosen moment is confirmed by the target", "[loader_scenario][engine]")
{
    Step step = step_of(Action::abort, {}, When{TriggerKind::status_file, 2});
    step.fault.status = 0x1005;
    const Run run = upload({step});
    REQUIRE(run.outcome);
    CHECK(run.outcome->code == OutcomeCode::aborted_by_operator);
    CHECK(run.outcome->abort_confirmed);
    REQUIRE(run.events->list.size() == 1);
    CHECK(run.events->list[0].action == Action::abort);
    const auto& aborts = run.rig->target.log().aborts_seen;
    CHECK(std::ranges::find(aborts, StatusCode::aborted_by_operator) != aborts.end());
}

TEST_CASE("an answer to a status file with an abort the loader did not decide on ends the operation at the target",
          "[loader_scenario][engine]")
{
    Selector on;
    on.datagram = DatagramKind::reply;
    on.file = ".LUS";
    Step step = step_of(Action::abort_message, on, When{TriggerKind::status_file, 2});
    step.fault.status = 0x1004;
    const Run run = upload({step});
    REQUIRE(run.outcome);
    REQUIRE(run.events->list.size() == 1);
    const auto& aborts = run.rig->target.log().aborts_seen;
    CHECK(std::ranges::find(aborts, StatusCode::aborted_by_data_loader) != aborts.end());
    CHECK(run.rig->loader->idle());
}

TEST_CASE("a loader that goes silent ends by itself and does not hang", "[loader_scenario][engine]")
{
    const Run run = upload({step_of(Action::silent, {}, When{TriggerKind::request, 2})});
    REQUIRE(run.outcome);
    CHECK(run.outcome->code != OutcomeCode::completed);
    CHECK(run.outcome->code != OutcomeCode::running);
    CHECK(run.rig->loader->idle());
    CHECK(run.rig->elapsed < std::chrono::minutes(5));
    REQUIRE(run.events->list.size() == 1);
    CHECK(run.events->list[0].action == Action::silent);
}

TEST_CASE("the same scenario gives the same reports and the same datagrams every time", "[loader_scenario][engine]")
{
    const auto make = [](std::uint64_t seed)
    {
        Selector on;
        on.datagram = DatagramKind::data;
        Step step = step_of(Action::drop, on);
        step.per_mille = 200;
        step.count = 4;
        return upload({step}, seed, 20'000);
    };
    const Run a = make(11);
    const Run b = make(11);
    const Run c = make(12);
    REQUIRE(a.outcome);
    REQUIRE(b.outcome);
    REQUIRE(c.outcome);
    REQUIRE(!a.events->list.empty());
    CHECK(a.events->list.size() == b.events->list.size());
    for (std::size_t i = 0; i < a.events->list.size(); ++i)
    {
        CHECK(a.events->list[i].block == b.events->list[i].block);
        CHECK(a.events->list[i].file == b.events->list[i].file);
        CHECK(a.events->list[i].at_ms == b.events->list[i].at_ms);
    }
    CHECK(loader_datagrams(*a.rig) == loader_datagrams(*b.rig));
    CHECK(loader_datagrams(*a.rig) != loader_datagrams(*c.rig));
}

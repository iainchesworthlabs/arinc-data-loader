// SPDX-License-Identifier: GPL-3.0-or-later
// `adl_load serve`, the whole command, on the fake platform: one loader on one port and simulated targets on
// addresses of their own, requests given at chosen moments of the fake clock, and the events read back. What a
// real standard input is (a thread that reads) is not here; the source of these tests gives its lines by the
// clock, so that a cancel arrives while an operation is under way and the same every time.

#include "adl/load/events.hpp"
#include "adl/load/serve.hpp"
#include "bytes.hpp"
#include "command_rig.hpp"
#include "faulty_host.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using adl::test::CommandRig;
using adl::test::CommandRun;
using adl::test::InstalledMediaSet;
using adl::test::Json;
using adl::test::SimPlan;
using adl::test::SimTarget;
using namespace std::chrono_literals;

namespace
{

// Requests that arrive at the fake clock's times.
class ScriptedSource final : public adl::load::LineSource
{
public:
    explicit ScriptedSource(adl::fake::FakeWorld& world) : world_(world) {}

    // A line that is read whole.
    ScriptedSource& at(std::chrono::milliseconds when, std::string text)
    {
        return at(when, adl::load::InputLine{adl::load::LineEnd::line, std::move(text)});
    }
    ScriptedSource& at(std::chrono::milliseconds when, adl::load::InputLine line)
    {
        entries_.push_back(Entry{when, std::move(line), false});
        end_ = std::max(end_, when);
        return *this;
    }
    // The input ends at this time (never before its last line).
    ScriptedSource& ends_at(std::chrono::milliseconds when)
    {
        end_ = std::max(end_, when);
        return *this;
    }

    adl::platform::Result<void> start() override
    {
        ++starts;
        return {};
    }
    void poll(adl::platform::Instant now, std::vector<adl::load::InputLine>& lines) override
    {
        for (Entry& entry : entries_)
        {
            if (!entry.given && entry.when <= std::chrono::duration_cast<std::chrono::milliseconds>(now.since_origin))
            {
                entry.given = true;
                lines.push_back(entry.line);
            }
        }
    }
    [[nodiscard]] bool ended() const override
    {
        return std::ranges::all_of(entries_, [](const Entry& entry) { return entry.given; }) &&
               std::chrono::duration_cast<std::chrono::milliseconds>(world_.now().since_origin) >= end_;
    }

    int starts = 0;

private:
    struct Entry
    {
        std::chrono::milliseconds when;
        adl::load::InputLine line;
        bool given;
    };
    adl::fake::FakeWorld& world_;
    std::vector<Entry> entries_;
    std::chrono::milliseconds end_{0};
};

// What an upload reads before it starts, as the fake clock says: one request at a time, each taking as long as the
// test says, and the work run on the test's thread when its time comes, so that every run is the same.
class ScriptedPreparer final : public adl::load::Preparer
{
public:
    explicit ScriptedPreparer(adl::fake::FakeWorld& world) : world_(world) {}

    // How long the preparation with this ticket takes.
    std::function<std::chrono::milliseconds(std::uint64_t)> takes = [](std::uint64_t)
    { return std::chrono::milliseconds{0}; };
    // The tickets in the order they were submitted, and when each one was (milliseconds of the fake clock).
    std::vector<std::uint64_t> submitted;
    std::vector<std::chrono::milliseconds> submitted_at;
    int starts = 0;

    adl::platform::Result<void> start() override
    {
        ++starts;
        return {};
    }
    void submit(std::uint64_t ticket, adl::load::PreparationWork work) override
    {
        const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(world_.now().since_origin);
        const auto begins = std::max(now, free_at_);
        free_at_ = begins + takes(ticket);
        submitted.push_back(ticket);
        submitted_at.push_back(now);
        jobs_.push_back(Job{ticket, std::move(work), free_at_});
    }
    void poll(adl::platform::Instant now, std::vector<adl::load::Preparation>& done) override
    {
        const auto at = std::chrono::duration_cast<std::chrono::milliseconds>(now.since_origin);
        while (!jobs_.empty() && jobs_.front().ends_at <= at)
        {
            Job job = std::move(jobs_.front());
            jobs_.erase(jobs_.begin());
            done.push_back(adl::load::Preparation{job.ticket, job.work()});
        }
    }

private:
    struct Job
    {
        std::uint64_t ticket;
        adl::load::PreparationWork work;
        std::chrono::milliseconds ends_at;
    };
    adl::fake::FakeWorld& world_;
    std::vector<Job> jobs_;
    std::chrono::milliseconds free_at_{0};
};

// The loader at 10.0.0.1 and `count` simulated targets at 10.0.1.1, 10.0.1.2 and so on, on one fake network.
class ServeBench
{
public:
    explicit ServeBench(std::size_t count, const std::function<SimPlan(std::size_t)>& plan_of = {})
        : world(1), network(world), loader_host(world, network, adl::platform::Ipv4Address{{10, 0, 0, 1}}),
          preparer(world)
    {
        for (std::size_t k = 0; k < count; ++k)
        {
            targets.push_back(std::make_unique<SimTarget>(
                world, network, adl::platform::Ipv4Address{{10, 0, 1, static_cast<std::uint8_t>(k + 1)}},
                plan_of ? plan_of(k) : SimPlan{}));
        }
    }

    [[nodiscard]] static std::string address(std::size_t k)
    {
        return "10.0.1." + std::to_string(k + 1);
    }

    // `serve` with `options`, run on the loader's host to its end, with the loop's own clock.
    CommandRun serve(ScriptedSource& source, std::initializer_list<std::string_view> options = {})
    {
        return serve_on(loader_host, source, options);
    }
    CommandRun serve_on(adl::platform::Host& host, ScriptedSource& source,
                        std::initializer_list<std::string_view> options = {})
    {
        std::vector<std::string_view> arguments{"serve"};
        arguments.insert(arguments.end(), options.begin(), options.end());
        const auto parsed = adl::load::parse_options(arguments);
        REQUIRE(parsed.has_value());
        std::ostringstream out;
        std::ostringstream err;
        CommandRun result;
        result.exit_code = adl::load::run_serve(host, parsed->options, source, preparer, out, err, host.clock().now());
        result.out = out.str();
        result.err = err.str();
        world.advance_by(std::chrono::seconds(1)); // the targets see the loader's last datagram
        return result;
    }

    adl::fake::FakeWorld world;
    adl::fake::FakeNetwork network;
    adl::fake::FakeHost loader_host;
    ScriptedPreparer preparer;
    std::vector<std::unique_ptr<SimTarget>> targets;
};

// The fields of a request, separated by tabs.
std::string tabs(std::initializer_list<std::string_view> fields)
{
    std::string line;
    bool first = true;
    for (const std::string_view field : fields)
    {
        line += first ? "" : "\t";
        line += field;
        first = false;
    }
    return line;
}

std::string information_request(std::string_view id, std::size_t k)
{
    return tabs({"run", id, "information", "--target", ServeBench::address(k), "--position", "HNPFMS_L"});
}

std::string upload_request(std::string_view id, std::size_t k, const InstalledMediaSet& set,
                           std::initializer_list<std::string_view> more = {"--all-loads"})
{
    std::string line = tabs(
        {"run", id, "upload", "--target", ServeBench::address(k), "--position", "HNPFMS_L", "--media-set", set.path()});
    for (const std::string_view field : more)
    {
        line += '\t';
        line += field;
    }
    return line;
}

// The events of one operation, in order.
std::vector<Json> events_of(const CommandRun& run, std::string_view id)
{
    std::vector<Json> found;
    for (const Json& event : run.events())
    {
        if (event.at("event").text != "request_refused" && event.find("id") != nullptr && event.at("id").text == id)
        {
            found.push_back(event);
        }
    }
    return found;
}

std::vector<std::string> names_of(const std::vector<Json>& events)
{
    std::vector<std::string> names;
    for (const Json& event : events)
    {
        names.push_back(event.at("event").text);
    }
    return names;
}

std::vector<Json> refusals_of(const CommandRun& run)
{
    return run.events_named("request_refused");
}

std::uint64_t milliseconds_of(const Json& event)
{
    const std::string& t = event.at("t").text;
    return std::stoull(t.substr(0, t.size() - 4)) * 1000 + std::stoull(t.substr(t.size() - 3));
}

// What every run of `serve` has in common: `started` first and `stopped` last, in the order of the contract, `t`
// that does not go back, an `id` after `t` in every event of an operation, and for each operation `started` first,
// exactly one `finished` last, and its exit code a number.
void check_serve_stream(const CommandRun& run, std::string_view stopped_because)
{
    const std::vector<Json> events = run.events();
    REQUIRE(events.size() >= 2);
    CHECK(events.front().keys() == std::vector<std::string>{"event", "t", "program", "version", "events", "command",
                                                            "address", "control_port", "max_operations"});
    CHECK(events.front().at("event").text == "started");
    CHECK(events.front().at("events").text == "1");
    CHECK(events.front().at("command").text == "serve");
    CHECK(events.back().keys() == std::vector<std::string>{"event", "t", "reason", "operations"});
    CHECK(events.back().at("event").text == "stopped");
    CHECK(events.back().at("reason").text == stopped_because);

    std::uint64_t last = 0;
    std::vector<std::string> ids;
    for (std::size_t i = 0; i < events.size(); ++i)
    {
        const Json& event = events[i];
        const std::vector<std::string> keys = event.keys();
        REQUIRE(keys.size() >= 2);
        CHECK(keys[0] == "event");
        CHECK(keys[1] == "t");
        const std::uint64_t at = milliseconds_of(event);
        CHECK(at >= last);
        last = at;
        if (i == 0 || i + 1 == events.size())
        {
            continue; // the program's own `started` and `stopped`
        }
        // Every other event has an `id` after `t`: a refusal's may be null, an operation's is the operation's.
        REQUIRE(keys.size() >= 3);
        CHECK(keys[2] == "id");
        if (event.at("event").text == "request_refused")
        {
            CHECK(event.keys() == std::vector<std::string>{"event", "t", "id", "request", "line", "detail"});
            continue;
        }
        const std::string& id = event.at("id").text;
        if (std::ranges::find(ids, id) == ids.end())
        {
            ids.push_back(id);
        }
    }
    // Each id (it may be used again after its operation: these tests do not) has `started` first and one
    // `finished` last, whose exit code is a number.
    for (const std::string& id : ids)
    {
        const std::vector<Json> own = events_of(run, id);
        REQUIRE(own.size() >= 2);
        CHECK(own.front().at("event").text == "started");
        CHECK(own.back().at("event").text == "finished");
        CHECK(std::ranges::count(names_of(own), "finished") == 1);
        CHECK(own.back().at("exit_code").is(Json::Kind::number));
    }
}

const adl::test::SampleLoad& sample_load()
{
    static const adl::test::SampleLoad load = adl::test::make_sample_load(5'000, 3);
    return load;
}

SimPlan uploads_the_sample(std::size_t = 0)
{
    SimPlan plan;
    plan.loads = {sample_load().item};
    return plan;
}

SimPlan holding(std::initializer_list<const char*> names)
{
    SimPlan plan;
    for (const char* name : names)
    {
        plan.files.push_back(adl::test::DownloadItem{name, adl::test::bytes_of(std::string{"content of "} + name),
                                                     std::string{"the "} + name});
    }
    return plan;
}

} // namespace

TEST_CASE("serve with nothing to do starts, and stops when the input ends", "[load][integration][serve]")
{
    ServeBench bench(0);
    ScriptedSource source(bench.world);
    const CommandRun run = bench.serve(source);
    CHECK(run.exit_code == 0);
    CHECK(run.err.empty());
    CHECK(source.starts == 1);
    check_serve_stream(run, "end_of_input");

    const std::vector<Json> events = run.events();
    REQUIRE(events.size() == 2);
    CHECK(events[0].at("address").text == "0.0.0.0");
    CHECK(events[0].at("control_port").as_number() == 59);
    CHECK(events[0].at("max_operations").as_number() == 8);
    CHECK(events[1].at("operations").as_number() == 0);
}

TEST_CASE("serve says its interface, its port and its limit as they were asked for", "[load][integration][serve]")
{
    ServeBench bench(0);
    ScriptedSource source(bench.world);
    source.at(0ms, "stop");
    const CommandRun run =
        bench.serve(source, {"--local-address", "10.0.0.1", "--control-port", "30901", "--max-operations", "64"});
    CHECK(run.exit_code == 0);
    check_serve_stream(run, "stop");
    const Json started = run.events().front();
    CHECK(started.at("address").text == "10.0.0.1");
    CHECK(started.at("control_port").as_number() == 30901);
    CHECK(started.at("max_operations").as_number() == 64);
}

TEST_CASE("a stop ends the program, and what is running first finishes", "[load][integration][serve]")
{
    ServeBench bench(2);
    ScriptedSource source(bench.world);
    source.at(0ms, information_request("a", 0)).at(0ms, information_request("b", 1)).at(10ms, "stop");
    const CommandRun run = bench.serve(source);
    CHECK(run.exit_code == 0);
    check_serve_stream(run, "stop");
    for (const std::string id : {"a", "b"})
    {
        const std::vector<Json> own = events_of(run, id);
        CHECK(own.back().at("reason").text == "completed");
    }
    const std::vector<Json> events = run.events();
    CHECK(events.back().at("operations").as_number() == 2);
    // `stopped` comes after both `finished` events.
    CHECK(std::ranges::count(run.names(), "finished") == 2);
    CHECK(run.names().back() == "stopped");
}

TEST_CASE("two operations at once give each the events one adl_load would have written, with an id",
          "[load][integration][serve]")
{
    // What one adl_load writes for an Information operation on its own.
    CommandRig alone;
    const CommandRun single = alone.run({"information", "--target", "10.0.0.2", "--position", "HNPFMS_L", "--json"});
    REQUIRE(single.exit_code == 0);

    ServeBench bench(2);
    ScriptedSource source(bench.world);
    source.at(0ms, information_request("left", 0)).at(0ms, information_request("right", 1));
    const CommandRun run = bench.serve(source);
    CHECK(run.exit_code == 0);
    check_serve_stream(run, "end_of_input");

    for (const std::string id : {"left", "right"})
    {
        INFO("operation " << id);
        const std::vector<Json> own = events_of(run, id);
        CHECK(names_of(own) == single.names());
        // The same keys, with `id` after `t`.
        for (std::size_t i = 0; i < own.size(); ++i)
        {
            std::vector<std::string> expected = single.events()[i].keys();
            expected.insert(expected.begin() + 2, "id");
            CHECK(own[i].keys() == expected);
        }
        CHECK(own.back().at("exit_code").as_number() == 0);
        CHECK(own.back().at("result").text == "completed");
        CHECK(own.back().at("status_code").as_number() == 3);
        CHECK(own.front().at("target").text == (id == "left" ? "10.0.1.1" : "10.0.1.2"));
        CHECK(own.front().at("command").text == "information");
    }
    // Each target was asked once, by the one loader.
    CHECK(bench.targets[0]->log().init_requests_at.size() == 1);
    CHECK(bench.targets[1]->log().init_requests_at.size() == 1);
    CHECK(run.events().back().at("operations").as_number() == 2);
}

TEST_CASE("targets that are not on port 59 are reached with --target-port alone, the loader being on 59, and with both",
          "[load][integration][serve]")
{
    ServeBench bench(2,
                     [](std::size_t)
                     {
                         SimPlan plan;
                         plan.control_port = 30959; // each target listens on 30959 of its own address
                         return plan;
                     });
    ScriptedSource source(bench.world);
    source.at(0ms, tabs({"run", "alone", "information", "--target", ServeBench::address(0), "--position", "HNPFMS_L",
                         "--target-port", "30959"}));
    const CommandRun run = bench.serve(source);
    check_serve_stream(run, "end_of_input");
    CHECK(events_of(run, "alone").back().at("reason").text == "completed");
    // The loader's own port is 59, and the target was told so, because it is not on 59 itself.
    CHECK(bench.targets[0]->log().loader_port_used == 59);
    CHECK(bench.targets[0]->log().init_options_seen == 1);

    // With the loader's port moved as well, the target is told that one.
    ServeBench moved(1,
                     [](std::size_t)
                     {
                         SimPlan plan;
                         plan.control_port = 30959;
                         return plan;
                     });
    ScriptedSource moved_source(moved.world);
    moved_source.at(0ms, tabs({"run", "both", "information", "--target", ServeBench::address(0), "--position",
                               "HNPFMS_L", "--target-port", "30959"}));
    const CommandRun both = moved.serve(moved_source, {"--control-port", "30901"});
    check_serve_stream(both, "end_of_input");
    CHECK(events_of(both, "both").back().at("reason").text == "completed");
    CHECK(moved.targets[0]->log().loader_port_used == 30901);
}

TEST_CASE("eight uploads at once from one port load eight targets, each with its own record",
          "[load][integration][serve]")
{
    // How long one upload takes by itself.
    std::chrono::nanoseconds alone{};
    {
        ServeBench bench(1, uploads_the_sample);
        const InstalledMediaSet set(bench.loader_host, sample_load().members);
        ScriptedSource source(bench.world);
        source.at(0ms, upload_request("only", 0, set));
        const CommandRun run = bench.serve(source);
        REQUIRE(run.exit_code == 0);
        CHECK(events_of(run, "only").back().at("reason").text == "completed");
        alone = bench.world.now().since_origin;
    }

    ServeBench bench(8, uploads_the_sample);
    const InstalledMediaSet set(bench.loader_host, sample_load().members);
    ScriptedSource source(bench.world);
    for (std::size_t k = 0; k < 8; ++k)
    {
        const std::string id = "u" + std::to_string(k);
        const std::string record = "/" + id + ".json";
        source.at(0ms, upload_request(id, k, set, {"--all-loads", "--record", record}));
    }
    const CommandRun run = bench.serve(source);
    const auto together = bench.world.now().since_origin;
    CHECK(run.exit_code == 0);
    CHECK(run.err.empty());
    check_serve_stream(run, "end_of_input");

    std::vector<std::string> records;
    for (std::size_t k = 0; k < 8; ++k)
    {
        const std::string id = "u" + std::to_string(k);
        INFO("operation " << id);
        const std::vector<Json> own = events_of(run, id);
        REQUIRE_FALSE(own.empty());
        const Json& finished = own.back();
        CHECK(finished.at("reason").text == "completed");
        CHECK(finished.at("exit_code").as_number() == 0);
        CHECK(finished.at("record").text == "/" + id + ".json");
        CHECK(names_of(own)[1] == "media_set_checked");
        CHECK(bench.targets[k]->log().files_read.size() == sample_load().item.reads.size());
        CHECK(bench.targets[k]->log().files_corrupt.empty());
        records.push_back(adl::test::fake_text(bench.loader_host, "/" + id + ".json"));
        CHECK(records.back().find("\"address\": \"" + ServeBench::address(k) + "\"") != std::string::npos);
    }
    std::ranges::sort(records);
    CHECK(std::ranges::adjacent_find(records) == records.end()); // no two are the same
    // At once, not one after the other.
    CHECK(together < alone * 3);
}

TEST_CASE("sixty-four operations at once, the most there can be, and a sixty-fifth is refused",
          "[load][integration][serve]")
{
    // More sockets are in use at once than one wait on the network takes (64): the loop still sees every datagram.
    ServeBench bench(65);
    ScriptedSource source(bench.world);
    for (std::size_t k = 0; k < 65; ++k)
    {
        source.at(0ms, information_request("t" + std::to_string(k), k));
    }
    const CommandRun run = bench.serve(source, {"--max-operations", "64"});
    CHECK(run.exit_code == 0);
    check_serve_stream(run, "end_of_input");

    const std::vector<Json> refused = refusals_of(run);
    REQUIRE(refused.size() == 1);
    CHECK(refused[0].at("id").text == "t64");
    CHECK(refused[0].at("line").as_number() == 65);
    CHECK(refused[0].at("detail").text == "The program runs 64 operations at once, and that many have not finished.");
    for (std::size_t k = 0; k < 64; ++k)
    {
        INFO("operation t" << k);
        const std::vector<Json> own = events_of(run, "t" + std::to_string(k));
        REQUIRE_FALSE(own.empty());
        CHECK(own.back().at("reason").text == "completed");
    }
    CHECK(run.events().back().at("operations").as_number() == 64);
}

TEST_CASE("a cancel aborts that operation, and the others go on", "[load][integration][serve]")
{
    ServeBench bench(2,
                     [](std::size_t k)
                     {
                         SimPlan plan = uploads_the_sample();
                         if (k == 0)
                         {
                             plan.work_time = 60s; // it keeps at it long after the last file
                         }
                         return plan;
                     });
    const InstalledMediaSet set(bench.loader_host, sample_load().members);
    ScriptedSource source(bench.world);
    source.at(0ms, upload_request("a", 0, set)).at(0ms, upload_request("b", 1, set)).at(5000ms, "cancel\ta");
    const CommandRun run = bench.serve(source);
    CHECK(run.exit_code == 0);
    check_serve_stream(run, "end_of_input");

    const Json a = events_of(run, "a").back();
    CHECK(a.at("result").text == "failed");
    CHECK(a.at("reason").text == "aborted");
    CHECK(a.at("exit_code").as_number() == 1);
    // It was aborted well before the minute that the target would have gone on for.
    CHECK(milliseconds_of(a) < 30'000);
    const Json b = events_of(run, "b").back();
    CHECK(b.at("reason").text == "completed");
    CHECK(refusals_of(run).empty());
    CHECK(run.events().back().at("operations").as_number() == 2);
}

TEST_CASE("a cancel that names no operation is refused, and so is one for an operation that has finished",
          "[load][integration][serve]")
{
    ServeBench bench(1);
    ScriptedSource source(bench.world);
    source.at(0ms, "cancel\tnobody").at(0ms, information_request("a", 0)).at(60'000ms, "cancel\ta");
    const CommandRun run = bench.serve(source);
    CHECK(run.exit_code == 0);
    check_serve_stream(run, "end_of_input");
    const std::vector<Json> refused = refusals_of(run);
    REQUIRE(refused.size() == 2);
    CHECK(refused[0].keys() == std::vector<std::string>{"event", "t", "id", "request", "line", "detail"});
    CHECK(refused[0].at("request").text == "cancel");
    CHECK(refused[0].at("id").text == "nobody");
    CHECK(refused[0].at("line").as_number() == 1);
    CHECK(refused[0].at("detail").text == "No operation with this id is running.");
    CHECK(refused[1].at("id").text == "a");
    CHECK(refused[1].at("line").as_number() == 3);
    CHECK(events_of(run, "a").back().at("reason").text == "completed");
}

TEST_CASE("a request that cannot be started is refused with its line number, and nothing of it is written",
          "[load][integration][serve]")
{
    ServeBench bench(3);
    ScriptedSource source(bench.world);

    SECTION("a second operation on an address that is in one")
    {
        source.at(0ms, information_request("a", 0)).at(0ms, information_request("b", 0));
        const CommandRun run = bench.serve(source);
        check_serve_stream(run, "end_of_input");
        const std::vector<Json> refused = refusals_of(run);
        REQUIRE(refused.size() == 1);
        CHECK(refused[0].at("id").text == "b");
        CHECK(refused[0].at("line").as_number() == 2);
        CHECK(refused[0].at("request").text == "run");
        CHECK(refused[0].at("detail").text == "The target 10.0.1.1 has an operation that has not finished (id a).");
        CHECK(events_of(run, "b").empty());
        CHECK(events_of(run, "a").back().at("reason").text == "completed");
        CHECK(run.events().back().at("operations").as_number() == 1);
    }

    SECTION("an id that is in use")
    {
        source.at(0ms, information_request("a", 0)).at(0ms, information_request("a", 1));
        const CommandRun run = bench.serve(source);
        check_serve_stream(run, "end_of_input");
        const std::vector<Json> refused = refusals_of(run);
        REQUIRE(refused.size() == 1);
        CHECK(refused[0].at("id").text == "a");
        CHECK(refused[0].at("line").as_number() == 2);
        CHECK(refused[0].at("request").text == "run");
        CHECK(refused[0].at("detail").text == "An operation with this id has not finished.");
        // The operation of that id is the first one, and nothing was sent to the other target.
        CHECK(events_of(run, "a").front().at("target").text == "10.0.1.1");
        CHECK(bench.targets[1]->log().init_requests_at.empty());
    }

    SECTION("an id that is free again once its operation has finished")
    {
        source.at(0ms, information_request("a", 0)).at(60'000ms, information_request("a", 1));
        const CommandRun run = bench.serve(source);
        CHECK(run.exit_code == 0);
        CHECK(run.events().back().at("reason").text == "end_of_input");
        CHECK(refusals_of(run).empty());
        // Two operations of one id, one after the other: two `started` and two `finished`.
        const std::vector<Json> own = events_of(run, "a");
        CHECK(std::ranges::count(names_of(own), "started") == 2);
        CHECK(std::ranges::count(names_of(own), "finished") == 2);
        CHECK(run.events().back().at("operations").as_number() == 2);
    }

    SECTION("more operations than --max-operations")
    {
        source.at(0ms, information_request("a", 0))
            .at(0ms, information_request("b", 1))
            .at(0ms, information_request("c", 2));
        const CommandRun run = bench.serve(source, {"--max-operations", "2"});
        check_serve_stream(run, "end_of_input");
        const std::vector<Json> refused = refusals_of(run);
        REQUIRE(refused.size() == 1);
        CHECK(refused[0].at("id").text == "c");
        CHECK(refused[0].at("line").as_number() == 3);
        CHECK(refused[0].at("detail").text ==
              "The program runs 2 operations at once, and that many have not finished.");
        CHECK(run.events().front().at("max_operations").as_number() == 2);
        CHECK(run.events().back().at("operations").as_number() == 2);
        CHECK(bench.targets[2]->log().init_requests_at.empty());
    }

    SECTION("wrong arguments, one the program owns, and a command that is not a request")
    {
        source.at(0ms, tabs({"run", "a", "information", "--target", "10.0.1.1"}))
            .at(0ms, tabs({"run", "b", "information", "--target", "10.0.1.1", "--position", "P", "--json"}))
            .at(0ms, tabs({"run", "c", "find"}))
            .at(0ms, tabs({"run", "d", "upload", "--target", "10.0.1.1", "--position", "P", "--media-set", "m"}));
        const CommandRun run = bench.serve(source);
        check_serve_stream(run, "end_of_input");
        const std::vector<Json> refused = refusals_of(run);
        REQUIRE(refused.size() == 4);
        CHECK(refused[0].at("detail").text == "--position is missing.");
        CHECK(refused[1].at("detail").text == "--json is not taken in a request: it belongs to the program.");
        CHECK(refused[2].at("detail").text.find("is not an operation on one target") != std::string::npos);
        CHECK(refused[3].at("detail").text == "Name the loads to send with --load, or give --all-loads.");
        for (std::size_t i = 0; i < 4; ++i)
        {
            CHECK(refused[i].at("request").text == "run");
            CHECK(refused[i].at("id").text == std::string(1, static_cast<char>('a' + i)));
            CHECK(refused[i].at("line").as_number() == i + 1);
        }
        CHECK(run.events().back().at("operations").as_number() == 0);
        CHECK(run.names().size() == 6); // started, four refusals, stopped
    }

    SECTION("a load the media set does not hold is told once the set has been read, and nothing is started")
    {
        const InstalledMediaSet set(bench.loader_host, sample_load().members);
        source.at(0ms, upload_request("a", 0, set, {"--load", "NO-SUCH-PART"}));
        const CommandRun run = bench.serve(source);
        check_serve_stream(run, "end_of_input");
        const std::vector<Json> refused = refusals_of(run);
        REQUIRE(refused.size() == 1);
        CHECK(refused[0].at("id").text == "a");
        CHECK(refused[0].at("request").text == "run");
        CHECK(refused[0].at("detail").text == "The media set has no load with the part number \"NO-SUCH-PART\".");
        CHECK(events_of(run, "a").empty());
        CHECK(run.events().back().at("operations").as_number() == 0);
    }
}

TEST_CASE("a line that cannot be read is refused with no id, and the lines after it are read",
          "[load][integration][serve]")
{
    ServeBench bench(1);
    ScriptedSource source(bench.world);
    source.at(0ms, "this is not a request")
        .at(0ms, "")
        .at(0ms, adl::load::InputLine{adl::load::LineEnd::too_long, {}})
        .at(0ms, "run\ta1\tinformation\t--target\t10.0.1.1\t--position\tP\xff")
        .at(0ms, "stop\tnow")
        .at(0ms, information_request("a", 0))
        .at(10ms, adl::load::InputLine{adl::load::LineEnd::unfinished, "cancel\ta"});
    const CommandRun run = bench.serve(source);
    CHECK(run.exit_code == 0);
    check_serve_stream(run, "end_of_input");

    const std::vector<Json> refused = refusals_of(run);
    REQUIRE(refused.size() == 6);
    struct Expected
    {
        std::uint64_t line;
        std::optional<std::string> request; // which request the line was meant to be, where that could be read
        std::string detail;
    };
    const std::vector<Expected> expected{
        {1, std::nullopt, "Unknown request \"this is not a request\": run, cancel or stop."},
        {2, std::nullopt, "The line is empty."},
        {3, std::nullopt, "The line is longer than 1048576 bytes."},
        {4, std::nullopt, "The line is not valid UTF-8."},
        {5, "stop", "stop takes no fields."},
        {7, std::nullopt, "The input ended in the middle of a line, which is not a request."},
    };
    for (std::size_t i = 0; i < expected.size(); ++i)
    {
        INFO("refusal " << i);
        CHECK(refused[i].at("id").is(Json::Kind::null));
        if (expected[i].request)
        {
            CHECK(refused[i].at("request").text == *expected[i].request);
        }
        else
        {
            CHECK(refused[i].at("request").is(Json::Kind::null));
        }
        CHECK(refused[i].at("line").as_number() == expected[i].line);
        CHECK(refused[i].at("detail").text == expected[i].detail);
    }
    // The cut-off cancel was not taken for one, and the request on line 6 ran.
    CHECK(events_of(run, "a").back().at("reason").text == "completed");
    CHECK(run.events().back().at("operations").as_number() == 1);
}

TEST_CASE("after a stop a run is refused, a cancel is still taken and a second stop changes nothing",
          "[load][integration][serve]")
{
    ServeBench bench(2);
    ScriptedSource source(bench.world);
    source.at(0ms, information_request("a", 0))
        .at(0ms, "stop")
        .at(0ms, information_request("b", 1))
        .at(0ms, "cancel	a")
        .at(0ms, "stop")
        .at(0ms, "cancel	nobody");
    const CommandRun run = bench.serve(source);
    CHECK(run.exit_code == 0);
    check_serve_stream(run, "stop");
    const std::vector<Json> refused = refusals_of(run);
    REQUIRE(refused.size() == 2);
    // The run after the stop is refused, and says it was a run; the cancel of an id that is not running is refused as
    // it is at any time. The cancel of the one that is, and the second stop, are not.
    CHECK(refused[0].at("id").text == "b");
    CHECK(refused[0].at("request").text == "run");
    CHECK(refused[0].at("line").as_number() == 3);
    CHECK(refused[0].at("detail").text == "The program has been told to stop and starts no more operations.");
    CHECK(refused[1].at("id").text == "nobody");
    CHECK(refused[1].at("request").text == "cancel");
    CHECK(refused[1].at("line").as_number() == 6);
    // What was running was ended by the cancel, which is what a caller that is told to end needs to be able to do.
    CHECK(events_of(run, "a").back().at("reason").text == "aborted");
    CHECK(events_of(run, "b").empty());
    CHECK(run.events().back().at("operations").as_number() == 1);
}

TEST_CASE("the end of the input while operations run is a stop: they finish, then it ends",
          "[load][integration][serve]")
{
    ServeBench bench(2, uploads_the_sample);
    const InstalledMediaSet set(bench.loader_host, sample_load().members);
    ScriptedSource source(bench.world);
    source.at(0ms, upload_request("a", 0, set)).at(0ms, upload_request("b", 1, set)).ends_at(100ms);
    const CommandRun run = bench.serve(source);
    CHECK(run.exit_code == 0);
    check_serve_stream(run, "end_of_input");
    CHECK(events_of(run, "a").back().at("reason").text == "completed");
    CHECK(events_of(run, "b").back().at("reason").text == "completed");
    // The input ended long before the operations did.
    const std::vector<Json> events = run.events();
    CHECK(milliseconds_of(events.back()) > 1000);
    CHECK(events.back().at("operations").as_number() == 2);
}

TEST_CASE("an operation that fails ends beside one that completes, and serve still exits 0",
          "[load][integration][serve]")
{
    ServeBench bench(3,
                     [](std::size_t k)
                     {
                         SimPlan plan;
                         if (k == 1)
                         {
                             plan.init_status = adl::arinc615a::StatusCode::operation_denied;
                         }
                         if (k == 2)
                         {
                             plan.init_silent = true;
                         }
                         return plan;
                     });
    ScriptedSource source(bench.world);
    source.at(0ms, information_request("ok", 0))
        .at(0ms, information_request("refused", 1))
        .at(0ms, information_request("silent", 2));
    const CommandRun run = bench.serve(source);
    CHECK(run.exit_code == 0);
    check_serve_stream(run, "end_of_input");

    const Json ok = events_of(run, "ok").back();
    CHECK(ok.at("reason").text == "completed");
    CHECK(ok.at("exit_code").as_number() == 0);
    const Json refused = events_of(run, "refused").back();
    CHECK(refused.at("result").text == "failed");
    CHECK(refused.at("reason").text == "target_refused");
    CHECK(refused.at("status_code").as_number() == 0x1000);
    CHECK(refused.at("exit_code").as_number() == 1);
    const Json silent = events_of(run, "silent").back();
    CHECK(silent.at("reason").text == "no_answer");
    CHECK(silent.at("exit_code").as_number() == 4);
}

TEST_CASE("a media set that cannot be read ends that operation alone, with its own events",
          "[load][integration][serve]")
{
    ServeBench bench(2);
    ScriptedSource source(bench.world);
    source
        .at(0ms, tabs({"run", "bad", "upload", "--target", "10.0.1.1", "--position", "HNPFMS_L", "--media-set",
                       "/not/a/media/set", "--all-loads"}))
        .at(0ms, information_request("good", 1));
    const CommandRun run = bench.serve(source);
    CHECK(run.exit_code == 0);
    check_serve_stream(run, "end_of_input");

    const std::vector<Json> bad = events_of(run, "bad");
    CHECK(names_of(bad) == std::vector<std::string>{"started", "log", "log", "finished"});
    CHECK(bad.back().at("reason").text == "media_set_unreadable");
    CHECK(bad.back().at("exit_code").as_number() == 3);
    CHECK(events_of(run, "good").back().at("reason").text == "completed");
    // Both were started, and the program was not held up by the one that never reached the target.
    CHECK(run.events().back().at("operations").as_number() == 2);
    CHECK(bench.targets[0]->log().init_requests_at.empty());
}

TEST_CASE("a download that lists its files, picked from serve", "[load][integration][serve]")
{
    ServeBench bench(2, [](std::size_t) { return holding({"A.BIN", "B.BIN"}); });
    ScriptedSource source(bench.world);
    source
        .at(0ms, tabs({"run", "d1", "download", "--target", "10.0.1.1", "--position", "HNPFMS_L", "--into", "/dl",
                       "--pick-all"}))
        .at(0ms, tabs({"run", "d2", "download", "--target", "10.0.1.2", "--position", "HNPFMS_L", "--into", "/dl",
                       "--file", "B.BIN"}));
    const CommandRun run = bench.serve(source);
    CHECK(run.exit_code == 0);
    check_serve_stream(run, "end_of_input");

    const std::vector<Json> d1 = events_of(run, "d1");
    const std::vector<std::string> names = names_of(d1);
    CHECK(std::ranges::count(names, "file_list") == 1);
    CHECK(std::ranges::count(names, "file_received") == 2);
    CHECK(d1.back().at("reason").text == "completed");
    CHECK(d1.back().find("download_folder") != nullptr);
    const std::vector<Json> d2 = events_of(run, "d2");
    CHECK(std::ranges::count(names_of(d2), "file_received") == 1);
    CHECK(d2.back().at("reason").text == "completed");
    // Each download has a folder of its own in the one named.
    CHECK(d1.back().at("download_folder").text != d2.back().at("download_folder").text);
}

TEST_CASE("serve that cannot open its port says so and writes no event", "[load][integration][serve]")
{
    ServeBench bench(0);
    ScriptedSource source(bench.world);
    source.at(0ms, "stop");

    SECTION("the port is held by something else")
    {
        auto holder = bench.loader_host.network().open_udp(
            adl::platform::UdpOptions{adl::platform::Endpoint{adl::platform::Ipv4Address::any(), 59}});
        REQUIRE(holder.has_value());
        const CommandRun run = bench.serve(source);
        CHECK(run.exit_code == 5);
        CHECK(run.out.empty());
        CHECK(run.err ==
              "adl_load: the loader's port could not be opened (address in use). Give --local-address <ipv4> to listen "
              "on one interface only.\n");
        CHECK(source.starts == 0); // nothing was read
    }

    SECTION("the host refuses the socket")
    {
        adl::test::FaultyHost host(bench.loader_host);
        host.faulty_network().open_fails = adl::platform::ErrorCode::permission_denied;
        const CommandRun run = bench.serve_on(host, source);
        CHECK(run.exit_code == 5);
        CHECK(run.out.empty());
        CHECK(run.err.find("could not be opened") != std::string::npos);
    }
}

TEST_CASE("serve whose input cannot be read ends with exit code 5", "[load][integration][serve]")
{
    class Failing final : public adl::load::LineSource
    {
    public:
        adl::platform::Result<void> start() override
        {
            return adl::platform::fail(adl::platform::ErrorCode::resource_exhausted);
        }
        void poll(adl::platform::Instant, std::vector<adl::load::InputLine>&) override {}
        [[nodiscard]] bool ended() const override
        {
            return false;
        }
    };
    ServeBench bench(0);
    Failing source;
    std::vector<std::string_view> arguments{"serve"};
    const auto parsed = adl::load::parse_options(arguments);
    REQUIRE(parsed.has_value());
    std::ostringstream out;
    std::ostringstream err;
    const int code = adl::load::run_serve(bench.loader_host, parsed->options, source, bench.preparer, out, err,
                                          bench.loader_host.clock().now());
    CHECK(code == 5);
    CHECK(out.str().empty());
    CHECK(err.str() == "adl_load: the requests could not be read (resource exhausted).\n");
}

TEST_CASE("standard output that cannot be written to ends serve with exit code 5", "[load][integration][serve]")
{
    ServeBench bench(1);
    ScriptedSource source(bench.world);
    source.at(0ms, information_request("a", 0));
    std::vector<std::string_view> arguments{"serve"};
    const auto parsed = adl::load::parse_options(arguments);
    REQUIRE(parsed.has_value());
    std::ostringstream out;
    out.setstate(std::ios::badbit);
    std::ostringstream err;
    const int code = adl::load::run_serve(bench.loader_host, parsed->options, source, bench.preparer, out, err,
                                          bench.loader_host.clock().now());
    CHECK(code == 5);
}

TEST_CASE("an upload's media set is read away from the loop: the operations beside it are not held up, and nothing is "
          "written for its id until it is read",
          "[load][integration][serve][prepare]")
{
    ServeBench bench(4, uploads_the_sample);
    const InstalledMediaSet set(bench.loader_host, sample_load().members);
    // The first request to be prepared takes twenty seconds, longer than a target waits for an answer.
    bench.preparer.takes = [](std::uint64_t ticket) { return ticket == 1 ? 20'000ms : 0ms; };
    ScriptedSource source(bench.world);
    source.at(0ms, upload_request("big", 0, set))
        .at(0ms, information_request("beside", 1))
        // While the set is read, its id and its address are in use, and it counts among the operations.
        .at(1000ms, information_request("big", 2))
        .at(1000ms, information_request("other", 0))
        .at(1000ms, "cancel\tnobody");
    const CommandRun run = bench.serve(source, {"--max-operations", "3"});
    CHECK(run.exit_code == 0);
    check_serve_stream(run, "end_of_input");

    const std::vector<Json> beside = events_of(run, "beside");
    const std::vector<Json> big = events_of(run, "big");
    REQUIRE_FALSE(beside.empty());
    REQUIRE_FALSE(big.empty());
    CHECK(beside.back().at("reason").text == "completed");
    // The operation beside it ended while the set was still being read, and nothing at all was written for the set's id
    // before the read ended.
    CHECK(milliseconds_of(beside.back()) < 20'000);
    CHECK(big.front().at("event").text == "started");
    CHECK(milliseconds_of(big.front()) >= 20'000);
    CHECK(big.back().at("reason").text == "completed");
    CHECK(names_of(big)[1] == "media_set_checked");
    CHECK(bench.targets[0]->log().files_read.size() == sample_load().item.reads.size());

    const std::vector<Json> refused = refusals_of(run);
    REQUIRE(refused.size() == 3);
    CHECK(refused[0].at("id").text == "big");
    CHECK(refused[0].at("detail").text == "An operation with this id has not finished.");
    CHECK(refused[1].at("id").text == "other");
    CHECK(refused[1].at("detail").text == "The target 10.0.1.1 has an operation that has not finished (id big).");
    CHECK(refused[2].at("request").text == "cancel");
    CHECK(run.events().back().at("operations").as_number() == 2);
    // `started` of the big one comes after the end of the one beside it.
    const std::vector<std::string> names = run.names();
    std::size_t beside_ended = 0;
    std::size_t big_started = 0;
    for (std::size_t i = 0; i < run.events().size(); ++i)
    {
        const Json event = run.events()[i];
        if (event.find("id") == nullptr || event.at("event").text == "request_refused")
        {
            continue;
        }
        if (event.at("id").text == "beside" && event.at("event").text == "finished")
        {
            beside_ended = i;
        }
        if (event.at("id").text == "big" && event.at("event").text == "started")
        {
            big_started = i;
        }
    }
    CHECK(beside_ended < big_started);
}

TEST_CASE("a preparation counts among the operations that run at once", "[load][integration][serve][prepare]")
{
    ServeBench bench(3, uploads_the_sample);
    const InstalledMediaSet set(bench.loader_host, sample_load().members);
    bench.preparer.takes = [](std::uint64_t) { return 10'000ms; };
    ScriptedSource source(bench.world);
    source.at(0ms, upload_request("a", 0, set))
        .at(0ms, upload_request("b", 1, set))
        .at(0ms, upload_request("c", 2, set));
    const CommandRun run = bench.serve(source, {"--max-operations", "2"});
    check_serve_stream(run, "end_of_input");
    const std::vector<Json> refused = refusals_of(run);
    REQUIRE(refused.size() == 1);
    CHECK(refused[0].at("id").text == "c");
    CHECK(refused[0].at("detail").text == "The program runs 2 operations at once, and that many have not finished.");
    CHECK(events_of(run, "a").back().at("reason").text == "completed");
    CHECK(events_of(run, "b").back().at("reason").text == "completed");
    // One request is prepared at a time, in the order they came: b's set is read after a's.
    CHECK(milliseconds_of(events_of(run, "a").front()) >= 10'000);
    CHECK(milliseconds_of(events_of(run, "b").front()) >= 20'000);
    CHECK(bench.preparer.submitted == std::vector<std::uint64_t>{1, 2});
}

TEST_CASE("a cancel while the media set is read is taken: the operation starts, and ends aborted at once",
          "[load][integration][serve][prepare]")
{
    ServeBench bench(1, uploads_the_sample);
    const InstalledMediaSet set(bench.loader_host, sample_load().members);
    bench.preparer.takes = [](std::uint64_t) { return 10'000ms; };
    ScriptedSource source(bench.world);
    source.at(0ms, upload_request("u", 0, set)).at(2000ms, "cancel\tu");
    const CommandRun run = bench.serve(source);
    CHECK(run.exit_code == 0);
    check_serve_stream(run, "end_of_input");
    CHECK(refusals_of(run).empty()); // the cancel was taken, not refused

    const std::vector<Json> own = events_of(run, "u");
    REQUIRE_FALSE(own.empty());
    // Nothing was written until the set had been read, and then the operation says how it ended and no more.
    CHECK(milliseconds_of(own.front()) >= 10'000);
    CHECK(names_of(own).front() == "started");
    CHECK(names_of(own)[1] == "media_set_checked");
    CHECK(own.back().at("reason").text == "aborted");
    CHECK(own.back().at("result").text == "failed");
    CHECK(own.back().at("exit_code").as_number() == 1);
    CHECK(own.back().at("status_code").is(Json::Kind::null));
    // The target was never asked anything.
    CHECK(bench.targets[0]->log().init_requests_at.empty());
    CHECK(run.events().back().at("operations").as_number() == 1);
}

TEST_CASE("a preparation that finds wrong usage refuses the run, with its own line, and frees its id and address",
          "[load][integration][serve][prepare]")
{
    ServeBench bench(2, uploads_the_sample);
    const InstalledMediaSet set(bench.loader_host, sample_load().members);
    bench.preparer.takes = [](std::uint64_t ticket) { return ticket == 1 ? 5'000ms : 0ms; };
    ScriptedSource source(bench.world);
    source.at(0ms, upload_request("u", 0, set, {"--load", "NO-SUCH-PART"}))
        .at(1000ms, information_request("i", 1))
        .at(20'000ms, upload_request("u", 0, set));
    const CommandRun run = bench.serve(source);
    CHECK(run.exit_code == 0);
    check_serve_stream(run, "end_of_input");

    const std::vector<Json> refused = refusals_of(run);
    REQUIRE(refused.size() == 1);
    CHECK(refused[0].at("id").text == "u");
    CHECK(refused[0].at("request").text == "run");
    CHECK(refused[0].at("line").as_number() == 1); // the line of the request, not of the input when it was found out
    CHECK(refused[0].at("detail").text == "The media set has no load with the part number \"NO-SUCH-PART\".");
    CHECK(milliseconds_of(refused[0]) >= 5'000);
    // The same id and the same address are free again, and the second request ran.
    const std::vector<Json> own = events_of(run, "u");
    REQUIRE_FALSE(own.empty());
    CHECK(own.back().at("reason").text == "completed");
    CHECK(run.events().back().at("operations").as_number() == 2);
}

TEST_CASE("a stop while a media set is read waits for it, and the operation goes on",
          "[load][integration][serve][prepare]")
{
    ServeBench bench(1, uploads_the_sample);
    const InstalledMediaSet set(bench.loader_host, sample_load().members);
    bench.preparer.takes = [](std::uint64_t) { return 10'000ms; };
    ScriptedSource source(bench.world);
    source.at(0ms, upload_request("u", 0, set)).at(1000ms, "stop");
    const CommandRun run = bench.serve(source);
    CHECK(run.exit_code == 0);
    check_serve_stream(run, "stop");
    CHECK(events_of(run, "u").back().at("reason").text == "completed");
    CHECK(run.events().back().at("operations").as_number() == 1);
    CHECK(milliseconds_of(run.events().back()) > 10'000);
}

TEST_CASE("an upload whose media set cannot be read ends there, after the wait, with its own events",
          "[load][integration][serve][prepare]")
{
    ServeBench bench(2);
    bench.preparer.takes = [](std::uint64_t) { return 3'000ms; };
    ScriptedSource source(bench.world);
    source
        .at(0ms, tabs({"run", "bad", "upload", "--target", "10.0.1.1", "--position", "HNPFMS_L", "--media-set",
                       "/not/a/media/set", "--all-loads"}))
        .at(0ms, information_request("good", 1));
    const CommandRun run = bench.serve(source);
    check_serve_stream(run, "end_of_input");
    const std::vector<Json> bad = events_of(run, "bad");
    CHECK(names_of(bad) == std::vector<std::string>{"started", "log", "log", "finished"});
    CHECK(milliseconds_of(bad.front()) >= 3'000);
    CHECK(bad.back().at("reason").text == "media_set_unreadable");
    CHECK(events_of(run, "good").back().at("reason").text == "completed");
}

TEST_CASE("a preparer that cannot start ends serve with exit code 5, before anything is read",
          "[load][integration][serve][prepare]")
{
    class Failing final : public adl::load::Preparer
    {
    public:
        adl::platform::Result<void> start() override
        {
            return adl::platform::fail(adl::platform::ErrorCode::resource_exhausted);
        }
        void submit(std::uint64_t, adl::load::PreparationWork) override {}
        void poll(adl::platform::Instant, std::vector<adl::load::Preparation>&) override {}
    };
    ServeBench bench(0);
    ScriptedSource source(bench.world);
    Failing preparer;
    std::vector<std::string_view> arguments{"serve"};
    const auto parsed = adl::load::parse_options(arguments);
    REQUIRE(parsed.has_value());
    std::ostringstream out;
    std::ostringstream err;
    const int code = adl::load::run_serve(bench.loader_host, parsed->options, source, preparer, out, err,
                                          bench.loader_host.clock().now());
    CHECK(code == 5);
    CHECK(out.str().empty());
    CHECK(err.str() == "adl_load: the media sets could not be prepared (resource exhausted).\n");
    CHECK(source.starts == 0);
}

TEST_CASE("a request line can be read whatever the characters of its fields", "[load][integration][serve]")
{
    // A media set in a folder named with characters outside any one code page, given as the bytes they are in a
    // request: serve takes them as they arrive and the program finds the folder.
    const std::string name = "wide-\xc3\xa9-\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e-\xf0\x9f\x98\x80 and spaces";
    ServeBench bench(1, uploads_the_sample);
    const InstalledMediaSet set(bench.loader_host, sample_load().members);
    ScriptedSource source(bench.world);
    source.at(0ms, upload_request("w", 0, set, {"--all-loads", "--record", "/" + name + ".json"}));
    const CommandRun run = bench.serve(source);
    check_serve_stream(run, "end_of_input");
    const Json finished = events_of(run, "w").back();
    CHECK(finished.at("reason").text == "completed");
    CHECK(finished.at("record").text == "/" + name + ".json");
}

TEST_CASE("a line source over a stream gives the lines as the stream has them, and says when it has ended",
          "[load][integration][serve]")
{
    ServeBench bench(0);

    SECTION("lines, a line too long and a line left unfinished")
    {
        std::istringstream in("run\ta\tinformation\r\nstop\n\nlast");
        adl::load::StreamLineSource source(bench.loader_host, in);
        CHECK_FALSE(source.ended());
        REQUIRE(source.start().has_value());
        CHECK_FALSE(source.ended()); // a fake thread has not run yet
        std::vector<adl::load::InputLine> lines;
        source.poll(bench.loader_host.clock().now(), lines);
        CHECK(lines.empty());

        CHECK(bench.loader_host.fake_threads().run_pending() == 1);
        CHECK_FALSE(source.ended()); // there are lines that nobody has taken
        source.poll(bench.loader_host.clock().now(), lines);
        REQUIRE(lines.size() == 4);
        CHECK(lines[0].end == adl::load::LineEnd::line);
        CHECK(lines[0].text == "run\ta\tinformation");
        CHECK(lines[1].text == "stop");
        CHECK(lines[2].text.empty());
        CHECK(lines[3].end == adl::load::LineEnd::unfinished);
        CHECK(lines[3].text == "last");
        CHECK(source.ended());
        lines.clear();
        source.poll(bench.loader_host.clock().now(), lines);
        CHECK(lines.empty());
    }

    SECTION("an empty input has ended once it has been read")
    {
        std::istringstream in;
        adl::load::StreamLineSource source(bench.loader_host, in);
        REQUIRE(source.start().has_value());
        static_cast<void>(bench.loader_host.fake_threads().run_pending());
        CHECK(source.ended());
    }

    SECTION("a source that is dropped before its input has ended leaves its thread alone")
    {
        std::istringstream in("never read\n");
        {
            adl::load::StreamLineSource source(bench.loader_host, in);
            REQUIRE(source.start().has_value());
            // The (fake) thread has not run, as a real one blocked in a read has not returned.
        }
        SUCCEED();
    }

    SECTION("a source that was never started has nothing and can be dropped")
    {
        std::istringstream in("x\n");
        adl::load::StreamLineSource source(bench.loader_host, in);
        std::vector<adl::load::InputLine> lines;
        source.poll(bench.loader_host.clock().now(), lines);
        CHECK(lines.empty());
        CHECK_FALSE(source.ended());
    }
}

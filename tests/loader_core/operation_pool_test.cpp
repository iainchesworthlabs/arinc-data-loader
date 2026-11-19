// SPDX-License-Identifier: GPL-3.0-or-later
// Several operations at once from one control port: the pool of adl::loader_core against simulated targets on the
// fake platform, each target on an address of its own. The loader is one host with one control socket on port 59,
// which is what a real rig has; the targets are the simulated ones of tests/engine.

#include "adl/arinc615a/directory_files.hpp"
#include "adl/loader_core/download_directory.hpp"
#include "adl/loader_core/gate.hpp"
#include "adl/loader_core/media_source.hpp"
#include "adl/loader_core/operation_plan.hpp"
#include "adl/loader_core/operation_pool.hpp"
#include "adl/loader_core/run.hpp"
#include "bytes.hpp"
#include "faulty_host.hpp"
#include "rig.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace adl::loader_core;
using adl::arinc615a::OutcomeCode;
using adl::arinc615a::StatusCode;
using adl::test::Recorder;
using adl::test::SimPlan;
using adl::test::SimTarget;

namespace
{

constexpr adl::platform::Ipv4Address loader_address{{10, 0, 0, 1}};

// The target of number `k` (from 0), each on an address of its own.
adl::platform::Ipv4Address target_address(std::size_t k)
{
    return adl::platform::Ipv4Address{
        {10, 0, static_cast<std::uint8_t>(1 + k / 200), static_cast<std::uint8_t>(1 + k % 200)}};
}

adl::arinc615a::Address address_of(std::size_t k)
{
    return adl::arinc615a::Address{target_address(k).octets};
}

// The loader's host, and `count` simulated targets on one fake network.
class Bench
{
public:
    explicit Bench(std::size_t count, const std::function<SimPlan(std::size_t)>& plan_of = {})
        : world(1), network(world), loader_host(world, network, loader_address)
    {
        for (std::size_t k = 0; k < count; ++k)
        {
            targets.push_back(
                std::make_unique<SimTarget>(world, network, target_address(k), plan_of ? plan_of(k) : SimPlan{}));
        }
    }

    adl::fake::FakeWorld world;
    adl::fake::FakeNetwork network;
    adl::fake::FakeHost loader_host;
    std::vector<std::unique_ptr<SimTarget>> targets;

    // Lets the targets see the datagrams the loader sent last.
    void settle()
    {
        world.advance_by(std::chrono::seconds(1));
    }
};

// What one operation of the pool is given and what comes back.
struct Job
{
    Recorder recorder;
    std::optional<OperationResult> result;
    std::optional<MediaTreeSource> source;
    std::optional<adl::arinc615a::DirectoryFileSink> sink;
    OperationGate gate;
    std::string folder;

    [[nodiscard]] OperationPool::Completion completion()
    {
        return [this](OperationResult&& ended) { result = std::move(ended); };
    }
};

PoolTarget pool_target(std::size_t k, std::string_view thw_id_pos = "HNPFMS_L")
{
    PoolTarget target;
    target.address = address_of(k);
    target.thw_id_pos = std::string{thw_id_pos};
    return target;
}

PoolConfig pool_config(std::size_t max_operations = 8)
{
    PoolConfig config;
    config.max_operations = max_operations;
    return config;
}

// Steps the pool until `done` or until `steps` passes of 20 ms of the fake clock have gone: a bound, not a wait.
bool step_until(OperationPool& pool, const std::function<bool()>& done, int steps = 20'000)
{
    for (int i = 0; i < steps; ++i)
    {
        if (done())
        {
            return true;
        }
        pool.step(std::chrono::milliseconds(20));
    }
    return done();
}

bool all_ended(const std::vector<std::unique_ptr<Job>>& jobs)
{
    return std::ranges::all_of(jobs, [](const auto& job) { return job->result.has_value(); });
}

std::vector<std::unique_ptr<Job>> make_jobs(std::size_t count)
{
    std::vector<std::unique_ptr<Job>> jobs;
    for (std::size_t i = 0; i < count; ++i)
    {
        jobs.push_back(std::make_unique<Job>());
    }
    return jobs;
}

// Puts the files of the sample load's media set into the loader's file system, as a tree under /set, and makes
// the index an upload reads them through.
MediaSetIndex put_media_set(adl::fake::FakeHost& host, const adl::test::SampleLoad& load)
{
    const auto set = adl::arinc665::read_media_set(load.members);
    REQUIRE(set.has_value());
    auto index = MediaSetIndex::make("/set", set->files_list, load.members);
    REQUIRE(index.has_value());
    for (const MediaSetIndex::Entry& entry : index->entries())
    {
        for (const adl::arinc665::MediaMember& member : load.members)
        {
            for (const adl::arinc665::MediaFile& file : member.files)
            {
                if (member.sequence_number == entry.member && file.pathname == entry.pathname &&
                    file.name == entry.name)
                {
                    host.fake_files().put(entry.path, file.bytes);
                }
            }
        }
    }
    return std::move(*index);
}

SimPlan with_files(std::initializer_list<const char*> names)
{
    SimPlan plan;
    for (const char* name : names)
    {
        plan.files.push_back(
            adl::test::DownloadItem{name, adl::test::bytes_of(std::string{"content of "} + name), "held"});
    }
    return plan;
}

RunServices information_services(Job& job)
{
    RunServices services;
    services.listener = &job.recorder;
    return services;
}

} // namespace

TEST_CASE("an operation through the pool ends as run_operation's does", "[loader_core][pool]")
{
    const OperationPlan plan = OperationPlan::information();

    adl::test::Rig rig;
    RunServices alone;
    alone.listener = &rig.recorder;
    RunTarget target;
    target.address = adl::arinc615a::Address{adl::test::Rig::target_address.octets};
    target.thw_id_pos = "HNPFMS_L";
    const OperationResult expected = run_operation(rig.loader_host, target, plan.operation(), alone);

    Bench bench(1);
    OperationPool pool(bench.loader_host, pool_config());
    REQUIRE(pool.open());
    CHECK(pool.control_port() == 59);
    Job job;
    REQUIRE(pool.start(pool_target(0), plan.operation(), information_services(job), job.completion()));
    CHECK(pool.running() == 1);
    CHECK(pool.is_running(address_of(0)));
    REQUIRE(step_until(pool, [&] { return job.result.has_value(); }));
    bench.settle();

    CHECK(pool.running() == 0);
    CHECK_FALSE(pool.is_running(address_of(0)));
    CHECK(job.result->reason == expected.reason);
    CHECK(job.result->reason == Reason::completed);
    CHECK(job.result->outcome.code == expected.outcome.code);
    CHECK(job.result->outcome.statuses_received == expected.outcome.statuses_received);
    CHECK(job.result->status_code() == expected.status_code());
    CHECK(job.recorder.configuration_lists == 1);
    CHECK(bench.targets[0]->finished());
    CHECK(pool.unrouted_datagrams() == 0);
}

TEST_CASE("two operations run at once from one port, and each datagram goes to its own operation",
          "[loader_core][pool]")
{
    Bench bench(2);
    OperationPool pool(bench.loader_host, pool_config());
    REQUIRE(pool.open());
    const OperationPlan plan = OperationPlan::information();
    auto jobs = make_jobs(2);
    for (std::size_t k = 0; k < 2; ++k)
    {
        REQUIRE(pool.start(pool_target(k), plan.operation(), information_services(*jobs[k]), jobs[k]->completion()));
    }
    CHECK(pool.running() == 2);
    REQUIRE(step_until(pool, [&] { return all_ended(jobs); }));
    bench.settle();

    for (std::size_t k = 0; k < 2; ++k)
    {
        INFO("target " << k);
        CHECK(jobs[k]->result->reason == Reason::completed);
        CHECK(jobs[k]->recorder.configuration_lists == 1);
        CHECK(jobs[k]->recorder.status_codes.size() == 2);
        CHECK(bench.targets[k]->log().init_requests_at.size() == 1);
        CHECK(bench.targets[k]->log().loader_port_used == 59);
    }
    CHECK(pool.unrouted_datagrams() == 0);
}

TEST_CASE("eight uploads run at once from one port, in about the time of one", "[loader_core][pool]")
{
    const adl::test::SampleLoad load = adl::test::make_sample_load(5'000, 3);
    const std::vector<LoadChoice> choice{{load.item.header_name, load.item.load_part_number}};
    const auto plan = OperationPlan::upload(choice);
    REQUIRE(plan.has_value());
    const auto plan_of = [&load](std::size_t)
    {
        SimPlan sim;
        sim.loads = {load.item};
        return sim;
    };

    // How long one takes, alone.
    std::chrono::nanoseconds alone{};
    {
        Bench bench(1, plan_of);
        MediaSetIndex index = put_media_set(bench.loader_host, load);
        OperationPool pool(bench.loader_host, pool_config());
        REQUIRE(pool.open());
        Job job;
        job.source.emplace(bench.loader_host.files(), index);
        RunServices services = information_services(job);
        services.upload_source = &*job.source;
        REQUIRE(pool.start(pool_target(0), plan->operation(), services, job.completion()));
        REQUIRE(step_until(pool, [&] { return job.result.has_value(); }));
        CHECK(job.result->reason == Reason::completed);
        alone = bench.world.now().since_origin;
    }

    Bench bench(8, plan_of);
    MediaSetIndex index = put_media_set(bench.loader_host, load);
    OperationPool pool(bench.loader_host, pool_config());
    REQUIRE(pool.open());
    auto jobs = make_jobs(8);
    for (std::size_t k = 0; k < 8; ++k)
    {
        jobs[k]->source.emplace(bench.loader_host.files(), index);
        RunServices services = information_services(*jobs[k]);
        services.upload_source = &*jobs[k]->source;
        REQUIRE(pool.start(pool_target(k), plan->operation(), services, jobs[k]->completion()));
    }
    CHECK(pool.running() == 8);
    REQUIRE(step_until(pool, [&] { return all_ended(jobs); }));
    const auto together = bench.world.now().since_origin;
    bench.settle();

    for (std::size_t k = 0; k < 8; ++k)
    {
        INFO("target " << k);
        CHECK(jobs[k]->result->reason == Reason::completed);
        CHECK(jobs[k]->result->outcome.files_served == load.item.reads.size());
        CHECK(bench.targets[k]->log().files_read.size() == load.item.reads.size());
        CHECK(bench.targets[k]->log().files_corrupt.empty());
    }
    CHECK(pool.unrouted_datagrams() == 0);
    // At once, not one after the other: far less than eight times as long as one.
    CHECK(together < alone * 3);
}

TEST_CASE("an operation that fails ends beside one that completes", "[loader_core][pool]")
{
    Bench bench(3,
                [](std::size_t k)
                {
                    SimPlan sim;
                    if (k == 1)
                    {
                        sim.init_status = StatusCode::operation_denied;
                    }
                    if (k == 2)
                    {
                        sim.init_silent = true; // never answers
                    }
                    return sim;
                });
    OperationPool pool(bench.loader_host, pool_config());
    REQUIRE(pool.open());
    const OperationPlan plan = OperationPlan::information();
    auto jobs = make_jobs(3);
    for (std::size_t k = 0; k < 3; ++k)
    {
        REQUIRE(pool.start(pool_target(k), plan.operation(), information_services(*jobs[k]), jobs[k]->completion()));
    }
    REQUIRE(step_until(pool, [&] { return all_ended(jobs); }));

    CHECK(jobs[0]->result->reason == Reason::completed);
    CHECK(jobs[1]->result->reason == Reason::target_refused);
    CHECK(jobs[1]->result->outcome.code == OutcomeCode::denied);
    CHECK(jobs[2]->result->reason == Reason::no_answer);
    CHECK(jobs[0]->recorder.configuration_lists == 1);
    CHECK(pool.running() == 0);
}

TEST_CASE("downloads run together, the media defined one and the operator defined one", "[loader_core][pool]")
{
    Bench bench(2, [](std::size_t) { return with_files({"A.BIN", "B.BIN", "C.BIN"}); });
    OperationPool pool(bench.loader_host, pool_config());
    REQUIRE(pool.open());
    auto jobs = make_jobs(2);

    // 0: the loader names the files.
    const auto named = OperationPlan::download_media(std::vector<std::string>{"A.BIN", "C.BIN"});
    REQUIRE(named.has_value());
    // 1: the target lists them and the operator picks one.
    const OperationPlan listed = OperationPlan::download_operator();
    jobs[1]->recorder.decision = adl::arinc615a::SelectionDecision::deferred;
    jobs[1]->gate.post_selection({"B.BIN"});

    for (std::size_t k = 0; k < 2; ++k)
    {
        const auto folder = make_download_directory(bench.loader_host.files(), "/dl", "HNPFMS_L");
        REQUIRE(folder.has_value());
        jobs[k]->folder = *folder;
        jobs[k]->sink.emplace(bench.loader_host.files(), *folder);
        RunServices services = information_services(*jobs[k]);
        services.controller = &jobs[k]->gate;
        services.download_sink = &*jobs[k]->sink;
        services.download_folder = jobs[k]->folder;
        REQUIRE(pool.start(pool_target(k), k == 0 ? named->operation() : listed.operation(), services,
                           jobs[k]->completion()));
    }
    REQUIRE(step_until(pool, [&] { return all_ended(jobs); }));

    CHECK(jobs[0]->result->reason == Reason::completed);
    CHECK(jobs[0]->result->outcome.files_received == 2);
    CHECK(jobs[0]->result->download_folder == jobs[0]->folder);
    CHECK(bench.loader_host.fake_files().get(jobs[0]->folder + "/A.BIN").has_value());
    CHECK(bench.loader_host.fake_files().get(jobs[0]->folder + "/C.BIN").has_value());
    CHECK(jobs[1]->result->reason == Reason::completed);
    CHECK(jobs[1]->result->outcome.files_received == 1);
    CHECK(bench.loader_host.fake_files().get(jobs[1]->folder + "/B.BIN").has_value());
    CHECK_FALSE(bench.loader_host.fake_files().get(jobs[1]->folder + "/A.BIN").has_value());
    CHECK(jobs[0]->folder != jobs[1]->folder);
}

TEST_CASE("a download that received nothing leaves no folder, as with run_operation", "[loader_core][pool]")
{
    Bench bench(1, [](std::size_t) { return with_files({"A.BIN"}); });
    OperationPool pool(bench.loader_host, pool_config());
    REQUIRE(pool.open());
    const auto plan = OperationPlan::download_media(std::vector<std::string>{"NOT-THERE.BIN"});
    REQUIRE(plan.has_value());
    Job job;
    const auto folder = make_download_directory(bench.loader_host.files(), "/dl", "HNPFMS_L");
    REQUIRE(folder.has_value());
    job.folder = *folder;
    job.sink.emplace(bench.loader_host.files(), *folder);
    RunServices services = information_services(job);
    services.download_sink = &*job.sink;
    services.download_folder = job.folder;
    REQUIRE(pool.start(pool_target(0), plan->operation(), services, job.completion()));
    REQUIRE(step_until(pool, [&] { return job.result.has_value(); }));

    CHECK(job.result->reason == Reason::missing_files);
    CHECK(job.result->download_folder_removed);
    CHECK(job.result->download_folder.empty());
}

TEST_CASE("an operation can be aborted from this side while the others go on", "[loader_core][pool]")
{
    const adl::test::SampleLoad load = adl::test::make_sample_load(5'000, 3);
    const std::vector<LoadChoice> choice{{load.item.header_name, load.item.load_part_number}};
    const auto upload = OperationPlan::upload(choice);
    REQUIRE(upload.has_value());
    Bench bench(2,
                [&load](std::size_t k)
                {
                    SimPlan sim;
                    sim.loads = {load.item};
                    if (k == 0)
                    {
                        sim.work_time = std::chrono::seconds(60); // it keeps at it long after the last file
                    }
                    return sim;
                });
    MediaSetIndex index = put_media_set(bench.loader_host, load);
    OperationPool pool(bench.loader_host, pool_config());
    REQUIRE(pool.open());
    auto jobs = make_jobs(2);
    for (std::size_t k = 0; k < 2; ++k)
    {
        jobs[k]->source.emplace(bench.loader_host.files(), index);
        RunServices services = information_services(*jobs[k]);
        services.upload_source = &*jobs[k]->source;
        REQUIRE(pool.start(pool_target(k), upload->operation(), services, jobs[k]->completion()));
    }

    CHECK_FALSE(pool.abort(address_of(7))); // no operation there
    // Three seconds of the fake clock in, the operation on target 0 is well under way and not done.
    REQUIRE(step_until(pool, [&] { return bench.world.now().since_origin > std::chrono::seconds(3); }));
    REQUIRE_FALSE(jobs[0]->result.has_value());
    CHECK(pool.abort(address_of(0)));
    REQUIRE(step_until(pool, [&] { return all_ended(jobs); }));

    CHECK(jobs[0]->result->reason == Reason::aborted);
    CHECK(jobs[0]->result->outcome.code == OutcomeCode::aborted_by_operator);
    CHECK(jobs[1]->result->reason == Reason::completed);
    CHECK(pool.running() == 0);
    // The abort ended the one operation only: the other target was told nothing of it.
    CHECK(bench.targets[1]->log().aborts_seen.empty());
}

TEST_CASE("start refuses what the pool cannot take", "[loader_core][pool]")
{
    Bench bench(4);
    const OperationPlan plan = OperationPlan::information();
    auto jobs = make_jobs(4);

    SECTION("before the port is open")
    {
        OperationPool pool(bench.loader_host, pool_config());
        CHECK_FALSE(pool.is_open());
        const auto started =
            pool.start(pool_target(0), plan.operation(), information_services(*jobs[0]), jobs[0]->completion());
        REQUIRE_FALSE(started.has_value());
        CHECK(started.error() == PoolRefusal::not_open);
        CHECK(to_string(started.error()) == "not_open");
        pool.step(std::chrono::milliseconds(1)); // nothing to do, and no harm
    }

    SECTION("a second operation on an address that is in one")
    {
        OperationPool pool(bench.loader_host, pool_config());
        REQUIRE(pool.open());
        REQUIRE(pool.start(pool_target(0), plan.operation(), information_services(*jobs[0]), jobs[0]->completion()));
        const auto again =
            pool.start(pool_target(0), plan.operation(), information_services(*jobs[1]), jobs[1]->completion());
        REQUIRE_FALSE(again.has_value());
        CHECK(again.error() == PoolRefusal::address_busy);
        CHECK(pool.running() == 1);

        // The address is free once the operation has ended.
        REQUIRE(step_until(pool, [&] { return jobs[0]->result.has_value(); }));
        bench.settle();
        bench.targets[0]->reset_operation(); // a simulated target does one operation, then is made ready again
        CHECK(pool.start(pool_target(0), plan.operation(), information_services(*jobs[1]), jobs[1]->completion()));
        REQUIRE(step_until(pool, [&] { return jobs[1]->result.has_value(); }));
        CHECK(jobs[1]->result->reason == Reason::completed);
    }

    SECTION("more operations than the pool is set for")
    {
        OperationPool pool(bench.loader_host, pool_config(2));
        CHECK(pool.max_operations() == 2);
        REQUIRE(pool.open());
        REQUIRE(pool.start(pool_target(0), plan.operation(), information_services(*jobs[0]), jobs[0]->completion()));
        REQUIRE(pool.start(pool_target(1), plan.operation(), information_services(*jobs[1]), jobs[1]->completion()));
        const auto third =
            pool.start(pool_target(2), plan.operation(), information_services(*jobs[2]), jobs[2]->completion());
        REQUIRE_FALSE(third.has_value());
        CHECK(third.error() == PoolRefusal::too_many_operations);

        REQUIRE(step_until(pool, [&] { return jobs[0]->result.has_value() && jobs[1]->result.has_value(); }));
        CHECK(pool.start(pool_target(2), plan.operation(), information_services(*jobs[2]), jobs[2]->completion()));
        REQUIRE(step_until(pool, [&] { return jobs[2]->result.has_value(); }));
        CHECK(jobs[2]->result->reason == Reason::completed);
    }

    SECTION("a limit of 0 is 1")
    {
        OperationPool pool(bench.loader_host, pool_config(0));
        CHECK(pool.max_operations() == 1);
    }
}

TEST_CASE("a datagram from an address with no operation is dropped and counted", "[loader_core][pool]")
{
    Bench bench(2);
    OperationPool pool(bench.loader_host, pool_config());
    REQUIRE(pool.open());
    const OperationPlan plan = OperationPlan::information();
    Job job;
    REQUIRE(pool.start(pool_target(0), plan.operation(), information_services(job), job.completion()));

    // Something else on the network writes to port 59: a request for a status file, from an address the pool has
    // no operation for, and a datagram that is not a request at all.
    const std::string request =
        std::string("\0\x02", 2) + "HNPFMS_L.LCS" + std::string("\0", 1) + "octet" + std::string("\0", 1);
    const adl::platform::Endpoint stranger{adl::platform::Ipv4Address{{10, 0, 9, 9}}, 4000};
    const adl::platform::Endpoint loader{loader_address, 59};
    bench.network.send(stranger, loader, adl::test::bytes_of(request));
    bench.network.send(stranger, loader, adl::test::bytes_of("garbage"));
    // From the second target, which has no operation either: its request does not reach the first one's.
    bench.network.send(adl::platform::Endpoint{target_address(1), 4000}, loader, adl::test::bytes_of(request));

    REQUIRE(step_until(pool, [&] { return job.result.has_value(); }));
    CHECK(pool.unrouted_datagrams() == 3);
    // The operation that is in the pool did not notice.
    CHECK(job.result->reason == Reason::completed);
    CHECK(job.recorder.status_codes.size() == 2);

    // Nor is a datagram from the address of an operation that has ended given to anyone.
    bench.network.send(adl::platform::Endpoint{target_address(0), 4000}, loader, adl::test::bytes_of(request));
    pool.step(std::chrono::milliseconds(50));
    CHECK(pool.unrouted_datagrams() == 4);
}

TEST_CASE("an operation that the engine refuses to start ends at the next step", "[loader_core][pool]")
{
    Bench bench(1);
    OperationPool pool(bench.loader_host, pool_config());
    REQUIRE(pool.open());
    const OperationPlan plan = OperationPlan::information();
    Job job;

    SECTION("a THW_ID_POS that would be a path")
    {
        REQUIRE(pool.start(pool_target(0, "A/B"), plan.operation(), information_services(job), job.completion()));
        CHECK_FALSE(job.result.has_value()); // told from step, not from start
        CHECK(pool.running() == 0);
        CHECK_FALSE(pool.is_running(address_of(0)));
        pool.step(std::chrono::milliseconds(1));
        REQUIRE(job.result.has_value());
        CHECK(job.result->reason == Reason::local_error);
        REQUIRE(job.result->not_run.has_value());
        CHECK(job.result->not_run->stage == adl::arinc615a::RunError::Stage::start);
    }

    SECTION("a target port of 0")
    {
        PoolTarget target = pool_target(0);
        target.port = 0;
        REQUIRE(pool.start(target, plan.operation(), information_services(job), job.completion()));
        pool.step(std::chrono::milliseconds(1));
        REQUIRE(job.result.has_value());
        CHECK(job.result->reason == Reason::local_error);
        REQUIRE(job.result->not_run.has_value());
        CHECK(job.result->not_run->stage == adl::arinc615a::RunError::Stage::start);
    }

    // The address is free for the next one.
    Job next;
    CHECK(pool.start(pool_target(0), plan.operation(), information_services(next), next.completion()));
    REQUIRE(step_until(pool, [&] { return next.result.has_value(); }));
    CHECK(next.result->reason == Reason::completed);
}

TEST_CASE("sockets beyond the wait window are looked at without waiting, and no datagram is missed",
          "[loader_core][pool]")
{
    const adl::test::SampleLoad load = adl::test::make_sample_load(20'000, 3);
    const std::vector<LoadChoice> choice{{load.item.header_name, load.item.load_part_number}};
    const auto plan = OperationPlan::upload(choice);
    REQUIRE(plan.has_value());
    Bench bench(6,
                [&load](std::size_t)
                {
                    SimPlan sim;
                    sim.loads = {load.item};
                    return sim;
                });
    MediaSetIndex index = put_media_set(bench.loader_host, load);
    PoolConfig config = pool_config(6);
    config.wait_window = 2; // the control socket and one other
    OperationPool pool(bench.loader_host, config);
    REQUIRE(pool.open());
    auto jobs = make_jobs(6);
    for (std::size_t k = 0; k < 6; ++k)
    {
        jobs[k]->source.emplace(bench.loader_host.files(), index);
        RunServices services = information_services(*jobs[k]);
        services.upload_source = &*jobs[k]->source;
        REQUIRE(pool.start(pool_target(k), plan->operation(), services, jobs[k]->completion()));
    }
    REQUIRE(step_until(pool, [&] { return all_ended(jobs); }));
    bench.settle();
    for (std::size_t k = 0; k < 6; ++k)
    {
        INFO("target " << k);
        CHECK(jobs[k]->result->reason == Reason::completed);
        CHECK(jobs[k]->result->outcome.files_served == load.item.reads.size());
        CHECK(bench.targets[k]->log().files_corrupt.empty());
    }
}

TEST_CASE("a completion may start the next operation on the address that has just been freed", "[loader_core][pool]")
{
    Bench bench(1);
    OperationPool pool(bench.loader_host, pool_config());
    REQUIRE(pool.open());
    const OperationPlan plan = OperationPlan::information();
    Job first;
    Job second;
    bool refused = true;
    REQUIRE(pool.start(pool_target(0), plan.operation(), information_services(first),
                       [&](OperationResult&& ended)
                       {
                           first.result = std::move(ended);
                           bench.targets[0]->reset_operation();
                           refused = !pool.start(pool_target(0), plan.operation(), information_services(second),
                                                 second.completion());
                       }));
    REQUIRE(step_until(pool, [&] { return second.result.has_value(); }));
    CHECK_FALSE(refused);
    CHECK(first.result->reason == Reason::completed);
    CHECK(second.result->reason == Reason::completed);
    CHECK(bench.targets[0]->log().init_requests_at.size() ==
          1); // the log is the second operation's: the target was reset between them
}

TEST_CASE("the port the pool listens on", "[loader_core][pool]")
{
    SECTION("a port that something holds")
    {
        Bench bench(1);
        OperationPool first(bench.loader_host, pool_config());
        REQUIRE(first.open());
        OperationPool second(bench.loader_host, pool_config());
        const auto opened = second.open();
        REQUIRE_FALSE(opened.has_value());
        CHECK(opened.error().code == adl::platform::ErrorCode::address_in_use);
        CHECK_FALSE(second.is_open());
    }

    SECTION("a port the system chooses, which the target is told of")
    {
        Bench bench(1);
        PoolConfig config = pool_config();
        config.control_port = 0;
        OperationPool pool(bench.loader_host, config);
        REQUIRE(pool.open());
        CHECK(pool.control_port() != 0);
        CHECK(pool.control_port() != 59);
        const OperationPlan plan = OperationPlan::information();
        Job job;
        REQUIRE(pool.start(pool_target(0), plan.operation(), information_services(job), job.completion()));
        REQUIRE(step_until(pool, [&] { return job.result.has_value(); }));
        CHECK(job.result->reason == Reason::completed);
        CHECK(bench.targets[0]->log().loader_port_used == pool.control_port());
    }

    SECTION("an interface that the host does not have")
    {
        Bench bench(1);
        PoolConfig config = pool_config();
        config.local_address = adl::platform::Ipv4Address{{192, 168, 77, 7}};
        OperationPool pool(bench.loader_host, config);
        const auto opened = pool.open();
        REQUIRE_FALSE(opened.has_value());
        CHECK(opened.error().code == adl::platform::ErrorCode::address_unavailable);
    }
}

TEST_CASE("a wait that fails ends every operation as one that could not run", "[loader_core][pool]")
{
    Bench bench(2);
    adl::test::FaultyHost host(bench.loader_host);
    OperationPool pool(host, pool_config());
    REQUIRE(pool.open());
    const OperationPlan plan = OperationPlan::information();
    auto jobs = make_jobs(2);
    for (std::size_t k = 0; k < 2; ++k)
    {
        REQUIRE(pool.start(pool_target(k), plan.operation(), information_services(*jobs[k]), jobs[k]->completion()));
    }
    host.faulty_network().wait_fails = adl::platform::ErrorCode::io_error;
    pool.step(std::chrono::milliseconds(20));
    for (std::size_t k = 0; k < 2; ++k)
    {
        REQUIRE(jobs[k]->result.has_value());
        CHECK(jobs[k]->result->reason == Reason::local_error);
        REQUIRE(jobs[k]->result->not_run.has_value());
        CHECK(jobs[k]->result->not_run->stage == adl::arinc615a::RunError::Stage::wait);
        CHECK(jobs[k]->result->not_run->platform == adl::platform::ErrorCode::io_error);
    }
    CHECK(pool.running() == 0);

    SECTION("an interrupted wait is not a failure")
    {
        host.faulty_network().wait_fails.reset();
        Job job;
        REQUIRE(pool.start(pool_target(0), plan.operation(), information_services(job), job.completion()));
        host.faulty_network().wait_fails = adl::platform::ErrorCode::interrupted;
        pool.step(std::chrono::milliseconds(20));
        CHECK_FALSE(job.result.has_value());
        CHECK(pool.running() == 1);
    }
}

TEST_CASE("a port that cannot be opened is the platform's error", "[loader_core][pool]")
{
    Bench bench(1);
    adl::test::FaultyHost host(bench.loader_host);
    host.faulty_network().open_fails = adl::platform::ErrorCode::permission_denied;
    OperationPool pool(host, pool_config());
    const auto opened = pool.open();
    REQUIRE_FALSE(opened.has_value());
    CHECK(opened.error().code == adl::platform::ErrorCode::permission_denied);
    const OperationPlan plan = OperationPlan::information();
    Job job;
    const auto started = pool.start(pool_target(0), plan.operation(), information_services(job), job.completion());
    REQUIRE_FALSE(started.has_value());
    CHECK(started.error() == PoolRefusal::not_open);
}

TEST_CASE("operations that are still running when the pool goes are abandoned without being called",
          "[loader_core][pool]")
{
    Bench bench(2);
    const OperationPlan plan = OperationPlan::information();
    auto jobs = make_jobs(2);
    {
        OperationPool pool(bench.loader_host, pool_config());
        REQUIRE(pool.open());
        for (std::size_t k = 0; k < 2; ++k)
        {
            REQUIRE(
                pool.start(pool_target(k), plan.operation(), information_services(*jobs[k]), jobs[k]->completion()));
        }
        pool.step(std::chrono::milliseconds(20));
    }
    CHECK_FALSE(jobs[0]->result.has_value());
    CHECK_FALSE(jobs[1]->result.has_value());
    bench.settle(); // the targets find nobody to answer to, and that is all
}

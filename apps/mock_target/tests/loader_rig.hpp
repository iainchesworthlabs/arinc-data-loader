#pragma once

// What the mock target's tests need on the loader's side: the loader's real engine (DataLoader
// and Runner) over a fake host at 10.0.0.1, run against the mock target at 10.0.0.2 on the same
// fake network, a recorder that writes down what the loader reported, a sink for downloaded
// files in memory, and the sample load put into the fake file system for a DirectoryFileSource.

#include "adl/arinc615a/data_loader.hpp"
#include "adl/arinc615a/runner.hpp"
#include "adl/arinc615a_mini/mini_tftp.hpp"
#include "adl/arinc665/load.hpp"
#include "adl/samples/scenarios.hpp"
#include "fake_host.hpp"
#include "fake_network.hpp"
#include "fake_world.hpp"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <expected>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace adl::test
{

using namespace adl::arinc615a;
using adl::arinc615a_mini::Bytes;

// A minimal OperationListener: writes down what the loader reported.
struct LoaderRecorder final : public OperationListener
{
    std::vector<StatusCode> initialization;
    std::vector<std::string> initialization_text;
    std::vector<std::uint16_t> status_counters;
    std::vector<StatusCode> status_codes;
    std::vector<std::string> status_texts;
    std::size_t configuration_lists = 0;
    std::vector<std::string> hardware_names;
    std::vector<std::string> part_numbers;
    std::vector<std::string> received_names;
    std::vector<std::string> sent_names;
    std::vector<std::string> listed_names;
    std::vector<std::pair<Notice, std::string>> notices;
    // Which of the listed files an operator defined download chooses (empty: everything).
    std::vector<std::string> choose;
    // Set when the first upload status arrives: the signal for a controller to abort.
    std::atomic<bool>* abort_on_first_status = nullptr;

    void on_initialization(const InitializationFile& file) noexcept override
    {
        initialization.push_back(file.status);
        initialization_text.emplace_back(file.description);
    }
    void on_configuration_status(const ConfigurationStatus& status) noexcept override
    {
        record(status.counter, status.status, status.description);
    }
    void on_upload_status(const UploadStatus& status) noexcept override
    {
        if (abort_on_first_status != nullptr)
        {
            abort_on_first_status->store(true, std::memory_order_release);
        }
        record(status.counter, status.status, status.description);
    }
    void on_download_status(const DownloadStatus& status) noexcept override
    {
        record(status.counter, status.status, status.description);
    }
    void on_configuration_list(const ConfigurationList& list) noexcept override
    {
        ++configuration_lists;
        for (const HardwareItem& item : list.hardware)
        {
            hardware_names.emplace_back(item.literal_name);
            for (const HardwarePart& part : item.parts)
            {
                part_numbers.emplace_back(part.part_number);
            }
        }
    }
    SelectionDecision on_download_list(const DownloadList& list, FileSelection& selection) noexcept override
    {
        for (const FileListing& file : list.files)
        {
            listed_names.emplace_back(file.file_name);
        }
        for (const FileListing& file : list.files)
        {
            const std::string name{file.file_name};
            if (choose.empty() || std::ranges::find(choose, name) != choose.end())
            {
                selection.select(file.file_name);
            }
        }
        return SelectionDecision::selected;
    }
    void on_file_received(std::string_view name, const ReceivedFile&) noexcept override
    {
        received_names.emplace_back(name);
    }
    void on_file_sent(std::string_view name, std::uint64_t) noexcept override
    {
        sent_names.emplace_back(name);
    }
    void on_notice(Notice notice, std::string_view subject, std::uint32_t) noexcept override
    {
        notices.emplace_back(notice, std::string{subject});
    }

    [[nodiscard]] bool saw(Notice notice) const noexcept
    {
        return std::ranges::any_of(notices, [notice](const auto& entry) { return entry.first == notice; });
    }

private:
    void record(std::uint16_t counter, StatusCode code, std::string_view text)
    {
        status_counters.push_back(counter);
        status_codes.push_back(code);
        status_texts.emplace_back(text);
    }
};

// Where downloaded files land: kept in memory, by name, once complete.
struct MemorySink final : public FileSink
{
    std::map<std::string, Bytes> files;

    IoResult<SinkHandle> open(std::string_view name, std::optional<std::uint64_t>) noexcept override
    {
        const SinkHandle handle = next_++;
        open_[handle] = std::string{name};
        return handle;
    }
    IoResult<void> write(SinkHandle file, std::span<const std::byte> data) noexcept override
    {
        bytes_[file].insert(bytes_[file].end(), data.begin(), data.end());
        return {};
    }
    IoResult<void> finish(SinkHandle file, const ReceivedFile&) noexcept override
    {
        files[open_.at(file)] = std::move(bytes_[file]);
        open_.erase(file);
        bytes_.erase(file);
        return {};
    }
    void discard(SinkHandle file) noexcept override
    {
        open_.erase(file);
        bytes_.erase(file);
    }

private:
    std::map<SinkHandle, std::string> open_;
    std::map<SinkHandle, Bytes> bytes_;
    SinkHandle next_ = 1;
};

// The operator's abort, as the runner's controller: aborts once `wanted` has been set (by the
// recorder, when the first status file arrived).
class AbortWhenWanted final : public RunController
{
public:
    std::atomic<bool> wanted{false};

    void on_poll(DataLoader& loader, Instant now) noexcept override
    {
        if (wanted.load(std::memory_order_acquire) && !issued_)
        {
            issued_ = true;
            loader.abort(now, StatusCode::aborted_by_operator);
        }
    }

private:
    bool issued_ = false;
};

// One run of the loader's engine, scheduled as a world event: the mock target's reactions
// (world events too) interleave with the loader's waits, all on the thread that drives it.
struct PendingRun
{
    std::expected<Outcome, RunError> outcome{std::unexpected(RunError{})};
    std::atomic<bool> finished{false};
    std::vector<std::byte> workspace;
    std::unique_ptr<DataLoader> loader;
};

// The loader at 10.0.0.1 and the mock target at 10.0.0.2 on one fake network.
struct MockRig
{
    static constexpr adl::platform::Ipv4Address loader_address{{10, 0, 0, 1}};
    static constexpr adl::platform::Ipv4Address target_address{{10, 0, 0, 2}};

    explicit MockRig(std::uint64_t seed = 1) : world(seed), network(world), loader_host(world, network, loader_address)
    {
    }

    [[nodiscard]] LoaderConfig loader_config() const
    {
        LoaderConfig config;
        config.target = Endpoint{Address{target_address.octets}, tftp_control_port};
        config.thw_id_pos = thw;
        return config;
    }

    // Schedules one operation of the loader's engine as a world event (whoever advances the
    // world next runs it: the engine test directly, the model test through the model's worker
    // thread). `operation` is copied, so the caller's bytes must outlive the run, which is
    // what the caller's scope gives.
    [[nodiscard]] std::shared_ptr<PendingRun> schedule_run(const Operation& operation, LoaderServices services,
                                                           const RunOptions& options = {})
    {
        auto pending = std::make_shared<PendingRun>();
        const LoaderConfig config = loader_config();
        pending->workspace.assign(DataLoader::workspace_bytes(config), std::byte{0});
        pending->loader = std::make_unique<DataLoader>(config, pending->workspace, services);
        world.schedule_after(adl::platform::Duration::zero(),
                             [this, pending, operation, options]
                             {
                                 Runner runner(loader_host);
                                 pending->outcome = runner.run(*pending->loader, operation, options);
                                 pending->finished.store(true, std::memory_order_release);
                             });
        return pending;
    }

    // Runs one operation to its end and returns how it ended. The world drives the loader
    // and the target together, on this thread.
    [[nodiscard]] std::expected<Outcome, RunError> run(const Operation& operation, LoaderServices services,
                                                       const RunOptions& options = {})
    {
        const auto pending = schedule_run(operation, services, options);
        const bool finished = world.wait_until([&] { return pending->finished.load(std::memory_order_acquire); },
                                               std::chrono::seconds(120));
        if (!finished)
        {
            return std::unexpected(RunError{});
        }
        // The loader's last datagram was sent as it stopped: let the target receive it.
        world.advance_by(std::chrono::seconds(1));
        return pending->outcome;
    }

    adl::fake::FakeWorld world;
    adl::fake::FakeNetwork network;
    adl::fake::FakeHost loader_host;
    std::string thw = "HNPFMS_L";
};

// ---- The sample load -------------------------------------------------------------------------

[[nodiscard]] inline Bytes pattern(std::size_t size, std::uint64_t seed)
{
    Bytes bytes(size);
    std::uint64_t state = seed + 0x9e3779b97f4a7c15ULL;
    for (std::byte& byte : bytes)
    {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        byte = std::byte{static_cast<std::uint8_t>(state >> 56)};
    }
    return bytes;
}

// The sample load, with the first data file grown to `data_bytes` (0: as the sample has it).
[[nodiscard]] inline adl::arinc665::Load make_load(std::size_t data_bytes = 0, std::uint64_t seed = 5)
{
    adl::arinc665::LoadSpec spec = adl::samples::sample_load_spec();
    if (data_bytes != 0)
    {
        spec.data_files[0].content = pattern(data_bytes, seed);
    }
    return std::move(*adl::arinc665::build_load(spec));
}

// Puts every file of the load into the host's file system, flat, for a DirectoryFileSource.
inline void put_load_files(adl::fake::FakeHost& host, const adl::arinc665::Load& load,
                           const std::string& directory = "/load")
{
    auto& files = host.fake_files();
    files.put(directory + "/" + load.header_file_name, load.header_bytes);
    for (const auto& file : load.data_files)
    {
        files.put(directory + "/" + file.name, file.bytes);
    }
    for (const auto& file : load.support_files)
    {
        files.put(directory + "/" + file.name, file.bytes);
    }
}

// The upload operation of the whole sample load.
[[nodiscard]] inline std::vector<LoadReference> upload_references(const adl::arinc665::Load& load)
{
    return {LoadReference{load.header_file_name, adl::samples::sample_load_part_number}};
}

} // namespace adl::test

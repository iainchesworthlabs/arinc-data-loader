// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// What an end-to-end test of an operation needs: a loader host and a simulated target on one
// fake network, a media set built with adl::arinc665 to upload from, a place for downloaded
// files, and a listener that writes down what the engine reported.

#include "adl/arinc615a/data_loader.hpp"
#include "adl/arinc615a/runner.hpp"
#include "adl/arinc665/check_value.hpp"
#include "adl/arinc665/load.hpp"
#include "adl/arinc665/media_set.hpp"
#include "adl/samples/scenarios.hpp"
#include "fake_host.hpp"
#include "sim_target.hpp"
#include "transfer_pair.hpp"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace adl::test
{

using namespace adl::arinc615a;

[[nodiscard]] inline std::uint16_t crc16_value(const Bytes& bytes)
{
    const auto value = adl::arinc665::compute_check_value(adl::arinc665::CheckValueType::crc16, bytes);
    return static_cast<std::uint16_t>((std::to_integer<unsigned>(value->bytes[0]) << 8) |
                                      std::to_integer<unsigned>(value->bytes[1]));
}

// Every file of a media set, with the member and directory it is in.
struct MediaFiles
{
    struct Entry
    {
        std::string name;
        std::string pathname;
        std::uint16_t member = 1;
        Bytes bytes;
        std::uint16_t crc16 = 0;
    };
    std::vector<Entry> entries;

    static MediaFiles of(const std::vector<adl::arinc665::MediaMember>& members)
    {
        MediaFiles files;
        for (const auto& member : members)
        {
            for (const auto& file : member.files)
            {
                files.entries.push_back(
                    Entry{file.name, file.pathname, member.sequence_number, file.bytes, crc16_value(file.bytes)});
            }
        }
        return files;
    }
};

// A FileSource over a media set in memory. Files of one name are told apart by the 16-bit CRC
// in the request (5.3.2.3.8.5), as a header file's entries would.
class MediaSource final : public FileSource
{
public:
    explicit MediaSource(MediaFiles files) : files_(std::move(files)) {}

    // Names that are "not there yet": the first open of each says wait this long.
    std::map<std::string, std::uint16_t> not_yet;
    std::set<std::string> denied;
    // What was asked for, in order, and the checksum the request carried.
    struct Request
    {
        std::string name;
        std::optional<std::uint16_t> crc16;
    };
    std::vector<Request> requests;
    std::size_t open_files() const
    {
        return open_.size();
    }

    IoResult<SourceFile> open(std::string_view name, const std::optional<ChecksumOption>& checksum) noexcept override
    {
        std::optional<std::uint16_t> crc;
        if (checksum && checksum->type == adl::arinc665::CheckValueType::crc16)
        {
            crc = static_cast<std::uint16_t>((std::to_integer<unsigned>(checksum->value[0]) << 8) |
                                             std::to_integer<unsigned>(checksum->value[1]));
        }
        requests.push_back(Request{std::string{name}, crc});
        if (denied.contains(std::string{name}))
        {
            return std::unexpected(IoFailure{IoError::access_denied, 0});
        }
        if (const auto wait = not_yet.find(std::string{name}); wait != not_yet.end())
        {
            const std::uint16_t seconds = wait->second;
            not_yet.erase(wait);
            return std::unexpected(IoFailure{IoError::not_available, seconds});
        }
        const MediaFiles::Entry* found = nullptr;
        std::size_t candidates = 0;
        for (const auto& entry : files_.entries)
        {
            if (entry.name == name)
            {
                ++candidates;
                if (found == nullptr || (crc && entry.crc16 == *crc))
                {
                    found = &entry;
                }
            }
        }
        if (found == nullptr)
        {
            return std::unexpected(IoFailure{IoError::not_found, 0});
        }
        (void)candidates;
        const SourceHandle handle = next_++;
        open_[handle] = found;
        return SourceFile{handle, found->bytes.size()};
    }

    IoResult<std::size_t> read(SourceHandle file, std::uint64_t offset, std::span<std::byte> out) noexcept override
    {
        const auto it = open_.find(file);
        if (it == open_.end())
        {
            return std::unexpected(IoFailure{IoError::io_error, 0});
        }
        const Bytes& bytes = it->second->bytes;
        if (offset >= bytes.size())
        {
            return std::size_t{0};
        }
        const std::size_t count = std::min<std::size_t>(out.size(), bytes.size() - static_cast<std::size_t>(offset));
        std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(offset), count, out.begin());
        return count;
    }

    void close(SourceHandle file) noexcept override
    {
        open_.erase(file);
    }

private:
    MediaFiles files_;
    std::map<SourceHandle, const MediaFiles::Entry*> open_;
    SourceHandle next_ = 1;
};

// Where downloaded files land: kept in memory, by name, once they are complete.
class MemoryFileSink final : public FileSink
{
public:
    std::map<std::string, Bytes> files;
    std::vector<std::string> opened;
    std::vector<std::string> discarded;
    std::vector<ReceivedFile> received;
    bool fail_finish = false;
    bool refuse_open = false;
    std::optional<std::uint64_t> last_expected_size;

    IoResult<SinkHandle> open(std::string_view name, std::optional<std::uint64_t> expected_size) noexcept override
    {
        if (refuse_open)
        {
            return std::unexpected(IoFailure{IoError::no_space, 0});
        }
        last_expected_size = expected_size;
        const SinkHandle handle = next_++;
        open_[handle] = Open{std::string{name}, {}};
        opened.emplace_back(name);
        return handle;
    }
    IoResult<void> write(SinkHandle file, std::span<const std::byte> data) noexcept override
    {
        open_.at(file).bytes.insert(open_.at(file).bytes.end(), data.begin(), data.end());
        return {};
    }
    IoResult<void> finish(SinkHandle file, const ReceivedFile& info) noexcept override
    {
        Open entry = std::move(open_.at(file));
        open_.erase(file);
        if (fail_finish)
        {
            return std::unexpected(IoFailure{IoError::io_error, 0});
        }
        files[entry.name] = std::move(entry.bytes);
        received.push_back(info);
        return {};
    }
    void discard(SinkHandle file) noexcept override
    {
        discarded.push_back(open_.at(file).name);
        open_.erase(file);
    }
    std::size_t open_files() const
    {
        return open_.size();
    }

private:
    struct Open
    {
        std::string name;
        Bytes bytes;
    };
    std::map<SinkHandle, Open> open_;
    SinkHandle next_ = 1;
};

// Writes down everything the engine reports, and chooses files when asked.
class Recorder final : public OperationListener
{
public:
    struct NoticeRecord
    {
        Notice notice;
        std::string subject;
        std::uint32_t value;
    };

    std::vector<StatusCode> initialization;
    std::vector<std::string> initialization_text;
    std::vector<std::uint16_t> status_counters;
    std::vector<StatusCode> status_codes;
    std::vector<std::uint16_t> exception_timers;
    std::vector<std::uint8_t> list_ratios;
    std::size_t last_status_loads = 0;
    std::vector<std::string> status_texts;
    std::size_t configuration_lists = 0;
    std::vector<std::string> hardware_names;
    std::vector<std::string> part_numbers;
    std::vector<NoticeRecord> notices;
    std::vector<std::string> received_names;
    std::vector<ChecksumVerdict> received_checksums;
    std::vector<std::string> sent_names;
    std::vector<std::string> listed_names;

    // What to do when the target's list of files arrives.
    SelectionDecision decision = SelectionDecision::selected;
    std::vector<std::string> choose; // empty: everything listed

    void on_initialization(const InitializationFile& file) noexcept override
    {
        initialization.push_back(file.status);
        initialization_text.emplace_back(file.description);
    }
    void on_configuration_status(const ConfigurationStatus& status) noexcept override
    {
        record(status.counter, status.status, status.exception_timer, 0, status.description);
    }
    void on_upload_status(const UploadStatus& status) noexcept override
    {
        last_status_loads = status.loads.size();
        record(status.counter, status.status, status.exception_timer, status.list_ratio, status.description);
    }
    void on_download_status(const DownloadStatus& status) noexcept override
    {
        record(status.counter, status.status, status.exception_timer, status.list_ratio, status.description);
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
        deferred_names = listed_names;
        if (decision != SelectionDecision::selected)
        {
            return decision;
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
    void on_file_received(std::string_view name, const ReceivedFile& file) noexcept override
    {
        received_names.emplace_back(name);
        received_checksums.push_back(file.checksum);
    }
    void on_file_sent(std::string_view name, std::uint64_t) noexcept override
    {
        sent_names.emplace_back(name);
    }
    void on_notice(Notice notice, std::string_view subject, std::uint32_t value) noexcept override
    {
        notices.push_back(NoticeRecord{notice, std::string{subject}, value});
    }

    [[nodiscard]] std::size_t count(Notice notice) const
    {
        return static_cast<std::size_t>(
            std::ranges::count_if(notices, [notice](const NoticeRecord& record) { return record.notice == notice; }));
    }

    std::vector<std::string> deferred_names;

private:
    void record(std::uint16_t counter, StatusCode code, std::uint16_t exception, std::uint8_t ratio,
                std::string_view text)
    {
        status_counters.push_back(counter);
        status_codes.push_back(code);
        exception_timers.push_back(exception);
        list_ratios.push_back(ratio);
        status_texts.emplace_back(text);
    }
};

// The loader at 10.0.0.1 and a target at 10.0.0.2.
class Rig
{
public:
    static constexpr adl::platform::Ipv4Address loader_address{{10, 0, 0, 1}};
    static constexpr adl::platform::Ipv4Address target_address{{10, 0, 0, 2}};

    explicit Rig(SimPlan plan = {}, std::uint64_t seed = 1)
        : world(seed), network(world), loader_host(world, network, loader_address),
          target(world, network, target_address, std::move(plan))
    {
        config.target = Endpoint{Address{target_address.octets}, tftp_control_port};
        config.thw_id_pos = "HNPFMS_L";
    }

    [[nodiscard]] Instant now() const
    {
        return Instant{world.now().since_origin};
    }

    // Runs an operation to its end and returns how it ended.
    std::expected<Outcome, RunError> run(const Operation& operation, const RunOptions& options = {})
    {
        workspace.assign(DataLoader::workspace_bytes(config), std::byte{0});
        LoaderServices services;
        services.source = source_override != nullptr ? source_override : source.get();
        services.sink = sink_override != nullptr ? sink_override : &sink;
        services.listener = &recorder;
        loader = std::make_unique<DataLoader>(config, workspace, services);
        Runner runner(loader_host);
        const Instant started = now();
        auto outcome = runner.run(*loader, operation, options);
        ended = now();
        elapsed = ended - started;
        // The loader's last datagram was sent as it stopped: let the target receive it.
        world.advance_by(std::chrono::seconds(1));
        return outcome;
    }

    adl::fake::FakeWorld world;
    adl::fake::FakeNetwork network;
    adl::fake::FakeHost loader_host;
    SimTarget target;

    LoaderConfig config;
    std::vector<std::byte> workspace;
    std::unique_ptr<MediaSource> source;
    FileSource* source_override = nullptr;
    FileSink* sink_override = nullptr;
    MemoryFileSink sink;
    Recorder recorder;
    std::unique_ptr<DataLoader> loader;
    Duration elapsed{};
    Instant ended{}; // when the runner returned, before the target was given time to catch up
};

// The sample media set, and the plan that has a target read all of one of its loads.
struct SampleLoad
{
    std::vector<adl::arinc665::MediaMember> members;
    MediaFiles files;
    UploadItem item;
    adl::arinc665::Load load;
};

[[nodiscard]] inline SampleLoad make_sample_load(std::size_t data_bytes = 0, std::uint64_t seed = 5)
{
    SampleLoad result;
    auto spec_value = adl::samples::sample_media_set_spec();
    adl::arinc665::MediaSetSpec spec = std::move(*spec_value);
    if (data_bytes != 0)
    {
        auto load_spec = adl::samples::sample_load_spec();
        load_spec.data_files[0].content = pattern(data_bytes, seed);
        auto load = adl::arinc665::build_load(load_spec);
        spec.loads[0].load = std::move(*load);
    }
    result.load = spec.loads[0].load;
    result.members = std::move(*adl::arinc665::build_media_set(spec));
    result.files = MediaFiles::of(result.members);
    result.item.header_name = result.load.header_file_name;
    result.item.load_part_number = adl::samples::sample_load_part_number;
    const auto add = [&](const std::string& name, const Bytes& bytes)
    { result.item.reads.push_back(ReadItem{name, bytes, crc16_value(bytes)}); };
    add(result.load.header_file_name, result.load.header_bytes);
    for (const auto& file : result.load.data_files)
    {
        add(file.name, file.bytes);
    }
    for (const auto& file : result.load.support_files)
    {
        add(file.name, file.bytes);
    }
    return result;
}

[[nodiscard]] inline UploadOperation upload_of(const SampleLoad& load, std::vector<LoadReference>& storage)
{
    storage = {LoadReference{load.item.header_name, load.item.load_part_number}};
    return UploadOperation{UploadRequest{storage}};
}

} // namespace adl::test

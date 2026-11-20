// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/set_maker/make.hpp"

#include "adl/arinc665/batch_file.hpp"
#include "adl/arinc665/load.hpp"
#include "adl/arinc665/media_set.hpp"
#include "adl/arinc665/part_number.hpp"
#include "adl/media_tree/media_tree.hpp"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <new>
#include <stdexcept>
#include <utility>
#include <vector>

namespace adl::set_maker
{

namespace
{

using adl::arinc665::BatchFile;
using adl::arinc665::BatchLoad;
using adl::arinc665::BatchTarget;
using adl::arinc665::FilePlacement;
using adl::arinc665::LoadSpec;
using adl::arinc665::MediaSetSpec;
using adl::arinc665::PartNumber;
using adl::arinc665::PlacedBatch;
using adl::arinc665::PlacedLoad;

constexpr int most_names_tried = 100;

// The directory a media set is written to before it is given its place. Whatever is in it when the job ends
// without keep() is the job's own debris, and is removed.
class PartialTree
{
public:
    PartialTree() = default;
    PartialTree(const PartialTree&) = delete;
    PartialTree& operator=(const PartialTree&) = delete;
    ~PartialTree()
    {
        if (claimed())
        {
            static_cast<void>(discard());
        }
    }

    void claim(std::filesystem::path path)
    {
        path_ = std::move(path);
    }
    [[nodiscard]] bool claimed() const noexcept
    {
        return !path_.empty();
    }
    [[nodiscard]] const std::filesystem::path& path() const noexcept
    {
        return path_;
    }
    // The set is whole and has its place: there is nothing to remove.
    void keep() noexcept
    {
        path_.clear();
    }

    // Removes the directory and what is in it, and says whether anything is left.
    [[nodiscard]] Discard discard()
    {
        Discard result;
        result.attempted = true;
        result.where = path_;
        std::error_code error;
        std::filesystem::remove_all(path_, error);
        result.removed = !std::filesystem::exists(path_, error);
        path_.clear();
        return result;
    }

private:
    std::filesystem::path path_;
};

std::unexpected<MakeFailure> failure(FailureKind kind, std::filesystem::path path = {}, std::string detail = {},
                                     std::string system = {}, Discard discard = {}, std::filesystem::path at = {})
{
    MakeFailure result;
    result.kind = kind;
    result.path = std::move(path);
    result.at = std::move(at);
    result.detail = std::move(detail);
    result.system = std::move(system);
    result.discard = std::move(discard);
    return std::unexpected{std::move(result)};
}

std::string text_of(const adl::arinc665::Error error)
{
    return std::string{adl::arinc665::to_string(error)};
}

// The final folder of an exact destination, with no trailing separator.
std::filesystem::path exact_folder(const Destination& destination)
{
    std::filesystem::path folder = destination.path;
    if (!folder.has_filename())
    {
        folder = folder.parent_path();
    }
    return folder;
}

// Whether `folder` can be the set's place: it is not there, or it is there and empty. False with the system's
// words when it cannot be looked at.
enum class Place
{
    free,
    taken,
    unknown,
};

Place place_state(const std::filesystem::path& folder, std::string& system)
{
    std::error_code error;
    const bool exists = std::filesystem::exists(folder, error);
    if (error)
    {
        system = error.message();
        return Place::unknown;
    }
    if (!exists)
    {
        return Place::free;
    }
    if (!std::filesystem::is_directory(folder, error) || error)
    {
        return Place::taken;
    }
    const bool empty = std::filesystem::is_empty(folder, error);
    if (error)
    {
        system = error.message();
        return Place::unknown;
    }
    return empty ? Place::free : Place::taken;
}

struct SourceFile
{
    const PlannedFile* planned = nullptr;
    std::vector<std::byte> bytes;
};

// Step 1: every file, read whole, with the progress of the reading.
std::expected<std::vector<SourceFile>, MakeFailure> read_sources(const SetPlan& plan, Hooks& hooks,
                                                                 std::uint64_t& total_bytes)
{
    std::vector<SourceFile> sources;
    std::vector<std::uint64_t> sizes;
    for (const PlannedLoad& load : plan.loads)
    {
        for (const PlannedFile& file : load.files)
        {
            std::error_code error;
            const auto size = std::filesystem::file_size(file.source, error);
            if (error)
            {
                return failure(FailureKind::source_unreadable, file.source, {}, error.message());
            }
            sources.push_back(SourceFile{&file, {}});
            sizes.push_back(size);
            total_bytes += size;
        }
    }

    std::uint64_t bytes_read = 0;
    for (std::size_t i = 0; i < sources.size(); ++i)
    {
        if (hooks.checkpoint("read"))
        {
            return failure(FailureKind::cancelled);
        }
        const PlannedFile& file = *sources[i].planned;
        std::ifstream stream(file.source, std::ios::binary);
        if (!stream)
        {
            return failure(FailureKind::source_unreadable, file.source);
        }
        try
        {
            static_cast<void>(hooks.checkpoint("allocate"));
            sources[i].bytes.resize(static_cast<std::size_t>(sizes[i]));
        }
        catch (const std::length_error&)
        {
            return failure(FailureKind::source_too_large, file.source);
        }
        catch (const std::bad_alloc&)
        {
            return failure(FailureKind::source_does_not_fit, file.source);
        }
        stream.read(reinterpret_cast<char*>(sources[i].bytes.data()), static_cast<std::streamsize>(sizes[i]));
        if (!stream)
        {
            return failure(FailureKind::source_unreadable, file.source);
        }
        bytes_read += sizes[i];
        hooks.progress(total_bytes == 0 ? 100 : static_cast<int>(bytes_read * 100 / total_bytes));
    }
    return sources;
}

struct BuiltSet
{
    std::vector<adl::arinc665::MediaMember> members;
    std::size_t data_files = 0;
    std::size_t support_files = 0;
};

// Steps 2 and 3: the loads, and the set with its batch.
std::expected<BuiltSet, MakeFailure> build(const SetPlan& plan, std::vector<SourceFile>& sources)
{
    BuiltSet built;
    MediaSetSpec media_set;
    media_set.media_set_part_number = PartNumber{plan.part_number};
    media_set.number_of_members = static_cast<std::uint8_t>(plan.members);
    media_set.file_check_type = plan.files_check;
    media_set.files_list_check_type = plan.files_list_check;

    std::size_t next_source = 0;
    for (const PlannedLoad& load : plan.loads)
    {
        LoadSpec load_spec;
        load_spec.header_file_name = load.header_file_name;
        load_spec.part_number = PartNumber{load.part_number};
        load_spec.load_type = load.type;
        load_spec.target_hardware_ids = load.target_hardware_ids;
        load_spec.load_check_type = load.check;
        for (std::size_t i = 0; i < load.files.size(); ++i, ++next_source)
        {
            SourceFile& source = sources[next_source];
            adl::arinc665::LoadFileSpec listed;
            listed.name = load.files[i].name;
            listed.content = std::move(source.bytes);
            listed.check_type = load.files[i].check;
            if (load.files[i].kind == FileKind::data)
            {
                ++built.data_files;
                load_spec.data_files.push_back(std::move(listed));
            }
            else
            {
                ++built.support_files;
                load_spec.support_files.push_back(std::move(listed));
            }
        }
        if (load_spec.data_files.empty())
        {
            return failure(FailureKind::no_data_files);
        }
        auto made = adl::arinc665::build_load(load_spec);
        if (!made)
        {
            return failure(FailureKind::load_not_built, {}, text_of(made.error()));
        }

        PlacedLoad placed;
        placed.load = std::move(*made);
        placed.header_member = 1;
        placed.part_root_directory = part_root_directory(load.part_number);
        if (plan.members > 1)
        {
            // The data files go round-robin over the members, each in a directory of the part root's name
            // (on the header file's member that is a subdirectory below the part root itself); the support
            // files stay on the header file's member in the part root.
            for (std::size_t i = 0; i < placed.load.data_files.size(); ++i)
            {
                FilePlacement placement;
                placement.member = static_cast<std::uint16_t>((i % plan.members) + 1);
                placement.subdirectory = part_root_name(load.part_number) + "\\";
                placed.data_placements.push_back(placement);
            }
            for (std::size_t i = 0; i < placed.load.support_files.size(); ++i)
            {
                FilePlacement placement;
                placement.member = placed.header_member;
                placed.support_placements.push_back(placement);
            }
        }
        media_set.loads.push_back(std::move(placed));
    }

    if (plan.batch)
    {
        // The batch names the set's loads for each target, on the first member.
        BatchFile batch;
        batch.batch_part_number = PartNumber{plan.batch->part_number};
        batch.comment = plan.batch->comment;
        for (const PlannedBatchTarget& planned : plan.batch->targets)
        {
            BatchTarget target;
            target.target_hardware_id_position = planned.id;
            for (const std::string& part_number : planned.load_part_numbers)
            {
                const auto load = std::ranges::find(plan.loads, part_number, &PlannedLoad::part_number);
                if (load != plan.loads.end())
                {
                    target.loads.push_back(BatchLoad{load->header_file_name, PartNumber{part_number}});
                }
            }
            batch.targets.push_back(std::move(target));
        }
        PlacedBatch placed_batch;
        placed_batch.file_name = plan.batch->file_name;
        placed_batch.batch = std::move(batch);
        placed_batch.member = 1;
        media_set.batches.push_back(std::move(placed_batch));
    }

    auto members = adl::arinc665::build_media_set(media_set);
    if (!members)
    {
        return failure(FailureKind::set_not_built, {}, text_of(members.error()));
    }
    built.members = std::move(*members);
    return built;
}

// What the written set holds is what the plan said: the loads and their files, the batch, the members. It
// cannot differ unless a library is wrong, which is why it is looked at.
std::string differs_from_plan(const SetPlan& plan, const adl::loader_core::MediaSetReport& report)
{
    if (report.part_number != plan.part_number)
    {
        return "its part number is " + report.part_number;
    }
    if (report.members != plan.members)
    {
        return "it has " + std::to_string(report.members) + " members";
    }
    if (report.loads.size() != plan.loads.size())
    {
        return "it has " + std::to_string(report.loads.size()) + " loads";
    }
    if (report.batches != (plan.batch ? 1U : 0U))
    {
        return "it has " + std::to_string(report.batches) + " batches";
    }
    for (std::size_t i = 0; i < plan.loads.size(); ++i)
    {
        const PlannedLoad& load = plan.loads[i];
        const auto& read = report.loads[i];
        const auto data = static_cast<std::size_t>(std::ranges::count(load.files, FileKind::data, &PlannedFile::kind));
        if (read.part_number != load.part_number || read.header_file != load.header_file_name ||
            read.data_files != data || read.support_files != load.files.size() - data)
        {
            return "load " + load.part_number + " is not as planned";
        }
    }
    return {};
}

struct Written
{
    std::size_t files = 0;
    std::uint64_t bytes = 0;
};

Written totals(const std::vector<adl::arinc665::MediaMember>& members)
{
    Written written;
    for (const auto& member : members)
    {
        written.files += member.files.size();
        for (const auto& file : member.files)
        {
            written.bytes += file.bytes.size();
        }
    }
    return written;
}

std::expected<MadeSet, MakeFailure> run(const SetPlan& plan, const Destination& destination, Hooks& hooks)
{
    const bool exact = destination.kind == Destination::Kind::exact;
    std::filesystem::path final_folder;
    std::filesystem::path base;
    std::filesystem::path stem;
    if (exact)
    {
        final_folder = exact_folder(destination);
        stem = final_folder.filename();
        base = final_folder.parent_path();
        if (base.empty())
        {
            base = ".";
        }
        if (stem.empty() || stem == "." || stem == "..")
        {
            return failure(FailureKind::not_written_below, base, "the folder has no name of its own", {}, {},
                           final_folder);
        }
        // Before anything is read: a set that cannot be put where it was asked for is not worth making.
        std::string system;
        const Place place = place_state(final_folder, system);
        if (place == Place::taken)
        {
            return failure(FailureKind::output_exists, final_folder);
        }
        if (place == Place::unknown)
        {
            return failure(FailureKind::not_written_below, base, {}, system, {}, final_folder);
        }
    }
    else
    {
        base = destination.path;
        stem = destination.name;
    }

    // ---- 1. read every file, reporting how far the reading has got ----
    std::uint64_t source_bytes = 0;
    auto sources = read_sources(plan, hooks, source_bytes);
    if (!sources)
    {
        return std::unexpected{std::move(sources.error())};
    }

    // ---- 2 and 3. the loads and the media set ----
    if (hooks.checkpoint("load"))
    {
        return failure(FailureKind::cancelled);
    }
    auto built = build(plan, *sources);
    if (!built)
    {
        return std::unexpected{std::move(built.error())};
    }

    // ---- 4. write the tree to a temporary directory beside the final one ----
    // The set appears in its place in one step, when it is whole and has been read back; until then, and
    // whatever goes wrong (a full disk, a cancel, a set that does not verify), what was written is removed.
    if (hooks.checkpoint("write"))
    {
        return failure(FailureKind::cancelled);
    }
    std::error_code error;
    std::filesystem::create_directories(base, error);
    if (error || !std::filesystem::is_directory(base, error))
    {
        return failure(FailureKind::not_written_below, base, {}, error.message(), {}, base);
    }
    PartialTree partial;
    for (int attempt = 1; attempt <= most_names_tried && !partial.claimed(); ++attempt)
    {
        std::filesystem::path candidate = base / stem;
        candidate += attempt == 1 ? ".writing" : ".writing_" + std::to_string(attempt);
        if (std::filesystem::create_directory(candidate, error))
        {
            partial.claim(candidate);
        }
        else if (error)
        {
            return failure(FailureKind::not_written_below, base, {}, error.message(), {}, candidate);
        }
    }
    if (!partial.claimed())
    {
        return failure(FailureKind::too_many_unfinished, stem);
    }
    const auto written = adl::media_tree::write_media_set_tree(partial.path(), built->members);
    if (!written)
    {
        return failure(FailureKind::write_failed, base, std::string{adl::media_tree::to_string(written.error().error)},
                       {}, partial.discard(), written.error().path);
    }
    const Written on_disk = totals(built->members);
    hooks.written(built->members.size(), on_disk.files, on_disk.bytes);

    // ---- 5. read the tree back and check what is on the disk, as the loader does ----
    // A cancel after the write finds the whole set on the disk, and takes it away again.
    if (hooks.checkpoint("written"))
    {
        return failure(FailureKind::cancelled, {}, {}, {}, partial.discard());
    }
    adl::loader_core::MediaSetReport report = adl::loader_core::check_media_set(partial.path());
    if (!report.error.empty())
    {
        // check_media_set says one thing for both failures; the reader of the tree says which it was.
        const auto tree = adl::media_tree::read_media_set_tree(partial.path());
        if (!tree)
        {
            return failure(FailureKind::read_back_failed, base,
                           std::string{adl::media_tree::to_string(tree.error().error)}, {}, partial.discard(),
                           tree.error().path);
        }
        return failure(FailureKind::not_verified, {}, report.error, {}, partial.discard());
    }
    if (const std::string difference = differs_from_plan(plan, report); !difference.empty())
    {
        return failure(FailureKind::not_the_set_described, {}, difference, {}, partial.discard());
    }
    hooks.checked(report);
    if (hooks.checkpoint("verified"))
    {
        return failure(FailureKind::cancelled, {}, {}, {}, partial.discard());
    }

    // ---- 6. give the set its place ----
    std::filesystem::path root;
    std::filesystem::path rename_at;
    std::string rename_system;
    for (int attempt = 1; attempt <= most_names_tried; ++attempt)
    {
        std::filesystem::path candidate = exact ? final_folder : base / stem;
        if (!exact && attempt > 1)
        {
            candidate += "_" + std::to_string(attempt);
        }
        std::string system;
        const Place place = place_state(candidate, system);
        if (place == Place::unknown)
        {
            rename_at = candidate;
            rename_system = system;
            break;
        }
        if (place == Place::taken)
        {
            if (exact)
            {
                return failure(FailureKind::output_exists, candidate, {}, {}, partial.discard());
            }
            continue; // a set (or anything) is there already: take the next name
        }
        if (std::filesystem::exists(candidate, error))
        {
            // An empty directory is taken over (a rename onto it fails on some systems).
            hooks.took_over_empty_folder(candidate);
            std::filesystem::remove(candidate, error);
        }
        std::filesystem::rename(partial.path(), candidate, error);
        if (!error)
        {
            root = candidate;
            partial.keep();
            break;
        }
        if (std::filesystem::exists(candidate))
        {
            if (exact)
            {
                return failure(FailureKind::output_exists, candidate, {}, {}, partial.discard());
            }
            continue; // appeared while we looked: take the next name
        }
        rename_at = candidate;
        rename_system = error.message();
        break;
    }
    if (root.empty())
    {
        return failure(FailureKind::not_given_its_name, base, {}, rename_system, partial.discard(), rename_at);
    }

    MadeSet made;
    made.path = root;
    made.members = built->members.size();
    made.data_files = built->data_files;
    made.support_files = built->support_files;
    made.files = on_disk.files;
    made.source_bytes = source_bytes;
    made.bytes = on_disk.bytes;
    made.report = std::move(report);
    hooks.progress(100);
    return made;
}

} // namespace

std::expected<MadeSet, MakeFailure> make_set(const SetPlan& plan, const Destination& destination, Hooks& hooks)
{
    try
    {
        return run(plan, destination, hooks);
    }
    catch (const std::bad_alloc&)
    {
        return failure(FailureKind::out_of_memory);
    }
    catch (const std::exception& problem)
    {
        return failure(FailureKind::error, {}, problem.what());
    }
}

} // namespace adl::set_maker

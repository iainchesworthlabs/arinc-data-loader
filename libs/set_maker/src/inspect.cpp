// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/set_maker/inspect.hpp"

#include "adl/arinc665/naming.hpp"
#include "adl/media_tree/media_tree.hpp"

#include <algorithm>
#include <iterator>
#include <tuple>

namespace adl::set_maker
{

namespace
{

using adl::arinc665::CheckValue;
using adl::arinc665::FilesList;
using adl::arinc665::FilesListEntry;
using adl::arinc665::MediaFile;
using adl::arinc665::MediaMember;
using adl::arinc665::OptionalCheckValue;

constexpr std::string_view root_pathname = "\\";

bool starts_with(std::string_view text, std::string_view prefix)
{
    return text.substr(0, prefix.size()) == prefix;
}

std::string version_text(std::uint16_t value)
{
    return "0x" + hex4_of(value);
}

std::string check_type_of(const OptionalCheckValue& value)
{
    return value ? check_type_word(value->type) : std::string{"none"};
}

std::optional<std::string> check_value_of(const OptionalCheckValue& value)
{
    if (!value)
    {
        return std::nullopt;
    }
    return hex_of(value->bytes);
}

std::string crc32_text(std::uint32_t value)
{
    return hex4_of(static_cast<std::uint16_t>(value >> 16U)) + hex4_of(static_cast<std::uint16_t>(value & 0xFFFFU));
}

const MediaMember* find_member(std::span<const MediaMember> members, std::uint16_t sequence_number)
{
    const auto found = std::ranges::find(members, sequence_number, &MediaMember::sequence_number);
    return found == members.end() ? nullptr : &*found;
}

const MediaFile* find_file(const MediaMember& member, std::string_view pathname, std::string_view name)
{
    const auto found = std::ranges::find_if(member.files, [&](const MediaFile& file)
                                            { return file.pathname == pathname && file.name == name; });
    return found == member.files.end() ? nullptr : &*found;
}

std::uint64_t size_of(std::span<const MediaMember> members, const FilesListEntry& entry)
{
    const MediaMember* member = find_member(members, entry.member_sequence_number);
    const MediaFile* file = member == nullptr ? nullptr : find_file(*member, entry.pathname, entry.file_name);
    return file == nullptr ? 0 : file->bytes.size();
}

InspectFailure invalid(adl::arinc665::Error error)
{
    return InspectFailure{InspectFailureKind::set_invalid, std::string{adl::arinc665::to_string(error)}, {}};
}

// One file of a load: what its header file records, and where FILES.LUM has it.
InspectedLoadFile load_file(std::span<const MediaMember> members, const FilesList& list, std::string_view part_root,
                            const adl::arinc665::UserSuppliedFile& wanted)
{
    InspectedLoadFile file;
    file.name = wanted.file_name;
    file.part_number = std::string{wanted.part_number.view()};
    file.crc16 = hex4_of(wanted.crc);
    file.check_type = check_type_of(wanted.check_value);
    file.check_value = check_value_of(wanted.check_value);
    if (const FilesListEntry* entry = locate_listed_file(list, part_root, wanted))
    {
        file.size = size_of(members, *entry);
        file.member = entry->member_sequence_number;
        file.pathname = entry->pathname;
    }
    return file;
}

const FilesListEntry* listed_on_member(const FilesList& list, std::string_view name, std::uint16_t member)
{
    const auto found =
        std::ranges::find_if(list.files, [&](const FilesListEntry& entry)
                             { return entry.file_name == name && entry.member_sequence_number == member; });
    return found == list.files.end() ? nullptr : &*found;
}

} // namespace

const FilesListEntry* locate_listed_file(const FilesList& list, std::string_view part_root,
                                         const adl::arinc665::UserSuppliedFile& wanted)
{
    std::vector<const FilesListEntry*> candidates;
    for (const FilesListEntry& entry : list.files)
    {
        if (entry.file_name == wanted.file_name)
        {
            candidates.push_back(&entry);
        }
    }
    if (candidates.size() > 1)
    {
        std::vector<const FilesListEntry*> in_root;
        std::ranges::copy_if(candidates, std::back_inserter(in_root),
                             [&](const FilesListEntry* entry) { return starts_with(entry->pathname, part_root); });
        candidates = std::move(in_root);
    }
    if (candidates.size() > 1)
    {
        std::vector<const FilesListEntry*> same_crc;
        std::ranges::copy_if(candidates, std::back_inserter(same_crc),
                             [&](const FilesListEntry* entry) { return entry->crc == wanted.crc; });
        if (!same_crc.empty())
        {
            candidates = std::move(same_crc);
        }
    }
    return candidates.empty() ? nullptr : candidates.front();
}

std::expected<Inspection, InspectFailure> inspect_members(std::span<const MediaMember> members)
{
    auto read = adl::arinc665::read_media_set(members);
    if (!read)
    {
        return std::unexpected{invalid(read.error())};
    }
    const adl::arinc665::MediaSet& set = *read;
    const FilesList& list = set.files_list;

    Inspection result;
    result.part_number = std::string{set.loads_list.identity.media_set_part_number.view()};
    result.members = set.loads_list.identity.number_of_media_set_members;
    for (const MediaMember& member : members)
    {
        result.members_present.push_back(member.sequence_number);
    }
    std::ranges::sort(result.members_present);
    result.loads_list_version = version_text(static_cast<std::uint16_t>(set.loads_list.format_version));
    result.files_list_version = version_text(static_cast<std::uint16_t>(list.format_version));
    result.loads_list_user_data = hex_of(set.loads_list.user_defined_data);
    result.files_list_user_data = hex_of(list.user_defined_data);
    if (set.batches_list)
    {
        result.batches_list_version = version_text(static_cast<std::uint16_t>(set.batches_list->format_version));
        result.batches_list_user_data = hex_of(set.batches_list->user_defined_data);
    }
    result.files_list_check_type = check_type_of(list.check_value);
    result.files_list_check_value = check_value_of(list.check_value);

    for (std::size_t i = 0; i < set.loads.size(); ++i)
    {
        const adl::arinc665::Load& load = set.loads[i];
        const adl::arinc665::LoadsListEntry& entry = set.loads_list.loads[i];
        const FilesListEntry* header_entry =
            listed_on_member(list, entry.header_file_name, entry.member_sequence_number);

        InspectedLoad out;
        out.part_number = std::string{load.header.load_part_number.view()};
        out.header_file = load.header_file_name;
        out.header_member = entry.member_sequence_number;
        out.header_pathname = header_entry == nullptr ? std::string{} : header_entry->pathname;
        out.format_version = version_text(static_cast<std::uint16_t>(load.header.format_version));
        out.type = load.header.load_type;
        out.is_download = load.header.part_flags.is_download;
        out.target_hardware_ids = load.header.target_hardware_ids;
        for (const adl::arinc665::TargetHardware& hardware : load.header.target_hardware_ids_with_positions)
        {
            out.target_hardware_ids_with_positions.push_back(InspectedPositions{hardware.id, hardware.positions});
        }
        out.check_type = check_type_of(load.header.load_check_value);
        out.check_value = check_value_of(load.header.load_check_value);
        out.load_crc = crc32_text(load.header.load_crc);
        out.user_defined_data = hex_of(load.header.user_defined_data);
        for (const adl::arinc665::DataFile& file : load.header.data_files)
        {
            out.data_files.push_back(load_file(members, list, out.header_pathname, file));
        }
        for (const adl::arinc665::SupportFile& file : load.header.support_files)
        {
            out.support_files.push_back(load_file(members, list, out.header_pathname, file));
        }
        result.loads.push_back(std::move(out));
    }

    for (std::size_t i = 0; i < set.batches.size(); ++i)
    {
        const adl::arinc665::ReadBatch& read_batch = set.batches[i];
        const adl::arinc665::BatchesListEntry& entry = set.batches_list->batches[i];
        const FilesListEntry* listed = listed_on_member(list, entry.batch_file_name, entry.member_sequence_number);

        InspectedBatch out;
        out.part_number = std::string{read_batch.batch.batch_part_number.view()};
        out.file_name = read_batch.file_name;
        out.member = entry.member_sequence_number;
        out.pathname = listed == nullptr ? std::string{} : listed->pathname;
        out.format_version = version_text(static_cast<std::uint16_t>(read_batch.batch.format_version));
        out.comment = read_batch.batch.comment;
        for (const adl::arinc665::BatchTarget& target : read_batch.batch.targets)
        {
            InspectedBatchTarget planned;
            planned.target_hardware_id_position = target.target_hardware_id_position;
            for (const adl::arinc665::BatchLoad& load : target.loads)
            {
                planned.loads.push_back(
                    InspectedBatchLoad{load.header_file_name, std::string{load.load_part_number.view()}});
            }
            out.targets.push_back(std::move(planned));
        }
        result.batches.push_back(std::move(out));
    }

    for (const FilesListEntry& entry : list.files)
    {
        InspectedFile file;
        file.name = entry.file_name;
        file.pathname = entry.pathname;
        file.member = entry.member_sequence_number;
        file.size = size_of(members, entry);
        file.crc16 = hex4_of(entry.crc);
        file.check_type = check_type_of(entry.check_value);
        file.check_value = check_value_of(entry.check_value);
        result.files.push_back(std::move(file));
    }

    // What is on a member and not in FILES.LUM, which never lists itself (section 3.2.1).
    std::vector<const MediaMember*> ordered;
    for (const MediaMember& member : members)
    {
        ordered.push_back(&member);
    }
    std::ranges::sort(ordered, {}, &MediaMember::sequence_number);
    for (const MediaMember* member : ordered)
    {
        std::vector<const MediaFile*> files;
        for (const MediaFile& file : member->files)
        {
            files.push_back(&file);
        }
        std::ranges::sort(files, [](const MediaFile* a, const MediaFile* b)
                          { return std::tie(a->pathname, a->name) < std::tie(b->pathname, b->name); });
        for (const MediaFile* file : files)
        {
            const bool is_files_list =
                file->pathname == root_pathname && file->name == adl::arinc665::files_list_file_name;
            const bool listed = std::ranges::any_of(list.files,
                                                    [&](const FilesListEntry& entry)
                                                    {
                                                        return entry.file_name == file->name &&
                                                               entry.pathname == file->pathname &&
                                                               entry.member_sequence_number == member->sequence_number;
                                                    });
            if (!is_files_list && !listed)
            {
                result.unlisted_files.push_back(
                    UnlistedFile{file->name, file->pathname, member->sequence_number, file->bytes.size()});
            }
        }
    }

    for (const adl::arinc665::Finding& finding : set.findings)
    {
        result.findings.push_back(finding.clause + ": " + finding.message);
    }
    return result;
}

std::expected<Inspection, InspectFailure> inspect_set(const std::filesystem::path& root)
{
    std::error_code error;
    if (!std::filesystem::is_directory(root, error))
    {
        return std::unexpected{InspectFailure{InspectFailureKind::folder_unreadable, {}, root}};
    }
    auto members = adl::media_tree::read_media_set_tree(root);
    if (!members)
    {
        // A root that is not a folder is the folder's fault; anything else is the set's.
        const bool not_a_folder = members.error().error == adl::media_tree::TreeError::not_a_directory;
        return std::unexpected{
            InspectFailure{not_a_folder ? InspectFailureKind::folder_unreadable : InspectFailureKind::set_invalid,
                           std::string{adl::media_tree::to_string(members.error().error)}, members.error().path}};
    }
    return inspect_members(*members);
}

} // namespace adl::set_maker

// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/arinc665/media_set.hpp"

#include "adl/arinc665/check_value.hpp"
#include "adl/arinc665/integrity_checker.hpp"
#include "adl/arinc665/naming.hpp"
#include "wire.hpp"

#include <adl/profiling.hpp>

#include <algorithm>
#include <iterator>
#include <set>

namespace adl::arinc665
{

namespace
{

constexpr std::string_view root_pathname = "\\";

// A file with the member it is on.
struct Placed
{
    MediaFile file;
    std::uint16_t member = 1;
};

// The standard recommends the part root directory be named for the part number
// without its delimiters (section 3.2.4.1.1).
std::string default_part_root(const PartNumber& part_number)
{
    std::string directory = "\\";
    for (const char c : part_number.view())
    {
        if (c != '-')
        {
            directory += c;
        }
    }
    directory += '\\';
    return directory;
}

const MediaFile* find_file(const MediaMember& member, std::string_view pathname, std::string_view name)
{
    const auto found = std::find_if(member.files.begin(), member.files.end(), [&](const MediaFile& file)
                                    { return file.pathname == pathname && file.name == name; });
    return found == member.files.end() ? nullptr : &*found;
}

const MediaMember* find_member(std::span<const MediaMember> members, std::uint16_t sequence_number)
{
    const auto found = std::find_if(members.begin(), members.end(), [&](const MediaMember& member)
                                    { return member.sequence_number == sequence_number; });
    return found == members.end() ? nullptr : &*found;
}

OptionalCheckValue file_check_value(CheckValueType type, std::span<const std::byte> bytes, Error& error)
{
    if (type == CheckValueType::none)
    {
        return std::nullopt;
    }
    auto value = compute_check_value(type, bytes);
    if (!value)
    {
        error = value.error();
        return std::nullopt;
    }
    return std::move(*value);
}

// Whether `bytes` have the CRC and check value that a list records for them. A
// check value of a type that cannot be calculated here is left unchecked.
Result<void> verify_listed(std::span<const std::byte> bytes, std::uint16_t crc, const OptionalCheckValue& check_value)
{
    if (detail::crc16(bytes) != crc)
    {
        return std::unexpected{Error::bad_crc};
    }
    if (check_value)
    {
        const auto recomputed = compute_check_value(check_value->type, bytes);
        if (recomputed && *recomputed != *check_value)
        {
            return std::unexpected{Error::bad_check_value};
        }
    }
    return {};
}

MediaSetIdentity identity_of(const MediaSetSpec& spec, std::uint16_t member)
{
    MediaSetIdentity identity;
    identity.media_set_part_number = spec.media_set_part_number;
    identity.media_sequence_number = static_cast<std::uint8_t>(member);
    identity.number_of_media_set_members = spec.number_of_members;
    return identity;
}

bool starts_with(std::string_view text, std::string_view prefix)
{
    return text.substr(0, prefix.size()) == prefix;
}

} // namespace

Result<std::vector<MediaMember>> build_media_set(const MediaSetSpec& spec)
{
    const adl::profiling::Zone zone{"arinc665: build media set"};
    const std::size_t member_count = spec.number_of_members;
    if (member_count == 0)
    {
        return std::unexpected{Error::invalid_value};
    }
    const auto member_exists = [member_count](std::uint16_t member) { return member >= 1 && member <= member_count; };

    // The files the person placed, and the lists that describe the loads and
    // batches among them.
    std::vector<Placed> placed;
    LoadsList loads_list;
    BatchesList batches_list;

    for (const PlacedLoad& entry : spec.loads)
    {
        const Load& load = entry.load;
        if (!member_exists(entry.header_member) ||
            (!entry.data_placements.empty() && entry.data_placements.size() != load.data_files.size()) ||
            (!entry.support_placements.empty() && entry.support_placements.size() != load.support_files.size()))
        {
            return std::unexpected{Error::invalid_value};
        }
        const std::string root = entry.part_root_directory.empty() ? default_part_root(load.header.load_part_number)
                                                                   : entry.part_root_directory;

        placed.push_back({{root, load.header_file_name, load.header_bytes}, entry.header_member});
        const auto place_all = [&](const std::vector<FileContent>& files,
                                   const std::vector<FilePlacement>& placements) -> bool
        {
            for (std::size_t i = 0; i < files.size(); ++i)
            {
                const FilePlacement placement = placements.empty() ? FilePlacement{} : placements[i];
                const std::uint16_t member = placement.member == 0 ? entry.header_member : placement.member;
                if (!member_exists(member))
                {
                    return false;
                }
                placed.push_back({{root + placement.subdirectory, files[i].name, files[i].bytes}, member});
            }
            return true;
        };
        if (!place_all(load.data_files, entry.data_placements) ||
            !place_all(load.support_files, entry.support_placements))
        {
            return std::unexpected{Error::invalid_value};
        }

        LoadsListEntry listed;
        listed.load_part_number = load.header.load_part_number;
        listed.header_file_name = load.header_file_name;
        listed.member_sequence_number = entry.header_member;
        listed.target_hardware_ids = load.header.target_hardware_ids;
        loads_list.loads.push_back(std::move(listed));
    }

    for (const PlacedBatch& entry : spec.batches)
    {
        if (!member_exists(entry.member))
        {
            return std::unexpected{Error::invalid_value};
        }
        auto bytes = encode_batch_file(entry.batch);
        if (!bytes)
        {
            return std::unexpected{bytes.error()};
        }
        placed.push_back({{entry.pathname, entry.file_name, std::move(*bytes)}, entry.member});
        batches_list.batches.push_back({entry.batch.batch_part_number, entry.file_name, entry.member});
    }

    for (const PlacedFile& entry : spec.other_files)
    {
        if (!member_exists(entry.member))
        {
            return std::unexpected{Error::invalid_value};
        }
        placed.push_back({entry.file, entry.member});
    }

    // LOADS.LUM and BATCHES.LUM differ from member to member only in their media
    // sequence number (and so their CRC), and every member's is listed.
    const bool has_batches = !spec.batches.empty();
    std::vector<MediaFile> loads_files;
    std::vector<MediaFile> batches_files;
    for (std::size_t member = 1; member <= member_count; ++member)
    {
        const auto member_number = static_cast<std::uint16_t>(member);
        loads_list.identity = identity_of(spec, member_number);
        auto loads_bytes = encode_loads_list(loads_list);
        if (!loads_bytes)
        {
            return std::unexpected{loads_bytes.error()};
        }
        loads_files.push_back({std::string{root_pathname}, std::string{loads_list_file_name}, std::move(*loads_bytes)});

        if (has_batches)
        {
            batches_list.identity = identity_of(spec, member_number);
            auto batches_bytes = encode_batches_list(batches_list);
            if (!batches_bytes)
            {
                return std::unexpected{batches_bytes.error()};
            }
            batches_files.push_back(
                {std::string{root_pathname}, std::string{batches_list_file_name}, std::move(*batches_bytes)});
        }
    }

    // FILES.LUM lists everything on the set but itself: the list files of every
    // member first, then the loads' files, batches and other files in the order
    // they were given.
    Error error = Error::invalid_value;
    std::vector<FilesListEntry> entries;
    const auto list = [&](const MediaFile& file, std::uint16_t member) -> bool
    {
        FilesListEntry entry;
        entry.file_name = file.name;
        entry.pathname = file.pathname;
        entry.member_sequence_number = member;
        entry.crc = detail::crc16(file.bytes);
        entry.check_value = file_check_value(spec.file_check_type, file.bytes, error);
        if (spec.file_check_type != CheckValueType::none && !entry.check_value)
        {
            return false;
        }
        entries.push_back(std::move(entry));
        return true;
    };
    for (std::size_t member = 1; member <= member_count; ++member)
    {
        if (!list(loads_files[member - 1U], static_cast<std::uint16_t>(member)))
        {
            return std::unexpected{error};
        }
    }
    for (std::size_t member = 1; has_batches && member <= member_count; ++member)
    {
        if (!list(batches_files[member - 1U], static_cast<std::uint16_t>(member)))
        {
            return std::unexpected{error};
        }
    }
    for (const Placed& entry : placed)
    {
        if (!list(entry.file, entry.member))
        {
            return std::unexpected{error};
        }
    }

    if (spec.files_list_check_type != CheckValueType::none && !IntegrityChecker::make(spec.files_list_check_type))
    {
        return std::unexpected{Error::unsupported_check_type};
    }

    std::vector<MediaMember> members(member_count);
    for (std::size_t member = 1; member <= member_count; ++member)
    {
        const auto member_number = static_cast<std::uint16_t>(member);
        FilesList files_list;
        files_list.identity = identity_of(spec, member_number);
        files_list.files = entries;
        // The check value of FILES.LUM sits inside it, so it is written first with
        // a value of the right size, then worked out and written again.
        if (spec.files_list_check_type != CheckValueType::none)
        {
            files_list.check_value = CheckValue{spec.files_list_check_type,
                                                std::vector<std::byte>(check_value_length(spec.files_list_check_type))};
        }
        auto bytes = encode_files_list(files_list);
        if (!bytes)
        {
            return std::unexpected{bytes.error()};
        }
        if (spec.files_list_check_type != CheckValueType::none)
        {
            const auto offset = files_list_check_value_offset(*bytes);
            if (!offset)
            {
                return std::unexpected{offset.error()};
            }
            auto value =
                compute_check_value(spec.files_list_check_type, std::span<const std::byte>{*bytes}.first(*offset));
            if (!value)
            {
                return std::unexpected{value.error()};
            }
            files_list.check_value = std::move(*value);
            bytes = encode_files_list(files_list);
            if (!bytes)
            {
                return std::unexpected{bytes.error()};
            }
        }

        MediaMember& result = members[member - 1U];
        result.sequence_number = member_number;
        result.files.push_back({std::string{root_pathname}, std::string{files_list_file_name}, std::move(*bytes)});
        result.files.push_back(loads_files[member - 1U]);
        if (has_batches)
        {
            result.files.push_back(batches_files[member - 1U]);
        }
        for (const Placed& entry : placed)
        {
            if (entry.member == member_number)
            {
                result.files.push_back(entry.file);
            }
        }

        // Two files cannot share a name in one directory of a medium.
        std::set<std::pair<std::string, std::string>> names;
        for (const MediaFile& file : result.files)
        {
            if (!names.insert({file.pathname, file.name}).second)
            {
                return std::unexpected{Error::invalid_value};
            }
        }
    }
    return members;
}

namespace
{

// What section 3.2.3 says is the same in a list file on every member: all but the
// media sequence number and the CRC.
bool same_set(const MediaSetIdentity& a, const MediaSetIdentity& b)
{
    return a.media_set_part_number == b.media_set_part_number &&
           a.number_of_media_set_members == b.number_of_media_set_members;
}

// Section 3.2.4.2: pick the file FILES.LUM means for a name in a header file.
// Among files of that name, those in the part root directory if there are
// several; then those with the CRC the header records if there are still
// several; then the first.
const FilesListEntry* resolve_listed(const FilesList& list, std::string_view part_root, const UserSuppliedFile& wanted)
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
        std::copy_if(candidates.begin(), candidates.end(), std::back_inserter(in_root),
                     [&](const FilesListEntry* entry) { return starts_with(entry->pathname, part_root); });
        candidates = std::move(in_root);
    }
    if (candidates.size() > 1)
    {
        std::vector<const FilesListEntry*> same_crc;
        std::copy_if(candidates.begin(), candidates.end(), std::back_inserter(same_crc),
                     [&](const FilesListEntry* entry) { return entry->crc == wanted.crc; });
        if (!same_crc.empty())
        {
            candidates = std::move(same_crc);
        }
    }
    return candidates.empty() ? nullptr : candidates.front();
}

Result<const MediaFile*> fetch(std::span<const MediaMember> members, const FilesListEntry& entry)
{
    const MediaMember* member = find_member(members, entry.member_sequence_number);
    if (member == nullptr)
    {
        return std::unexpected{Error::file_not_found};
    }
    const MediaFile* file = find_file(*member, entry.pathname, entry.file_name);
    if (file == nullptr)
    {
        return std::unexpected{Error::file_not_found};
    }
    return file;
}

Result<std::vector<FileContent>> resolve_files(std::span<const MediaMember> members, const FilesList& list,
                                               std::string_view part_root, const std::vector<DataFile>& data_files,
                                               const std::vector<SupportFile>& support_files, bool want_support)
{
    std::vector<FileContent> contents;
    const auto resolve_one = [&](const UserSuppliedFile& wanted) -> Result<void>
    {
        const FilesListEntry* entry = resolve_listed(list, part_root, wanted);
        if (entry == nullptr)
        {
            return std::unexpected{Error::file_not_found};
        }
        const auto file = fetch(members, *entry);
        if (!file)
        {
            return std::unexpected{file.error()};
        }
        contents.push_back({wanted.file_name, (*file)->bytes});
        return {};
    };
    if (want_support)
    {
        for (const SupportFile& wanted : support_files)
        {
            if (auto result = resolve_one(wanted); !result)
            {
                return std::unexpected{result.error()};
            }
        }
    }
    else
    {
        for (const DataFile& wanted : data_files)
        {
            if (auto result = resolve_one(wanted); !result)
            {
                return std::unexpected{result.error()};
            }
        }
    }
    return contents;
}

} // namespace

Result<MediaSet> read_media_set(std::span<const MediaMember> members)
{
    if (members.empty())
    {
        return std::unexpected{Error::invalid_value};
    }

    MediaSet set;
    bool first = true;
    std::set<std::uint16_t> sequence_numbers;
    for (const MediaMember& member : members)
    {
        if (!sequence_numbers.insert(member.sequence_number).second)
        {
            return std::unexpected{Error::inconsistent};
        }
        const MediaFile* loads_file = find_file(member, root_pathname, loads_list_file_name);
        const MediaFile* files_file = find_file(member, root_pathname, files_list_file_name);
        const MediaFile* batches_file = find_file(member, root_pathname, batches_list_file_name);
        if (loads_file == nullptr || files_file == nullptr)
        {
            return std::unexpected{Error::file_not_found};
        }

        auto loads_list = decode_loads_list(loads_file->bytes);
        if (!loads_list)
        {
            return std::unexpected{loads_list.error()};
        }
        auto files_list = decode_files_list(files_file->bytes);
        if (!files_list)
        {
            return std::unexpected{files_list.error()};
        }
        std::optional<BatchesList> batches_list;
        if (batches_file != nullptr)
        {
            auto decoded = decode_batches_list(batches_file->bytes);
            if (!decoded)
            {
                return std::unexpected{decoded.error()};
            }
            batches_list = std::move(*decoded);
        }

        // Each list file says which member it is on; it has to be this one.
        if (loads_list->identity.media_sequence_number != member.sequence_number ||
            files_list->identity.media_sequence_number != member.sequence_number ||
            (batches_list && batches_list->identity.media_sequence_number != member.sequence_number))
        {
            return std::unexpected{Error::inconsistent};
        }

        // FILES.LUM's own check value covers it but for the check value fields.
        if (files_list->check_value && IntegrityChecker::make(files_list->check_value->type))
        {
            const auto offset = files_list_check_value_offset(files_file->bytes);
            if (!offset)
            {
                return std::unexpected{offset.error()};
            }
            const auto value = compute_check_value(files_list->check_value->type,
                                                   std::span<const std::byte>{files_file->bytes}.first(*offset));
            if (!value || *value != *files_list->check_value)
            {
                return std::unexpected{Error::bad_check_value};
            }
        }

        if (first)
        {
            set.loads_list = std::move(*loads_list);
            set.files_list = std::move(*files_list);
            set.batches_list = std::move(batches_list);
            first = false;
            continue;
        }
        if (!same_set(set.loads_list.identity, loads_list->identity) ||
            !same_set(set.files_list.identity, files_list->identity) || set.loads_list.loads != loads_list->loads ||
            set.loads_list.user_defined_data != loads_list->user_defined_data ||
            set.files_list.files != files_list->files ||
            set.files_list.user_defined_data != files_list->user_defined_data ||
            set.batches_list.has_value() != batches_list.has_value() ||
            (batches_list && (!same_set(set.batches_list->identity, batches_list->identity) ||
                              set.batches_list->batches != batches_list->batches)))
        {
            return std::unexpected{Error::inconsistent};
        }
    }

    // Every file FILES.LUM lists on a member we have is there, and is the file.
    for (const FilesListEntry& entry : set.files_list.files)
    {
        if (find_member(members, entry.member_sequence_number) == nullptr)
        {
            continue;
        }
        const auto file = fetch(members, entry);
        if (!file)
        {
            return std::unexpected{file.error()};
        }
        if (auto result = verify_listed((*file)->bytes, entry.crc, entry.check_value); !result)
        {
            return std::unexpected{result.error()};
        }
    }

    for (const LoadsListEntry& entry : set.loads_list.loads)
    {
        // Section 3.2.4.1.1: the header file's entry in FILES.LUM says where the
        // load's part root directory is.
        const auto header_entry = std::find_if(set.files_list.files.begin(), set.files_list.files.end(),
                                               [&](const FilesListEntry& file)
                                               {
                                                   return file.file_name == entry.header_file_name &&
                                                          file.member_sequence_number == entry.member_sequence_number;
                                               });
        if (header_entry == set.files_list.files.end())
        {
            return std::unexpected{Error::file_not_found};
        }
        const auto header_file = fetch(members, *header_entry);
        if (!header_file)
        {
            return std::unexpected{header_file.error()};
        }
        auto header = decode_header_file((*header_file)->bytes);
        if (!header)
        {
            return std::unexpected{header.error()};
        }
        if (header->load_part_number != entry.load_part_number ||
            header->target_hardware_ids != entry.target_hardware_ids)
        {
            return std::unexpected{Error::inconsistent};
        }

        Load load;
        load.header_file_name = entry.header_file_name;
        load.header_bytes = (*header_file)->bytes;
        auto data = resolve_files(members, set.files_list, header_entry->pathname, header->data_files,
                                  header->support_files, false);
        if (!data)
        {
            return std::unexpected{data.error()};
        }
        auto support = resolve_files(members, set.files_list, header_entry->pathname, header->data_files,
                                     header->support_files, true);
        if (!support)
        {
            return std::unexpected{support.error()};
        }
        load.data_files = std::move(*data);
        load.support_files = std::move(*support);
        load.header = std::move(*header);
        if (auto result = verify_load(load); !result)
        {
            return std::unexpected{result.error()};
        }
        set.loads.push_back(std::move(load));
    }

    if (set.batches_list)
    {
        for (const BatchesListEntry& entry : set.batches_list->batches)
        {
            const auto listed = std::find_if(set.files_list.files.begin(), set.files_list.files.end(),
                                             [&](const FilesListEntry& file)
                                             {
                                                 return file.file_name == entry.batch_file_name &&
                                                        file.member_sequence_number == entry.member_sequence_number;
                                             });
            if (listed == set.files_list.files.end())
            {
                return std::unexpected{Error::file_not_found};
            }
            const auto file = fetch(members, *listed);
            if (!file)
            {
                return std::unexpected{file.error()};
            }
            auto batch = decode_batch_file((*file)->bytes);
            if (!batch)
            {
                return std::unexpected{batch.error()};
            }
            if (batch->batch_part_number != entry.batch_part_number)
            {
                return std::unexpected{Error::inconsistent};
            }
            set.batches.push_back({entry.batch_file_name, std::move(*batch)});
        }
    }

    // Everything on a member should be listed in FILES.LUM but for FILES.LUM itself
    // (section 3.2.1).
    for (const MediaMember& member : members)
    {
        for (const MediaFile& file : member.files)
        {
            const bool is_files_list = file.pathname == root_pathname && file.name == files_list_file_name;
            const bool listed = std::any_of(set.files_list.files.begin(), set.files_list.files.end(),
                                            [&](const FilesListEntry& entry)
                                            {
                                                return entry.file_name == file.name &&
                                                       entry.pathname == file.pathname &&
                                                       entry.member_sequence_number == member.sequence_number;
                                            });
            if (!is_files_list && !listed)
            {
                set.findings.push_back({"3.2.1", "the file " + file.pathname + file.name + " on member " +
                                                     std::to_string(member.sequence_number) +
                                                     " is not listed in FILES.LUM"});
            }
        }
    }

    const auto append = [&set](Findings more)
    {
        set.findings.insert(set.findings.end(), std::make_move_iterator(more.begin()),
                            std::make_move_iterator(more.end()));
    };
    append(check_loads_list(set.loads_list));
    append(check_files_list(set.files_list));
    if (set.batches_list)
    {
        append(check_batches_list(*set.batches_list));
    }
    for (const Load& load : set.loads)
    {
        append(check_load(load));
    }
    for (const ReadBatch& batch : set.batches)
    {
        append(check_batch_file(batch.batch));
        append(check_batch_file_name(batch.file_name));
    }
    return set;
}

} // namespace adl::arinc665

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Reading a media set into the facts that a comparison, a manifest or a rule about a site's own part
// numbers and layout needs: the header fields of the set, each load with its files, the check values and
// where each file is, the batches and who they name, and every file FILES.LUM lists. Any set can be read,
// not only one this library made.
//
// The reading is adl::arinc665::read_media_set(), which verifies every file and every check value, so a
// set that can be inspected is a set that verifies. What this adds is the joining of what the header file
// says about a file with where FILES.LUM has it, and the words for the check values.

#include "adl/arinc665/files.hpp"
#include "adl/arinc665/media_files.hpp"
#include "adl/arinc665/media_set.hpp"
#include "adl/set_maker/plan.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace adl::set_maker
{

// A file as FILES.LUM lists it, with its size on the member.
struct InspectedFile
{
    std::string name;
    std::string pathname;
    std::uint16_t member = 0;
    std::uint64_t size = 0;
    std::string crc16; // four upper-case hexadecimal digits
    std::string check_type;
    std::optional<std::string> check_value; // upper-case hexadecimal of the bytes as stored
};

// A file of a load as the header file records it, with where it is.
struct InspectedLoadFile
{
    std::string name;
    std::string part_number;
    std::uint64_t size = 0;
    std::string crc16;
    std::string check_type;
    std::optional<std::string> check_value;
    std::uint16_t member = 0;
    std::string pathname;
};

struct InspectedPositions
{
    std::string id;
    std::vector<std::string> positions;
};

struct InspectedLoad
{
    std::string part_number;
    std::string header_file;
    std::uint16_t header_member = 0;
    std::string header_pathname;
    std::string format_version; // "0x8004"
    std::optional<adl::arinc665::LoadType> type;
    bool is_download = false;
    std::vector<std::string> target_hardware_ids;
    std::vector<InspectedPositions> target_hardware_ids_with_positions;
    std::string check_type; // the load check value
    std::optional<std::string> check_value;
    std::string load_crc; // eight upper-case hexadecimal digits
    std::string user_defined_data;
    std::vector<InspectedLoadFile> data_files;
    std::vector<InspectedLoadFile> support_files;
};

struct InspectedBatchLoad
{
    std::string header_file_name;
    std::string load_part_number;
};

struct InspectedBatchTarget
{
    std::string target_hardware_id_position;
    std::vector<InspectedBatchLoad> loads;
};

struct InspectedBatch
{
    std::string part_number;
    std::string file_name;
    std::uint16_t member = 0;
    std::string pathname;
    std::string format_version; // "0x9004"
    std::string comment;
    std::vector<InspectedBatchTarget> targets;
};

// A file on a member that FILES.LUM does not list.
struct UnlistedFile
{
    std::string name;
    std::string pathname;
    std::uint16_t member = 0;
    std::uint64_t size = 0;
};

struct Inspection
{
    std::string part_number;
    unsigned members = 0;                  // the number of members the lists say
    std::vector<unsigned> members_present; // the members that were read, in order
    std::string loads_list_version;        // "0xA004"
    std::string files_list_version;
    std::optional<std::string> batches_list_version;
    std::string loads_list_user_data; // upper-case hexadecimal, empty for none
    std::string files_list_user_data;
    std::optional<std::string> batches_list_user_data;
    std::string files_list_check_type; // the check value FILES.LUM records for itself
    std::optional<std::string> files_list_check_value;
    std::vector<InspectedLoad> loads;
    std::vector<InspectedBatch> batches;
    std::vector<InspectedFile> files;
    std::vector<UnlistedFile> unlisted_files;
    std::vector<std::string> findings; // "clause: message"
};

enum class InspectFailureKind
{
    folder_unreadable, // the folder is not there, is not a folder, or cannot be listed
    set_invalid,       // it does not read as a media set, or does not verify
};

struct InspectFailure
{
    InspectFailureKind kind = InspectFailureKind::set_invalid;
    std::string detail;         // the libraries' words, English
    std::filesystem::path path; // the entry the words are about, when there is one
};

// The files of the members in `members` are looked at, not the disk. The members are as
// adl::media_tree::read_media_set_tree() gives them.
[[nodiscard]] std::expected<Inspection, InspectFailure>
inspect_members(std::span<const adl::arinc665::MediaMember> members);

// Reads the tree under `root` and inspects it.
[[nodiscard]] std::expected<Inspection, InspectFailure> inspect_set(const std::filesystem::path& root);

// Section 3.2.4.2: the file FILES.LUM means for a name in a header file. Among the files of that name, the
// ones whose folder starts with the load's part root directory if there are several; then the ones with the
// CRC the header records if there are still several; then the first. Null if there is none.
//
// This is a copy of resolve_listed() in libs/arinc665/src/media_set.cpp, which read_media_set() uses to find
// the files of a load. The two have to find the same file, so a change to one is a change to the other, and
// the test set_maker.inspect holds them to it.
[[nodiscard]] const adl::arinc665::FilesListEntry* locate_listed_file(const adl::arinc665::FilesList& list,
                                                                      std::string_view part_root,
                                                                      const adl::arinc665::UserSuppliedFile& wanted);

} // namespace adl::set_maker

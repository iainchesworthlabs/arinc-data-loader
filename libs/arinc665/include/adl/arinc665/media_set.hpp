// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// A media set in memory: its members and the files on each. build_media_set()
// lays loads, batches and other files out over the members and writes the list
// files; read_media_set() does the reverse and checks what it finds. No file
// system is used here; adl::media_tree maps members to directories.

#include "adl/arinc665/batch_file.hpp"
#include "adl/arinc665/check_value_type.hpp"
#include "adl/arinc665/error.hpp"
#include "adl/arinc665/finding.hpp"
#include "adl/arinc665/load.hpp"
#include "adl/arinc665/media_files.hpp"
#include "adl/arinc665/part_number.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace adl::arinc665
{

// A file as it sits on one member of a media set. The pathname is the
// directory it is in, from the root of the member and ending in a backslash
// ("\" for the root); the name is the file's own.
struct MediaFile
{
    std::string pathname = "\\";
    std::string name;
    std::vector<std::byte> bytes;

    friend bool operator==(const MediaFile&, const MediaFile&) = default;
};

// One medium of a media set (a disk, a card, a CD): its number in the set and
// the files on it.
struct MediaMember
{
    std::uint16_t sequence_number = 1;
    std::vector<MediaFile> files;
};

// Where one of a load's data or support files goes, if not with the header file.
struct FilePlacement
{
    std::uint16_t member = 0; // 0: the same member as the header file
    // A directory below the load's part root directory, ending in a backslash
    // ("disk001\"), or empty for the part root directory itself. Files with the
    // same name in one load are told apart this way (section 3.2.4).
    std::string subdirectory;
};

// A load, and where it goes in a media set: the member that holds its header file
// and where its data and support files are put (see FilePlacement).
struct PlacedLoad
{
    Load load;
    // The part root directory (section 3.2.4.1.1), starting and ending with a
    // backslash. Empty: the load's part number without its hyphens, which the
    // standard recommends.
    std::string part_root_directory;
    std::uint16_t header_member = 1;
    // Parallel to load.data_files and load.support_files, or empty for "all in
    // the part root directory on the header file's member".
    std::vector<FilePlacement> data_placements;
    std::vector<FilePlacement> support_placements;
};

// A batch file, and the member and directory it is placed in.
struct PlacedBatch
{
    std::string file_name; // for example "ACM47.LUB"
    BatchFile batch;
    std::uint16_t member = 1;
    std::string pathname = "\\";
};

// A file on the media set that is not part of a load or a batch, which the
// standard allows (section 3.2.1): NON_LOAD.CRC and DISK.DIR for Boeing legacy
// loads, for instance.
struct PlacedFile
{
    MediaFile file;
    std::uint16_t member = 1;
};

// What a person decides about a media set; build_media_set() works out the rest.
struct MediaSetSpec
{
    PartNumber media_set_part_number;
    std::uint8_t number_of_members = 1; // 1 to 255
    std::vector<PlacedLoad> loads;
    std::vector<PlacedBatch> batches;
    std::vector<PlacedFile> other_files;
    // The check value recorded in FILES.LUM for each file it lists, and for
    // FILES.LUM itself; none for no check value.
    CheckValueType file_check_type = CheckValueType::none;
    CheckValueType files_list_check_type = CheckValueType::none;
};

// Makes the members of the media set. Each has LOADS.LUM and FILES.LUM in its
// root, and BATCHES.LUM if the set has a batch, identical but for their media
// sequence number and CRC (section 3.2.3), and the files placed on it.
//
// FILES.LUM lists every file on the set but itself (section 3.2.3.2.13). The
// list files differ from member to member, so each member's LOADS.LUM and
// BATCHES.LUM is listed, with the member it is on. A file's CRC in FILES.LUM is
// the CRC of the whole file; for LOADS.LUM and BATCHES.LUM, which end in their
// own CRC, that is 0x0000.
//
// Fails with Error::invalid_value for a number of members outside 1 to 255, a
// load placed on a member the set does not have, or two files with the same
// name in one directory of a member.
[[nodiscard]] Result<std::vector<MediaMember>> build_media_set(const MediaSetSpec& spec);

// A batch file as read from a media set: its name and what it decodes to.
struct ReadBatch
{
    std::string file_name;
    BatchFile batch;
};

// What reading a media set found.
struct MediaSet
{
    LoadsList loads_list;
    FilesList files_list;
    std::optional<BatchesList> batches_list;
    std::vector<Load> loads;
    std::vector<ReadBatch> batches;
    // Where the set differs from what the standard says it "should" be: the
    // lists, the loads, the batches, and files on the members that FILES.LUM
    // does not list (section 3.2.1). Reading it was not stopped by any of these.
    Findings findings;
};

// Reads a media set from some or all of its members and checks it all the way
// down: the list files in each member's root, and that they agree between
// members; every file FILES.LUM lists on the members given is there with the CRC
// and check value it records; every load in LOADS.LUM, found as section 3.2.4.2
// says, with its header file, data files and support files verified (see
// verify_load()); and every batch.
//
// Members can be left out: files on a member that was not given are not checked,
// and a load that needs one fails with Error::file_not_found.
[[nodiscard]] Result<MediaSet> read_media_set(std::span<const MediaMember> members);

} // namespace adl::arinc665

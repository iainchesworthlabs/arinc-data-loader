// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The three list files in the root of every member of a media set, LOADS.LUM,
// FILES.LUM and BATCHES.LUM (ARINC 665-3 section 3.2.3): a model of each, with
// its encoder, its decoder and its check. How they are laid out over the members
// of a set is in media_set.hpp.

#include "adl/arinc665/check_value.hpp"
#include "adl/arinc665/error.hpp"
#include "adl/arinc665/file_format_version.hpp"
#include "adl/arinc665/finding.hpp"
#include "adl/arinc665/part_number.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace adl::arinc665
{

// Which media set a list file is part of, and which member of it: the Media Set
// PN, the Media Sequence Number (X) and the Number of Media Set Members (Y).
// Members are numbered 1 to 255 and X is at most Y (sections 3.2 and 3.2.3.1.10).
struct MediaSetIdentity
{
    PartNumber media_set_part_number;
    std::uint8_t media_sequence_number = 1;       // X
    std::uint8_t number_of_media_set_members = 1; // Y

    friend bool operator==(const MediaSetIdentity&, const MediaSetIdentity&) = default;
};

// ---- LOADS.LUM: the list of loads (section 3.2.3.1, Table 3.2.3.1) ----

// An entry of LOADS.LUM for each load, and the contents of the file as a whole.
struct LoadsListEntry
{
    PartNumber load_part_number;
    std::string header_file_name;
    std::uint16_t member_sequence_number = 1;     // the member the header file is on
    std::vector<std::string> target_hardware_ids; // the same list as in the header file

    friend bool operator==(const LoadsListEntry&, const LoadsListEntry&) = default;
};

struct LoadsList
{
    FileFormatVersion format_version = FileFormatVersion::media;
    MediaSetIdentity identity;
    std::vector<LoadsListEntry> loads;
    std::vector<std::byte> user_defined_data; // whole 16-bit words; may be empty

    friend bool operator==(const LoadsList&, const LoadsList&) = default;
};

// ---- FILES.LUM: the list of files (section 3.2.3.2, Table 3.2.3.2) ----

// An entry of FILES.LUM for each file, and the contents of the file as a whole.
struct FilesListEntry
{
    std::string file_name;
    std::string pathname; // from the root of the member, "\" for a file in the root
    std::uint16_t member_sequence_number = 1;
    std::uint16_t crc = 0; // the 16-bit CRC of the whole file
    OptionalCheckValue check_value;

    friend bool operator==(const FilesListEntry&, const FilesListEntry&) = default;
};

struct FilesList
{
    FileFormatVersion format_version = FileFormatVersion::media;
    MediaSetIdentity identity;
    std::vector<FilesListEntry> files; // every file on the media set but FILES.LUM
    std::vector<std::byte> user_defined_data;
    // The check value of FILES.LUM itself, over the file but for the check value
    // fields and the CRC; held as read or as set (see build_media_set()).
    OptionalCheckValue check_value;

    friend bool operator==(const FilesList&, const FilesList&) = default;
};

// ---- BATCHES.LUM: the list of batches (section 3.2.3.3, Table 3.2.3.3-1) ----

// An entry of BATCHES.LUM for each batch, and the contents of the file as a whole.
struct BatchesListEntry
{
    PartNumber batch_part_number;
    std::string batch_file_name;
    std::uint16_t member_sequence_number = 1;

    friend bool operator==(const BatchesListEntry&, const BatchesListEntry&) = default;
};

struct BatchesList
{
    FileFormatVersion format_version = FileFormatVersion::media;
    MediaSetIdentity identity;
    std::vector<BatchesListEntry> batches;
    std::vector<std::byte> user_defined_data;

    friend bool operator==(const BatchesList&, const BatchesList&) = default;
};

// Each encoder writes the file with its length, pointers and CRC. It fails with
// Error::invalid_value for a media sequence number that is zero or past the
// number of members, or user defined data that is not whole words, and with
// Error::field_too_large for something that does not fit.
[[nodiscard]] Result<std::vector<std::byte>> encode_loads_list(const LoadsList& list);
[[nodiscard]] Result<std::vector<std::byte>> encode_files_list(const FilesList& list);
[[nodiscard]] Result<std::vector<std::byte>> encode_batches_list(const BatchesList& list);

// Each decoder checks the length, the pointers and the CRC of the file, and
// reads a file of a later format version of the media class as it would this
// one (section 1.4.3).
[[nodiscard]] Result<LoadsList> decode_loads_list(std::span<const std::byte> bytes);
[[nodiscard]] Result<FilesList> decode_files_list(std::span<const std::byte> bytes);
[[nodiscard]] Result<BatchesList> decode_batches_list(std::span<const std::byte> bytes);

// The places a list differs from what the standard says it "should" be.
[[nodiscard]] Findings check_loads_list(const LoadsList& list);
[[nodiscard]] Findings check_files_list(const FilesList& list);
[[nodiscard]] Findings check_batches_list(const BatchesList& list);

// Where in FILES.LUM the check value fields begin, in bytes, so that the check
// value can be calculated over what precedes them. Fails if `bytes` is not a
// FILES.LUM this library can read.
[[nodiscard]] Result<std::size_t> files_list_check_value_offset(std::span<const std::byte> bytes);

} // namespace adl::arinc665

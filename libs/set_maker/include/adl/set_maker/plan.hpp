// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// What a person decides about a media set, as plain data: the plan that the generator's window and the
// command adl_make_set both fill in and hand to make_set(). Everything else (the lengths, the CRCs, the
// check values, the lists) follows from it, and adl::arinc665 works it out.
//
// The words for check value types and load types, and the names that the standard's recommendations and
// the window's habit give a file by default, are here so that the two programs cannot differ.

#include "adl/arinc665/check_value_type.hpp"
#include "adl/arinc665/header_file.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace adl::set_maker
{

enum class FileKind
{
    data,
    support,
};

// One file to read and put into a load.
struct PlannedFile
{
    std::filesystem::path source; // where it is read from; never written into the set
    std::string name;             // its name on the media set
    FileKind kind = FileKind::data;
    adl::arinc665::CheckValueType check = adl::arinc665::CheckValueType::none; // recorded in the header file
};

struct PlannedLoad
{
    std::string part_number;
    std::string header_file_name;
    std::optional<adl::arinc665::LoadType> type;
    std::vector<std::string> target_hardware_ids;
    adl::arinc665::CheckValueType check = adl::arinc665::CheckValueType::none; // the load check value
    std::vector<PlannedFile> files;
};

// The loads one target (a THW_ID_POS) gets in a batch, named by their part numbers.
struct PlannedBatchTarget
{
    std::string id;
    std::vector<std::string> load_part_numbers;
};

struct PlannedBatch
{
    std::string part_number;
    std::string file_name;
    std::string comment;
    std::vector<PlannedBatchTarget> targets;
};

struct SetPlan
{
    std::string part_number;
    unsigned members = 1; // 1 to 255
    // The check value FILES.LUM records for each file it lists, and for FILES.LUM itself.
    adl::arinc665::CheckValueType files_check = adl::arinc665::CheckValueType::none;
    adl::arinc665::CheckValueType files_list_check = adl::arinc665::CheckValueType::none;
    std::vector<PlannedLoad> loads;
    std::optional<PlannedBatch> batch;
};

// The header file's name when none is given: the first three characters of the load part number (the
// manufacturer's code) and .LUH.
[[nodiscard]] std::string default_header_file_name(std::string_view load_part_number);

// The batch file's name when none is given: the first three characters of the batch part number and .LUB.
[[nodiscard]] std::string default_batch_file_name(std::string_view batch_part_number);

// The load's part number without its hyphens, which the standard recommends for the part root
// directory (section 3.2.4.1.1), and the same between backslashes, as a placement takes it.
[[nodiscard]] std::string part_root_name(std::string_view load_part_number);
[[nodiscard]] std::string part_root_directory(std::string_view load_part_number);

// Which loads each target gets when a batch does not say: every distinct target hardware id in the loads,
// in the order it is first met, with the loads that list it, in the order of the loads.
[[nodiscard]] std::vector<PlannedBatchTarget> derive_batch_targets(std::span<const PlannedLoad> loads);

// The load types that the window offers and a description names by a code: the Load Type ID is the system
// designer's to assign, so these are the examples the ARINC 665 world uses.
struct LoadTypeCode
{
    std::string_view code;        // "OPS"
    std::string_view description; // "Operational software": what is written into the header file
    std::uint16_t id;
};

[[nodiscard]] std::span<const LoadTypeCode> load_type_codes() noexcept;
[[nodiscard]] std::optional<adl::arinc665::LoadType> load_type_from_code(std::string_view code);

// A check value type as a word: none, crc8, crc16, crc32, md5 or sha1, and type_<n> for a number that is
// none of them. The words are the description's and the inspection's.
[[nodiscard]] std::string check_type_word(adl::arinc665::CheckValueType type);
[[nodiscard]] std::optional<adl::arinc665::CheckValueType> check_type_from_word(std::string_view word) noexcept;

// Upper-case hexadecimal of bytes, and of a 16-bit value in four digits.
[[nodiscard]] std::string hex_of(std::span<const std::byte> bytes);
[[nodiscard]] std::string hex4_of(std::uint16_t value);

} // namespace adl::set_maker

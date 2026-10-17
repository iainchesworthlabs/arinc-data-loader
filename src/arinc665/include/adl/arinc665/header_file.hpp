#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "adl/arinc665/check_value.hpp"
#include "adl/arinc665/error.hpp"
#include "adl/arinc665/file_format_version.hpp"
#include "adl/arinc665/files.hpp"
#include "adl/arinc665/finding.hpp"
#include "adl/arinc665/part_number.hpp"

namespace adl::arinc665 {

// The Part Flags field (ARINC 665-3 section 2.2.3.1.3): 16 bits, of which only
// the least significant is defined; the other fifteen are spare and written as
// zero.
struct PartFlags {
    // Set for a part whose files tell the loader how to download from the
    // target. Clear, the default, is an upload part.
    bool is_download = false;

    [[nodiscard]] constexpr std::uint16_t bits() const noexcept { return is_download ? 1U : 0U; }

    friend bool operator==(const PartFlags&, const PartFlags&) = default;
};

// A target hardware ID together with the positions it is valid for.
struct TargetHardware {
    std::string id;
    std::vector<std::string> positions;

    friend bool operator==(const TargetHardware&, const TargetHardware&) = default;
};

// The Load Type Description and Load Type ID (sections 2.2.3.1.16 to 2.2.3.1.18).
struct LoadType {
    std::string description;  // for example "EEC Operational Software"
    std::uint16_t id = 0;

    friend bool operator==(const LoadType&, const LoadType&) = default;
};

// The header file of a load (Table 2.2.3-1, Figure C-1), field for field.
//
// The lengths and pointers are worked out when it is written, and the Header
// File CRC is calculated then too, so none of them is a field here. The Load
// Check Value and the Load CRC cover the data and support files as well, so the
// header cannot work them out alone: they are held as read or as set, and
// build_load() (load.hpp) is what calculates them.
struct HeaderFile {
    FileFormatVersion format_version = FileFormatVersion::load;
    PartFlags part_flags;
    PartNumber load_part_number;
    std::optional<LoadType> load_type;  // absent: the pointer to it is 0x0000
    std::vector<std::string> target_hardware_ids;
    // Absent (empty): the load is for every position of the IDs above.
    std::vector<TargetHardware> target_hardware_ids_with_positions;
    std::vector<DataFile> data_files;  // at least one
    std::vector<SupportFile> support_files;
    std::vector<std::byte> user_defined_data;  // whole 16-bit words; may be empty
    OptionalCheckValue load_check_value;
    std::uint32_t load_crc = 0;

    friend bool operator==(const HeaderFile&, const HeaderFile&) = default;
};

// Writes the header file. Fails with Error::invalid_value if there is no data
// file or the user defined data is not whole words, and with
// Error::field_too_large if something does not fit its field.
[[nodiscard]] Result<std::vector<std::byte>> encode_header_file(const HeaderFile& header);

// Reads a header file, checking its length, its pointers, the lengths of the
// files it lists and its Header File CRC. The Load CRC and the Load Check Value
// cover files this function has not got, so they are returned as read; see
// verify_load() (load.hpp).
//
// A file of a later format version of the load class is read as version 0x8004
// is, through its pointers (section 1.4.3), and the fields a later version adds
// at the expansion points are skipped.
[[nodiscard]] Result<HeaderFile> decode_header_file(std::span<const std::byte> bytes);

// The places a header file differs from what the standard says it "should" be:
// part number formats, file names, target hardware IDs, duplicate names.
[[nodiscard]] Findings check_header_file(const HeaderFile& header);

}  // namespace adl::arinc665

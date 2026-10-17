#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include "adl/arinc665/error.hpp"
#include "adl/arinc665/file_format_version.hpp"
#include "adl/arinc665/finding.hpp"
#include "adl/arinc665/part_number.hpp"

namespace adl::arinc665 {

// One load in a batch: its header file's name and its part number.
struct BatchLoad {
    std::string header_file_name;
    PartNumber load_part_number;

    friend bool operator==(const BatchLoad&, const BatchLoad&) = default;
};

// The loads for one position of one piece of target hardware: a Target HW ID
// Load-List block (section 2.3.1).
struct BatchTarget {
    std::string target_hardware_id_position;  // THW_ID_POS as ARINC 615A defines it
    std::vector<BatchLoad> loads;

    friend bool operator==(const BatchTarget&, const BatchTarget&) = default;
};

// A batch file (<name>.LUB, Table 2.3.1-1): a series of loads that a maintainer
// can select in one go, grouped by the target they go to.
struct BatchFile {
    FileFormatVersion format_version = FileFormatVersion::batch;
    PartNumber batch_part_number;
    std::string comment;  // empty: no comment
    std::vector<BatchTarget> targets;

    friend bool operator==(const BatchFile&, const BatchFile&) = default;
};

// Writes the batch file, with its Batch File CRC.
[[nodiscard]] Result<std::vector<std::byte>> encode_batch_file(const BatchFile& batch);

// Reads a batch file, checking its length, its pointers and its CRC.
[[nodiscard]] Result<BatchFile> decode_batch_file(std::span<const std::byte> bytes);

// Part numbers and names the standard says should follow its recommendations.
[[nodiscard]] Findings check_batch_file(const BatchFile& batch);

}  // namespace adl::arinc665

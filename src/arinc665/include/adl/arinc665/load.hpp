#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "adl/arinc665/check_value_type.hpp"
#include "adl/arinc665/error.hpp"
#include "adl/arinc665/finding.hpp"
#include "adl/arinc665/header_file.hpp"
#include "adl/arinc665/part_number.hpp"

namespace adl::arinc665 {

// The bytes of a file together with its name.
struct FileContent {
    std::string name;
    std::vector<std::byte> bytes;

    friend bool operator==(const FileContent&, const FileContent&) = default;
};

// A data or support file to put into a load, and the check value to record for
// it in the header file.
struct LoadFileSpec {
    std::string name;
    PartNumber part_number;
    std::vector<std::byte> content;
    CheckValueType check_type = CheckValueType::none;  // none: record no check value
};

// What a person decides about a load. Everything else in it (the lengths, the
// CRCs, the check values, the pointers) follows from this, and build_load()
// works it out.
struct LoadSpec {
    std::string header_file_name;  // for example "ACM47.LUH"
    PartFlags part_flags;
    PartNumber part_number;
    std::optional<LoadType> load_type;
    std::vector<std::string> target_hardware_ids;
    std::vector<TargetHardware> target_hardware_ids_with_positions;
    std::vector<LoadFileSpec> data_files;     // at least one
    std::vector<LoadFileSpec> support_files;  // for example CONFIG.LDR
    std::vector<std::byte> user_defined_data;
    CheckValueType load_check_type = CheckValueType::none;  // none: record no load check value
};

// A complete software load: the header file as it is written, the model it
// decodes to, and the data and support files in the order the header lists them.
struct Load {
    std::string header_file_name;
    HeaderFile header;
    std::vector<std::byte> header_bytes;
    std::vector<FileContent> data_files;
    std::vector<FileContent> support_files;
};

// Makes the load: records each file's length, CRC and check value in the header,
// works out the Load Check Value and then the Load CRC, which depend on
// everything before them, and writes the header file.
//
// What the Load Check Value and the Load CRC cover is described in sections
// 2.2.3.1.61 and 2.2.3.1.63 but not in what order the files are taken. This
// takes the header file first, then the data files, then the support files, each
// in the order the header lists them. The Load Check Value stops short of the
// Load Check Value fields and the CRCs; the Load CRC covers the header file up
// to its own last two words.
[[nodiscard]] Result<Load> build_load(const LoadSpec& spec);

// Checks a load end to end: the header file decodes (its length, pointers and
// Header File CRC are right), every listed file is there with the length, CRC
// and check value the header records, and the Load Check Value and the Load CRC
// are what the whole load works out to. A check value of a type this library
// does not know is not checked.
[[nodiscard]] Result<void> verify_load(const Load& load);

// The places a load differs from what the standard says it "should" be, in the
// header file and in the header file's name.
[[nodiscard]] Findings check_load(const Load& load);

}  // namespace adl::arinc665

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// What a header file records about each file of a load: the data files, the
// support files, and the two ARINC 615 configuration files, which are support
// files with names of their own. These hold the name, part number, length and
// check values of a file, not its bytes (see FileContent in load.hpp).

#include "adl/arinc665/check_value.hpp"
#include "adl/arinc665/part_number.hpp"

#include <cstdint>
#include <string>

namespace adl::arinc665
{

// A file supplied by whoever made the software, as the header file lists it.
struct UserSuppliedFile
{
    std::string file_name;
    PartNumber part_number;
    std::uint64_t length = 0; // in 8-bit bytes
    std::uint16_t crc = 0;    // the 16-bit CRC of the whole file
    OptionalCheckValue check_value;

    friend bool operator==(const UserSuppliedFile&, const UserSuppliedFile&) = default;
};

// A file of the software itself. It is a whole number of 16-bit words, or the
// header file's Length in Bytes says how many bytes of the last word count.
struct DataFile : UserSuppliedFile
{
    friend bool operator==(const DataFile&, const DataFile&) = default;
};

// A file that describes or supports the load without being part of the software.
struct SupportFile : UserSuppliedFile
{
    friend bool operator==(const SupportFile&, const SupportFile&) = default;
};

// The files ARINC 615 adds to a load. Both are support files: ARINC 665-3
// section 2.2.3.3 says a load for the 615 protocol carries CONFIG.LDR as a
// support file, and Appendix F lists EXCONFIG.LDR beside it.
namespace arinc615
{

// CONFIG.LDR.
struct ConfigLdrFile : SupportFile
{
    friend bool operator==(const ConfigLdrFile&, const ConfigLdrFile&) = default;
};

// EXCONFIG.LDR.
struct ExcConfigLdrFile : SupportFile
{
    friend bool operator==(const ExcConfigLdrFile&, const ExcConfigLdrFile&) = default;
};

} // namespace arinc615

} // namespace adl::arinc665

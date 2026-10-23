#pragma once

// The check value types of ARINC 665 Table 5.1, with their titles and the number
// of bytes each takes in a file. The enumerators have the numbers that are
// written to the files.

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace adl::arinc665 {

// The check value enumeration of ARINC 665 section 5.2 (Table 5.1): which
// calculation a file's, or a load's, check value was made with. The values are
// the ones written to the file.
enum class CheckValueType : std::uint16_t {
    none = 0,
    crc8 = 1,
    crc16 = 2,
    crc32 = 3,
    md5 = 4,
    sha1 = 5,
};

// The title column of Table 5.1. "none" is not in the table: it is what the
// standard writes when a file has no check value.
[[nodiscard]] constexpr std::string_view to_string(CheckValueType type) noexcept {
    switch (type) {
        case CheckValueType::none:
            return "none";
        case CheckValueType::crc8:
            return "8-bit CRC";
        case CheckValueType::crc16:
            return "16-bit CRC";
        case CheckValueType::crc32:
            return "32-bit CRC";
        case CheckValueType::md5:
            return "MD5";
        case CheckValueType::sha1:
            return "SHA-1";
    }
    return "unknown";
}

// The length column of Table 5.1: how many bytes the check value takes in the
// file. A value is stored in whole 16-bit words, so an 8-bit CRC takes two.
[[nodiscard]] constexpr std::size_t check_value_length(CheckValueType type) noexcept {
    switch (type) {
        case CheckValueType::none:
            return 0;
        case CheckValueType::crc8:
        case CheckValueType::crc16:
            return 2;
        case CheckValueType::crc32:
            return 4;
        case CheckValueType::md5:
            return 16;
        case CheckValueType::sha1:
            return 20;
    }
    return 0;
}

}  // namespace adl::arinc665

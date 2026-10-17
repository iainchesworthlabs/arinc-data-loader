#pragma once

#include <cstdint>

namespace adl::arinc665 {

// The file format version field, as ARINC 665-3 section 1.4.1 assigns it to
// each of the three kinds of file that carry one.
enum class FileFormatVersion : std::uint16_t {
    load = 0x8004,
    batch = 0x9004,
    media = 0xA004,
};

}  // namespace adl::arinc665

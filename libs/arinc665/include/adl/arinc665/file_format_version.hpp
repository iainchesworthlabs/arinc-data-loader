// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The values of the file format version field that ARINC 665-3 gives to its load,
// batch and media files. A later version of a class is read through the file's
// pointers (section 1.4.3); see header_file.hpp and media_files.hpp.

#include <cstdint>

namespace adl::arinc665
{

// The file format version field, as ARINC 665-3 section 1.4.1 assigns it to
// each of the three kinds of file that carry one.
enum class FileFormatVersion : std::uint16_t
{
    load = 0x8004,
    batch = 0x9004,
    media = 0xA004,
};

} // namespace adl::arinc665

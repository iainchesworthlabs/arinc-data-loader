#pragma once

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

#include "adl/arinc665/batch_file.hpp"
#include "adl/arinc665/error.hpp"
#include "adl/arinc665/load.hpp"
#include "adl/arinc665/media_set.hpp"

namespace adl::samples {

// ---- The scenario of tests/golden/make_golden.py, input for input ----
// Change one and the script has to change with it.

inline constexpr const char* sample_load_part_number = "ACM47-1234-5678";  // Appendix E's example
inline constexpr const char* sample_batch_part_number = "ACM4C-0100-0200";
inline constexpr const char* sample_media_set_part_number = "ACM-SET-0001";

[[nodiscard]] std::vector<std::byte> sample_data_1();
[[nodiscard]] std::vector<std::byte> sample_data_2();  // five bytes: three words, one half full
[[nodiscard]] std::vector<std::byte> sample_support();

// Every part of the header file is present: a load type, target hardware with
// positions, two data files (one with a check value, one of odd length), a
// support file with an MD5, user defined data and a Load Check Value.
[[nodiscard]] arinc665::LoadSpec sample_load_spec();
[[nodiscard]] arinc665::Result<arinc665::Load> sample_load();
[[nodiscard]] arinc665::BatchFile sample_batch();

// Two members: the header file and application on the first, the data file on the
// second (in a subdirectory), a batch on the second and a stray file on the first.
[[nodiscard]] arinc665::Result<arinc665::MediaSetSpec> sample_media_set_spec();

// ---- The layouts of Appendix F, which other loaders can load too ----

// F-1: an ARINC 615 part, with a directory for each disk. CONFIG.LDR and
// EXCONFIG.LDR are support files; the two disks have a LOAD.BIN each.
[[nodiscard]] arinc665::Result<arinc665::MediaSetSpec> arinc615_media_set_spec();

// F-2: a Boeing 777 (ARINC 629) load. Its legacy header file is a support file,
// and DISK.DIR and NON_LOAD.CRC are on the media set but in no header file.
[[nodiscard]] arinc665::Result<arinc665::MediaSetSpec> boeing777_media_set_spec();

// F-3: both at once.
[[nodiscard]] arinc665::Result<arinc665::MediaSetSpec> combined_media_set_spec();

// A sample media set by name, for the program that writes them.
struct Scenario {
    std::string_view name;         // also the name of its directory
    std::string_view description;  // one line
    arinc665::Result<arinc665::MediaSetSpec> (*make)();
};

[[nodiscard]] std::span<const Scenario> scenarios();

}  // namespace adl::samples

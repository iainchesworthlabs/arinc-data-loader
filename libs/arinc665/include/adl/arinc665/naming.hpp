#pragma once

// The names ARINC 665-3 gives to the files of a media set, and checks of file
// names, pathnames and target hardware IDs against what it recommends. The checks
// return Findings: a name that departs from the standard can still be read.

#include <string_view>

#include "adl/arinc665/finding.hpp"

namespace adl::arinc665 {

// The file name extensions of ARINC 665-3 and the standards around it.
inline constexpr std::string_view header_file_extension = "LUH";  // 2.2.2.1
inline constexpr std::string_view data_file_extension = "LUP";    // 2.2.2.2, recommended
inline constexpr std::string_view batch_file_extension = "LUB";   // 2.3.1

// The names the standard gives the three lists in the root of every media
// member (section 3.2.1).
inline constexpr std::string_view loads_list_file_name = "LOADS.LUM";
inline constexpr std::string_view files_list_file_name = "FILES.LUM";
inline constexpr std::string_view batches_list_file_name = "BATCHES.LUM";

// The ARINC 615 loader configuration files, which a load for the 615 protocol
// carries as support files (section 2.2.3.3, Appendix F).
inline constexpr std::string_view config_ldr_file_name = "CONFIG.LDR";
inline constexpr std::string_view exconfig_ldr_file_name = "EXCONFIG.LDR";

// The part of `name` after its last '.', or empty if it has none.
[[nodiscard]] std::string_view file_extension(std::string_view name) noexcept;

// Whether `extension` (without the dot) is one of those Table 3.2.2-1 reserves
// for a particular use.
[[nodiscard]] bool is_reserved_extension(std::string_view extension) noexcept;

// Section 2.2.2: at most 255 characters, printable, none of "~", "/", ":", "\"
// or a blank, and not "." or "..".
//
// The standard's list goes on to name "I", "O" and "Q" in the copy this was
// written from. Those cannot be meant for file names, since CONFIG.LDR (which
// the same standard prescribes) contains two of them, so they are not checked.
[[nodiscard]] Findings check_file_name(std::string_view name);

// A name in the header file or in LOADS.LUM: the above, the extension LUH, and a
// manufacturer's code for the first three characters.
[[nodiscard]] Findings check_header_file_name(std::string_view name);

// A data file name: the above, and none of the extensions Table 3.2.2-1
// reserves, except LUP, which is the recommended one.
[[nodiscard]] Findings check_data_file_name(std::string_view name);

// A support file name: the same, but LDR and HDR are allowed because the
// standard lists CONFIG.LDR, EXCONFIG.LDR and a Boeing header file as support
// files (section 2.2.3.3, Appendix F).
[[nodiscard]] Findings check_support_file_name(std::string_view name);

// A batch file name: the above, and the extension LUB.
[[nodiscard]] Findings check_batch_file_name(std::string_view name);

// A File Pathname in FILES.LUM (section 3.2.3.2.18): it starts at the root of
// the member, with a backslash, and ends with one, so a file in the root has
// "\". Directories are at most four deep (section 3.2.1).
[[nodiscard]] Findings check_pathname(std::string_view pathname);

// Section 1.5: four hexadecimal characters (an ARINC 429 equipment code), or 4
// to 15 characters beginning with a manufacturer's code.
[[nodiscard]] Findings check_target_hardware_id(std::string_view id);

}  // namespace adl::arinc665

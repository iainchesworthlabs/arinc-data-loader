#pragma once

// The directory a download is written into: DNLD_DATA_<THW_ID_POS>_<number> under the operator's
// download directory (ARINC 615A-3 5.4.4.3), made through adl::platform::FileSystem so that the
// tests can run it against files in memory and make the file system misbehave. Nothing here is
// Qt but the text that is shown to the operator.

#include <expected>
#include <string>
#include <string_view>

#include <QString>

#include "adl/app/message.hpp"
#include "adl/platform/files.hpp"

namespace adl::app {

// The most numbered directories a download looks through before it gives up.
inline constexpr unsigned max_download_directories = 10'000;

// "DNLD_DATA_HNPFMS_L_3".
[[nodiscard]] std::string download_directory_name(std::string_view thw_id_pos, unsigned number);

// Whether a download from the target called `thw_id_pos` can be written: the loader takes the
// id (adl::arinc615a::is_valid_thw_id_pos), and the directory named after it is a name every
// platform can make, so a colon or a trailing dot in the id is not carried into a path.
// Checked before anything is made on the disk.
[[nodiscard]] bool can_download_from(std::string_view thw_id_pos);

// Makes `path` a directory, creating the directories on the way: the platform's make_directory
// wants the parent to exist, and the operator's download directory may be a path that is not
// there yet (the default Documents/ArincDownloads is made on demand). A directory that is there
// is left as it is. On failure, what to tell the operator.
[[nodiscard]] std::expected<void, Message> make_directory_chain(adl::platform::FileSystem& files,
                                                                std::string_view path);

// Makes the directory of a download under `directory` (which is created if it is not there):
// the one whose <number> is the lowest that does not exist yet, starting at 1. Two copies of the
// loader may look for a free number at once, so a number that is taken between looking and
// making is not an error: the next one is tried. Returns the platform path (UTF-8, '/'
// separated) of the new directory, or what to tell the operator.
[[nodiscard]] std::expected<std::string, Message> make_download_directory(
    adl::platform::FileSystem& files, std::string_view directory, std::string_view thw_id_pos);

// Removes `path` if it is a directory with nothing in it, and says whether it did. A directory
// that holds anything, however little, and anything that is not a directory are left alone.
[[nodiscard]] bool remove_if_empty(adl::platform::FileSystem& files, std::string_view path);

}  // namespace adl::app

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The directory a download is written into: DNLD_DATA_<THW_ID_POS>_<number> under the operator's
// download directory (ARINC 615A-3 5.4.4.3), made through adl::platform::FileSystem so that the
// tests can run it against files in memory and make the file system misbehave. A failure comes back
// as a DirectoryError, which says what went wrong and where and leaves the wording to the caller;
// describe() puts it in English.

#include "adl/platform/files.hpp"

#include <expected>
#include <string>
#include <string_view>

namespace adl::loader_core
{

// The most numbered directories a download looks through before it gives up.
inline constexpr unsigned max_download_directories = 10'000;

// "DNLD_DATA_HNPFMS_L_3".
[[nodiscard]] std::string download_directory_name(std::string_view thw_id_pos, unsigned number);

// Whether a download from the target called `thw_id_pos` can be written: the loader takes the
// id (adl::arinc615a::is_valid_thw_id_pos), and the directory named after it is a name every
// platform can make, so a colon or a trailing dot in the id is not carried into a path.
// Checked before anything is made on the disk.
[[nodiscard]] bool can_download_from(std::string_view thw_id_pos);

// Why a directory could not be made.
struct DirectoryError
{
    enum class Kind
    {
        not_a_directory, // `path` is there and is not a directory
        cannot_look_at,  // looking at `path` failed with `code`
        cannot_create,   // making `path` failed with `code`
        too_many         // `path` already holds max_download_directories directories for `thw_id_pos`
    };

    Kind kind = Kind::not_a_directory;
    std::string path;
    adl::platform::ErrorCode code = adl::platform::ErrorCode::other; // cannot_look_at, cannot_create
    std::string thw_id_pos;                                          // too_many

    // "/data/file is not a folder", "cannot create /dl/DNLD_DATA_T_1 (no space)": the same words the
    // window says, in English.
    [[nodiscard]] std::string describe() const;
};

// Makes `path` a directory, creating the directories on the way: the platform's make_directory
// wants the parent to exist, and the operator's download directory may be a path that is not
// there yet (the default Documents/ArincDownloads is made on demand). A directory that is there
// is left as it is.
[[nodiscard]] std::expected<void, DirectoryError> make_directory_chain(adl::platform::FileSystem& files,
                                                                       std::string_view path);

// Makes the directory of a download under `directory` (which is created if it is not there):
// the one whose <number> is the lowest that does not exist yet, starting at 1. Two copies of the
// loader may look for a free number at once, so a number that is taken between looking and
// making is not an error: the next one is tried. Returns the platform path (UTF-8, '/'
// separated) of the new directory.
[[nodiscard]] std::expected<std::string, DirectoryError>
make_download_directory(adl::platform::FileSystem& files, std::string_view directory, std::string_view thw_id_pos);

// Removes `path` if it is a directory with nothing in it, and says whether it did. A directory
// that holds anything, however little, and anything that is not a directory are left alone.
[[nodiscard]] bool remove_if_empty(adl::platform::FileSystem& files, std::string_view path);

} // namespace adl::loader_core

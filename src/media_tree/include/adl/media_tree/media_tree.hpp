#pragma once

// A media set written to a directory tree and read back: one directory per member
// under a root, and below it the directories that the files' pathnames name. File
// names are limited to ones every platform accepts, so that a tree written on one
// can be read on another. Failures are returned, not thrown.

#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "adl/arinc665/media_set.hpp"

namespace adl::media_tree {

// Why writing or reading a tree failed. Failures are returned, never thrown, as
// in adl::arinc665.
enum class TreeError {
    no_members,         // nothing to write, or no member directory to read
    invalid_member,     // a sequence number outside 1 to 255, or given twice
    invalid_pathname,   // a pathname that is not "\", "\dir\", ... or has a bad directory name
    invalid_file_name,  // a name that cannot be a file on every platform
    duplicate_file,     // two files of one name in one directory of a member
    already_exists,     // a member directory that is there already and not empty
    cannot_create_directory,
    cannot_write_file,
    cannot_read_file,
    not_a_directory,   // the root is missing, or is a file
    unexpected_entry,  // something in the tree that is not a member, directory or plain file
};

// The error, and the path (native, absolute or as given) it concerns; for a
// pathname or a file name that is not valid, the offending text as given.
struct TreeFailure {
    TreeError error = TreeError::no_members;
    std::filesystem::path path;
};

template <typename T>
using TreeResult = std::expected<T, TreeFailure>;

[[nodiscard]] std::string_view to_string(TreeError error) noexcept;

// The directory of a member under the root of a tree: "member_001" for sequence
// number 1 and so on up to "member_255".
[[nodiscard]] std::string member_directory_name(std::uint16_t sequence_number);

// Maps a pathname of the standard ("\", "\disk001\") to a path below `base`:
// each directory in turn, with the separator of this platform. Fails with
// TreeError::invalid_pathname for a pathname that does not start and end with a
// backslash, has an empty directory name, or names one that is not a plain
// directory name (see below).
[[nodiscard]] TreeResult<std::filesystem::path> native_directory(const std::filesystem::path& base,
                                                                 std::string_view pathname);

// Writes each member to `root` / member_directory_name(sequence number), and
// each file of a member to the directory its pathname names below that, creating
// directories as needed. `root` is created if it is missing.
//
// A name has to be usable on every platform, so that a tree written here can be
// read on any: printable ASCII, none of the characters / \ : * ? " < > |, not
// "." or "..", not ending in a dot or a space, and not a device name (CON, NUL,
// COM1, ...; with or without an extension). Two names that differ only in case
// count as one, as they are on Windows and macOS.
//
// Everything is checked before anything is written, so a member that cannot be
// written does not leave the others behind, and nothing is ever overwritten: a
// member directory that exists and is not empty is an error.
[[nodiscard]] TreeResult<void> write_media_set_tree(const std::filesystem::path& root,
                                                    std::span<const arinc665::MediaMember> members);

// Reads the members under `root`, ordered by sequence number, with the files of
// each ordered by pathname and name. Every plain file below a member directory is
// a file of that member, whatever its name; empty directories leave nothing.
// A root entry other than a member directory, and anything below one that is
// neither a directory nor a plain file (a link, for instance), is refused rather
// than followed.
//
// What it reads can be given to adl::arinc665::read_media_set().
[[nodiscard]] TreeResult<std::vector<arinc665::MediaMember>> read_media_set_tree(
    const std::filesystem::path& root);

}  // namespace adl::media_tree

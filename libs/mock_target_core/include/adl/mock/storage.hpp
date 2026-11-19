// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// What the mock target does with folders: it reads the files it holds for a download and it
// writes the files an upload left in it. Plain C++ over std::filesystem, so the window's model and
// a program with no window do exactly the same, and report what went wrong in their own words (the
// window translates it; a command writes it as it is).

#include "adl/mock/engine.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace adl::mock
{

// Whether a file the loader sent can be stored under `name`: a plain name (letters, digits and
// . _ -) that every platform takes for the same file, so a name from the wire cannot reach
// outside the storage, name a stream, or open a device.
[[nodiscard]] bool is_storable_name(std::string_view name) noexcept;

// The directory a load is stored in, made from its part number: the same characters, anything
// else replaced by '_'; never a name that is a device or ends in a dot.
[[nodiscard]] std::string storage_directory_name(std::string_view part_number);

// What went wrong when an upload was stored. The target keeps going after any of these; each is
// one thing the operator should be told.
enum class StoreProblem : std::uint8_t
{
    no_storage_folder,     // there is no storage folder, so nothing is stored
    storage_folder_failed, // the storage folder could not be made; `detail` says why
    load_folder_failed,    // the load's folder could not be made; `detail` says why
    unsafe_file_name,      // `subject` is not a name that is safe to store; the file was skipped
    file_write_failed,     // `subject` could not be written
};

struct StoreIssue
{
    StoreProblem problem = StoreProblem::no_storage_folder;
    std::string subject; // the file name, for the two problems about a file
    std::string detail;  // the operating system's words, for the two problems about a folder
};

// A file the upload left on disk.
struct StoredOnDisk
{
    std::string name;
    std::uint64_t size = 0;
    std::filesystem::path path;
};

struct StoreOutcome
{
    std::vector<StoredOnDisk> stored;
    std::vector<StoreIssue> issues;
};

// Writes the files of a completed upload to `storage` / storage_directory_name(part number) /
// <file name>. An empty `storage` means there is no folder: nothing is written and the one issue
// is no_storage_folder. A file with an unsafe name is skipped and reported; the others are still
// written.
[[nodiscard]] StoreOutcome store_upload(const std::filesystem::path& storage, const UploadRecord& record);

// The regular files directly in `folder`, by name, with their contents, for a download to serve.
// An error (with the operating system's words) when `folder` is not a folder or cannot be listed.
// A folder that changes or fails while it is being read ends the list at that point; it does not
// fail the call.
[[nodiscard]] std::expected<std::vector<HeldFile>, std::string> read_held_files(const std::filesystem::path& folder);

// The one file a target holds when it has been given none: DEMO.TXT, with a line saying where
// the files a download serves come from.
[[nodiscard]] HeldFile demonstration_file();

} // namespace adl::mock

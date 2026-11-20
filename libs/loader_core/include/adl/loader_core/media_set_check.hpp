// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The check of a media set on a disk: reads it the way the loader does (a directory with one
// directory per member, as adl::media_tree writes them), has adl::arinc665 verify every file and its
// check values, and reports what the set holds and where it departs from the standard's
// recommendations.
//
// A set that reads is structurally valid: the read verifies every file and its check values, so a read
// error is what makes a set invalid and nothing else does. The findings are deviations from the
// standard's recommendations; they are warnings and do not stop an upload.

#include "adl/loader_core/media_source.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace adl::loader_core
{

// One load of a media set.
struct LoadReport
{
    std::string part_number;
    std::string header_file;
    std::size_t data_files = 0;
    std::size_t support_files = 0;
    std::vector<std::string> target_hardware_ids;
};

// One file of a media set, as FILES.LUM lists it.
struct FileReport
{
    std::string name;
    std::string pathname;     // from the root of the member, as FILES.LUM writes it ("\" for the root)
    std::uint16_t member = 0; // the member the file is on
    std::string crc16;        // the file's 16-bit CRC, four hexadecimal digits
};

// What reading a media set found. `error` is set, and the rest empty, when it could not be read.
struct MediaSetReport
{
    std::string error;
    std::string part_number;
    unsigned members = 0;
    std::size_t batches = 0;
    std::vector<LoadReport> loads;
    std::vector<FileReport> files;     // every file of the set but FILES.LUM, from FILES.LUM
    std::vector<std::string> findings; // "<clause>: <message>"
};

// Reads and checks the media set under `root`. Blocks while it reads. A set that cannot be read comes
// back with `error` set. With `index`, also makes the index of the set's files that an upload reads
// them through (media_source.hpp), with the fingerprint of every file as it was read; `index` is left
// empty when the set cannot be read or indexed.
[[nodiscard]] MediaSetReport check_media_set(const std::filesystem::path& root,
                                             std::shared_ptr<const MediaSetIndex>* index = nullptr);

} // namespace adl::loader_core

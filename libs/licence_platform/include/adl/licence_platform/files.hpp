// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The store's file interface (adl/licence/store.hpp) over the platform's FileSystem. A path is the text the
// FileSystem takes: UTF-8, '/' between a folder and a name.

#include "adl/licence/store.hpp"
#include "adl/platform/files.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace adl::licence_platform
{

class PlatformFiles final : public licence::store::Files
{
public:
    // `files` has to outlive this object.
    explicit PlatformFiles(platform::FileSystem& files) noexcept : files_(files) {}

    // The regular files of the folder. A folder that is not there is `not_found`, and so is a name in the
    // path that is a file.
    licence::store::FilesResult<std::vector<std::string>> list(std::string_view folder) override;

    licence::store::FilesResult<std::string> read(std::string_view path, std::size_t max_bytes) override;

    // Makes the folders it needs, and writes the file under a name of its own with `exclusive`, so that a
    // name taken in the meantime is `already_exists` and nothing is overwritten. A file that was not written
    // whole is removed.
    licence::store::FilesResult<void> create(std::string_view path, std::string_view content) override;

    // Writes `<path>.tmp` and renames it over `path`, so that a reader sees the old file or the new one.
    // The temporary file is removed when the write fails.
    licence::store::FilesResult<void> replace(std::string_view path, std::string_view content) override;

    licence::store::FilesResult<void> remove(std::string_view path) override;

private:
    platform::FileSystem& files_;
};

} // namespace adl::licence_platform

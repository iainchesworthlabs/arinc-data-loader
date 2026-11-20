// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/licence_platform/files.hpp"

#include <algorithm>
#include <span>
#include <utility>

namespace adl::licence_platform
{

namespace
{

using licence::store::FilesError;
using licence::store::FilesResult;

[[nodiscard]] FilesError map(const platform::Error& error) noexcept
{
    switch (error.code)
    {
    case platform::ErrorCode::not_found:
        return FilesError::not_found;
    case platform::ErrorCode::already_exists:
        return FilesError::already_exists;
    default:
        return FilesError::failed;
    }
}

// The folder a path is in: everything before its last '/'. Empty for a name with no folder or for one in the
// root.
[[nodiscard]] std::string_view parent_of(std::string_view path) noexcept
{
    const auto slash = path.rfind('/');
    return slash == std::string_view::npos ? std::string_view{} : path.substr(0, slash);
}

// Makes the folder holding `path` and the ones above it that are missing. A folder that is there already, or
// that appears while this runs, is not a fault.
[[nodiscard]] FilesResult<void> make_folders_for(platform::FileSystem& files, std::string_view path)
{
    std::vector<std::string_view> missing;
    for (std::string_view folder = parent_of(path); !folder.empty(); folder = parent_of(folder))
    {
        const auto info = files.stat(folder);
        if (info)
        {
            if (info->kind != platform::FileKind::directory)
            {
                return std::unexpected(FilesError::failed);
            }
            break;
        }
        if (info.error().code != platform::ErrorCode::not_found)
        {
            return std::unexpected(FilesError::failed);
        }
        missing.push_back(folder);
    }
    for (auto folder = missing.rbegin(); folder != missing.rend(); ++folder)
    {
        const auto made = files.make_directory(*folder);
        if (!made && made.error().code != platform::ErrorCode::already_exists)
        {
            return std::unexpected(FilesError::failed);
        }
    }
    return {};
}

// Writes `content` to a new file, and takes the file away again if any step fails.
[[nodiscard]] FilesResult<void> write_new(platform::FileSystem& files, std::string_view path, platform::CreateMode mode,
                                          std::string_view content)
{
    auto file = files.create(path, mode);
    if (!file)
    {
        return std::unexpected(map(file.error()));
    }
    const auto bytes = std::as_bytes(std::span{content.data(), content.size()});
    auto done = (*file)->write(bytes);
    if (done)
    {
        done = (*file)->sync();
    }
    if (done)
    {
        done = (*file)->close();
    }
    if (!done)
    {
        file->reset();
        (void)files.remove(path);
        return std::unexpected(FilesError::failed);
    }
    return {};
}

} // namespace

FilesResult<std::vector<std::string>> PlatformFiles::list(std::string_view folder)
{
    std::vector<std::string> names;
    const auto listed = platform::for_each_entry(files_, folder,
                                                 [&names](std::string_view name, platform::FileKind kind)
                                                 {
                                                     if (kind == platform::FileKind::regular)
                                                     {
                                                         names.emplace_back(name);
                                                     }
                                                     return true;
                                                 });
    if (!listed)
    {
        // A name in the path that is a file is as good as no folder.
        return std::unexpected(listed.error().code == platform::ErrorCode::not_a_directory ? FilesError::not_found
                                                                                           : map(listed.error()));
    }
    return names;
}

FilesResult<std::string> PlatformFiles::read(std::string_view path, std::size_t max_bytes)
{
    auto file = files_.open_read(path);
    if (!file)
    {
        return std::unexpected(map(file.error()));
    }
    const auto size = (*file)->size();
    if (!size)
    {
        return std::unexpected(FilesError::failed);
    }
    if (*size > max_bytes)
    {
        return std::unexpected(FilesError::too_large);
    }
    std::string content(static_cast<std::size_t>(*size), '\0');
    std::size_t done = 0;
    while (done < content.size())
    {
        const auto got = (*file)->read_at(done, std::as_writable_bytes(std::span{content}.subspan(done)));
        if (!got)
        {
            return std::unexpected(FilesError::failed);
        }
        if (*got == 0)
        {
            break;
        }
        done += *got;
    }
    content.resize(done);
    return content;
}

FilesResult<void> PlatformFiles::create(std::string_view path, std::string_view content)
{
    if (auto folders = make_folders_for(files_, path); !folders)
    {
        return folders;
    }
    return write_new(files_, path, platform::CreateMode::exclusive, content);
}

FilesResult<void> PlatformFiles::replace(std::string_view path, std::string_view content)
{
    if (auto folders = make_folders_for(files_, path); !folders)
    {
        return folders;
    }
    const std::string temporary = std::string{path} + ".tmp";
    if (auto written = write_new(files_, temporary, platform::CreateMode::replace, content); !written)
    {
        return written;
    }
    if (const auto moved = files_.rename(temporary, path); !moved)
    {
        (void)files_.remove(temporary);
        return std::unexpected(FilesError::failed);
    }
    return {};
}

FilesResult<void> PlatformFiles::remove(std::string_view path)
{
    if (const auto removed = files_.remove(path); !removed)
    {
        return std::unexpected(map(removed.error()));
    }
    return {};
}

} // namespace adl::licence_platform

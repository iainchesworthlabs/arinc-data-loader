// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/loader_core/download_directory.hpp"

#include "adl/arinc615a/data_loader.hpp"
#include "adl/arinc665/portable_name.hpp"

namespace adl::loader_core
{

namespace
{

using adl::platform::ErrorCode;

std::unexpected<DirectoryError> failure(DirectoryError::Kind kind, std::string_view path,
                                        ErrorCode code = ErrorCode::other, std::string_view thw_id_pos = {})
{
    return std::unexpected(DirectoryError{kind, std::string{path}, code, std::string{thw_id_pos}});
}

} // namespace

std::string DirectoryError::describe() const
{
    switch (kind)
    {
    case Kind::not_a_directory:
        return path + " is not a folder";
    case Kind::cannot_look_at:
        return "cannot look at " + path + " (" + adl::platform::to_string(code) + ")";
    case Kind::cannot_create:
        return "cannot create " + path + " (" + adl::platform::to_string(code) + ")";
    case Kind::too_many:
        return "there are already " + std::to_string(max_download_directories) + " download folders for " + thw_id_pos +
               " in " + path;
    }
    return {};
}

std::string download_directory_name(std::string_view thw_id_pos, unsigned number)
{
    return "DNLD_DATA_" + std::string{thw_id_pos} + "_" + std::to_string(number);
}

bool can_download_from(std::string_view thw_id_pos)
{
    return adl::arinc615a::is_valid_thw_id_pos(thw_id_pos) &&
           adl::arinc665::is_portable_file_name(download_directory_name(thw_id_pos, 1));
}

std::expected<void, DirectoryError> make_directory_chain(adl::platform::FileSystem& files, std::string_view path)
{
    const auto existing = files.stat(path);
    if (existing)
    {
        if (existing->kind != adl::platform::FileKind::directory)
        {
            return failure(DirectoryError::Kind::not_a_directory, path);
        }
        return {};
    }
    if (existing.error().code != ErrorCode::not_found)
    {
        return failure(DirectoryError::Kind::cannot_look_at, path, existing.error().code);
    }
    const std::size_t cut = path.rfind('/');
    if (cut != std::string_view::npos && cut != 0)
    {
        if (auto parent = make_directory_chain(files, path.substr(0, cut)); !parent)
        {
            return parent;
        }
    }
    const auto made = files.make_directory(path);
    if (!made)
    {
        if (made.error().code == ErrorCode::already_exists)
        {
            // Made by someone else since the look above: as good as made here, if a directory.
            const auto now = files.stat(path);
            if (now && now->kind == adl::platform::FileKind::directory)
            {
                return {};
            }
        }
        return failure(DirectoryError::Kind::cannot_create, path, made.error().code);
    }
    return {};
}

std::expected<std::string, DirectoryError>
make_download_directory(adl::platform::FileSystem& files, std::string_view directory, std::string_view thw_id_pos)
{
    if (auto chain = make_directory_chain(files, directory); !chain)
    {
        return std::unexpected(chain.error());
    }
    for (unsigned number = 1; number <= max_download_directories; ++number)
    {
        const std::string path = std::string{directory} + "/" + download_directory_name(thw_id_pos, number);
        const auto taken = files.stat(path);
        if (taken)
        {
            continue; // whatever it is, the number is not free
        }
        if (taken.error().code != ErrorCode::not_found)
        {
            return failure(DirectoryError::Kind::cannot_look_at, path, taken.error().code);
        }
        const auto made = files.make_directory(path);
        if (made)
        {
            return path;
        }
        if (made.error().code == ErrorCode::already_exists)
        {
            continue; // another loader took this number since the look: the next one
        }
        return failure(DirectoryError::Kind::cannot_create, path, made.error().code);
    }
    return failure(DirectoryError::Kind::too_many, directory, ErrorCode::other, thw_id_pos);
}

bool remove_if_empty(adl::platform::FileSystem& files, std::string_view path)
{
    const auto info = files.stat(path);
    if (!info || info->kind != adl::platform::FileKind::directory)
    {
        return false;
    }
    bool empty = true;
    const auto listed = adl::platform::for_each_entry(files, path,
                                                      [&](std::string_view, auto)
                                                      {
                                                          empty = false;
                                                          return false;
                                                      });
    return listed && empty && files.remove(path).has_value();
}

} // namespace adl::loader_core

// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
#include "files.hpp"

#include "messages.hpp"

#include <fstream>
#include <ios>
#include <string>
#include <system_error>

namespace adl::licence_owner
{

namespace fs = std::filesystem;

std::expected<std::string, std::string> read_file(const fs::path& path, std::size_t max_size)
{
    std::error_code error;
    const fs::file_status status = fs::status(path, error);
    if (error || !fs::exists(status))
    {
        return std::unexpected(std::string(message::reason_missing));
    }
    if (!fs::is_regular_file(status))
    {
        return std::unexpected(std::string(message::reason_not_a_file));
    }
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        return std::unexpected(std::string(message::reason_unreadable));
    }
    std::string bytes;
    bytes.resize(max_size + 1);
    in.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    const auto got = static_cast<std::size_t>(in.gcount());
    if (got > max_size)
    {
        return std::unexpected(format(message::reason_too_large, {std::to_string(max_size)}));
    }
    if (in.bad())
    {
        return std::unexpected(std::string(message::reason_unreadable));
    }
    bytes.resize(got);
    return bytes;
}

std::expected<void, std::string> write_new_file(const fs::path& path, std::string_view bytes, bool private_file)
{
    std::error_code error;
    if (fs::exists(fs::symlink_status(path, error)))
    {
        return std::unexpected(std::string(message::reason_exists));
    }
    std::ofstream out(path, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!out)
    {
        return std::unexpected(std::string(message::reason_unwritable));
    }
    if (private_file)
    {
        out.flush();
        // Owner only, before the first byte of the secret is written. Where the system has no such
        // permission the call does nothing harmful: the folder's own access rules then apply.
        fs::permissions(path, fs::perms::owner_read | fs::perms::owner_write, fs::perm_options::replace, error);
    }
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    out.flush();
    if (!out)
    {
        out.close();
        remove_file(path);
        return std::unexpected(std::string(message::reason_unwritable));
    }
    return {};
}

void remove_file(const fs::path& path) noexcept
{
    std::error_code error;
    fs::remove(path, error);
}

bool inside_git_work_tree(const fs::path& folder)
{
    std::error_code error;
    fs::path here = fs::canonical(folder, error);
    if (error)
    {
        here = fs::absolute(folder, error).lexically_normal();
    }
    while (!here.empty())
    {
        if (fs::exists(here / ".git", error))
        {
            return true;
        }
        const fs::path up = here.parent_path();
        if (up == here)
        {
            break;
        }
        here = up;
    }
    return false;
}

} // namespace adl::licence_owner

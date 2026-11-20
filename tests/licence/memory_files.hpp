// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
#pragma once

// Files held in memory, for the tests of the store. It fills in the interface the store asks of its caller
// (adl/licence/store.hpp) with a map from a path to the bytes, and it can be made to fail, so that a full
// disk, a folder that cannot be listed and a file that cannot be removed are tested.

#include "adl/licence/store.hpp"

#include <algorithm>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace adl::test
{

class MemoryFiles final : public licence::store::Files
{
public:
    using FilesError = licence::store::FilesError;
    template <typename T>
    using FilesResult = licence::store::FilesResult<T>;

    // path -> bytes. A path is a folder, '/' and a name; a folder exists when a file is in it.
    std::map<std::string, std::string> files;

    // A fault for the next call of each kind, and for every call after it until cleared.
    std::optional<FilesError> list_fails;
    std::optional<FilesError> read_fails;
    std::optional<FilesError> create_fails;
    std::optional<FilesError> replace_fails;
    std::optional<FilesError> remove_fails;

    // `list` gives the names in the reverse of name order. The interface promises no order, and the store must
    // not depend on one.
    bool list_in_reverse = false;

    // How many calls of each kind were made.
    int creates = 0;
    int replaces = 0;
    int removes = 0;

    void put(std::string_view path, std::string_view content)
    {
        files[std::string{path}] = std::string{content};
    }

    [[nodiscard]] bool has(std::string_view path) const
    {
        return files.contains(std::string{path});
    }

    // The names directly in `folder`, in order.
    [[nodiscard]] std::vector<std::string> names_in(std::string_view folder) const
    {
        std::vector<std::string> names;
        const std::string prefix = std::string{folder} + "/";
        for (const auto& [path, content] : files)
        {
            if (path.starts_with(prefix) && path.find('/', prefix.size()) == std::string::npos)
            {
                names.push_back(path.substr(prefix.size()));
            }
        }
        return names;
    }

    FilesResult<std::vector<std::string>> list(std::string_view folder) override
    {
        if (list_fails)
        {
            return std::unexpected(*list_fails);
        }
        auto names = names_in(folder);
        if (names.empty())
        {
            // A folder with nothing in it is a folder that is not there, as far as a test can tell.
            return std::unexpected(FilesError::not_found);
        }
        if (list_in_reverse)
        {
            std::ranges::reverse(names);
        }
        return names;
    }

    FilesResult<std::string> read(std::string_view path, std::size_t max_bytes) override
    {
        if (read_fails)
        {
            return std::unexpected(*read_fails);
        }
        const auto found = files.find(std::string{path});
        if (found == files.end())
        {
            return std::unexpected(FilesError::not_found);
        }
        if (found->second.size() > max_bytes)
        {
            return std::unexpected(FilesError::too_large);
        }
        return found->second;
    }

    FilesResult<void> create(std::string_view path, std::string_view content) override
    {
        ++creates;
        if (create_fails)
        {
            return std::unexpected(*create_fails);
        }
        if (!files.try_emplace(std::string{path}, std::string{content}).second)
        {
            return std::unexpected(FilesError::already_exists);
        }
        return {};
    }

    FilesResult<void> replace(std::string_view path, std::string_view content) override
    {
        ++replaces;
        if (replace_fails)
        {
            return std::unexpected(*replace_fails);
        }
        files[std::string{path}] = std::string{content};
        return {};
    }

    FilesResult<void> remove(std::string_view path) override
    {
        ++removes;
        if (remove_fails)
        {
            return std::unexpected(*remove_fails);
        }
        if (files.erase(std::string{path}) == 0)
        {
            return std::unexpected(FilesError::not_found);
        }
        return {};
    }
};

} // namespace adl::test

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// What the tests of libs/set_maker and apps/make_set share: files written to a temporary folder, a plan to make
// a set from, and a tree read back as bytes.

#include "adl/set_maker/plan.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace adl::test::set_maker
{

inline std::filesystem::path write_file(const std::filesystem::path& directory, std::string_view name,
                                        std::string_view content)
{
    std::filesystem::create_directories(directory);
    const std::filesystem::path path = directory / std::string{name};
    std::ofstream out(path, std::ios::binary);
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    return path;
}

// Everything below `root` as relative path (with /) -> bytes, so two trees compare with ==.
inline std::map<std::string, std::vector<std::byte>> tree_bytes(const std::filesystem::path& root)
{
    std::map<std::string, std::vector<std::byte>> tree;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }
        std::ifstream in(entry.path(), std::ios::binary);
        std::vector<std::byte> bytes;
        for (char c; in.get(c);)
        {
            bytes.push_back(static_cast<std::byte>(c));
        }
        tree[std::filesystem::relative(entry.path(), root).generic_string()] = std::move(bytes);
    }
    return tree;
}

// The names directly in `directory`, sorted (empty when it is empty or missing).
inline std::vector<std::string> names_in(const std::filesystem::path& directory)
{
    std::vector<std::string> names;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(directory, error))
    {
        names.push_back(entry.path().filename().string());
    }
    std::sort(names.begin(), names.end());
    return names;
}

// A plan of one load with two data files and a support file, whose sources are written into `sources`.
// The data files are 17 and 5 bytes, so one of them is not a whole number of 16-bit words.
inline adl::set_maker::SetPlan one_load_plan(const std::filesystem::path& sources, unsigned members = 1)
{
    using adl::arinc665::CheckValueType;
    using adl::set_maker::FileKind;
    using adl::set_maker::PlannedFile;

    adl::set_maker::SetPlan plan;
    plan.part_number = "ACM-SET-0001";
    plan.members = members;
    adl::set_maker::PlannedLoad load;
    load.part_number = "ACM47-1234-5678";
    load.header_file_name = "ACM.LUH";
    load.target_hardware_ids = {"ACME"};
    load.files.push_back(PlannedFile{write_file(sources, "app.bin", "0123456789ABCDEFG"), "APP.LUP", FileKind::data,
                                     CheckValueType::crc32});
    load.files.push_back(
        PlannedFile{write_file(sources, "data.bin", "HELLO"), "DATA.LUP", FileKind::data, CheckValueType::none});
    load.files.push_back(
        PlannedFile{write_file(sources, "config.ldr", "ABCD"), "CONFIG.LDR", FileKind::support, CheckValueType::md5});
    plan.loads.push_back(std::move(load));
    return plan;
}

} // namespace adl::test::set_maker

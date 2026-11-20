// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/loader_core/media_set_check.hpp"

#include "adl/arinc665/media_set.hpp"
#include "adl/media_tree/media_tree.hpp"
#include "adl/platform/paths.hpp"

#include <adl/profiling.hpp>

namespace adl::loader_core
{

namespace
{

// The four hexadecimal digits of a 16-bit value, upper case.
std::string hex4(std::uint16_t value)
{
    constexpr std::string_view digits = "0123456789ABCDEF";
    std::string text(4, '0');
    for (std::size_t i = 4; i-- > 0; value >>= 4)
    {
        text[i] = digits[value & 0xF];
    }
    return text;
}

} // namespace

MediaSetReport check_media_set(const std::filesystem::path& root, std::shared_ptr<const MediaSetIndex>* index)
{
    const adl::profiling::Zone zone{"app: read and validate media set"};
    if (index != nullptr)
    {
        index->reset();
    }
    MediaSetReport report;

    auto members = adl::media_tree::read_media_set_tree(root);
    if (!members)
    {
        report.error = std::string{adl::media_tree::to_string(members.error().error)} + " (" +
                       adl::platform::to_utf8(members.error().path.u8string()) + ")";
        return report;
    }

    auto set = adl::arinc665::read_media_set(*members);
    if (!set)
    {
        report.error = adl::arinc665::to_string(set.error());
        return report;
    }

    const auto& identity = set->loads_list.identity;
    if (index != nullptr)
    {
        auto made = MediaSetIndex::make(root, set->files_list, *members);
        if (!made)
        {
            report.error = std::move(made.error());
            return report;
        }
        *index = std::make_shared<const MediaSetIndex>(std::move(*made));
    }
    report.part_number = identity.media_set_part_number.view();
    report.members = identity.number_of_media_set_members;
    report.batches = set->batches.size();
    for (const auto& file : set->files_list.files)
    {
        report.files.push_back({file.file_name, file.pathname, file.member_sequence_number, hex4(file.crc)});
    }
    for (const auto& load : set->loads)
    {
        LoadReport entry;
        entry.part_number = load.header.load_part_number.view();
        entry.header_file = load.header_file_name;
        entry.data_files = load.header.data_files.size();
        entry.support_files = load.header.support_files.size();
        for (const auto& id : load.header.target_hardware_ids)
        {
            entry.target_hardware_ids.emplace_back(id);
        }
        report.loads.push_back(std::move(entry));
    }
    for (const auto& finding : set->findings)
    {
        report.findings.push_back(finding.clause + ": " + finding.message);
    }
    return report;
}

} // namespace adl::loader_core

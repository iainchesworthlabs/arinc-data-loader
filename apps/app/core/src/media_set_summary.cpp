// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/app/media_set_summary.hpp"

#include "adl/app/text.hpp"
#include "adl/loader_core/media_set_check.hpp"

#include <filesystem>

namespace adl::app
{

MediaSetSummary read_media_set_summary(const QString& path,
                                       std::shared_ptr<const adl::loader_core::MediaSetIndex>* index)
{
    const adl::loader_core::MediaSetReport report =
        adl::loader_core::check_media_set(std::filesystem::path{path.toStdU16String()}, index);

    MediaSetSummary summary;
    summary.path = path;
    summary.error = to_qstring(report.error);
    summary.part_number = to_qstring(report.part_number);
    summary.members = static_cast<int>(report.members);
    summary.batches = static_cast<int>(report.batches);
    for (const auto& file : report.files)
    {
        summary.files.append({to_qstring(file.name), to_qstring(file.pathname), file.member, to_qstring(file.crc16)});
    }
    for (const auto& load : report.loads)
    {
        LoadSummary entry;
        entry.part_number = to_qstring(load.part_number);
        entry.header_file = to_qstring(load.header_file);
        entry.data_files = static_cast<int>(load.data_files);
        entry.support_files = static_cast<int>(load.support_files);
        for (const auto& id : load.target_hardware_ids)
        {
            entry.target_hardware_ids.append(to_qstring(id));
        }
        summary.loads.append(entry);
    }
    for (const auto& finding : report.findings)
    {
        summary.findings.append(to_qstring(finding));
    }
    return summary;
}

} // namespace adl::app

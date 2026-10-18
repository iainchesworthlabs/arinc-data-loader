#include "adl/app/media_set_summary.hpp"

#include <filesystem>

#include <QObject>

#include "adl/app/text.hpp"
#include "adl/arinc665/media_set.hpp"
#include "adl/media_tree/media_tree.hpp"

namespace adl::app {

MediaSetSummary read_media_set_summary(const QString& path) {
    MediaSetSummary summary;
    summary.path = path;

    const std::filesystem::path root{path.toStdU16String()};
    auto members = adl::media_tree::read_media_set_tree(root);
    if (!members) {
        summary.error = QObject::tr("%1 (%2)").arg(
            to_qstring(adl::media_tree::to_string(members.error().error)),
            QString::fromStdU16String(members.error().path.u16string()));
        return summary;
    }

    auto set = adl::arinc665::read_media_set(*members);
    if (!set) {
        summary.error = to_qstring(adl::arinc665::to_string(set.error()));
        return summary;
    }

    const auto& identity = set->loads_list.identity;
    summary.part_number = to_qstring(identity.media_set_part_number.view());
    summary.members = identity.number_of_media_set_members;
    summary.batches = static_cast<int>(set->batches.size());
    for (const auto& load : set->loads) {
        LoadSummary entry;
        entry.part_number = to_qstring(load.header.load_part_number.view());
        entry.header_file = to_qstring(load.header_file_name);
        entry.data_files = static_cast<int>(load.header.data_files.size());
        entry.support_files = static_cast<int>(load.header.support_files.size());
        for (const auto& id : load.header.target_hardware_ids) {
            entry.target_hardware_ids.append(to_qstring(id));
        }
        summary.loads.append(entry);
    }
    for (const auto& finding : set->findings) {
        summary.findings.append(
            QObject::tr("%1: %2").arg(to_qstring(finding.clause), to_qstring(finding.message)));
    }
    return summary;
}

}  // namespace adl::app

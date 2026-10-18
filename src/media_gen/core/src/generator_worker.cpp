#include "adl/gen/generator_worker.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "adl/arinc665/batch_file.hpp"
#include "adl/arinc665/check_value_type.hpp"
#include "adl/arinc665/load.hpp"
#include "adl/arinc665/media_set.hpp"
#include "adl/arinc665/naming.hpp"
#include "adl/arinc665/part_number.hpp"
#include "adl/media_tree/media_tree.hpp"

#include "adl/gen/source_file_model.hpp"

namespace adl::gen {

namespace {

using adl::arinc665::BatchFile;
using adl::arinc665::BatchLoad;
using adl::arinc665::BatchTarget;
using adl::arinc665::CheckValueType;
using adl::arinc665::FilePlacement;
using adl::arinc665::LoadSpec;
using adl::arinc665::LoadType;
using adl::arinc665::MediaSetSpec;
using adl::arinc665::PartNumber;
using adl::arinc665::PlacedBatch;
using adl::arinc665::PlacedLoad;

// The media set's part root directory (section 3.2.4.1.1): the load part number without its
// hyphens, which the standard recommends, with the backslashes the placement takes.
std::string part_root_directory(const QString& load_part_number) {
    std::string root = "\\";
    for (const QChar c : load_part_number) {
        if (c != QLatin1Char('-')) {
            root += c.toLatin1();
        }
    }
    root += '\\';
    return root;
}

// The same name without the backslashes, for the subdirectories on the other members.
std::string part_root_name(const QString& load_part_number) {
    std::string name;
    for (const QChar c : load_part_number) {
        if (c != QLatin1Char('-')) {
            name += c.toLatin1();
        }
    }
    return name;
}

std::filesystem::path to_path(const QString& text) {
    return std::filesystem::path{text.toStdU16String()};
}

QString to_qstring(std::string_view text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

QString path_to_qstring(const std::filesystem::path& path) {
    return QString::fromStdU16String(path.u16string());
}

CheckValueType check_type(int index) {
    if (index < 0 || index > 5) {
        return CheckValueType::none;
    }
    return static_cast<CheckValueType>(index);
}

QString findings_line(const adl::arinc665::Finding& finding) {
    return QStringLiteral("%1: %2").arg(to_qstring(finding.clause), to_qstring(finding.message));
}

}  // namespace

const QList<LoadTypeOption>& load_type_options() {
    static const QList<LoadTypeOption> options = {
        {GeneratorWorker::tr("Operational software"), QStringLiteral("OPS"), 0x0001},
        {GeneratorWorker::tr("Navigation database"), QStringLiteral("NDB"), 0x0002},
        {GeneratorWorker::tr("Aeronautical database"), QStringLiteral("ADB"), 0x0003},
        {GeneratorWorker::tr("Test software"), QStringLiteral("TST"), 0x0004},
    };
    return options;
}

GeneratorWorker::GeneratorWorker(QObject* parent) : QObject(parent) {}

void GeneratorWorker::fail(GenerateOutcome& outcome, const QString& text, const QString& detail) {
    outcome.state = 2;
    outcome.text = text;
    outcome.detail = detail;
    Q_EMIT progress(-1);
    Q_EMIT finished(outcome);
}

void GeneratorWorker::generate(const GenerateJob& job) {
    GenerateOutcome outcome;
    outcome.state = 2;

    // ---- 1. read every file, reporting how far the reading has got ----
    struct PendingFile {
        JobFile meta;
        std::vector<std::byte> bytes;
    };
    std::vector<PendingFile> pending;
    pending.reserve(static_cast<std::size_t>(job.files.size()));

    qint64 total_bytes = 0;
    std::vector<qint64> sizes;
    sizes.reserve(static_cast<std::size_t>(job.files.size()));
    for (const JobFile& file : job.files) {
        std::error_code error;
        const auto size = std::filesystem::file_size(to_path(file.source_path), error);
        if (error) {
            fail(outcome, tr("Cannot read %1.").arg(file.source_path));
            return;
        }
        sizes.push_back(static_cast<qint64>(size));
        total_bytes += sizes.back();
    }

    qint64 bytes_read = 0;
    for (qsizetype i = 0; i < job.files.size(); ++i) {
        if (cancelled()) {
            fail(outcome, tr("Cancelled."), tr("Nothing was written."));
            return;
        }
        const JobFile& meta = job.files.at(i);
        std::ifstream stream(to_path(meta.source_path), std::ios::binary);
        if (!stream) {
            fail(outcome, tr("Cannot read %1.").arg(meta.source_path));
            return;
        }
        PendingFile entry;
        entry.meta = meta;
        entry.bytes.resize(static_cast<std::size_t>(sizes.at(i)));
        stream.read(reinterpret_cast<char*>(entry.bytes.data()),
                    static_cast<std::streamsize>(sizes.at(i)));
        if (!stream) {
            fail(outcome, tr("Cannot read %1.").arg(meta.source_path));
            return;
        }
        bytes_read += sizes.at(i);
        pending.push_back(std::move(entry));
        const int percent =
            total_bytes == 0 ? 100 : static_cast<int>(bytes_read * 100 / total_bytes);
        Q_EMIT progress(percent);
    }

    // ---- 2. the load ----
    if (cancelled()) {
        fail(outcome, tr("Cancelled."), tr("Nothing was written."));
        return;
    }
    LoadSpec load_spec;
    load_spec.header_file_name = job.header_file_name.toStdString();
    load_spec.part_number = PartNumber{job.load_part_number.toStdString()};
    if (job.load_type_index >= 0 && job.load_type_index < load_type_options().size()) {
        const LoadTypeOption& option = load_type_options().at(job.load_type_index);
        load_spec.load_type = LoadType{option.description.toStdString(), option.id};
    }
    for (const QString& id : job.target_hardware_ids) {
        load_spec.target_hardware_ids.push_back(id.toStdString());
    }
    int data_files = 0;
    int support_files = 0;
    for (PendingFile& file : pending) {
        adl::arinc665::LoadFileSpec listed;
        listed.name = file.meta.file_name.toStdString();
        listed.content = std::move(file.bytes);
        listed.check_type = check_type(file.meta.check_type);
        if (file.meta.kind == SourceFileModel::data_kind) {
            ++data_files;
            load_spec.data_files.push_back(std::move(listed));
        } else {
            ++support_files;
            load_spec.support_files.push_back(std::move(listed));
        }
    }
    if (load_spec.data_files.empty()) {
        fail(outcome, tr("No data files chosen."));
        return;
    }
    load_spec.load_check_type = check_type(job.load_check_type);

    auto load = adl::arinc665::build_load(load_spec);
    if (!load) {
        fail(outcome, tr("The load could not be built: %1.")
                          .arg(to_qstring(adl::arinc665::to_string(load.error()))));
        return;
    }

    // ---- 3. the media set ----
    PlacedLoad placed;
    placed.load = std::move(*load);
    placed.header_member = 1;
    placed.part_root_directory = part_root_directory(job.load_part_number);
    if (job.member_count > 1) {
        // The data files go round-robin over the members, each in a directory of the part
        // root's name (on the header file's member that is a subdirectory below the part
        // root itself); the support files stay on the header file's member in the part root.
        for (std::size_t i = 0; i < placed.load.data_files.size(); ++i) {
            const std::uint16_t member =
                static_cast<std::uint16_t>((i % static_cast<std::size_t>(job.member_count)) + 1);
            FilePlacement placement;
            placement.member = member;
            placement.subdirectory = part_root_name(job.load_part_number) + "\\";
            placed.data_placements.push_back(placement);
        }
        for (std::size_t i = 0; i < placed.load.support_files.size(); ++i) {
            FilePlacement placement;
            placement.member = placed.header_member;
            placed.support_placements.push_back(placement);
        }
    }

    MediaSetSpec media_set;
    media_set.media_set_part_number = PartNumber{job.media_set_part_number.toStdString()};
    media_set.number_of_members = static_cast<std::uint8_t>(job.member_count);
    media_set.file_check_type = check_type(job.file_check_type);
    media_set.files_list_check_type = check_type(job.files_list_check_type);
    media_set.loads.push_back(std::move(placed));

    if (job.include_batch) {
        // The batch names the set's loads for each target, on the first member.
        BatchFile batch;
        batch.batch_part_number = PartNumber{job.batch_part_number.toStdString()};
        for (const QString& id : job.target_hardware_ids) {
            BatchTarget target;
            target.target_hardware_id_position = id.toStdString();
            target.loads.push_back(BatchLoad{job.header_file_name.toStdString(),
                                             PartNumber{job.load_part_number.toStdString()}});
            batch.targets.push_back(std::move(target));
        }
        PlacedBatch placed_batch;
        placed_batch.file_name = job.batch_part_number.left(3).toStdString() + "." +
                                 std::string{adl::arinc665::batch_file_extension};
        placed_batch.batch = std::move(batch);
        placed_batch.member = 1;
        media_set.batches.push_back(std::move(placed_batch));
    }

    auto members = adl::arinc665::build_media_set(media_set);
    if (!members) {
        fail(outcome, tr("The media set could not be built: %1.")
                          .arg(to_qstring(adl::arinc665::to_string(members.error()))));
        return;
    }

    // ---- 4. write the tree; never overwrite what is there ----
    if (cancelled()) {
        fail(outcome, tr("Cancelled."), tr("Nothing was written."));
        return;
    }
    const std::filesystem::path base = to_path(job.output_directory);
    std::filesystem::path root;
    QString write_error;
    for (int attempt = 1; attempt <= 100; ++attempt) {
        const QString directory_name =
            attempt == 1 ? job.media_set_part_number
                         : QStringLiteral("%1_%2").arg(job.media_set_part_number).arg(attempt);
        const std::filesystem::path candidate = base / to_path(directory_name);
        std::error_code error;
        const bool exists = std::filesystem::exists(candidate, error);
        if (error) {
            write_error = path_to_qstring(candidate);
            break;
        }
        if (exists) {
            const bool empty = std::filesystem::is_empty(candidate, error);
            if (error) {
                write_error = path_to_qstring(candidate);
                break;
            }
            if (!empty) {
                continue;  // a set (or anything) is there already: take the next name
            }
        }
        const auto written = adl::media_tree::write_media_set_tree(candidate, *members);
        if (written) {
            root = candidate;
            break;
        }
        if (written.error().error == adl::media_tree::TreeError::already_exists) {
            continue;  // appeared while we looked: take the next name
        }
        write_error = tr("%1 (%2)").arg(
            path_to_qstring(written.error().path),
            to_qstring(adl::media_tree::to_string(written.error().error)));
        break;
    }
    if (root.empty()) {
        fail(outcome, tr("The media set could not be written below %1.").arg(job.output_directory),
             write_error);
        return;
    }

    // ---- 5. read the tree back and verify what is on the disk ----
    if (cancelled()) {
        fail(outcome, tr("Cancelled."), tr("Nothing was written."));
        return;
    }
    const auto on_disk = adl::media_tree::read_media_set_tree(root);
    if (!on_disk) {
        fail(outcome, tr("The written media set could not be read back: %1 (%2).")
                          .arg(to_qstring(adl::media_tree::to_string(on_disk.error().error)),
                               path_to_qstring(on_disk.error().path)));
        return;
    }
    const auto set = adl::arinc665::read_media_set(*on_disk);
    if (!set) {
        fail(outcome, tr("The written media set does not verify: %1.")
                          .arg(to_qstring(adl::arinc665::to_string(set.error()))));
        return;
    }

    outcome.state = 1;
    outcome.output_path = path_to_qstring(root);
    outcome.members = static_cast<int>(on_disk->size());
    outcome.data_files = data_files;
    outcome.support_files = support_files;
    outcome.total_bytes = total_bytes;
    for (const adl::arinc665::Finding& finding : set->findings) {
        outcome.findings.append(findings_line(finding));
    }
    outcome.text = tr("Media set %1 written and verified.").arg(job.media_set_part_number);
    outcome.detail =
        tr("%1 data file(s), %2 support file(s), %3 bytes in %4 member(s), in %5.")
            .arg(outcome.data_files)
            .arg(outcome.support_files)
            .arg(outcome.total_bytes)
            .arg(outcome.members)
            .arg(outcome.output_path);
    if (!outcome.findings.isEmpty()) {
        outcome.detail += QStringLiteral(" ") +
                          tr("%n difference(s) from the standard's recommendations.", "",
                             outcome.findings.size());
    }
    Q_EMIT progress(100);
    Q_EMIT finished(outcome);
}

}  // namespace adl::gen

#include "adl/gen/generator_worker.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <new>
#include <stdexcept>
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
using adl::ui::Message;

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

// The directory a media set is written to before it is given its name. Whatever is in it when
// the job ends without keep() is the job's own debris, and is removed.
class PartialTree {
public:
    PartialTree() = default;
    PartialTree(const PartialTree&) = delete;
    PartialTree& operator=(const PartialTree&) = delete;
    ~PartialTree() {
        if (claimed()) {
            static_cast<void>(discard());
        }
    }

    void claim(std::filesystem::path path) { path_ = std::move(path); }
    [[nodiscard]] bool claimed() const noexcept { return !path_.empty(); }
    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }
    // The set is whole and has its name: there is nothing to remove.
    void keep() noexcept { path_.clear(); }

    // Removes the directory and what is in it, and says in words, for the operator, whether
    // anything is left.
    [[nodiscard]] Message discard() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
        const bool gone = !std::filesystem::exists(path_, error);
        const QString where = path_to_qstring(path_);
        path_.clear();
        // The context is the worker's: the words are the generator's, not this helper's.
        return gone ? Message::translated(
                          &GeneratorWorker::tr,
                          QT_TRANSLATE_NOOP("adl::gen::GeneratorWorker",
                                            "Nothing was kept: what had been written was removed."))
                    : Message::translated(
                          &GeneratorWorker::tr,
                          QT_TRANSLATE_NOOP(
                              "adl::gen::GeneratorWorker",
                              "What had been written could not be removed, and is still in %1."),
                          {Message::verbatim(where)});
    }

private:
    std::filesystem::path path_;
};

}  // namespace

const QList<LoadTypeOption>& load_type_options() {
    static const QList<LoadTypeOption> options = {
        {QT_TRANSLATE_NOOP("adl::gen::GeneratorWorker", "Operational software"),
         QStringLiteral("OPS"), 0x0001},
        {QT_TRANSLATE_NOOP("adl::gen::GeneratorWorker", "Navigation database"),
         QStringLiteral("NDB"), 0x0002},
        {QT_TRANSLATE_NOOP("adl::gen::GeneratorWorker", "Aeronautical database"),
         QStringLiteral("ADB"), 0x0003},
        {QT_TRANSLATE_NOOP("adl::gen::GeneratorWorker", "Test software"), QStringLiteral("TST"),
         0x0004},
    };
    return options;
}

GeneratorWorker::GeneratorWorker(QObject* parent) : QObject(parent) {}

namespace {

Message too_large(const QString& path) {
    return Message::translated(
        &GeneratorWorker::tr,
        QT_TRANSLATE_NOOP("adl::gen::GeneratorWorker", "%1 is too large to be read into memory."),
        {Message::verbatim(path)});
}

Message not_written_below(const QString& directory) {
    return Message::translated(&GeneratorWorker::tr,
                               QT_TRANSLATE_NOOP("adl::gen::GeneratorWorker",
                                                 "The media set could not be written below %1."),
                               {Message::verbatim(directory)});
}

}  // namespace

Message GeneratorWorker::cannot_read(const QString& path) {
    return Message::translated(&GeneratorWorker::tr, QT_TR_NOOP("Cannot read %1."),
                               {Message::verbatim(path)});
}

void GeneratorWorker::fail(GenerateOutcome& outcome, const Message& text, const Message& detail) {
    outcome.state = 2;
    outcome.text = text;
    outcome.detail = detail;
    Q_EMIT progress(-1);
    Q_EMIT finished(outcome);
}

void GeneratorWorker::generate(const GenerateJob& job) {
    GenerateOutcome outcome;
    outcome.state = 2;
    try {
        run(job, outcome);
    } catch (const std::bad_alloc&) {
        fail(outcome,
             Message::translated(
                 &GeneratorWorker::tr,
                 QT_TR_NOOP("The computer ran out of memory while the media set was being made.")),
             Message::translated(&GeneratorWorker::tr,
                                 QT_TR_NOOP("Nothing was kept. Choose smaller files, or close "
                                            "other programs and try again.")));
    } catch (const std::exception& error) {
        fail(outcome,
             Message::translated(&GeneratorWorker::tr,
                                 QT_TR_NOOP("The media set could not be made.")),
             Message::verbatim(QString::fromLocal8Bit(error.what())));
    }
}

void GeneratorWorker::run(const GenerateJob& job, GenerateOutcome& outcome) {
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
            fail(outcome, cannot_read(file.source_path));
            return;
        }
        sizes.push_back(static_cast<qint64>(size));
        total_bytes += sizes.back();
    }

    qint64 bytes_read = 0;
    for (qsizetype i = 0; i < job.files.size(); ++i) {
        if (checkpoint("read")) {
            fail(outcome, Message::translated(&GeneratorWorker::tr, QT_TR_NOOP("Cancelled.")),
                 Message::translated(&GeneratorWorker::tr, QT_TR_NOOP("Nothing was written.")));
            return;
        }
        const JobFile& meta = job.files.at(i);
        std::ifstream stream(to_path(meta.source_path), std::ios::binary);
        if (!stream) {
            fail(outcome, cannot_read(meta.source_path));
            return;
        }
        PendingFile entry;
        entry.meta = meta;
        const qint64 size = sizes[static_cast<std::size_t>(i)];
        try {
            static_cast<void>(checkpoint("allocate"));
            entry.bytes.resize(static_cast<std::size_t>(size));
        } catch (const std::length_error&) {
            fail(outcome, too_large(meta.source_path));
            return;
        } catch (const std::bad_alloc&) {
            fail(outcome, too_large(meta.source_path),
                 Message::translated(&GeneratorWorker::tr,
                                     QT_TR_NOOP("Nothing was written. Choose smaller files, or "
                                                "close other programs and try again.")));
            return;
        }
        stream.read(reinterpret_cast<char*>(entry.bytes.data()),
                    static_cast<std::streamsize>(size));
        if (!stream) {
            fail(outcome, cannot_read(meta.source_path));
            return;
        }
        bytes_read += size;
        pending.push_back(std::move(entry));
        const int percent =
            total_bytes == 0 ? 100 : static_cast<int>(bytes_read * 100 / total_bytes);
        Q_EMIT progress(percent);
    }

    // ---- 2. the load ----
    if (checkpoint("load")) {
        fail(outcome, Message::translated(&GeneratorWorker::tr, QT_TR_NOOP("Cancelled.")),
             Message::translated(&GeneratorWorker::tr, QT_TR_NOOP("Nothing was written.")));
        return;
    }
    LoadSpec load_spec;
    load_spec.header_file_name = job.header_file_name.toStdString();
    load_spec.part_number = PartNumber{job.load_part_number.toStdString()};
    if (job.load_type_index >= 0 && job.load_type_index < load_type_options().size()) {
        const LoadTypeOption& option = load_type_options().at(job.load_type_index);
        load_spec.load_type = LoadType{std::string{option.description}, option.id};
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
        fail(outcome,
             Message::translated(&GeneratorWorker::tr, QT_TR_NOOP("No data files chosen.")));
        return;
    }
    load_spec.load_check_type = check_type(job.load_check_type);

    auto load = adl::arinc665::build_load(load_spec);
    if (!load) {
        fail(outcome, Message::translated(
                          &GeneratorWorker::tr, QT_TR_NOOP("The load could not be built: %1."),
                          {Message::verbatim(to_qstring(adl::arinc665::to_string(load.error())))}));
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
        fail(outcome,
             Message::translated(
                 &GeneratorWorker::tr, QT_TR_NOOP("The media set could not be built: %1."),
                 {Message::verbatim(to_qstring(adl::arinc665::to_string(members.error())))}));
        return;
    }

    // ---- 4. write the tree to a temporary directory beside the final one ----
    // The set appears under its name in one step, when it is whole and has been read back; until
    // then, and whatever goes wrong (a full disk, a cancel, a set that does not verify), what was
    // written is removed. Nothing already there is overwritten.
    if (checkpoint("write")) {
        fail(outcome, Message::translated(&GeneratorWorker::tr, QT_TR_NOOP("Cancelled.")),
             Message::translated(&GeneratorWorker::tr, QT_TR_NOOP("Nothing was written.")));
        return;
    }
    const std::filesystem::path base = to_path(job.output_directory);
    std::error_code error;
    std::filesystem::create_directories(base, error);
    if (error || !std::filesystem::is_directory(base, error)) {
        fail(outcome, not_written_below(job.output_directory),
             Message::verbatim(QStringLiteral("%1 (%2)").arg(
                 job.output_directory, QString::fromLocal8Bit(error.message().c_str()))));
        return;
    }
    PartialTree partial;
    for (int attempt = 1; attempt <= 100 && !partial.claimed(); ++attempt) {
        const QString temporary_name =
            attempt == 1
                ? QStringLiteral("%1.writing").arg(job.media_set_part_number)
                : QStringLiteral("%1.writing_%2").arg(job.media_set_part_number).arg(attempt);
        const std::filesystem::path candidate = base / to_path(temporary_name);
        if (std::filesystem::create_directory(candidate, error)) {
            partial.claim(candidate);
        } else if (error) {
            fail(outcome, not_written_below(job.output_directory),
                 Message::verbatim(QStringLiteral("%1 (%2)").arg(
                     path_to_qstring(candidate), QString::fromLocal8Bit(error.message().c_str()))));
            return;
        }
    }
    if (!partial.claimed()) {
        fail(outcome, not_written_below(job.output_directory),
             Message::translated(&GeneratorWorker::tr,
                                 QT_TR_NOOP("Too many unfinished sets (%1.writing...) are there "
                                            "already."),
                                 {Message::verbatim(job.media_set_part_number)}));
        return;
    }
    const auto written = adl::media_tree::write_media_set_tree(partial.path(), *members);
    if (!written) {
        const Message what = Message::verbatim(QStringLiteral("%1 (%2)").arg(
            path_to_qstring(written.error().path),
            to_qstring(adl::media_tree::to_string(written.error().error))));
        fail(outcome, not_written_below(job.output_directory),
             Message::sequence({what, Message::verbatim(QStringLiteral(" ")), partial.discard()}));
        return;
    }

    // ---- 5. read the tree back and verify what is on the disk ----
    // A cancel after the write finds the whole set on the disk, and takes it away again.
    if (checkpoint("written")) {
        fail(outcome, Message::translated(&GeneratorWorker::tr, QT_TR_NOOP("Cancelled.")),
             partial.discard());
        return;
    }
    const auto on_disk = adl::media_tree::read_media_set_tree(partial.path());
    if (!on_disk) {
        fail(outcome,
             Message::translated(
                 &GeneratorWorker::tr,
                 QT_TR_NOOP("The written media set could not be read back: %1 (%2)."),
                 {Message::verbatim(to_qstring(adl::media_tree::to_string(on_disk.error().error))),
                  Message::verbatim(path_to_qstring(on_disk.error().path))}),
             partial.discard());
        return;
    }
    const auto set = adl::arinc665::read_media_set(*on_disk);
    if (!set) {
        fail(outcome,
             Message::translated(
                 &GeneratorWorker::tr, QT_TR_NOOP("The written media set does not verify: %1."),
                 {Message::verbatim(to_qstring(adl::arinc665::to_string(set.error())))}),
             partial.discard());
        return;
    }
    if (checkpoint("verified")) {
        fail(outcome, Message::translated(&GeneratorWorker::tr, QT_TR_NOOP("Cancelled.")),
             partial.discard());
        return;
    }

    // ---- 6. give the set its name: the first of the names that is free ----
    std::filesystem::path root;
    Message rename_error;
    for (int attempt = 1; attempt <= 100; ++attempt) {
        const QString directory_name =
            attempt == 1 ? job.media_set_part_number
                         : QStringLiteral("%1_%2").arg(job.media_set_part_number).arg(attempt);
        const std::filesystem::path candidate = base / to_path(directory_name);
        const bool exists = std::filesystem::exists(candidate, error);
        if (error) {
            rename_error = Message::verbatim(path_to_qstring(candidate));
            break;
        }
        if (exists) {
            const bool empty = std::filesystem::is_empty(candidate, error);
            if (error) {
                rename_error = Message::verbatim(path_to_qstring(candidate));
                break;
            }
            if (!empty) {
                continue;  // a set (or anything) is there already: take the next name
            }
            // An empty directory is taken over (a rename onto it fails on some systems).
            std::filesystem::remove(candidate, error);
        }
        std::filesystem::rename(partial.path(), candidate, error);
        if (!error) {
            root = candidate;
            partial.keep();
            break;
        }
        if (std::filesystem::exists(candidate)) {
            continue;  // appeared while we looked: take the next name
        }
        rename_error = Message::verbatim(QStringLiteral("%1 (%2)").arg(
            path_to_qstring(candidate), QString::fromLocal8Bit(error.message().c_str())));
        break;
    }
    if (root.empty()) {
        fail(outcome, not_written_below(job.output_directory),
             rename_error.isEmpty()
                 ? partial.discard()
                 : Message::sequence(
                       {rename_error, Message::verbatim(QStringLiteral(" ")), partial.discard()}));
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
    outcome.text =
        Message::translated(&GeneratorWorker::tr, QT_TR_NOOP("Media set %1 written and verified."),
                            {Message::verbatim(job.media_set_part_number)});
    // One count to a sentence: the files and the members are phrases of their own.
    outcome.detail = Message::translated(
        &GeneratorWorker::tr, QT_TR_NOOP("%1, %2, %3 bytes in %4, in %5."),
        {Message::translated(&GeneratorWorker::tr, QT_TR_N_NOOP("%n data file(s)"), {},
                             outcome.data_files),
         Message::translated(&GeneratorWorker::tr, QT_TR_N_NOOP("%n support file(s)"), {},
                             outcome.support_files),
         Message::verbatim(QString::number(outcome.total_bytes)),
         Message::translated(&GeneratorWorker::tr, QT_TR_N_NOOP("%n member(s)"), {},
                             outcome.members),
         Message::verbatim(outcome.output_path)});
    if (!outcome.findings.isEmpty()) {
        outcome.detail = Message::sequence(
            {outcome.detail, Message::verbatim(QStringLiteral(" ")),
             Message::translated(
                 &GeneratorWorker::tr,
                 QT_TR_N_NOOP("%n difference(s) from the standard's recommendations."), {},
                 static_cast<int>(outcome.findings.size()))});
    }
    Q_EMIT progress(100);
    Q_EMIT finished(outcome);
}

}  // namespace adl::gen

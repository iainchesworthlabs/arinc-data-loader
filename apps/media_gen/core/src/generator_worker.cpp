// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/gen/generator_worker.hpp"

#include "adl/arinc665/check_value_type.hpp"
#include "adl/gen/source_file_model.hpp"
#include "adl/set_maker/make.hpp"
#include "adl/set_maker/plan.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

namespace adl::gen
{

namespace
{

using adl::arinc665::CheckValueType;
using adl::ui::Message;

std::filesystem::path to_path(const QString& text)
{
    return std::filesystem::path{text.toStdU16String()};
}

QString to_qstring(std::string_view text)
{
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

QString path_to_qstring(const std::filesystem::path& path)
{
    return QString::fromStdU16String(path.u16string());
}

CheckValueType check_type(int index)
{
    if (index < 0 || index > 5)
    {
        return CheckValueType::none;
    }
    return static_cast<CheckValueType>(index);
}

// What the operator decided, as the plan the library makes a set from.
adl::set_maker::SetPlan plan_of(const GenerateJob& job)
{
    using adl::set_maker::FileKind;

    adl::set_maker::PlannedLoad load;
    load.part_number = job.load_part_number.toStdString();
    load.header_file_name = job.header_file_name.toStdString();
    if (job.load_type_index >= 0 && job.load_type_index < load_type_options().size())
    {
        const LoadTypeOption& option = load_type_options().at(job.load_type_index);
        load.type = adl::arinc665::LoadType{std::string{option.description}, option.id};
    }
    for (const QString& id : job.target_hardware_ids)
    {
        load.target_hardware_ids.push_back(id.toStdString());
    }
    load.check = check_type(job.load_check_type);
    for (const JobFile& file : job.files)
    {
        adl::set_maker::PlannedFile planned;
        planned.source = to_path(file.source_path);
        planned.name = file.file_name.toStdString();
        planned.kind = file.kind == SourceFileModel::data_kind ? FileKind::data : FileKind::support;
        planned.check = check_type(file.check_type);
        load.files.push_back(std::move(planned));
    }

    adl::set_maker::SetPlan plan;
    plan.part_number = job.media_set_part_number.toStdString();
    plan.members = static_cast<unsigned>(job.member_count);
    plan.files_check = check_type(job.file_check_type);
    plan.files_list_check = check_type(job.files_list_check_type);
    if (job.include_batch)
    {
        // The batch names the set's load for each target.
        adl::set_maker::PlannedBatch batch;
        batch.part_number = job.batch_part_number.toStdString();
        batch.file_name = adl::set_maker::default_batch_file_name(batch.part_number);
        for (const std::string& id : load.target_hardware_ids)
        {
            batch.targets.push_back({id, {load.part_number}});
        }
        plan.batch = std::move(batch);
    }
    plan.loads.push_back(std::move(load));
    return plan;
}

Message too_large(const QString& path)
{
    return Message::translated(
        &GeneratorWorker::tr, QT_TRANSLATE_NOOP("adl::gen::GeneratorWorker", "%1 is too large to be read into memory."),
        {Message::verbatim(path)});
}

Message not_written_below(const QString& directory)
{
    return Message::translated(
        &GeneratorWorker::tr,
        QT_TRANSLATE_NOOP("adl::gen::GeneratorWorker", "The media set could not be written below %1."),
        {Message::verbatim(directory)});
}

// In words, for the operator, whether anything the job had written is left.
Message discard_message(const adl::set_maker::Discard& discard)
{
    // The context is the worker's: the words are the generator's, not this helper's.
    return discard.removed
               ? Message::translated(&GeneratorWorker::tr,
                                     QT_TRANSLATE_NOOP("adl::gen::GeneratorWorker",
                                                       "Nothing was kept: what had been written was removed."))
               : Message::translated(
                     &GeneratorWorker::tr,
                     QT_TRANSLATE_NOOP("adl::gen::GeneratorWorker",
                                       "What had been written could not be removed, and is still in %1."),
                     {Message::verbatim(path_to_qstring(discard.where))});
}

// A path and what the system said about it, as the window has always shown them: "path (words)".
Message path_and_words(const std::filesystem::path& path, const std::string& system)
{
    if (path.empty())
    {
        return {};
    }
    if (system.empty())
    {
        return Message::verbatim(path_to_qstring(path));
    }
    return Message::verbatim(
        QStringLiteral("%1 (%2)").arg(path_to_qstring(path), QString::fromLocal8Bit(system.c_str())));
}

Message with_discard(const Message& what, const adl::set_maker::Discard& discard)
{
    if (!discard.attempted)
    {
        return what;
    }
    if (what.isEmpty())
    {
        return discard_message(discard);
    }
    return Message::sequence({what, Message::verbatim(QStringLiteral(" ")), discard_message(discard)});
}

} // namespace

// What the library asks of the worker while it makes a set: to say how far the reading has got, and where
// the job may be stopped.
class GeneratorWorker::Bridge final : public adl::set_maker::Hooks
{
public:
    explicit Bridge(GeneratorWorker& worker) : worker_(worker) {}

    void progress(int percent) override
    {
        Q_EMIT worker_.progress(percent);
    }

    [[nodiscard]] bool checkpoint(std::string_view place) override
    {
        return worker_.checkpoint(place);
    }

private:
    GeneratorWorker& worker_;
};

const QList<LoadTypeOption>& load_type_options()
{
    static const QList<LoadTypeOption> options = {
        {QT_TRANSLATE_NOOP("adl::gen::GeneratorWorker", "Operational software"), QStringLiteral("OPS"), 0x0001},
        {QT_TRANSLATE_NOOP("adl::gen::GeneratorWorker", "Navigation database"), QStringLiteral("NDB"), 0x0002},
        {QT_TRANSLATE_NOOP("adl::gen::GeneratorWorker", "Aeronautical database"), QStringLiteral("ADB"), 0x0003},
        {QT_TRANSLATE_NOOP("adl::gen::GeneratorWorker", "Test software"), QStringLiteral("TST"), 0x0004},
    };
    return options;
}

GeneratorWorker::GeneratorWorker(QObject* parent) : QObject(parent) {}

Message GeneratorWorker::cannot_read(const QString& path)
{
    return Message::translated(&GeneratorWorker::tr, QT_TR_NOOP("Cannot read %1."), {Message::verbatim(path)});
}

void GeneratorWorker::fail(GenerateOutcome& outcome, const Message& text, const Message& detail)
{
    outcome.state = 2;
    outcome.text = text;
    outcome.detail = detail;
    Q_EMIT progress(-1);
    Q_EMIT finished(outcome);
}

void GeneratorWorker::stop(GenerateOutcome& outcome, const Message& detail)
{
    outcome.state = 3;
    outcome.text = Message::translated(&GeneratorWorker::tr, QT_TR_NOOP("Cancelled."));
    outcome.detail =
        detail.isEmpty() ? Message::translated(&GeneratorWorker::tr, QT_TR_NOOP("Nothing was written.")) : detail;
    Q_EMIT progress(-1);
    Q_EMIT finished(outcome);
}

void GeneratorWorker::generate(const GenerateJob& job)
{
    GenerateOutcome outcome;
    outcome.state = 2;
    try
    {
        run(job, outcome);
    }
    catch (const std::bad_alloc&)
    {
        fail(outcome,
             Message::translated(&GeneratorWorker::tr,
                                 QT_TR_NOOP("The computer ran out of memory while the media set was being made.")),
             Message::translated(&GeneratorWorker::tr, QT_TR_NOOP("Nothing was kept. Choose smaller files, or close "
                                                                  "other programs and try again.")));
    }
    catch (const std::exception& error)
    {
        fail(outcome, Message::translated(&GeneratorWorker::tr, QT_TR_NOOP("The media set could not be made.")),
             Message::verbatim(QString::fromLocal8Bit(error.what())));
    }
}

void GeneratorWorker::report_failure(const GenerateJob& job, const adl::set_maker::MakeFailure& failure,
                                     GenerateOutcome& outcome)
{
    using adl::set_maker::FailureKind;

    switch (failure.kind)
    {
    case FailureKind::source_unreadable:
        fail(outcome, cannot_read(path_to_qstring(failure.path)));
        break;
    case FailureKind::source_too_large:
        fail(outcome, too_large(path_to_qstring(failure.path)));
        break;
    case FailureKind::source_does_not_fit:
        fail(outcome, too_large(path_to_qstring(failure.path)),
             Message::translated(&GeneratorWorker::tr, QT_TR_NOOP("Nothing was written. Choose smaller files, or "
                                                                  "close other programs and try again.")));
        break;
    case FailureKind::no_data_files:
        fail(outcome, Message::translated(&GeneratorWorker::tr, QT_TR_NOOP("No data files chosen.")));
        break;
    case FailureKind::load_not_built:
        fail(outcome, Message::translated(&GeneratorWorker::tr, QT_TR_NOOP("The load could not be built: %1."),
                                          {Message::verbatim(to_qstring(failure.detail))}));
        break;
    case FailureKind::set_not_built:
        fail(outcome, Message::translated(&GeneratorWorker::tr, QT_TR_NOOP("The media set could not be built: %1."),
                                          {Message::verbatim(to_qstring(failure.detail))}));
        break;
    case FailureKind::not_written_below:
        fail(outcome, not_written_below(job.output_directory), path_and_words(failure.at, failure.system));
        break;
    case FailureKind::too_many_unfinished:
        fail(outcome, not_written_below(job.output_directory),
             Message::translated(&GeneratorWorker::tr,
                                 QT_TR_NOOP("Too many unfinished sets (%1.writing...) are there "
                                            "already."),
                                 {Message::verbatim(job.media_set_part_number)}));
        break;
    case FailureKind::write_failed:
        fail(outcome, not_written_below(job.output_directory),
             with_discard(Message::verbatim(
                              QStringLiteral("%1 (%2)").arg(path_to_qstring(failure.at), to_qstring(failure.detail))),
                          failure.discard));
        break;
    case FailureKind::read_back_failed:
        fail(outcome,
             Message::translated(
                 &GeneratorWorker::tr, QT_TR_NOOP("The written media set could not be read back: %1 (%2)."),
                 {Message::verbatim(to_qstring(failure.detail)), Message::verbatim(path_to_qstring(failure.at))}),
             discard_message(failure.discard));
        break;
    case FailureKind::not_verified:
    case FailureKind::not_the_set_described:
        fail(outcome,
             Message::translated(&GeneratorWorker::tr, QT_TR_NOOP("The written media set does not verify: %1."),
                                 {Message::verbatim(to_qstring(failure.detail))}),
             discard_message(failure.discard));
        break;
    case FailureKind::not_given_its_name:
        fail(outcome, not_written_below(job.output_directory),
             with_discard(path_and_words(failure.at, failure.system), failure.discard));
        break;
    case FailureKind::output_exists:
        // Only a destination that must be free can say this, and the window's is the first free name.
        fail(outcome, not_written_below(job.output_directory), Message::verbatim(path_to_qstring(failure.path)));
        break;
    case FailureKind::cancelled:
        stop(outcome, failure.discard.attempted ? discard_message(failure.discard) : Message{});
        break;
    case FailureKind::out_of_memory:
        fail(outcome,
             Message::translated(&GeneratorWorker::tr,
                                 QT_TR_NOOP("The computer ran out of memory while the media set was being made.")),
             Message::translated(&GeneratorWorker::tr, QT_TR_NOOP("Nothing was kept. Choose smaller files, or close "
                                                                  "other programs and try again.")));
        break;
    case FailureKind::error:
        fail(outcome, Message::translated(&GeneratorWorker::tr, QT_TR_NOOP("The media set could not be made.")),
             Message::verbatim(QString::fromLocal8Bit(failure.detail.c_str())));
        break;
    }
}

void GeneratorWorker::run(const GenerateJob& job, GenerateOutcome& outcome)
{
    const adl::set_maker::SetPlan plan = plan_of(job);
    Bridge bridge{*this};
    // The set is written below the output folder, in the first of its names that is free, so nothing
    // that is there is overwritten.
    const auto made = adl::set_maker::make_set(
        plan,
        adl::set_maker::Destination::first_free(to_path(job.output_directory), to_path(job.media_set_part_number)),
        bridge);
    if (!made)
    {
        report_failure(job, made.error(), outcome);
        return;
    }

    outcome.state = 1;
    outcome.output_path = path_to_qstring(made->path);
    outcome.members = static_cast<int>(made->members);
    outcome.data_files = static_cast<int>(made->data_files);
    outcome.support_files = static_cast<int>(made->support_files);
    outcome.total_bytes = static_cast<qint64>(made->source_bytes);
    for (const std::string& finding : made->report.findings)
    {
        outcome.findings.append(to_qstring(finding));
    }
    outcome.text = Message::translated(&GeneratorWorker::tr, QT_TR_NOOP("Media set %1 written and verified."),
                                       {Message::verbatim(job.media_set_part_number)});
    // One count to a sentence: the files and the members are phrases of their own.
    outcome.detail = Message::translated(
        &GeneratorWorker::tr, QT_TR_NOOP("%1, %2, %3 bytes in %4, in %5."),
        {Message::translated(&GeneratorWorker::tr, QT_TR_N_NOOP("%n data file(s)"), {}, outcome.data_files),
         Message::translated(&GeneratorWorker::tr, QT_TR_N_NOOP("%n support file(s)"), {}, outcome.support_files),
         Message::verbatim(QString::number(outcome.total_bytes)),
         Message::translated(&GeneratorWorker::tr, QT_TR_N_NOOP("%n member(s)"), {}, outcome.members),
         Message::verbatim(outcome.output_path)});
    if (!outcome.findings.isEmpty())
    {
        outcome.detail = Message::sequence(
            {outcome.detail, Message::verbatim(QStringLiteral(" ")),
             Message::translated(&GeneratorWorker::tr,
                                 QT_TR_N_NOOP("%n difference(s) from the standard's recommendations."), {},
                                 static_cast<int>(outcome.findings.size()))});
    }
    Q_EMIT finished(outcome);
}

} // namespace adl::gen

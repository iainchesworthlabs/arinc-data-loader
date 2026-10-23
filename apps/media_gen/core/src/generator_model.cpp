#include "adl/gen/generator_model.hpp"

#include "adl/arinc665/check_value_type.hpp"
#include "adl/arinc665/naming.hpp"
#include "adl/arinc665/part_number.hpp"

#include <QFileInfo>
#include <QMetaObject>
#include <algorithm>
#include <string>

namespace adl::gen
{

using adl::ui::Message;

namespace
{

QString to_qstring(std::string_view text)
{
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

// A library's findings as one message, in the window's usual "clause: text" form.
QString findings_text(const adl::arinc665::Findings& findings)
{
    QStringList pieces;
    for (const adl::arinc665::Finding& finding : findings)
    {
        pieces.append(QStringLiteral("%1: %2").arg(to_qstring(finding.clause), to_qstring(finding.message)));
    }
    return pieces.join(QStringLiteral("; "));
}

// The findings against one name, or nothing when the name is empty (a field the operator has not
// filled in yet, which the callers report with their own text).
QString check_text(const QString& name, const adl::arinc665::Findings& findings, const QString& missing)
{
    if (name.isEmpty())
    {
        return missing;
    }
    return findings_text(findings);
}

} // namespace

GeneratorModel::GeneratorModel(QObject* parent) : QObject(parent)
{
    qRegisterMetaType<GenerateJob>();
    qRegisterMetaType<GenerateOutcome>();

    worker_ = new GeneratorWorker();
    worker_->moveToThread(&thread_);
    connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);

    // The worker's thread emits these; the receiver (this) lives on the window's thread, so Qt
    // queues them and they arrive here in the order they were emitted.
    connect(worker_, &GeneratorWorker::finished, this, &GeneratorModel::on_finished);
    connect(worker_, &GeneratorWorker::progress, this, [this](int percent) { set_progress(percent); });
    connect(&files_, &SourceFileModel::filesChanged, this, [this] { refresh_validation(); });
    fill_load_type_model();

    thread_.setObjectName(QStringLiteral("generator"));
    thread_.start();
    refresh_validation();
}

GeneratorModel::~GeneratorModel()
{
    worker_->request_cancel();
    thread_.quit();
    thread_.wait();
}

void GeneratorModel::setLoadPartNumber(const QString& part_number)
{
    const QString trimmed = part_number.trimmed();
    if (trimmed == load_part_number_)
    {
        return;
    }
    load_part_number_ = trimmed;
    Q_EMIT loadPartNumberChanged();
    // The header file's name follows the load part number until the operator names it himself.
    if (!header_file_edited_)
    {
        header_file_name_ = trimmed.left(3) + QStringLiteral(".LUH");
        Q_EMIT headerFileNameChanged();
    }
    refresh_validation();
}

void GeneratorModel::setMediaSetPartNumber(const QString& part_number)
{
    const QString trimmed = part_number.trimmed();
    if (trimmed == media_set_part_number_)
    {
        return;
    }
    media_set_part_number_ = trimmed;
    Q_EMIT mediaSetPartNumberChanged();
    refresh_validation();
}

void GeneratorModel::setBatchPartNumber(const QString& part_number)
{
    const QString trimmed = part_number.trimmed();
    if (trimmed == batch_part_number_)
    {
        return;
    }
    batch_part_number_ = trimmed;
    Q_EMIT batchPartNumberChanged();
    refresh_validation();
}

void GeneratorModel::setIncludeBatch(bool include)
{
    if (include == include_batch_)
    {
        return;
    }
    include_batch_ = include;
    Q_EMIT includeBatchChanged();
    refresh_validation();
}

void GeneratorModel::setHeaderFileName(const QString& name)
{
    const QString trimmed = name.trimmed();
    header_file_edited_ = true;
    if (trimmed == header_file_name_)
    {
        return;
    }
    header_file_name_ = trimmed;
    Q_EMIT headerFileNameChanged();
    refresh_validation();
}

void GeneratorModel::setLoadTypeIndex(int index)
{
    const int clamped = std::clamp(index, -1, static_cast<int>(load_type_options().size()) - 1);
    if (clamped == load_type_index_)
    {
        return;
    }
    load_type_index_ = clamped;
    Q_EMIT loadTypeIndexChanged();
    refresh_validation();
}

void GeneratorModel::setTargetHardwareIds(const QStringList& ids)
{
    QStringList trimmed;
    for (const QString& id : ids)
    {
        const QString text = id.trimmed();
        if (!text.isEmpty())
        {
            trimmed.append(text);
        }
    }
    if (trimmed == target_hardware_ids_)
    {
        return;
    }
    target_hardware_ids_ = trimmed;
    Q_EMIT targetHardwareIdsChanged();
    refresh_validation();
}

void GeneratorModel::setMemberCount(int members)
{
    const int clamped = std::clamp(members, 1, 255);
    if (clamped == member_count_)
    {
        return;
    }
    member_count_ = clamped;
    Q_EMIT memberCountChanged();
    refresh_validation();
}

void GeneratorModel::setFileCheckTypeIndex(int index)
{
    const int clamped = std::clamp(index, 0, 5);
    if (clamped == file_check_type_index_)
    {
        return;
    }
    file_check_type_index_ = clamped;
    Q_EMIT fileCheckTypeIndexChanged();
    refresh_validation();
}

void GeneratorModel::setFilesListCheckTypeIndex(int index)
{
    const int clamped = std::clamp(index, 0, 5);
    if (clamped == files_list_check_type_index_)
    {
        return;
    }
    files_list_check_type_index_ = clamped;
    Q_EMIT filesListCheckTypeIndexChanged();
    refresh_validation();
}

void GeneratorModel::setLoadCheckTypeIndex(int index)
{
    const int clamped = std::clamp(index, 0, 5);
    if (clamped == load_check_type_index_)
    {
        return;
    }
    load_check_type_index_ = clamped;
    Q_EMIT loadCheckTypeIndexChanged();
    refresh_validation();
}

void GeneratorModel::fill_load_type_model()
{
    const QStringList options = loadTypeOptions();
    if (load_type_model_.rowCount() != options.size())
    {
        load_type_model_.setStringList(options);
        return;
    }
    // The same rows with other text: changed in place, so the combo keeps its choice.
    for (int i = 0; i < options.size(); ++i)
    {
        load_type_model_.setData(load_type_model_.index(i), options.at(i));
    }
}

QStringList GeneratorModel::loadTypeOptions() const
{
    QStringList options;
    options.append(tr("None"));
    for (const LoadTypeOption& option : load_type_options())
    {
        options.append(tr("%1 (%2, ID 0x%3)")
                           .arg(GeneratorWorker::tr(option.description), option.code)
                           .arg(option.id, 4, 16, QLatin1Char('0')));
    }
    return options;
}

QStringList GeneratorModel::checkValueOptions() const
{
    QStringList options;
    for (int i = 0; i <= 5; ++i)
    {
        options.append(to_qstring(adl::arinc665::to_string(static_cast<adl::arinc665::CheckValueType>(i))));
    }
    return options;
}

void GeneratorModel::chooseOutputDirectory(const QUrl& directory)
{
    if (!directory.isLocalFile())
    {
        return;
    }
    const QString path = directory.toLocalFile();
    if (path == output_directory_)
    {
        return;
    }
    output_directory_ = path;
    Q_EMIT outputDirectoryChanged();
    refresh_validation();
}

void GeneratorModel::generate()
{
    if (busy_)
    {
        return;
    }
    if (!can_generate_)
    {
        set_result(
            2, Message::translated(&GeneratorModel::tr, QT_TR_NOOP("Cannot generate: fix the marked fields first.")),
            {});
        return;
    }
    // A request left over from an earlier job must not stop this one.
    worker_->clear_cancel();
    set_result(0, {}, {});
    output_path_.clear();
    validation_findings_.clear();
    Q_EMIT resultChanged();
    set_progress(0);

    GenerateJob job;
    job.load_part_number = load_part_number_;
    job.media_set_part_number = media_set_part_number_;
    job.batch_part_number = batch_part_number_;
    job.include_batch = include_batch_;
    job.header_file_name = header_file_name_;
    job.load_type_index = load_type_index_;
    job.target_hardware_ids = target_hardware_ids_;
    job.member_count = member_count_;
    job.file_check_type = file_check_type_index_;
    job.files_list_check_type = files_list_check_type_index_;
    job.load_check_type = load_check_type_index_;
    job.output_directory = output_directory_;
    job.files = files_.files();

    busy_ = true;
    Q_EMIT busyChanged();
    refresh_validation();
    QMetaObject::invokeMethod(
        worker_, [worker = worker_, job = std::move(job)]() mutable { worker->generate(job); }, Qt::QueuedConnection);
}

void GeneratorModel::cancel()
{
    worker_->request_cancel();
}

void GeneratorModel::addFiles(const QList<QUrl>& urls)
{
    files_.addFiles(urls);
}

void GeneratorModel::removeFile(int row)
{
    files_.remove(row);
}

void GeneratorModel::moveFileUp(int row)
{
    files_.moveUp(row);
}

void GeneratorModel::moveFileDown(int row)
{
    files_.moveDown(row);
}

void GeneratorModel::clearFiles()
{
    files_.clear();
}

void GeneratorModel::addTargetHardwareId(const QString& id)
{
    const QString trimmed = id.trimmed();
    if (trimmed.isEmpty())
    {
        return;
    }
    QStringList ids = target_hardware_ids_;
    ids.append(trimmed);
    setTargetHardwareIds(ids);
}

void GeneratorModel::removeTargetHardwareId(int row)
{
    if (row < 0 || row >= target_hardware_ids_.size())
    {
        return;
    }
    QStringList ids = target_hardware_ids_;
    ids.removeAt(row);
    setTargetHardwareIds(ids);
}

void GeneratorModel::refresh_validation(bool edited)
{
    // The outcome describes the form as it was when it ran: once the operator changes the form,
    // it no longer says what the window now holds.
    if (edited && !busy_ && result_state_ != 0)
    {
        output_path_.clear();
        validation_findings_.clear();
        set_result(0, {}, {});
    }
    const QString load_error =
        check_text(load_part_number_, adl::arinc665::check_load_part_number(load_part_number_.toStdString()),
                   tr("Enter the load part number, for example ACM47-1234-5678."));
    const QString media_set_error = check_text(
        media_set_part_number_, adl::arinc665::check_media_set_part_number(media_set_part_number_.toStdString()),
        tr("Enter the media set part number, for example ACM-SET-0001."));
    QString header_error =
        check_text(header_file_name_, adl::arinc665::check_header_file_name(header_file_name_.toStdString()),
                   tr("Enter the header file name, for example ACM47.LUH."));
    if (header_error.isEmpty())
    {
        // The standard's rules pass; the folder tree the file is written to has a stricter one.
        header_error = SourceFileModel::portable_name_message(header_file_name_);
    }

    QString batch_error;
    if (include_batch_)
    {
        batch_error =
            check_text(batch_part_number_, adl::arinc665::check_load_part_number(batch_part_number_.toStdString()),
                       tr("Enter the batch part number, for example ACM4C-0100-0200."));
        if (batch_error.isEmpty())
        {
            const std::string batch_file_name =
                batch_part_number_.left(3).toStdString() + "." + std::string{adl::arinc665::batch_file_extension};
            const auto findings = adl::arinc665::check_batch_file_name(batch_file_name);
            if (!findings.empty())
            {
                batch_error = findings_text(findings);
            }
            else
            {
                // A manufacturer's code such as NUL gives the batch file NUL.LUB, a device name.
                batch_error = SourceFileModel::portable_name_message(QString::fromStdString(batch_file_name));
            }
        }
    }

    QString ids_error;
    for (const QString& id : target_hardware_ids_)
    {
        const auto findings = adl::arinc665::check_target_hardware_id(id.toStdString());
        if (!findings.empty())
        {
            ids_error = tr("%1: %2").arg(id, findings_text(findings));
            break;
        }
    }

    QString output_error;
    if (output_directory_.isEmpty())
    {
        output_error = tr("Choose an output folder.");
    }
    else
    {
        const QFileInfo directory(output_directory_);
        if (!directory.exists() || !directory.isDir())
        {
            output_error = tr("The output folder does not exist.");
        }
    }

    QString files_error;
    if (files_.data_file_count() == 0)
    {
        files_error = tr("Choose at least one data file.");
    }
    else if (!files_.all_valid())
    {
        files_error = tr("Fix the marked files.");
    }

    const auto assign = [](QString& stored, const QString& value, const auto& notify)
    {
        if (stored != value)
        {
            stored = value;
            notify();
        }
    };
    assign(load_part_number_error_, load_error, [this] { Q_EMIT loadPartNumberErrorChanged(); });
    assign(media_set_part_number_error_, media_set_error, [this] { Q_EMIT mediaSetPartNumberErrorChanged(); });
    assign(batch_part_number_error_, batch_error, [this] { Q_EMIT batchPartNumberErrorChanged(); });
    assign(header_file_name_error_, header_error, [this] { Q_EMIT headerFileNameErrorChanged(); });
    assign(target_hardware_ids_error_, ids_error, [this] { Q_EMIT targetHardwareIdsErrorChanged(); });
    assign(output_directory_error_, output_error, [this] { Q_EMIT outputDirectoryErrorChanged(); });
    assign(files_error_, files_error, [this] { Q_EMIT filesErrorChanged(); });

    const bool can = !busy_ && load_error.isEmpty() && media_set_error.isEmpty() && batch_error.isEmpty() &&
                     header_error.isEmpty() && ids_error.isEmpty() && output_error.isEmpty() && files_error.isEmpty();
    if (can != can_generate_)
    {
        can_generate_ = can;
        Q_EMIT canGenerateChanged();
    }
}

void GeneratorModel::on_finished(const GenerateOutcome& outcome)
{
    busy_ = false;
    Q_EMIT busyChanged();
    set_progress(outcome.state == 1 ? 100 : -1);
    set_result(outcome.state, outcome.text, outcome.detail);
    output_path_ = outcome.output_path;
    validation_findings_ = outcome.findings;
    Q_EMIT resultChanged();
    refresh_validation(false);
}

void GeneratorModel::retranslate()
{
    // The fields' messages are made by the validation; the result and the options are read again.
    // The form has not been edited, so the outcome of the last run and its Open folder button stay.
    refresh_validation(false);
    fill_load_type_model();
    files_.retranslate();
    Q_EMIT resultChanged();
}

void GeneratorModel::set_result(int state, const Message& text, const Message& detail)
{
    result_state_ = state;
    result_text_ = text;
    result_detail_ = detail;
    Q_EMIT resultChanged();
}

void GeneratorModel::set_progress(int percent)
{
    if (percent != progress_percent_)
    {
        progress_percent_ = percent;
        Q_EMIT progressChanged();
    }
}

} // namespace adl::gen

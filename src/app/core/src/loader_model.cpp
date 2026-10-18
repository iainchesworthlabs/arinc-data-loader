#include "adl/app/loader_model.hpp"

#include <QDir>
#include <QMetaObject>
#include <QTimer>

#include "adl/app/address.hpp"

namespace adl::app {

LoaderModel::LoaderModel(HostFactory host_factory, std::unique_ptr<AppSettings> settings,
                         QObject* parent)
    : QObject(parent),
      settings_(settings != nullptr ? std::move(settings) : std::make_unique<AppSettings>()) {
    register_metatypes();

    // The operator's preferences: the log's length now, and the target fields pre-filled (they
    // are filled again whenever the operator clears them).
    events_.set_capacity(settings_->logCapacity());
    target_address_ = settings_->defaultTargetAddress();
    thw_id_pos_ = settings_->defaultThwIdPos();

    worker_ = new EngineWorker(std::move(host_factory));
    worker_->moveToThread(&thread_);
    connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);

    // The worker's thread emits these; the receiver (this) lives on the window's thread, so Qt
    // queues them and they arrive here in the order they were emitted.
    connect(worker_, &EngineWorker::log_line, this,
            [this](const EventEntry& entry) { events_.append(entry); });
    connect(worker_, &EngineWorker::targets_found, this, [this](const QList<FoundTarget>& found) {
        targets_.reset(found);
        selected_target_row_ = -1;
        Q_EMIT selectedTargetChanged();
    });
    connect(worker_, &EngineWorker::configuration_read, this,
            [this](const QList<HardwareRow>& rows) { configuration_.reset(rows); });
    connect(worker_, &EngineWorker::media_set_read, this, [this](const MediaSetSummary& summary) {
        media_set_ = summary;
        // A set that cannot be read keeps the loads of the last readable one, so a transient
        // problem (a file gone for a moment) does not throw the operator's ticks away; nothing
        // can be uploaded while the error is set, and the next successful read replaces them.
        if (summary.error.isEmpty()) {
            loads_.reset(summary.loads);
            files_.reset(summary.files);
        }
        Q_EMIT mediaSetChanged();
        refresh_availability();
    });
    connect(
        worker_, &EngineWorker::validation_finished, this, [this](const ValidationReport& report) {
            QString detail;
            if (report.error.isEmpty()) {
                detail = tr("%1 load(s), %2 file(s), all check values verified")
                             .arg(report.loads)
                             .arg(report.files);
                if (!report.findings.isEmpty()) {
                    detail += tr("; %n warning(s) against the standard's recommendations", "",
                                 static_cast<int>(report.findings.size()));
                }
            }
            set_validation(report.error.isEmpty() ? 2 : 3, report.error, report.findings, detail);
        });
    connect(worker_, &EngineWorker::upload_progress, this,
            [this](int list_ratio, const QList<LoadProgressRow>& rows) {
                loads_.apply_progress(rows);
                set_progress(list_ratio);
            });
    connect(&loads_, &LoadModel::selection_changed, this, [this] { refresh_availability(); });
    connect(worker_, &EngineWorker::finished, this,
            [this](const OperationSummary& summary) { on_finished(summary); });

    thread_.setObjectName(QStringLiteral("engine"));
    thread_.start();

    // The media set of the last session is read again once the window is up, quietly: it does
    // not hold the start-up up, and a set that is gone leaves nothing behind.
    QTimer::singleShot(0, this, [this] { restore_last_media_set(); });
    refresh_availability();
}

LoaderModel::~LoaderModel() {
    worker_->request_abort();
    thread_.quit();
    thread_.wait();
}

QString LoaderModel::activity() const {
    switch (job_) {
        case Job::none:
            return {};
        case Job::find:
            return tr("Looking for targets...");
        case Job::information:
            return abort_requested_ ? tr("Stopping the operation...")
                                    : tr("Reading the target's configuration...");
        case Job::upload:
            return abort_requested_ ? tr("Stopping the operation...")
                                    : tr("Uploading to the target...");
        case Job::media_set:
            return tr("Reading the media set...");
        case Job::validate:
            return tr("Checking the media set...");
    }
    return {};
}

void LoaderModel::setTargetAddress(const QString& address) {
    QString trimmed = address.trimmed();
    if (trimmed.isEmpty()) {
        // The operator cleared the field: the default comes back.
        trimmed = settings_->defaultTargetAddress();
    }
    if (trimmed == target_address_) {
        return;
    }
    target_address_ = trimmed;
    Q_EMIT targetChanged();
    refresh_availability();
}

void LoaderModel::setThwIdPos(const QString& thw_id_pos) {
    QString trimmed = thw_id_pos.trimmed();
    if (trimmed.isEmpty()) {
        trimmed = settings_->defaultThwIdPos();
    }
    if (trimmed == thw_id_pos_) {
        return;
    }
    thw_id_pos_ = trimmed;
    Q_EMIT targetChanged();
    refresh_availability();
}

bool LoaderModel::targetValid() const {
    return parse_ipv4(target_address_).has_value() && !thw_id_pos_.isEmpty();
}

QString LoaderModel::mediaSetTitle() const {
    if (media_set_.path.isEmpty() || !media_set_.error.isEmpty()) {
        return {};
    }
    return tr("%1: %n member(s), %2 load(s)", "", media_set_.members)
        .arg(media_set_.part_number)
        .arg(media_set_.loads.size());
}

bool LoaderModel::canUpload() const {
    // The set has to have been checked valid (validationState 2): choosing a set leaves it not
    // valid until the check has run and passed; findings do not count against it.
    return !busy() && (targetValid() || settings_->autoFindBeforeOperation()) &&
           media_set_.error.isEmpty() && selectedLoadCount() > 0 && validation_state_ == 2;
}

const FoundTarget* LoaderModel::selected_target() const {
    return selected_target_row_ >= 0 ? targets_.at(selected_target_row_) : nullptr;
}

QString LoaderModel::selectedTargetHardwareId() const {
    const FoundTarget* target = selected_target();
    return target != nullptr ? target->hardware_id : QString{};
}

QString LoaderModel::selectedTargetPosition() const {
    const FoundTarget* target = selected_target();
    return target != nullptr ? target->position : QString{};
}

QString LoaderModel::selectedTargetTypeName() const {
    const FoundTarget* target = selected_target();
    return target != nullptr ? target->type_name : QString{};
}

QString LoaderModel::selectedTargetLiteralName() const {
    const FoundTarget* target = selected_target();
    return target != nullptr ? target->literal_name : QString{};
}

QString LoaderModel::selectedTargetManufacturer() const {
    const FoundTarget* target = selected_target();
    return target != nullptr ? target->manufacturer_code : QString{};
}

QString LoaderModel::selectedTargetAddress() const {
    const FoundTarget* target = selected_target();
    return target != nullptr ? target->address : QString{};
}

void LoaderModel::setDefaultTargetAddress(const QString& address) {
    const QString trimmed = address.trimmed();
    if (trimmed == settings_->defaultTargetAddress()) {
        return;
    }
    settings_->setDefaultTargetAddress(trimmed);
    if (target_address_.isEmpty()) {
        target_address_ = trimmed;
        Q_EMIT targetChanged();
        refresh_availability();
    }
    Q_EMIT preferencesChanged();
}

void LoaderModel::setDefaultThwIdPos(const QString& thw_id_pos) {
    const QString trimmed = thw_id_pos.trimmed();
    if (trimmed == settings_->defaultThwIdPos()) {
        return;
    }
    settings_->setDefaultThwIdPos(trimmed);
    if (thw_id_pos_.isEmpty()) {
        thw_id_pos_ = trimmed;
        Q_EMIT targetChanged();
        refresh_availability();
    }
    Q_EMIT preferencesChanged();
}

void LoaderModel::setControlPort(int port) {
    if (port == settings_->controlPort()) {
        return;
    }
    settings_->setControlPort(port);
    Q_EMIT preferencesChanged();
}

void LoaderModel::setDownloadDirectory(const QString& directory) {
    if (directory == settings_->downloadDirectory()) {
        return;
    }
    settings_->setDownloadDirectory(directory);
    Q_EMIT preferencesChanged();
}

void LoaderModel::setAutoFindBeforeOperation(bool enabled) {
    if (enabled == settings_->autoFindBeforeOperation()) {
        return;
    }
    settings_->setAutoFindBeforeOperation(enabled);
    refresh_availability();
    Q_EMIT preferencesChanged();
}

void LoaderModel::setValidateOnOpen(bool enabled) {
    if (enabled == settings_->validateOnOpen()) {
        return;
    }
    settings_->setValidateOnOpen(enabled);
    Q_EMIT preferencesChanged();
}

void LoaderModel::setLogCapacity(int capacity) {
    if (capacity == settings_->logCapacity()) {
        return;
    }
    settings_->setLogCapacity(capacity);
    events_.set_capacity(settings_->logCapacity());
    Q_EMIT preferencesChanged();
}

void LoaderModel::refresh_availability() {
    const bool can_read = canReadInformation();
    if (can_read != could_read_information_) {
        could_read_information_ = can_read;
        Q_EMIT canReadInformationChanged();
    }
    const bool can_upload = canUpload();
    const int selected = selectedLoadCount();
    if (can_upload != could_upload_ || selected != selected_loads_) {
        could_upload_ = can_upload;
        selected_loads_ = selected;
        Q_EMIT canUploadChanged();
    }
}

void LoaderModel::set_progress(int percent) {
    if (percent != progress_) {
        progress_ = percent;
        Q_EMIT progressChanged();
    }
}

void LoaderModel::set_job(Job job) {
    const bool was_busy = busy();
    const bool could_abort = canAbort();
    job_ = job;
    abort_requested_ = false;
    loads_.set_locked(busy());
    if (was_busy != busy()) {
        Q_EMIT busyChanged();
    }
    if (could_abort != canAbort()) {
        Q_EMIT canAbortChanged();
    }
    refresh_availability();
}

bool LoaderModel::begin(Job job) {
    if (busy()) {
        return false;
    }
    // A request left over from an earlier job must not abort this one.
    worker_->clear_abort();
    set_result(0, {}, {});
    set_progress(-1);
    set_job(job);
    return true;
}

void LoaderModel::on_finished(const OperationSummary& summary) {
    set_result(summary.success ? 1 : 2, summary.outcome, summary.detail);
    if (!summary.success && !quiet_job_) {
        set_error(summary.outcome, summary.detail, summary.hint);
    }
    const Job ended = job_;
    set_job(Job::none);
    if (ended == Job::media_set) {
        if (!media_set_.error.isEmpty()) {
            // The set could not be read: it cannot be valid, and the reason is shown.
            set_validation(3, media_set_.error, {}, {});
            quiet_job_ = false;
        } else if (settings_->validateOnOpen()) {
            checkMediaSet();
        } else {
            quiet_job_ = false;
        }
    } else if (ended == Job::validate) {
        quiet_job_ = false;
    } else if (ended == Job::find && pending_ != Pending::none) {
        run_pending_after_find(summary.success);
    }
}

void LoaderModel::set_result(int state, const QString& text, const QString& detail) {
    result_state_ = state;
    result_text_ = text;
    result_detail_ = detail;
    Q_EMIT resultChanged();
}

void LoaderModel::set_error(const QString& text, const QString& detail, const QString& hint) {
    last_error_text_ = text;
    last_error_detail_ = detail;
    last_error_hint_ = hint;
    Q_EMIT errorChanged();
}

void LoaderModel::set_validation(int state, const QString& error, const QStringList& findings,
                                 const QString& detail) {
    validation_state_ = state;
    validation_error_ = error;
    validation_findings_ = findings;
    validation_detail_ = detail;
    Q_EMIT validationChanged();
    refresh_availability();
}

void LoaderModel::run_pending_after_find(bool found) {
    const Pending pending = pending_;
    pending_ = Pending::none;
    if (!found) {
        return;  // the FIND failure is already shown
    }
    if (targets_.rowCount() != 1) {
        // None or several: the FIND result stays on the screen, and the operation is not started.
        set_result(1, result_text_,
                   tr("Choose the target from the list, then run the operation again."));
        return;
    }
    selectTarget(0);
    if (pending == Pending::information) {
        readInformation();
    } else {
        uploadSelected();
    }
}

void LoaderModel::chooseMediaSet(const QUrl& directory) {
    if (!begin(Job::media_set)) {
        return;
    }
    // A new set is not the one that was checked; the check (with validateOnOpen) follows the
    // read.
    set_validation(0, {}, {}, {});
    const QString path = directory.isLocalFile() ? directory.toLocalFile() : directory.toString();
    const QString native = QDir::toNativeSeparators(path);
    settings_->setLastMediaSetDirectory(native);
    Q_EMIT preferencesChanged();
    QMetaObject::invokeMethod(worker_, &EngineWorker::read_media_set, Qt::QueuedConnection, native);
}

void LoaderModel::checkMediaSet() {
    if (media_set_.path.isEmpty() || !begin(Job::validate)) {
        return;
    }
    set_validation(1, {}, {}, {});
    QMetaObject::invokeMethod(worker_, &EngineWorker::validate_media_set, Qt::QueuedConnection,
                              QDir::toNativeSeparators(media_set_.path));
}

void LoaderModel::restore_last_media_set() {
    const QString path = settings_->lastMediaSetDirectory();
    if (path.isEmpty() || !QDir(path).exists()) {
        return;
    }
    quiet_job_ = true;
    chooseMediaSet(QUrl::fromLocalFile(path));
}

void LoaderModel::selectAllLoads(bool selected) {
    loads_.check_all(selected);
}

void LoaderModel::findTargets(const QString& address) {
    if (!begin(Job::find)) {
        return;
    }
    QMetaObject::invokeMethod(worker_, &EngineWorker::find, Qt::QueuedConnection,
                              address.trimmed());
}

void LoaderModel::selectTarget(int row) {
    const FoundTarget* target = targets_.at(row);
    if (target == nullptr) {
        return;
    }
    setTargetAddress(target->address);
    setThwIdPos(target->thw_id_pos());
    if (selected_target_row_ != row) {
        selected_target_row_ = row;
        Q_EMIT selectedTargetChanged();
    }
}

void LoaderModel::readInformation() {
    if (!targetValid()) {
        // The operator's preference: look for the target first, and go on only when exactly one
        // answers.
        if (!settings_->autoFindBeforeOperation() || !begin(Job::find)) {
            return;
        }
        pending_ = Pending::information;
        events_.append({Severity::info, tr("No target is selected; looking for one first.")});
        QMetaObject::invokeMethod(worker_, &EngineWorker::find, Qt::QueuedConnection, QString{});
        return;
    }
    if (!begin(Job::information)) {
        return;
    }
    configuration_.reset({});
    QMetaObject::invokeMethod(worker_, &EngineWorker::read_information, Qt::QueuedConnection,
                              target_address_, thw_id_pos_,
                              static_cast<quint16>(settings_->controlPort()));
}

void LoaderModel::uploadSelected() {
    if (!canUpload()) {
        return;
    }
    if (!targetValid()) {
        if (!begin(Job::find)) {
            return;
        }
        pending_ = Pending::upload;
        events_.append({Severity::info, tr("No target is selected; looking for one first.")});
        QMetaObject::invokeMethod(worker_, &EngineWorker::find, Qt::QueuedConnection, QString{});
        return;
    }
    if (!begin(Job::upload)) {
        return;
    }
    loads_.begin_upload();
    QMetaObject::invokeMethod(worker_, &EngineWorker::upload, Qt::QueuedConnection, target_address_,
                              thw_id_pos_, loads_.checked_loads(),
                              static_cast<quint16>(settings_->controlPort()));
}

void LoaderModel::abort() {
    if (!canAbort()) {
        return;
    }
    abort_requested_ = true;
    worker_->request_abort();
    Q_EMIT busyChanged();  // the activity text changes
    Q_EMIT canAbortChanged();
}

void LoaderModel::clearLog() {
    events_.clear();
}

void LoaderModel::dismissError() {
    if (last_error_text_.isEmpty() && last_error_detail_.isEmpty() && last_error_hint_.isEmpty()) {
        return;
    }
    last_error_text_.clear();
    last_error_detail_.clear();
    last_error_hint_.clear();
    Q_EMIT errorChanged();
}

}  // namespace adl::app

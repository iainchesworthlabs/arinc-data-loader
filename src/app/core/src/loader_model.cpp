#include "adl/app/loader_model.hpp"

#include <QDir>
#include <QMetaObject>
#include <QTimer>

#include "adl/app/address.hpp"

#include <adl/profiling.hpp>

namespace adl::app {

LoaderModel::LoaderModel(HostFactory host_factory, std::unique_ptr<AppSettings> settings,
                         QObject* parent)
    : QObject(parent),
      settings_(settings != nullptr ? std::move(settings) : std::make_unique<AppSettings>()) {
    register_metatypes();

    // The operator's preferences: the log's length now, and the target fields filled in - the
    // target of the last session where there was one (so a session starts where the last left
    // off), the defaults otherwise (and again whenever the operator clears a field).
    events_.set_capacity(settings_->logCapacity());
    target_address_ = settings_->lastTargetAddress();
    if (target_address_.isEmpty()) {
        target_address_ = settings_->defaultTargetAddress();
    }
    thw_id_pos_ = settings_->lastThwIdPos();
    if (thw_id_pos_.isEmpty()) {
        thw_id_pos_ = settings_->defaultThwIdPos();
    }

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
                const QString loads =
                    report.loads == 1 ? tr("1 load") : tr("%1 loads").arg(report.loads);
                const QString files =
                    report.files == 1 ? tr("1 file") : tr("%1 files").arg(report.files);
                detail = tr("%1, %2, all check values verified").arg(loads, files);
                if (!report.findings.isEmpty()) {
                    detail += report.findings.size() == 1
                                  ? tr("; 1 warning against the standard's recommendations")
                                  : tr("; %1 warnings against the standard's recommendations")
                                        .arg(report.findings.size());
                }
            }
            set_validation(report.error.isEmpty() ? 2 : 3, report.error, report.findings, detail);
        });
    connect(worker_, &EngineWorker::upload_progress, this,
            [this](int list_ratio, const QList<LoadProgressRow>& rows) {
                loads_.apply_progress(rows);
                set_progress(list_ratio);
            });
    connect(worker_, &EngineWorker::download_progress, this,
            [this](int list_ratio, const QList<FileProgressRow>& rows) {
                downloads_.apply_progress(rows);
                set_progress(list_ratio);
            });
    connect(worker_, &EngineWorker::download_list, this,
            [this](const QList<DownloadFileRow>& rows) {
                downloads_.set_locked(false);
                downloads_.reset_operator(rows);
                download_choice_pending_ = true;
                Q_EMIT downloadChoicePendingChanged();
            });
    connect(&loads_, &LoadModel::selection_changed, this, [this] { refresh_availability(); });
    connect(&downloads_, &DownloadModel::selection_changed, this, [this] {
        const int selected = downloads_.checked_count();
        if (selected != download_selected_count_) {
            download_selected_count_ = selected;
            Q_EMIT downloadSelectionChanged();
        }
        refresh_availability();
    });
    connect(worker_, &EngineWorker::finished, this,
            [this](const OperationSummary& summary) { on_finished(summary); });

    thread_.setObjectName(QStringLiteral("engine"));
    connect(&thread_, &QThread::started, worker_, [] { adl::profiling::thread_name("engine"); });
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
        case Job::download_media:
        case Job::download_operator:
            return abort_requested_ ? tr("Stopping the operation...")
                                    : tr("Downloading from the target...");
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
    reconcile_target();
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
    reconcile_target();
    Q_EMIT targetChanged();
    refresh_availability();
}

// What was observed against one target does not describe another. Editing the address or the
// THW_ID_POS drops the FIND selection, the configuration read from the previous target and a
// transfer receipt whose inputs no longer hold; the activity log keeps the history.
void LoaderModel::reconcile_target() {
    if (selected_target_row_ != -1) {
        selected_target_row_ = -1;
        Q_EMIT selectedTargetChanged();
    }
    configuration_.reset({});
    if (result_is_transfer_) {
        set_result(0, {}, {});
    }
}

bool LoaderModel::targetValid() const {
    return parse_ipv4(target_address_).has_value() && !thw_id_pos_.isEmpty();
}

QString LoaderModel::mediaSetTitle() const {
    if (media_set_.path.isEmpty() || !media_set_.error.isEmpty()) {
        return {};
    }
    const QString members =
        media_set_.members == 1 ? tr("1 member") : tr("%1 members").arg(media_set_.members);
    const QString loads =
        media_set_.loads.size() == 1 ? tr("1 load") : tr("%1 loads").arg(media_set_.loads.size());
    return tr("%1: %2, %3").arg(media_set_.part_number, members, loads);
}

bool LoaderModel::canUpload() const {
    // The set has to have been checked valid (validationState 2): choosing a set leaves it not
    // valid until the check has run and passed; findings do not count against it.
    return !busy() && (targetValid() || settings_->autoFindBeforeOperation()) &&
           media_set_.error.isEmpty() && selectedLoadCount() > 0 && validation_state_ == 2;
}

bool LoaderModel::canDownload() const {
    if (busy() || !(targetValid() || settings_->autoFindBeforeOperation())) {
        return false;
    }
    if (settings_->downloadDirectory().trimmed().isEmpty()) {
        return false;
    }
    // The media defined mode needs at least one name; the operator defined mode needs nothing
    // more than the target and the directory (the target's list comes while it runs).
    return download_mode_ == 1 || downloads_.rowCount() > 0;
}

void LoaderModel::setDownloadMode(int mode) {
    if (mode != 0 && mode != 1) {
        return;
    }
    if (mode == download_mode_) {
        return;
    }
    download_mode_ = mode;
    // The two modes are different lists (the operator's own names, or the target's list to
    // choose from): switching starts the new mode with an empty one.
    if (mode == 0) {
        downloads_.reset_media({});
    } else {
        downloads_.reset_operator({});
    }
    Q_EMIT downloadChanged();
    refresh_availability();
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

void LoaderModel::setDownloadDirectory(const QString& chosen) {
    // A FolderDialog's selectedFolder is a QUrl, and QML hands it over as "file:///...": the
    // engine is given a path.
    const QString directory = chosen.startsWith(QStringLiteral("file:"), Qt::CaseInsensitive)
                                  ? QUrl(chosen).toLocalFile()
                                  : chosen;
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
    const bool can_download = canDownload();
    if (can_download != could_download_) {
        could_download_ = can_download;
        Q_EMIT canDownloadChanged();
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
    // A request or a choice left over from an earlier job must not abort or answer this one.
    worker_->clear_controls();
    set_result(0, {}, {});
    set_progress(-1);
    if (download_choice_pending_) {
        download_choice_pending_ = false;
        Q_EMIT downloadChoicePendingChanged();
    }
    if ((job == Job::download_media || job == Job::download_operator) &&
        !last_download_directory_.isEmpty()) {
        // The directory shown is the last download's; a new one is its own.
        last_download_directory_.clear();
        Q_EMIT downloadDirectoryChanged();
    }
    // The target an operation starts against is the one the next session starts against. Only
    // an operation has a target: a media set read or a check does not.
    if (job == Job::information || job == Job::upload || job == Job::download_media ||
        job == Job::download_operator) {
        settings_->setLastTargetAddress(target_address_);
        settings_->setLastThwIdPos(thw_id_pos_);
    }
    set_job(job);
    return true;
}

void LoaderModel::on_finished(const OperationSummary& summary) {
    const Job ended = job_;
    result_is_transfer_ = ended == Job::information || ended == Job::upload ||
                          ended == Job::download_media || ended == Job::download_operator;
    result_was_download_ = ended == Job::download_media || ended == Job::download_operator;
    set_result(summary.success ? 1 : 2, summary.outcome, summary.detail);
    if (!summary.success && !quiet_job_) {
        set_error(summary.outcome, summary.detail, summary.hint);
    }
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
    if (ended == Job::download_media || ended == Job::download_operator) {
        downloads_.set_locked(false);
        if (download_choice_pending_) {
            download_choice_pending_ = false;
            Q_EMIT downloadChoicePendingChanged();
        }
        if (!summary.download_directory.isEmpty()) {
            last_download_directory_ = summary.download_directory;
            Q_EMIT downloadDirectoryChanged();
        }
    }
}

void LoaderModel::set_result(int state, const QString& text, const QString& detail) {
    if (state == 0) {
        result_is_transfer_ = false;
        result_was_download_ = false;
    }
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
    switch (pending) {
        case Pending::information:
            readInformation();
            break;
        case Pending::upload:
            uploadSelected();
            break;
        case Pending::download:
            downloadSelected();
            break;
        case Pending::none:
            break;
    }
}

void LoaderModel::chooseMediaSet(const QUrl& directory) {
    const adl::profiling::Zone zone{"app: choose media set"};
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
    const adl::profiling::Zone zone{"app: check media set"};
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
    const adl::profiling::Zone zone{"app: find targets"};
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
    const adl::profiling::Zone zone{"app: read information"};
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
    const adl::profiling::Zone zone{"app: upload selected"};
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

void LoaderModel::downloadSelected() {
    const adl::profiling::Zone zone{"app: download selected"};
    if (!canDownload()) {
        return;
    }
    if (!targetValid()) {
        if (!begin(Job::find)) {
            return;
        }
        pending_ = Pending::download;
        events_.append({Severity::info, tr("No target is selected; looking for one first.")});
        QMetaObject::invokeMethod(worker_, &EngineWorker::find, Qt::QueuedConnection, QString{});
        return;
    }
    const Job job = download_mode_ == 0 ? Job::download_media : Job::download_operator;
    if (!begin(job)) {
        return;
    }
    downloads_.set_locked(true);
    if (job == Job::download_operator) {
        // The target's list comes while the operation runs; until it has arrived, the rows of
        // the last list (if any) are gone and the window says it is waiting.
        downloads_.reset_operator({});
    } else {
        downloads_.begin_download();
    }
    const quint16 port = static_cast<quint16>(settings_->controlPort());
    if (job == Job::download_media) {
        QMetaObject::invokeMethod(worker_, &EngineWorker::download_media, Qt::QueuedConnection,
                                  target_address_, thw_id_pos_, downloads_.names(),
                                  settings_->downloadDirectory(), port);
    } else {
        QMetaObject::invokeMethod(worker_, &EngineWorker::download_operator, Qt::QueuedConnection,
                                  target_address_, thw_id_pos_, settings_->downloadDirectory(),
                                  port);
    }
}

void LoaderModel::addDownloadFile(const QString& name) {
    if (busy()) {
        return;
    }
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    if (downloads_.rowCount() >= static_cast<int>(adl::arinc615a::DataLoader::max_expected_files)) {
        return;
    }
    static_cast<void>(downloads_.add_name(trimmed));
}

void LoaderModel::removeDownloadFile(int row) {
    if (busy()) {
        return;
    }
    downloads_.remove_row(row);
}

void LoaderModel::confirmDownloadSelection() {
    if (!download_choice_pending_) {
        return;
    }
    download_choice_pending_ = false;
    const QStringList chosen = downloads_.checked_names();
    // An empty choice is "none": the gate turns it into the cancellation of the operation (what
    // a listener answering SelectionDecision::cancelled would do).
    downloads_.set_locked(true);
    downloads_.begin_download();
    worker_->post_selection(chosen);
    Q_EMIT downloadChoicePendingChanged();
}

void LoaderModel::chooseNoDownloadFiles() {
    if (!download_choice_pending_) {
        return;
    }
    download_choice_pending_ = false;
    downloads_.set_locked(true);
    worker_->post_selection({});
    Q_EMIT downloadChoicePendingChanged();
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

void LoaderModel::completeFirstRun() {
    if (settings_->firstRunComplete()) {
        return;
    }
    settings_->setFirstRunComplete(true);
    // The first-run fields are the preferences; the window starts with them, so the operator's
    // first operation does not need to retype what was just entered.
    setTargetAddress(settings_->defaultTargetAddress());
    setThwIdPos(settings_->defaultThwIdPos());
    Q_EMIT firstRunChanged();
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

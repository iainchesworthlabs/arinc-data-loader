#include "adl/app/loader_model.hpp"

#include <QDir>
#include <QMetaObject>

#include "adl/app/address.hpp"

namespace adl::app {

LoaderModel::LoaderModel(HostFactory host_factory, QObject* parent) : QObject(parent) {
    register_metatypes();

    worker_ = new EngineWorker(std::move(host_factory));
    worker_->moveToThread(&thread_);
    connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);

    // The worker's thread emits these; the receiver (this) lives on the window's thread, so Qt
    // queues them and they arrive here in the order they were emitted.
    connect(worker_, &EngineWorker::log_line, this,
            [this](const EventEntry& entry) { events_.append(entry); });
    connect(worker_, &EngineWorker::targets_found, this,
            [this](const QList<FoundTarget>& found) { targets_.reset(found); });
    connect(worker_, &EngineWorker::configuration_read, this,
            [this](const QList<HardwareRow>& rows) { configuration_.reset(rows); });
    connect(worker_, &EngineWorker::media_set_read, this, [this](const MediaSetSummary& summary) {
        media_set_ = summary;
        loads_.reset(summary.loads);
        Q_EMIT mediaSetChanged();
        refresh_availability();
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
    }
    return {};
}

void LoaderModel::setTargetAddress(const QString& address) {
    const QString trimmed = address.trimmed();
    if (trimmed == target_address_) {
        return;
    }
    target_address_ = trimmed;
    Q_EMIT targetChanged();
    refresh_availability();
}

void LoaderModel::setThwIdPos(const QString& thw_id_pos) {
    const QString trimmed = thw_id_pos.trimmed();
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
    return !busy() && targetValid() && media_set_.error.isEmpty() && selectedLoadCount() > 0;
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
    set_job(Job::none);
}

void LoaderModel::set_result(int state, const QString& text, const QString& detail) {
    result_state_ = state;
    result_text_ = text;
    result_detail_ = detail;
    Q_EMIT resultChanged();
}

void LoaderModel::chooseMediaSet(const QUrl& directory) {
    if (!begin(Job::media_set)) {
        return;
    }
    const QString path = directory.isLocalFile() ? directory.toLocalFile() : directory.toString();
    QMetaObject::invokeMethod(worker_, &EngineWorker::read_media_set, Qt::QueuedConnection,
                              QDir::toNativeSeparators(path));
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
}

void LoaderModel::readInformation() {
    if (!targetValid() || !begin(Job::information)) {
        return;
    }
    configuration_.reset({});
    QMetaObject::invokeMethod(worker_, &EngineWorker::read_information, Qt::QueuedConnection,
                              target_address_, thw_id_pos_);
}

void LoaderModel::uploadSelected() {
    if (!canUpload() || !begin(Job::upload)) {
        return;
    }
    loads_.begin_upload();
    QMetaObject::invokeMethod(worker_, &EngineWorker::upload, Qt::QueuedConnection, target_address_,
                              thw_id_pos_, loads_.checked_loads());
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

}  // namespace adl::app

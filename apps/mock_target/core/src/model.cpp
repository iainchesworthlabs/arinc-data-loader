// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/mock/model.hpp"

#include "adl/mock/settings.hpp"
#include "adl/mock/storage.hpp"
#include "adl/platform/host.hpp"
#include "adl/platform/paths.hpp"

#include <QDir>
#include <QMetaObject>
#include <QTime>
#include <QUrl>
#include <string>

namespace adl::mock
{

namespace
{

[[nodiscard]] QString to_qstring(std::string_view text)
{
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

[[nodiscard]] QString time_now()
{
    return QTime::currentTime().toString(QStringLiteral("hh:mm:ss"));
}

} // namespace

// ---- The list models -------------------------------------------------------------------------

ActivityModel::ActivityModel(QObject* parent) : QAbstractListModel(parent) {}

int ActivityModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(entries_.size());
}

QVariant ActivityModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
    {
        return {};
    }
    const LogEntry& entry = entries_[static_cast<std::size_t>(index.row())];
    switch (role)
    {
    case SeverityRole:
        return entry.severity;
    case TimeRole:
        return entry.time;
    case TextRole:
        return entry.text;
    default:
        return {};
    }
}

QHash<int, QByteArray> ActivityModel::roleNames() const
{
    return {{SeverityRole, "severity"}, {TimeRole, "time"}, {TextRole, "message"}};
}

void ActivityModel::append(const LogEntry& entry)
{
    beginInsertRows({}, rowCount(), rowCount());
    entries_.push_back(entry);
    endInsertRows();
}

void ActivityModel::clear()
{
    beginResetModel();
    entries_.clear();
    endResetModel();
}

StoredFilesModel::StoredFilesModel(QObject* parent) : QAbstractListModel(parent) {}

int StoredFilesModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QVariant StoredFilesModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount())
    {
        return {};
    }
    const StoredFileRow& row = rows_[static_cast<std::size_t>(index.row())];
    switch (role)
    {
    case NameRole:
        return row.name;
    case SizeRole:
        return row.size;
    case PathRole:
        return row.path;
    default:
        return {};
    }
}

QHash<int, QByteArray> StoredFilesModel::roleNames() const
{
    return {{NameRole, "name"}, {SizeRole, "size"}, {PathRole, "path"}};
}

void StoredFilesModel::append(const QList<StoredFileRow>& rows)
{
    if (rows.isEmpty())
    {
        return;
    }
    beginInsertRows({}, rowCount(), rowCount() + static_cast<int>(rows.size()) - 1);
    for (const StoredFileRow& row : rows)
    {
        rows_.push_back(row);
    }
    endInsertRows();
}

// ---- The worker ------------------------------------------------------------------------------

LinkFactory system_link_factory()
{
    return []() -> std::expected<std::unique_ptr<TargetLink>, QString>
    {
        auto host = adl::platform::make_host();
        if (!host)
        {
            return std::unexpected(QStringLiteral("the platform could not start (%1)")
                                       .arg(QString::fromLatin1(adl::platform::to_string(host.error().code))));
        }
        return std::unique_ptr<TargetLink>(std::make_unique<RealTargetLink>(std::move(*host)));
    };
}

// What the engine reports, copied into Qt values and emitted as queued signals. It runs on the
// worker's thread, inside the engine's loop.
class MockWorker::Adapter final : public EngineListener
{
public:
    explicit Adapter(MockWorker& worker) : worker_(worker) {}

    void log(Severity severity, std::string_view text) override
    {
        LogEntry entry;
        entry.severity = static_cast<int>(severity);
        entry.time = time_now();
        entry.text = to_qstring(text);
        Q_EMIT worker_.log_line(entry);
    }

    void operation_changed(std::string_view name) override
    {
        Q_EMIT worker_.operation_changed(to_qstring(name));
    }

    void progress_changed(int percent) override
    {
        Q_EMIT worker_.progress_changed(percent);
    }

    void upload_received(const UploadRecord& record) override
    {
        worker_.store_upload(record);
    }

private:
    MockWorker& worker_;
};

MockWorker::MockWorker(LinkFactory factory, QObject* parent)
    : QObject(parent), factory_(std::move(factory)), adapter_(std::make_unique<Adapter>(*this))
{
}

MockWorker::~MockWorker() = default;

void MockWorker::request_stop() noexcept
{
    const std::scoped_lock lock{link_mutex_};
    stopped_through_ = sessions_queued_;
    if (current_link_ != nullptr)
    {
        current_link_->request_stop();
    }
}

void MockWorker::session_queued() noexcept
{
    const std::scoped_lock lock{link_mutex_};
    ++sessions_queued_;
}

void MockWorker::start_session(Config config, QString storage_directory)
{
    std::uint64_t mine = 0;
    {
        const std::scoped_lock lock{link_mutex_};
        mine = ++sessions_begun_;
        // Stopped before it started.
        if (mine <= stopped_through_)
        {
            return;
        }
    }
    storage_path_ = storage_directory.trimmed().isEmpty() ? std::filesystem::path{}
                                                          : std::filesystem::path(storage_directory.toStdU16String());

    auto made = factory_();
    if (!made)
    {
        Q_EMIT error_occurred(made.error());
        return;
    }
    link_ = std::move(*made);
    {
        const std::scoped_lock lock{link_mutex_};
        current_link_ = link_.get();
        // A stop that came while the link was being made found no link to stop.
        if (mine <= stopped_through_)
        {
            link_->request_stop();
        }
    }
    engine_ = std::make_unique<MockEngine>(adapter_.get());
    link_->attach(*engine_);

    read_holds(config);
    if (const auto error = engine_->start(*link_, std::move(config)))
    {
        Q_EMIT error_occurred(to_qstring(*error));
    }
    else
    {
        engine_->run();
        if (!link_->stop_requested() && !engine_->last_error().empty())
        {
            Q_EMIT error_occurred(to_qstring(engine_->last_error()));
        }
    }
    // Cleared before the link is destroyed: a stop that arrives now finds no link.
    {
        const std::scoped_lock lock{link_mutex_};
        current_link_ = nullptr;
    }
    engine_.reset();
    link_.reset();
}

void MockWorker::read_holds(Config& config)
{
    if (!storage_path_.empty())
    {
        // A folder that is not there (or cannot be listed) holds nothing: the demonstration file
        // below is served instead.
        if (auto held = read_held_files(storage_path_ / "holds"))
        {
            config.holds = std::move(*held);
        }
    }
    if (config.holds.empty())
    {
        config.holds.push_back(demonstration_file());
    }
}

void MockWorker::store_upload(const UploadRecord& record)
{
    const StoreOutcome outcome = adl::mock::store_upload(storage_path_, record);
    const auto say = [](const StoreIssue& issue)
    {
        switch (issue.problem)
        {
        case StoreProblem::no_storage_folder:
            return tr("The storage folder is not set, so the upload is not stored.");
        case StoreProblem::storage_folder_failed:
            return tr("The storage folder could not be made: %1.").arg(to_qstring(issue.detail));
        case StoreProblem::load_folder_failed:
            return tr("The load's folder could not be made: %1.").arg(to_qstring(issue.detail));
        case StoreProblem::unsafe_file_name:
            return tr("%1 was not stored: not a safe file name.").arg(to_qstring(issue.subject));
        case StoreProblem::file_write_failed:
            return tr("%1 could not be written to the storage folder.").arg(to_qstring(issue.subject));
        }
        return QString{};
    };
    for (const StoreIssue& issue : outcome.issues)
    {
        Q_EMIT log_line({static_cast<int>(Severity::warning), time_now(), say(issue)});
    }
    QList<StoredFileRow> rows;
    for (const StoredOnDisk& file : outcome.stored)
    {
        StoredFileRow row;
        row.name = to_qstring(file.name);
        row.size = static_cast<qint64>(file.size);
        row.path = QDir::toNativeSeparators(to_qstring(adl::platform::to_utf8(file.path.u8string())));
        rows.append(row);
    }
    if (!rows.isEmpty())
    {
        Q_EMIT stored_files_changed(rows);
    }
}

// ---- The model -------------------------------------------------------------------------------

MockTargetModel::MockTargetModel(LinkFactory factory, QObject* parent) : QObject(parent)
{
    qRegisterMetaType<adl::mock::Config>();
    qRegisterMetaType<adl::mock::LogEntry>();
    qRegisterMetaType<adl::mock::StoredFileRow>();

    worker_ = new MockWorker(std::move(factory));
    worker_->moveToThread(&thread_);
    connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);

    // The worker's thread emits these; the receiver (this) lives on the window's thread, so Qt
    // queues them and they arrive here in the order they were emitted.
    connect(worker_, &MockWorker::log_line, this,
            [this](const LogEntry& entry)
            {
                activity_.append(entry);
                Q_EMIT logLine(entry);
            });
    connect(worker_, &MockWorker::operation_changed, this,
            [this](const QString& name)
            {
                if (name != current_operation_)
                {
                    current_operation_ = name;
                    Q_EMIT operationChanged();
                }
            });
    connect(worker_, &MockWorker::progress_changed, this,
            [this](int percent)
            {
                if (percent != progress_)
                {
                    progress_ = percent;
                    Q_EMIT progressChanged();
                }
            });
    connect(worker_, &MockWorker::stored_files_changed, this,
            [this](const QList<StoredFileRow>& rows)
            {
                stored_files_.append(rows);
                Q_EMIT storedFilesChanged();
            });
    connect(worker_, &MockWorker::error_occurred, this,
            [this](const QString& text)
            {
                set_error(adl::ui::Message::verbatim(text));
                if (running_)
                {
                    running_ = false;
                    Q_EMIT runningChanged();
                }
                Q_EMIT errorOccurred();
            });

    thread_.setObjectName(QStringLiteral("mock-engine"));
    thread_.start();
}

MockTargetModel::~MockTargetModel()
{
    worker_->request_stop();
    thread_.quit();
    thread_.wait();
}

void MockTargetModel::set_error(const adl::ui::Message& text)
{
    if (text.text() == error_text_.text())
    {
        return;
    }
    error_text_ = text;
    Q_EMIT errorTextChanged();
}

void MockTargetModel::retranslate()
{
    Q_EMIT errorTextChanged();
}

void MockTargetModel::setHardwareId(const QString& value)
{
    if (value == hardware_id_)
    {
        return;
    }
    hardware_id_ = value;
    Q_EMIT identityChanged();
}

void MockTargetModel::setTypeName(const QString& value)
{
    if (value == type_name_)
    {
        return;
    }
    type_name_ = value;
    Q_EMIT identityChanged();
}

void MockTargetModel::setPosition(const QString& value)
{
    if (value == position_)
    {
        return;
    }
    position_ = value;
    Q_EMIT identityChanged();
}

void MockTargetModel::setLiteralName(const QString& value)
{
    if (value == literal_name_)
    {
        return;
    }
    literal_name_ = value;
    Q_EMIT identityChanged();
}

void MockTargetModel::setManufacturerCode(const QString& value)
{
    if (value == manufacturer_code_)
    {
        return;
    }
    manufacturer_code_ = value;
    Q_EMIT identityChanged();
}

void MockTargetModel::setTftpPort(int value)
{
    if (value == tftp_port_)
    {
        return;
    }
    tftp_port_ = value;
    Q_EMIT portsChanged();
}

void MockTargetModel::setFindPort(int value)
{
    if (value == find_port_)
    {
        return;
    }
    find_port_ = value;
    Q_EMIT portsChanged();
}

void MockTargetModel::setLoopbackOnly(bool value)
{
    if (value == loopback_only_)
    {
        return;
    }
    loopback_only_ = value;
    Q_EMIT portsChanged();
}

void MockTargetModel::setThinkTimeMs(int value)
{
    if (value == think_time_ms_)
    {
        return;
    }
    think_time_ms_ = value;
    Q_EMIT behaviourChanged();
}

void MockTargetModel::setStatusPeriodSec(int value)
{
    if (value == status_period_sec_)
    {
        return;
    }
    status_period_sec_ = value;
    Q_EMIT behaviourChanged();
}

void MockTargetModel::setStorageDirectory(const QString& value)
{
    QString path = value.trimmed();
    if (path.startsWith(QStringLiteral("file:")))
    {
        path = QUrl(path).toLocalFile(); // what a FolderDialog hands over
    }
    if (path == storage_directory_)
    {
        return;
    }
    storage_directory_ = path;
    Q_EMIT storageDirectoryChanged();
}

void MockTargetModel::setWaitFirstSeconds(int value)
{
    if (value == wait_first_seconds_)
    {
        return;
    }
    wait_first_seconds_ = value;
    Q_EMIT injectionsChanged();
}

void MockTargetModel::setWaitEveryRequest(bool value)
{
    if (value == wait_every_request_)
    {
        return;
    }
    wait_every_request_ = value;
    Q_EMIT injectionsChanged();
}

void MockTargetModel::setRefuseOperation(const QString& value)
{
    if (value == refuse_operation_)
    {
        return;
    }
    refuse_operation_ = value;
    Q_EMIT injectionsChanged();
}

void MockTargetModel::setDropStatusEvery(int value)
{
    if (value == drop_status_every_)
    {
        return;
    }
    drop_status_every_ = value;
    Q_EMIT injectionsChanged();
}

void MockTargetModel::setCorruptServedFile(bool value)
{
    if (value == corrupt_served_file_)
    {
        return;
    }
    corrupt_served_file_ = value;
    Q_EMIT injectionsChanged();
}

void MockTargetModel::setWrongChecksum(bool value)
{
    if (value == wrong_checksum_)
    {
        return;
    }
    wrong_checksum_ = value;
    Q_EMIT injectionsChanged();
}

void MockTargetModel::setAbortAfterStatuses(int value)
{
    if (value == abort_after_statuses_)
    {
        return;
    }
    abort_after_statuses_ = value;
    Q_EMIT injectionsChanged();
}

void MockTargetModel::setSilent(bool value)
{
    if (value == silent_)
    {
        return;
    }
    silent_ = value;
    Q_EMIT injectionsChanged();
}

Config MockTargetModel::make_config() const
{
    TargetSettings settings;
    settings.hardware_id = hardware_id_.trimmed().toStdString();
    settings.type_name = type_name_.trimmed().toStdString();
    settings.position = position_.trimmed().toStdString();
    settings.literal_name = literal_name_.trimmed().toStdString();
    settings.manufacturer_code = manufacturer_code_.trimmed().toStdString();
    settings.tftp_port = static_cast<std::uint16_t>(tftp_port_);
    settings.find_port = static_cast<std::uint16_t>(find_port_);
    settings.loopback_only = loopback_only_;
    settings.think_time_ms = think_time_ms_;
    settings.status_period_sec = status_period_sec_;
    settings.wait_first_seconds = wait_first_seconds_;
    settings.wait_every_request = wait_every_request_;
    settings.drop_status_every = drop_status_every_;
    settings.corrupt_served_file = corrupt_served_file_;
    settings.wrong_checksum = wrong_checksum_;
    settings.abort_after_statuses = abort_after_statuses_;
    settings.silent = silent_;
    if (refuse_operation_ == QLatin1String("information"))
    {
        settings.refuse_operation = OperationKind::information;
    }
    else if (refuse_operation_ == QLatin1String("upload"))
    {
        settings.refuse_operation = OperationKind::upload;
    }
    else if (refuse_operation_ == QLatin1String("downloadMedia"))
    {
        settings.refuse_operation = OperationKind::download_media;
    }
    else if (refuse_operation_ == QLatin1String("downloadOperator"))
    {
        settings.refuse_operation = OperationKind::download_operator;
    }
    return adl::mock::make_config(settings);
}

void MockTargetModel::start()
{
    if (running_)
    {
        return;
    }
    if (tftp_port_ < 1 || tftp_port_ > 65535 || find_port_ < 1 || find_port_ > 65535)
    {
        set_error(adl::ui::Message::translated(
            &MockTargetModel::tr, QT_TR_NOOP("The TFTP and FIND ports must each be between 1 and 65535.")));
        return;
    }
    set_error({});
    worker_->session_queued();
    running_ = true;
    Q_EMIT runningChanged();
    QMetaObject::invokeMethod(worker_, &MockWorker::start_session, Qt::QueuedConnection, make_config(),
                              storage_directory_);
}

void MockTargetModel::stop()
{
    if (!running_)
    {
        return;
    }
    worker_->request_stop();
    running_ = false;
    Q_EMIT runningChanged();
}

void MockTargetModel::clearLog()
{
    activity_.clear();
}

} // namespace adl::mock

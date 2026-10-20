#include "adl/mock/model.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <string>

#include <QDir>
#include <QMetaObject>
#include <QTime>
#include <QUrl>

#include "adl/arinc615a/limits.hpp"
#include "adl/platform/host.hpp"

namespace adl::mock {

namespace {

[[nodiscard]] QString to_qstring(std::string_view text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

[[nodiscard]] QString to_qstring(const std::u8string& text) {
    return QString::fromUtf8(reinterpret_cast<const char*>(text.data()),
                             static_cast<qsizetype>(text.size()));
}

[[nodiscard]] Bytes to_bytes(std::string_view text) {
    Bytes bytes(text.size());
    std::transform(text.begin(), text.end(), bytes.begin(),
                   [](char c) { return std::byte{static_cast<std::uint8_t>(c)}; });
    return bytes;
}

[[nodiscard]] QString time_now() {
    return QTime::currentTime().toString(QStringLiteral("hh:mm:ss"));
}

// A file or directory name the target's data may be stored under: letters, digits and a few
// marks, never a separator, so a name from the wire cannot reach outside the storage.
[[nodiscard]] bool safe_name(std::string_view name) noexcept {
    if (name.empty() || name.size() > 254) {
        return false;
    }
    for (const char c : name) {
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                        (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
        if (!ok) {
            return false;
        }
    }
    return name != "." && name != "..";
}

[[nodiscard]] std::string sanitize_name(std::string_view name) {
    std::string result;
    result.reserve(name.size());
    for (const char c : name) {
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                        (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
        result.push_back(ok ? c : '_');
    }
    if (result == "." || result == "..") {
        return "_";
    }
    return result;
}

[[nodiscard]] std::filesystem::path fs_path(std::string_view utf8) {
    return std::filesystem::path(to_qstring(utf8).toStdU16String());
}

}  // namespace

// ---- The list models -------------------------------------------------------------------------

ActivityModel::ActivityModel(QObject* parent) : QAbstractListModel(parent) {}

int ActivityModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(entries_.size());
}

QVariant ActivityModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }
    const LogEntry& entry = entries_[static_cast<std::size_t>(index.row())];
    switch (role) {
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

QHash<int, QByteArray> ActivityModel::roleNames() const {
    return {{SeverityRole, "severity"}, {TimeRole, "time"}, {TextRole, "message"}};
}

void ActivityModel::append(const LogEntry& entry) {
    beginInsertRows({}, rowCount(), rowCount());
    entries_.push_back(entry);
    endInsertRows();
}

void ActivityModel::clear() {
    beginResetModel();
    entries_.clear();
    endResetModel();
}

StoredFilesModel::StoredFilesModel(QObject* parent) : QAbstractListModel(parent) {}

int StoredFilesModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

QVariant StoredFilesModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }
    const StoredFileRow& row = rows_[static_cast<std::size_t>(index.row())];
    switch (role) {
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

QHash<int, QByteArray> StoredFilesModel::roleNames() const {
    return {{NameRole, "name"}, {SizeRole, "size"}, {PathRole, "path"}};
}

void StoredFilesModel::append(const QList<StoredFileRow>& rows) {
    if (rows.isEmpty()) {
        return;
    }
    beginInsertRows({}, rowCount(), rowCount() + static_cast<int>(rows.size()) - 1);
    for (const StoredFileRow& row : rows) {
        rows_.push_back(row);
    }
    endInsertRows();
}

// ---- The worker ------------------------------------------------------------------------------

LinkFactory system_link_factory() {
    return []() -> std::expected<std::unique_ptr<TargetLink>, QString> {
        auto host = adl::platform::make_host();
        if (!host) {
            return std::unexpected(
                QStringLiteral("the platform could not start (%1)")
                    .arg(QString::fromLatin1(adl::platform::to_string(host.error().code))));
        }
        return std::unique_ptr<TargetLink>(std::make_unique<RealTargetLink>(std::move(*host)));
    };
}

// What the engine reports, copied into Qt values and emitted as queued signals. It runs on the
// worker's thread, inside the engine's loop.
class MockWorker::Adapter final : public EngineListener {
public:
    explicit Adapter(MockWorker& worker) : worker_(worker) {}

    void log(Severity severity, std::string_view text) override {
        LogEntry entry;
        entry.severity = static_cast<int>(severity);
        entry.time = time_now();
        entry.text = to_qstring(text);
        Q_EMIT worker_.log_line(entry);
    }

    void operation_changed(std::string_view name) override {
        Q_EMIT worker_.operation_changed(to_qstring(name));
    }

    void progress_changed(int percent) override { Q_EMIT worker_.progress_changed(percent); }

    void upload_received(const UploadRecord& record) override { worker_.store_upload(record); }

private:
    MockWorker& worker_;
};

MockWorker::MockWorker(LinkFactory factory, QObject* parent)
    : QObject(parent), factory_(std::move(factory)), adapter_(std::make_unique<Adapter>(*this)) {}

MockWorker::~MockWorker() = default;

void MockWorker::request_stop() noexcept {
    stop_wanted_.store(true, std::memory_order_release);
    const std::scoped_lock lock{link_mutex_};
    if (current_link_ != nullptr) {
        current_link_->request_stop();
    }
}

void MockWorker::clear_stop() noexcept {
    stop_wanted_.store(false, std::memory_order_release);
}

void MockWorker::start_session(Config config, QString storage_directory) {
    // A stop that arrived before the session began: it was stopped before it started.
    if (stop_wanted_.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    storage_path_ = storage_directory.trimmed().isEmpty()
                        ? std::filesystem::path{}
                        : std::filesystem::path(storage_directory.toStdU16String());

    auto made = factory_();
    if (!made) {
        Q_EMIT error_occurred(made.error());
        return;
    }
    link_ = std::move(*made);
    {
        const std::scoped_lock lock{link_mutex_};
        current_link_ = link_.get();
    }
    engine_ = std::make_unique<MockEngine>(adapter_.get());
    link_->attach(*engine_);

    read_holds(config);
    if (const auto error = engine_->start(*link_, std::move(config))) {
        Q_EMIT error_occurred(to_qstring(*error));
    } else {
        engine_->run();
        if (!link_->stop_requested() && !engine_->last_error().empty()) {
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

void MockWorker::read_holds(Config& config) {
    if (!storage_path_.empty()) {
        const std::filesystem::path holds = storage_path_ / "holds";
        std::error_code ec;
        if (std::filesystem::is_directory(holds, ec)) {
            std::vector<HeldFile> found;
            // The non-throwing iteration: a folder the account cannot list (or one that changes
            // while it is read) ends the list, it does not end the program.
            std::filesystem::directory_iterator it(holds, ec);
            for (const std::filesystem::directory_iterator end; !ec && it != end;
                 it.increment(ec)) {
                const std::filesystem::directory_entry& entry = *it;
                if (!entry.is_regular_file(ec)) {
                    ec.clear();
                    continue;
                }
                std::ifstream in(entry.path(), std::ios::binary);
                const std::vector<char> text{std::istreambuf_iterator<char>(in),
                                             std::istreambuf_iterator<char>()};
                Bytes bytes(text.size());
                std::transform(text.begin(), text.end(), bytes.begin(),
                               [](char c) { return std::byte{static_cast<std::uint8_t>(c)}; });
                const std::u8string name = entry.path().filename().u8string();
                found.push_back(
                    HeldFile{std::string{reinterpret_cast<const char*>(name.data()), name.size()},
                             std::move(bytes), ""});
            }
            std::ranges::sort(found,
                              [](const HeldFile& a, const HeldFile& b) { return a.name < b.name; });
            config.holds = std::move(found);
        }
    }
    if (config.holds.empty()) {
        const std::string note =
            "The mock target serves the files it holds from <storage>/holds in a download.\n"
            "Put files there and ask for them by name.\n";
        config.holds.push_back(HeldFile{"DEMO.TXT", to_bytes(note), "a demonstration file"});
    }
}

void MockWorker::store_upload(const UploadRecord& record) {
    if (storage_path_.empty()) {
        Q_EMIT log_line({static_cast<int>(Severity::warning), time_now(),
                         tr("The storage folder is not set, so the upload is not stored.")});
        return;
    }
    std::error_code ec;
    std::filesystem::create_directories(storage_path_, ec);
    if (ec) {
        Q_EMIT log_line({static_cast<int>(Severity::warning), time_now(),
                         tr("The storage folder could not be made: %1.")
                             .arg(QString::fromStdString(ec.message()))});
        return;
    }
    const std::filesystem::path part_dir =
        storage_path_ / fs_path(sanitize_name(record.load_part_number));
    std::filesystem::create_directories(part_dir, ec);
    if (ec) {
        Q_EMIT log_line({static_cast<int>(Severity::warning), time_now(),
                         tr("The load's folder could not be made: %1.")
                             .arg(QString::fromStdString(ec.message()))});
        return;
    }
    QList<StoredFileRow> rows;
    for (const StoredFile& file : record.files) {
        if (!safe_name(file.name)) {
            Q_EMIT log_line(
                {static_cast<int>(Severity::warning), time_now(),
                 tr("%1 was not stored: not a safe file name.").arg(to_qstring(file.name))});
            continue;
        }
        const std::filesystem::path path = part_dir / fs_path(file.name);
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(file.bytes.data()),
                  static_cast<std::streamsize>(file.bytes.size()));
        out.close();
        if (!out) {
            Q_EMIT log_line(
                {static_cast<int>(Severity::warning), time_now(),
                 tr("%1 could not be written to the storage folder.").arg(to_qstring(file.name))});
            continue;
        }
        StoredFileRow row;
        row.name = to_qstring(file.name);
        row.size = static_cast<qint64>(file.bytes.size());
        row.path = QDir::toNativeSeparators(to_qstring(path.u8string()));
        rows.append(row);
    }
    if (!rows.isEmpty()) {
        Q_EMIT stored_files_changed(rows);
    }
}

// ---- The model -------------------------------------------------------------------------------

MockTargetModel::MockTargetModel(LinkFactory factory, QObject* parent) : QObject(parent) {
    qRegisterMetaType<adl::mock::Config>();
    qRegisterMetaType<adl::mock::LogEntry>();
    qRegisterMetaType<adl::mock::StoredFileRow>();

    worker_ = new MockWorker(std::move(factory));
    worker_->moveToThread(&thread_);
    connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);

    // The worker's thread emits these; the receiver (this) lives on the window's thread, so Qt
    // queues them and they arrive here in the order they were emitted.
    connect(worker_, &MockWorker::log_line, this, [this](const LogEntry& entry) {
        activity_.append(entry);
        Q_EMIT logLine(entry);
    });
    connect(worker_, &MockWorker::operation_changed, this, [this](const QString& name) {
        if (name != current_operation_) {
            current_operation_ = name;
            Q_EMIT operationChanged();
        }
    });
    connect(worker_, &MockWorker::progress_changed, this, [this](int percent) {
        if (percent != progress_) {
            progress_ = percent;
            Q_EMIT progressChanged();
        }
    });
    connect(worker_, &MockWorker::stored_files_changed, this,
            [this](const QList<StoredFileRow>& rows) {
                stored_files_.append(rows);
                Q_EMIT storedFilesChanged();
            });
    connect(worker_, &MockWorker::error_occurred, this, [this](const QString& text) {
        set_error(text);
        if (running_) {
            running_ = false;
            Q_EMIT runningChanged();
        }
        Q_EMIT errorOccurred();
    });

    thread_.setObjectName(QStringLiteral("mock-engine"));
    thread_.start();
}

MockTargetModel::~MockTargetModel() {
    worker_->request_stop();
    thread_.quit();
    thread_.wait();
}

void MockTargetModel::set_error(const QString& text) {
    if (text == error_text_) {
        return;
    }
    error_text_ = text;
    Q_EMIT errorTextChanged();
}

void MockTargetModel::setHardwareId(const QString& value) {
    if (value == hardware_id_) {
        return;
    }
    hardware_id_ = value;
    Q_EMIT identityChanged();
}

void MockTargetModel::setTypeName(const QString& value) {
    if (value == type_name_) {
        return;
    }
    type_name_ = value;
    Q_EMIT identityChanged();
}

void MockTargetModel::setPosition(const QString& value) {
    if (value == position_) {
        return;
    }
    position_ = value;
    Q_EMIT identityChanged();
}

void MockTargetModel::setLiteralName(const QString& value) {
    if (value == literal_name_) {
        return;
    }
    literal_name_ = value;
    Q_EMIT identityChanged();
}

void MockTargetModel::setManufacturerCode(const QString& value) {
    if (value == manufacturer_code_) {
        return;
    }
    manufacturer_code_ = value;
    Q_EMIT identityChanged();
}

void MockTargetModel::setTftpPort(int value) {
    if (value == tftp_port_) {
        return;
    }
    tftp_port_ = value;
    Q_EMIT portsChanged();
}

void MockTargetModel::setFindPort(int value) {
    if (value == find_port_) {
        return;
    }
    find_port_ = value;
    Q_EMIT portsChanged();
}

void MockTargetModel::setLoopbackOnly(bool value) {
    if (value == loopback_only_) {
        return;
    }
    loopback_only_ = value;
    Q_EMIT portsChanged();
}

void MockTargetModel::setThinkTimeMs(int value) {
    if (value == think_time_ms_) {
        return;
    }
    think_time_ms_ = value;
    Q_EMIT behaviourChanged();
}

void MockTargetModel::setStatusPeriodSec(int value) {
    if (value == status_period_sec_) {
        return;
    }
    status_period_sec_ = value;
    Q_EMIT behaviourChanged();
}

void MockTargetModel::setStorageDirectory(const QString& value) {
    QString path = value.trimmed();
    if (path.startsWith(QStringLiteral("file:"))) {
        path = QUrl(path).toLocalFile();  // what a FolderDialog hands over
    }
    if (path == storage_directory_) {
        return;
    }
    storage_directory_ = path;
    Q_EMIT storageDirectoryChanged();
}

void MockTargetModel::setWaitFirstSeconds(int value) {
    if (value == wait_first_seconds_) {
        return;
    }
    wait_first_seconds_ = value;
    Q_EMIT injectionsChanged();
}

void MockTargetModel::setWaitEveryRequest(bool value) {
    if (value == wait_every_request_) {
        return;
    }
    wait_every_request_ = value;
    Q_EMIT injectionsChanged();
}

void MockTargetModel::setRefuseOperation(const QString& value) {
    if (value == refuse_operation_) {
        return;
    }
    refuse_operation_ = value;
    Q_EMIT injectionsChanged();
}

void MockTargetModel::setDropStatusEvery(int value) {
    if (value == drop_status_every_) {
        return;
    }
    drop_status_every_ = value;
    Q_EMIT injectionsChanged();
}

void MockTargetModel::setCorruptServedFile(bool value) {
    if (value == corrupt_served_file_) {
        return;
    }
    corrupt_served_file_ = value;
    Q_EMIT injectionsChanged();
}

void MockTargetModel::setWrongChecksum(bool value) {
    if (value == wrong_checksum_) {
        return;
    }
    wrong_checksum_ = value;
    Q_EMIT injectionsChanged();
}

void MockTargetModel::setAbortAfterStatuses(int value) {
    if (value == abort_after_statuses_) {
        return;
    }
    abort_after_statuses_ = value;
    Q_EMIT injectionsChanged();
}

void MockTargetModel::setSilent(bool value) {
    if (value == silent_) {
        return;
    }
    silent_ = value;
    Q_EMIT injectionsChanged();
}

Config MockTargetModel::make_config() const {
    Config config;
    const QString hardware = hardware_id_.trimmed();
    const QString at_position = position_.trimmed();
    config.thw = hardware.toStdString() +
                 (at_position.isEmpty() ? std::string{} : "_" + at_position.toStdString());
    config.find_answer = adl::arinc615a::FindAnswer{
        hardware.toStdString(), type_name_.trimmed().toStdString(), at_position.toStdString(),
        literal_name_.trimmed().toStdString(), manufacturer_code_.trimmed().toStdString()};
    config.tftp_port = static_cast<std::uint16_t>(tftp_port_);
    config.find_port = static_cast<std::uint16_t>(find_port_);
    if (loopback_only_) {
        config.bind_address = adl::arinc615a::Address::loopback();
    }
    config.think_time = std::chrono::milliseconds(think_time_ms_);
    config.status_period = std::chrono::seconds(status_period_sec_);
    config.wait_first_seconds = static_cast<std::uint16_t>(
        std::clamp(wait_first_seconds_, 0, static_cast<int>(adl::arinc615a::max_wait_seconds)));
    config.wait_always = wait_every_request_;
    config.drop_status_every = static_cast<unsigned>(std::max(drop_status_every_, 0));
    config.corrupt_served = corrupt_served_file_;
    config.wrong_checksum = wrong_checksum_;
    if (abort_after_statuses_ > 0) {
        config.abort_after_statuses = static_cast<std::size_t>(abort_after_statuses_);
    }
    config.silent = silent_;

    const auto make_refusal = [](OperationKind kind) {
        Refusal refusal;
        refusal.kind = kind;
        refusal.code = adl::arinc615a::StatusCode::operation_denied;
        refusal.text = "refused by the mock target";
        return refusal;
    };
    if (refuse_operation_ == QLatin1String("information")) {
        config.refusal = make_refusal(OperationKind::information);
    } else if (refuse_operation_ == QLatin1String("upload")) {
        config.refusal = make_refusal(OperationKind::upload);
    } else if (refuse_operation_ == QLatin1String("downloadMedia")) {
        config.refusal = make_refusal(OperationKind::download_media);
    } else if (refuse_operation_ == QLatin1String("downloadOperator")) {
        config.refusal = make_refusal(OperationKind::download_operator);
    }
    return config;
}

void MockTargetModel::start() {
    if (running_) {
        return;
    }
    if (tftp_port_ < 1 || tftp_port_ > 65535 || find_port_ < 1 || find_port_ > 65535) {
        set_error(tr("The TFTP and FIND ports must each be between 1 and 65535."));
        return;
    }
    set_error({});
    worker_->clear_stop();
    running_ = true;
    Q_EMIT runningChanged();
    QMetaObject::invokeMethod(worker_, &MockWorker::start_session, Qt::QueuedConnection,
                              make_config(), storage_directory_);
}

void MockTargetModel::stop() {
    if (!running_) {
        return;
    }
    worker_->request_stop();
    running_ = false;
    Q_EMIT runningChanged();
}

void MockTargetModel::clearLog() {
    activity_.clear();
}

}  // namespace adl::mock

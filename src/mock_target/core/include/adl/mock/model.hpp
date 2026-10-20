#pragma once

// What the window talks to: the state of the mock target as properties, and the few things the
// operator can do as invokable functions. It lives on the window's thread and owns the thread
// the engine runs on (the worker), turning the worker's signals into model changes. The worker
// slot takes a complete snapshot of the configuration plus the link a factory made, so the
// window uses the real link (adl::platform's sockets) and the tests hand over the fake one.
//
// Everything the engine reports is copied into Qt values before the worker emits a signal, so
// the signals are queued calls carrying copies, in order, to the window's thread.

#include <atomic>
#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include <QAbstractListModel>
#include <QList>
#include <QObject>
#include <QString>
#include <QThread>

#include "adl/mock/engine.hpp"
#include "adl/mock/link.hpp"
#include "adl/ui/message.hpp"

namespace adl::mock {

// One line of the activity log.
struct LogEntry {
    int severity = 0;  // adl::mock::Severity
    QString time;
    QString text;
};

// One file the target has stored, as the stored-files list shows it.
struct StoredFileRow {
    QString name;
    qint64 size = 0;
    QString path;
};

// The activity log: what the engine reported, oldest first, with its severity, time and text.
class ActivityModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        SeverityRole = Qt::UserRole,
        TimeRole,
        // Named "message" in QML because a delegate of a Label cannot carry a "text" role
        // (Label has a final property of that name).
        TextRole,
    };

    explicit ActivityModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void append(const LogEntry& entry);
    void clear();

private:
    std::vector<LogEntry> entries_;
};

// The files of the uploads the target has received, as stored under the storage directory.
class StoredFilesModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        NameRole = Qt::UserRole,
        SizeRole,
        PathRole,
    };

    explicit StoredFilesModel(QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void append(const QList<StoredFileRow>& rows);

private:
    std::vector<StoredFileRow> rows_;
};

// Makes the link a session runs over, on the worker's thread: the real one over
// adl::platform, or a test's fake one. The error is for the operator.
using LinkFactory = std::function<std::expected<std::unique_ptr<TargetLink>, QString>()>;

// The real link, over adl::platform's sockets (make_host on the worker's thread).
[[nodiscard]] LinkFactory system_link_factory();

// The engine on its own thread. The engine loop blocks, so the window never calls it: start()
// queues one session (a configuration snapshot), which runs until the stop is asked for. The
// stop cannot be a queued call (the worker's thread is inside the loop), so it is a flag the
// link carries and the loop polls.
class MockWorker : public QObject {
    Q_OBJECT

public:
    explicit MockWorker(LinkFactory factory, QObject* parent = nullptr);
    ~MockWorker() override;

    // Thread safe. Asks the session that is running (or the next one to start) to stop.
    void request_stop() noexcept;
    // Forgets a stop left over from an earlier session. Called before a session is queued.
    void clear_stop() noexcept;

public Q_SLOTS:
    // One session: makes the link, starts the engine with `config`, runs its loop until the
    // stop is asked for (or the loop fails), and reports through the signals.
    void start_session(adl::mock::Config config, QString storage_directory);

Q_SIGNALS:
    void log_line(adl::mock::LogEntry entry);
    void operation_changed(QString name);
    void progress_changed(int percent);
    void stored_files_changed(QList<adl::mock::StoredFileRow> rows);
    void error_occurred(QString text);

private:
    class Adapter;

    // The files a download serves: everything in <storage>/holds, or one demonstration file.
    void read_holds(adl::mock::Config& config);
    // A completed upload, written under <storage>/<load part number>/ and reported.
    void store_upload(const UploadRecord& record);

    LinkFactory factory_;
    std::unique_ptr<TargetLink> link_;
    std::unique_ptr<MockEngine> engine_;
    std::filesystem::path storage_path_;
    std::unique_ptr<Adapter> adapter_;
    // The link a stop request is delivered to. request_stop() runs on the GUI thread while the
    // session thread owns and destroys the link, so the pointer is only used, and only cleared,
    // under this mutex: a stop never calls into a link that is being destroyed.
    std::mutex link_mutex_;
    TargetLink* current_link_ = nullptr;
    std::atomic<bool> stop_wanted_{false};
};

class MockTargetModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)

    // The identity the target reports in FIND and in its protocol file names.
    Q_PROPERTY(QString hardwareId READ hardwareId WRITE setHardwareId NOTIFY identityChanged)
    Q_PROPERTY(QString typeName READ typeName WRITE setTypeName NOTIFY identityChanged)
    Q_PROPERTY(QString position READ position WRITE setPosition NOTIFY identityChanged)
    Q_PROPERTY(QString literalName READ literalName WRITE setLiteralName NOTIFY identityChanged)
    Q_PROPERTY(QString manufacturerCode READ manufacturerCode WRITE setManufacturerCode NOTIFY
                   identityChanged)

    // The ports it listens on.
    Q_PROPERTY(int tftpPort READ tftpPort WRITE setTftpPort NOTIFY portsChanged)
    Q_PROPERTY(int findPort READ findPort WRITE setFindPort NOTIFY portsChanged)
    // Listen on this computer only (127.0.0.1) rather than on every network interface. The
    // operating system's firewall asks about a program only when it listens on the network.
    Q_PROPERTY(bool loopbackOnly READ loopbackOnly WRITE setLoopbackOnly NOTIFY portsChanged)

    // How it works.
    Q_PROPERTY(int thinkTimeMs READ thinkTimeMs WRITE setThinkTimeMs NOTIFY behaviourChanged)
    Q_PROPERTY(
        int statusPeriodSec READ statusPeriodSec WRITE setStatusPeriodSec NOTIFY behaviourChanged)
    Q_PROPERTY(QString storageDirectory READ storageDirectory WRITE setStorageDirectory NOTIFY
                   storageDirectoryChanged)

    // The injections (all default to "behave").
    Q_PROPERTY(int waitFirstSeconds READ waitFirstSeconds WRITE setWaitFirstSeconds NOTIFY
                   injectionsChanged)
    Q_PROPERTY(bool waitEveryRequest READ waitEveryRequest WRITE setWaitEveryRequest NOTIFY
                   injectionsChanged)
    // "" or "information" / "upload" / "downloadMedia" / "downloadOperator".
    Q_PROPERTY(QString refuseOperation READ refuseOperation WRITE setRefuseOperation NOTIFY
                   injectionsChanged)
    Q_PROPERTY(
        int dropStatusEvery READ dropStatusEvery WRITE setDropStatusEvery NOTIFY injectionsChanged)
    Q_PROPERTY(bool corruptServedFile READ corruptServedFile WRITE setCorruptServedFile NOTIFY
                   injectionsChanged)
    Q_PROPERTY(
        bool wrongChecksum READ wrongChecksum WRITE setWrongChecksum NOTIFY injectionsChanged)
    Q_PROPERTY(int abortAfterStatuses READ abortAfterStatuses WRITE setAbortAfterStatuses NOTIFY
                   injectionsChanged)
    Q_PROPERTY(bool silent READ silent WRITE setSilent NOTIFY injectionsChanged)

    // What is happening.
    Q_PROPERTY(QString currentOperation READ currentOperation NOTIFY operationChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QObject* activity READ activity CONSTANT)
    Q_PROPERTY(QObject* storedFiles READ storedFiles CONSTANT)

public:
    explicit MockTargetModel(LinkFactory factory = system_link_factory(),
                             QObject* parent = nullptr);
    // Asks the session to stop and waits for the engine's thread to finish.
    ~MockTargetModel() override;

    [[nodiscard]] bool running() const noexcept { return running_; }
    [[nodiscard]] QString errorText() const { return error_text_.text(); }

    [[nodiscard]] QString hardwareId() const { return hardware_id_; }
    [[nodiscard]] QString typeName() const { return type_name_; }
    [[nodiscard]] QString position() const { return position_; }
    [[nodiscard]] QString literalName() const { return literal_name_; }
    [[nodiscard]] QString manufacturerCode() const { return manufacturer_code_; }
    void setHardwareId(const QString& value);
    void setTypeName(const QString& value);
    void setPosition(const QString& value);
    void setLiteralName(const QString& value);
    void setManufacturerCode(const QString& value);

    [[nodiscard]] int tftpPort() const noexcept { return tftp_port_; }
    [[nodiscard]] int findPort() const noexcept { return find_port_; }
    void setTftpPort(int value);
    void setFindPort(int value);
    [[nodiscard]] bool loopbackOnly() const noexcept { return loopback_only_; }
    void setLoopbackOnly(bool value);

    [[nodiscard]] int thinkTimeMs() const noexcept { return think_time_ms_; }
    [[nodiscard]] int statusPeriodSec() const noexcept { return status_period_sec_; }
    void setThinkTimeMs(int value);
    void setStatusPeriodSec(int value);

    [[nodiscard]] QString storageDirectory() const { return storage_directory_; }
    void setStorageDirectory(const QString& value);

    [[nodiscard]] int waitFirstSeconds() const noexcept { return wait_first_seconds_; }
    [[nodiscard]] bool waitEveryRequest() const noexcept { return wait_every_request_; }
    [[nodiscard]] QString refuseOperation() const { return refuse_operation_; }
    [[nodiscard]] int dropStatusEvery() const noexcept { return drop_status_every_; }
    [[nodiscard]] bool corruptServedFile() const noexcept { return corrupt_served_file_; }
    [[nodiscard]] bool wrongChecksum() const noexcept { return wrong_checksum_; }
    [[nodiscard]] int abortAfterStatuses() const noexcept { return abort_after_statuses_; }
    [[nodiscard]] bool silent() const noexcept { return silent_; }
    void setWaitFirstSeconds(int value);
    void setWaitEveryRequest(bool value);
    void setRefuseOperation(const QString& value);
    void setDropStatusEvery(int value);
    void setCorruptServedFile(bool value);
    void setWrongChecksum(bool value);
    void setAbortAfterStatuses(int value);
    void setSilent(bool value);

    [[nodiscard]] QString currentOperation() const { return current_operation_; }
    [[nodiscard]] int progress() const noexcept { return progress_; }
    [[nodiscard]] QObject* activity() { return &activity_; }
    [[nodiscard]] QObject* storedFiles() { return &stored_files_; }

public Q_SLOTS:
    // Starts serving on the worker's thread with a snapshot of the properties.
    void start();
    // Asks the session that is running to stop.
    void stop();
    void clearLog();
    // The language changed: the error on show is said again in the new one.
    void retranslate();

Q_SIGNALS:
    void runningChanged();
    void errorTextChanged();
    void identityChanged();
    void portsChanged();
    void behaviourChanged();
    void storageDirectoryChanged();
    void injectionsChanged();
    void operationChanged();
    void progressChanged();
    // The worker's reports, for the window.
    void logLine(adl::mock::LogEntry entry);
    void storedFilesChanged();
    void errorOccurred();

private:
    [[nodiscard]] adl::mock::Config make_config() const;
    void set_error(const adl::ui::Message& text);

    MockWorker* worker_ = nullptr;
    QThread thread_;

    bool running_ = false;
    adl::ui::Message error_text_;

    QString hardware_id_ = QStringLiteral("HNPFMS");
    QString type_name_ = QStringLiteral("FMS");
    QString position_ = QStringLiteral("L");
    QString literal_name_ = QStringLiteral("FMS LEFT");
    QString manufacturer_code_ = QStringLiteral("HNP");

    int tftp_port_ = 59;
    int find_port_ = 1001;
    bool loopback_only_ = false;

    int think_time_ms_ = 1000;
    int status_period_sec_ = 4;
    QString storage_directory_;

    int wait_first_seconds_ = 0;
    bool wait_every_request_ = false;
    QString refuse_operation_;
    int drop_status_every_ = 0;
    bool corrupt_served_file_ = false;
    bool wrong_checksum_ = false;
    int abort_after_statuses_ = 0;
    bool silent_ = false;

    QString current_operation_;
    int progress_ = -1;

    ActivityModel activity_;
    StoredFilesModel stored_files_;
};

}  // namespace adl::mock

Q_DECLARE_METATYPE(adl::mock::Config)
Q_DECLARE_METATYPE(adl::mock::LogEntry)
Q_DECLARE_METATYPE(adl::mock::StoredFileRow)

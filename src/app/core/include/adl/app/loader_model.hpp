#pragma once

// What the window talks to: the state of the data loader as properties, and the few things the
// operator can do as invokable functions. It lives on the window's thread, owns the thread the
// engine runs on, and turns the worker's signals into model changes. One job runs at a time
// (FIND, an Information operation, an Upload, the reading of a media set or the checking of
// one); while one runs, `busy` is true and the others are refused.

#include <memory>

#include <QObject>
#include <QStringList>
#include <QThread>
#include <QUrl>

#include "adl/app/engine_worker.hpp"
#include "adl/app/events.hpp"
#include "adl/app/host_access.hpp"
#include "adl/app/models.hpp"
#include "adl/app/settings.hpp"

namespace adl::app {

class LoaderModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString activity READ activity NOTIFY busyChanged)
    Q_PROPERTY(bool canAbort READ canAbort NOTIFY canAbortChanged)
    // A window that has never been through the first-run screen shows it before the workbench;
    // completing it stores the preferences it was given and hands over.
    Q_PROPERTY(bool firstRun READ firstRun NOTIFY firstRunChanged)

    // The target: chosen from what FIND found, or typed.
    Q_PROPERTY(QString targetAddress READ targetAddress WRITE setTargetAddress NOTIFY targetChanged)
    Q_PROPERTY(QString thwIdPos READ thwIdPos WRITE setThwIdPos NOTIFY targetChanged)
    Q_PROPERTY(bool targetValid READ targetValid NOTIFY targetChanged)
    Q_PROPERTY(bool canReadInformation READ canReadInformation NOTIFY canReadInformationChanged)
    // The target chosen from the FIND list, for its properties: the row, or -1 for none.
    Q_PROPERTY(int selectedTargetIndex READ selectedTargetIndex NOTIFY selectedTargetChanged)
    Q_PROPERTY(
        QString selectedTargetHardwareId READ selectedTargetHardwareId NOTIFY selectedTargetChanged)
    Q_PROPERTY(
        QString selectedTargetPosition READ selectedTargetPosition NOTIFY selectedTargetChanged)
    Q_PROPERTY(
        QString selectedTargetTypeName READ selectedTargetTypeName NOTIFY selectedTargetChanged)
    Q_PROPERTY(QString selectedTargetLiteralName READ selectedTargetLiteralName NOTIFY
                   selectedTargetChanged)
    Q_PROPERTY(QString selectedTargetManufacturer READ selectedTargetManufacturer NOTIFY
                   selectedTargetChanged)
    Q_PROPERTY(
        QString selectedTargetAddress READ selectedTargetAddress NOTIFY selectedTargetChanged)
    // An upload needs a media set that was read, at least one of its loads ticked, the set
    // checked valid and a target (typed, or one the automatic FIND will find).
    Q_PROPERTY(bool canUpload READ canUpload NOTIFY canUploadChanged)
    Q_PROPERTY(int selectedLoadCount READ selectedLoadCount NOTIFY canUploadChanged)
    // How far the target's list has got in the operation that runs (percent), or -1 while
    // nothing is known: before its first status file, and for jobs that report no progress.
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)

    // A download: which mode (0 media defined, the operator names the files; 1 operator
    // defined, the target's list is chosen from), the files as the window shows them, and
    // whether the choice of an operator defined download is the operator's to make now.
    // A download needs a target, a download directory (Preferences) and, in the media defined
    // mode, at least one named file.
    Q_PROPERTY(bool canDownload READ canDownload NOTIFY canDownloadChanged)
    Q_PROPERTY(int downloadMode READ downloadMode WRITE setDownloadMode NOTIFY downloadChanged)
    Q_PROPERTY(
        bool downloadChoicePending READ downloadChoicePending NOTIFY downloadChoicePendingChanged)
    Q_PROPERTY(int downloadSelectedCount READ downloadSelectedCount NOTIFY downloadSelectionChanged)
    // The DNLD_DATA_<THW_ID_POS>_<number> directory the last download was written into, or
    // empty while none has run.
    Q_PROPERTY(
        QString lastDownloadDirectory READ lastDownloadDirectory NOTIFY downloadDirectoryChanged)

    // The outcome of the last job: 0 none yet, 1 it succeeded, 2 it did not. A media set read or
    // check also has an outcome, but it is not a transfer; `resultIsTransfer` says whether the
    // outcome belongs in the transfer receipt or only in the log.
    Q_PROPERTY(int resultState READ resultState NOTIFY resultChanged)
    Q_PROPERTY(bool resultIsTransfer READ resultIsTransfer NOTIFY resultChanged)
    Q_PROPERTY(bool resultWasDownload READ resultWasDownload NOTIFY resultChanged)
    Q_PROPERTY(QString resultText READ resultText NOTIFY resultChanged)
    Q_PROPERTY(QString resultDetail READ resultDetail NOTIFY resultChanged)
    // A failed job, for the window's error dialog: what happened, what it means, and what the
    // operator can do (when the application knows). Set on every failure, one dialog per
    // failure; `dismissError` acknowledges it.
    Q_PROPERTY(QString lastErrorText READ lastErrorText NOTIFY errorChanged)
    Q_PROPERTY(QString lastErrorDetail READ lastErrorDetail NOTIFY errorChanged)
    Q_PROPERTY(QString lastErrorHint READ lastErrorHint NOTIFY errorChanged)

    // The media set that was read.
    Q_PROPERTY(QString mediaSetPath READ mediaSetPath NOTIFY mediaSetChanged)
    Q_PROPERTY(QString mediaSetTitle READ mediaSetTitle NOTIFY mediaSetChanged)
    Q_PROPERTY(QString mediaSetPartNumber READ mediaSetPartNumber NOTIFY mediaSetChanged)
    Q_PROPERTY(int mediaSetMemberCount READ mediaSetMemberCount NOTIFY mediaSetChanged)
    Q_PROPERTY(int mediaSetBatchCount READ mediaSetBatchCount NOTIFY mediaSetChanged)
    Q_PROPERTY(int loadCount READ loadCount NOTIFY mediaSetChanged)
    Q_PROPERTY(int fileCount READ fileCount NOTIFY mediaSetChanged)
    Q_PROPERTY(QString mediaSetError READ mediaSetError NOTIFY mediaSetChanged)
    Q_PROPERTY(QStringList mediaSetFindings READ mediaSetFindings NOTIFY mediaSetChanged)

    // The media set check (a "data file set" has to be checked valid before an upload is
    // allowed). 0 not checked, 1 checking, 2 valid, 3 invalid. DECISION: the check is the
    // read, which verifies every file and its check values, so a set that reads is
    // structurally valid and uploads may go ahead; a read error makes it invalid and uploads
    // stay refused with the reason shown. The findings are deviations from the standard's
    // recommendations: they are shown as warnings and do not stop an upload.
    Q_PROPERTY(int validationState READ validationState NOTIFY validationChanged)
    Q_PROPERTY(QString validationError READ validationError NOTIFY validationChanged)
    Q_PROPERTY(QStringList validationFindings READ validationFindings NOTIFY validationChanged)
    Q_PROPERTY(QString validationDetail READ validationDetail NOTIFY validationChanged)

    Q_PROPERTY(QObject* targets READ targets CONSTANT)
    Q_PROPERTY(QObject* events READ events CONSTANT)
    Q_PROPERTY(QObject* configuration READ configuration CONSTANT)
    Q_PROPERTY(QObject* loads READ loads CONSTANT)
    Q_PROPERTY(QObject* files READ files CONSTANT)
    Q_PROPERTY(QObject* downloads READ downloads CONSTANT)

    // The operator's preferences, saved as they are changed. lastMediaSetDirectory is set by
    // choosing a media set, not typed.
    Q_PROPERTY(QString lastMediaSetDirectory READ lastMediaSetDirectory NOTIFY preferencesChanged)
    Q_PROPERTY(QString defaultTargetAddress READ defaultTargetAddress WRITE setDefaultTargetAddress
                   NOTIFY preferencesChanged)
    Q_PROPERTY(QString defaultThwIdPos READ defaultThwIdPos WRITE setDefaultThwIdPos NOTIFY
                   preferencesChanged)
    Q_PROPERTY(int controlPort READ controlPort WRITE setControlPort NOTIFY preferencesChanged)
    Q_PROPERTY(QString downloadDirectory READ downloadDirectory WRITE setDownloadDirectory NOTIFY
                   preferencesChanged)
    Q_PROPERTY(bool autoFindBeforeOperation READ autoFindBeforeOperation WRITE
                   setAutoFindBeforeOperation NOTIFY preferencesChanged)
    Q_PROPERTY(
        bool validateOnOpen READ validateOnOpen WRITE setValidateOnOpen NOTIFY preferencesChanged)
    Q_PROPERTY(int logCapacity READ logCapacity WRITE setLogCapacity NOTIFY preferencesChanged)

public:
    explicit LoaderModel(HostFactory host_factory = system_host_factory(),
                         std::unique_ptr<AppSettings> settings = {}, QObject* parent = nullptr);
    // Aborts what is running and waits for the engine's thread to finish, which for an operation
    // the target has to confirm can take up to the data load protocol time-out (13 seconds).
    ~LoaderModel() override;

    [[nodiscard]] bool busy() const noexcept { return job_ != Job::none; }
    [[nodiscard]] bool firstRun() const { return !settings_->firstRunComplete(); }
    [[nodiscard]] QString activity() const;
    [[nodiscard]] bool canAbort() const noexcept {
        return (job_ == Job::information || job_ == Job::upload || job_ == Job::download_media ||
                job_ == Job::download_operator) &&
               !abort_requested_;
    }

    [[nodiscard]] QString targetAddress() const { return target_address_; }
    [[nodiscard]] QString thwIdPos() const { return thw_id_pos_; }
    void setTargetAddress(const QString& address);
    void setThwIdPos(const QString& thw_id_pos);
    [[nodiscard]] bool targetValid() const;
    [[nodiscard]] bool canReadInformation() const {
        return !busy() && (targetValid() || settings_->autoFindBeforeOperation());
    }
    [[nodiscard]] int selectedTargetIndex() const noexcept { return selected_target_row_; }
    [[nodiscard]] QString selectedTargetHardwareId() const;
    [[nodiscard]] QString selectedTargetPosition() const;
    [[nodiscard]] QString selectedTargetTypeName() const;
    [[nodiscard]] QString selectedTargetLiteralName() const;
    [[nodiscard]] QString selectedTargetManufacturer() const;
    [[nodiscard]] QString selectedTargetAddress() const;
    [[nodiscard]] bool canUpload() const;
    [[nodiscard]] int selectedLoadCount() const noexcept { return loads_.checked_count(); }
    [[nodiscard]] int progress() const noexcept { return progress_; }

    [[nodiscard]] bool canDownload() const;
    [[nodiscard]] int downloadMode() const noexcept { return download_mode_; }
    void setDownloadMode(int mode);
    [[nodiscard]] bool downloadChoicePending() const noexcept { return download_choice_pending_; }
    [[nodiscard]] int downloadSelectedCount() const noexcept { return download_selected_count_; }
    [[nodiscard]] QString lastDownloadDirectory() const { return last_download_directory_; }

    [[nodiscard]] int resultState() const noexcept { return result_state_; }
    [[nodiscard]] bool resultIsTransfer() const noexcept { return result_is_transfer_; }
    [[nodiscard]] bool resultWasDownload() const noexcept { return result_was_download_; }
    [[nodiscard]] QString resultText() const { return result_text_; }
    [[nodiscard]] QString resultDetail() const { return result_detail_; }
    [[nodiscard]] QString lastErrorText() const { return last_error_text_; }
    [[nodiscard]] QString lastErrorDetail() const { return last_error_detail_; }
    [[nodiscard]] QString lastErrorHint() const { return last_error_hint_; }

    [[nodiscard]] QString mediaSetPath() const { return media_set_.path; }
    [[nodiscard]] QString mediaSetTitle() const;
    [[nodiscard]] QString mediaSetPartNumber() const { return media_set_.part_number; }
    [[nodiscard]] int mediaSetMemberCount() const noexcept { return media_set_.members; }
    [[nodiscard]] int mediaSetBatchCount() const noexcept { return media_set_.batches; }
    // The rows of the loads and files lists, as properties a binding can follow: the models are
    // CONSTANT objects, so a binding that calls their rowCount() is evaluated once and never again.
    [[nodiscard]] int loadCount() const { return loads_.rowCount(); }
    [[nodiscard]] int fileCount() const { return files_.rowCount(); }
    [[nodiscard]] QString mediaSetError() const { return media_set_.error; }
    [[nodiscard]] QStringList mediaSetFindings() const { return media_set_.findings; }

    [[nodiscard]] int validationState() const noexcept { return validation_state_; }
    [[nodiscard]] QString validationError() const { return validation_error_; }
    [[nodiscard]] QStringList validationFindings() const { return validation_findings_; }
    [[nodiscard]] QString validationDetail() const { return validation_detail_; }

    [[nodiscard]] QObject* targets() { return &targets_; }
    [[nodiscard]] QObject* events() { return &events_; }
    [[nodiscard]] QObject* configuration() { return &configuration_; }
    [[nodiscard]] QObject* loads() { return &loads_; }
    [[nodiscard]] QObject* files() { return &files_; }
    [[nodiscard]] QObject* downloads() { return &downloads_; }

    [[nodiscard]] QString lastMediaSetDirectory() const {
        return settings_->lastMediaSetDirectory();
    }
    [[nodiscard]] QString defaultTargetAddress() const { return settings_->defaultTargetAddress(); }
    void setDefaultTargetAddress(const QString& address);
    [[nodiscard]] QString defaultThwIdPos() const { return settings_->defaultThwIdPos(); }
    void setDefaultThwIdPos(const QString& thw_id_pos);
    [[nodiscard]] int controlPort() const { return settings_->controlPort(); }
    void setControlPort(int port);
    [[nodiscard]] QString downloadDirectory() const { return settings_->downloadDirectory(); }
    void setDownloadDirectory(const QString& directory);
    [[nodiscard]] bool autoFindBeforeOperation() const {
        return settings_->autoFindBeforeOperation();
    }
    void setAutoFindBeforeOperation(bool enabled);
    [[nodiscard]] bool validateOnOpen() const { return settings_->validateOnOpen(); }
    void setValidateOnOpen(bool enabled);
    [[nodiscard]] int logCapacity() const { return settings_->logCapacity(); }
    void setLogCapacity(int capacity);

public Q_SLOTS:
    // The first-run screen has been passed: the preferences it set are the defaults, and the
    // next launch goes straight to the workbench.
    void completeFirstRun();
    // Reads the media set in the directory (one directory per member), and shows its loads,
    // which the operator can tick for an upload. Unless validateOnOpen is off, the set is
    // checked right after it is read; until the check says it is valid, no upload.
    void chooseMediaSet(const QUrl& directory);
    // Checks the media set that was chosen: reads it afresh (so files changed on disk since it
    // was read are what is checked), and sets validationState.
    void checkMediaSet();
    // Ticks (or unticks) every load.
    void selectAllLoads(bool selected);
    // FIND on the network; with an address, on that one host only.
    void findTargets(const QString& address = {});
    // Takes the address and <THW_ID_POS> of row `row` of the targets FIND found.
    void selectTarget(int row);
    void readInformation();
    // The Upload operation: the ticked loads of the media set, to the target.
    void uploadSelected();
    // The Download operation (5.4.4): in the media defined mode the named files, in the
    // operator defined mode the target's list (the choice is made when it arrives, with
    // confirmDownloadSelection or chooseNoDownloadFiles). The files land in a new
    // DNLD_DATA_<THW_ID_POS>_<number> directory under the download directory (Preferences).
    void downloadSelected();
    // Adds (or removes) a file name of a media defined download. Empty names and names that
    // are already listed are refused; a name that is not a safe file name is refused when the
    // download starts.
    void addDownloadFile(const QString& name);
    void removeDownloadFile(int row);
    // The choice of an operator defined download: sends the ticked files to the target. With
    // no file ticked the choice is "none", which cancels the download.
    void confirmDownloadSelection();
    // "Choose none" of an operator defined download: cancels the operation.
    void chooseNoDownloadFiles();
    // Asks the operation that is running to stop. The target is told, and the operation ends when
    // it confirms (or after the 13 second time-out).
    void abort();
    void clearLog();
    // The window's error dialog has been read: the next failure opens it again.
    void dismissError();

Q_SIGNALS:
    void busyChanged();
    void firstRunChanged();
    void canAbortChanged();
    void targetChanged();
    void selectedTargetChanged();
    void canReadInformationChanged();
    void canUploadChanged();
    void canDownloadChanged();
    void downloadChanged();
    void downloadChoicePendingChanged();
    void downloadSelectionChanged();
    void downloadDirectoryChanged();
    void progressChanged();
    void resultChanged();
    void errorChanged();
    void mediaSetChanged();
    void validationChanged();
    void preferencesChanged();

private:
    enum class Job {
        none,
        find,
        information,
        upload,
        media_set,
        validate,
        download_media,
        download_operator
    };
    // What the automatic FIND was run for, so the operation follows it when exactly one target
    // answered.
    enum class Pending { none, information, upload, download };

    bool begin(Job job);
    void on_finished(const OperationSummary& summary);
    void set_result(int state, const QString& text, const QString& detail);
    void set_error(const QString& text, const QString& detail, const QString& hint);
    void set_job(Job job);
    // Tells the window when what it may start has changed.
    void refresh_availability();
    void set_progress(int percent);
    void set_validation(int state, const QString& error, const QStringList& findings,
                        const QString& detail);
    // Drops what belonged to the target that was being addressed before the address or
    // THW_ID_POS changed.
    void reconcile_target();
    void run_pending_after_find(bool found);
    const FoundTarget* selected_target() const;
    // Re-reads the media set of the last session, after the window has shown.
    void restore_last_media_set();

    EngineWorker* worker_ = nullptr;
    QThread thread_;
    Job job_ = Job::none;
    Pending pending_ = Pending::none;
    // The start-up restore of the last media set is done quietly: a set that is no longer there
    // leaves no dialog behind, only the inline note.
    bool quiet_job_ = false;
    bool abort_requested_ = false;
    bool could_read_information_ = false;
    bool could_upload_ = false;
    bool could_download_ = false;
    int selected_loads_ = 0;
    int progress_ = -1;
    int selected_target_row_ = -1;
    int download_mode_ = 0;
    bool download_choice_pending_ = false;
    int download_selected_count_ = 0;
    QString last_download_directory_;

    TargetModel targets_;
    EventModel events_;
    ConfigurationModel configuration_;
    LoadModel loads_;
    FileModel files_;
    DownloadModel downloads_;

    QString target_address_;
    QString thw_id_pos_;
    int result_state_ = 0;
    bool result_is_transfer_ = false;
    bool result_was_download_ = false;
    QString result_text_;
    QString result_detail_;
    QString last_error_text_;
    QString last_error_detail_;
    QString last_error_hint_;
    MediaSetSummary media_set_;
    int validation_state_ = 0;
    QString validation_error_;
    QStringList validation_findings_;
    QString validation_detail_;

    std::unique_ptr<AppSettings> settings_;
};

}  // namespace adl::app

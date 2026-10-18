#pragma once

// What the window talks to: the state of the data loader as properties, and the few things the
// operator can do as invokable functions. It lives on the window's thread, owns the thread the
// engine runs on, and turns the worker's signals into model changes. One job runs at a time
// (FIND, an Information operation or the reading of a media set); while one runs, `busy` is true
// and the others are refused.

#include <QObject>
#include <QStringList>
#include <QThread>
#include <QUrl>

#include "adl/app/engine_worker.hpp"
#include "adl/app/events.hpp"
#include "adl/app/host_access.hpp"
#include "adl/app/models.hpp"

namespace adl::app {

class LoaderModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString activity READ activity NOTIFY busyChanged)
    Q_PROPERTY(bool canAbort READ canAbort NOTIFY canAbortChanged)

    // The target: chosen from what FIND found, or typed.
    Q_PROPERTY(QString targetAddress READ targetAddress WRITE setTargetAddress NOTIFY targetChanged)
    Q_PROPERTY(QString thwIdPos READ thwIdPos WRITE setThwIdPos NOTIFY targetChanged)
    Q_PROPERTY(bool targetValid READ targetValid NOTIFY targetChanged)
    Q_PROPERTY(bool canReadInformation READ canReadInformation NOTIFY canReadInformationChanged)

    // The outcome of the last job: 0 none yet, 1 it succeeded, 2 it did not.
    Q_PROPERTY(int resultState READ resultState NOTIFY resultChanged)
    Q_PROPERTY(QString resultText READ resultText NOTIFY resultChanged)
    Q_PROPERTY(QString resultDetail READ resultDetail NOTIFY resultChanged)

    // The media set that was read.
    Q_PROPERTY(QString mediaSetPath READ mediaSetPath NOTIFY mediaSetChanged)
    Q_PROPERTY(QString mediaSetTitle READ mediaSetTitle NOTIFY mediaSetChanged)
    Q_PROPERTY(QString mediaSetError READ mediaSetError NOTIFY mediaSetChanged)
    Q_PROPERTY(QStringList mediaSetFindings READ mediaSetFindings NOTIFY mediaSetChanged)

    Q_PROPERTY(QObject* targets READ targets CONSTANT)
    Q_PROPERTY(QObject* events READ events CONSTANT)
    Q_PROPERTY(QObject* configuration READ configuration CONSTANT)
    Q_PROPERTY(QObject* loads READ loads CONSTANT)

public:
    explicit LoaderModel(HostFactory host_factory = system_host_factory(),
                         QObject* parent = nullptr);
    // Aborts what is running and waits for the engine's thread to finish, which for an operation
    // the target has to confirm can take up to the data load protocol time-out (13 seconds).
    ~LoaderModel() override;

    [[nodiscard]] bool busy() const noexcept { return job_ != Job::none; }
    [[nodiscard]] QString activity() const;
    [[nodiscard]] bool canAbort() const noexcept {
        return job_ == Job::information && !abort_requested_;
    }

    [[nodiscard]] QString targetAddress() const { return target_address_; }
    [[nodiscard]] QString thwIdPos() const { return thw_id_pos_; }
    void setTargetAddress(const QString& address);
    void setThwIdPos(const QString& thw_id_pos);
    [[nodiscard]] bool targetValid() const;
    [[nodiscard]] bool canReadInformation() const { return !busy() && targetValid(); }

    [[nodiscard]] int resultState() const noexcept { return result_state_; }
    [[nodiscard]] QString resultText() const { return result_text_; }
    [[nodiscard]] QString resultDetail() const { return result_detail_; }

    [[nodiscard]] QString mediaSetPath() const { return media_set_.path; }
    [[nodiscard]] QString mediaSetTitle() const;
    [[nodiscard]] QString mediaSetError() const { return media_set_.error; }
    [[nodiscard]] QStringList mediaSetFindings() const { return media_set_.findings; }

    [[nodiscard]] QObject* targets() { return &targets_; }
    [[nodiscard]] QObject* events() { return &events_; }
    [[nodiscard]] QObject* configuration() { return &configuration_; }
    [[nodiscard]] QObject* loads() { return &loads_; }

public Q_SLOTS:
    // Reads the media set in the directory (one directory per member). The result is shown, not
    // yet used: the Upload operation will load from it.
    void chooseMediaSet(const QUrl& directory);
    // FIND on the network; with an address, on that one host only.
    void findTargets(const QString& address = {});
    // Takes the address and <THW_ID_POS> of row `row` of the targets FIND found.
    void selectTarget(int row);
    void readInformation();
    // Asks the operation that is running to stop. The target is told, and the operation ends when
    // it confirms (or after the 13 second time-out).
    void abort();
    void clearLog();

Q_SIGNALS:
    void busyChanged();
    void canAbortChanged();
    void targetChanged();
    void canReadInformationChanged();
    void resultChanged();
    void mediaSetChanged();

private:
    enum class Job { none, find, information, media_set };

    bool begin(Job job);
    void on_finished(const OperationSummary& summary);
    void set_result(int state, const QString& text, const QString& detail);
    void set_job(Job job);

    EngineWorker* worker_ = nullptr;
    QThread thread_;
    Job job_ = Job::none;
    bool abort_requested_ = false;

    TargetModel targets_;
    EventModel events_;
    ConfigurationModel configuration_;
    LoadModel loads_;

    QString target_address_;
    QString thw_id_pos_;
    int result_state_ = 0;
    QString result_text_;
    QString result_detail_;
    MediaSetSummary media_set_;
};

}  // namespace adl::app

#pragma once

// The engine on its own thread. The 615A runner blocks and is single-threaded, so the window
// never calls it: it queues a job to an EngineWorker that lives on a QThread. The worker runs
// the job to its end and reports through signals, which Qt delivers to the window's thread as
// queued calls, in the order they were emitted. What goes the other way while a job runs is
// the operator's abort and the deferred file choice of an operator defined download, which
// cannot be queued calls (the worker's thread is busy in the job): they are flags the runner
// reads between its waits, through the RunController.

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include "adl/app/events.hpp"
#include "adl/app/host_access.hpp"
#include "adl/app/media_source.hpp"
#include "adl/arinc615a/runner.hpp"

namespace adl::app {

// The operator's controls of a running operation, handed to the runner as its controller. Both
// are written from the window's thread (request_abort, post_selection) and turned into a call
// to the loader by the runner's thread in on_poll: the request into one DataLoader::abort, the
// choice into one DataLoader::provide_selection. An empty choice - "none" - is what a listener
// answering the target's list with SelectionDecision::cancelled would do: one abort, with
// aborted_by_operator. The abort wins if both have been made: once it has been asked for, a
// choice no longer matters. Each is issued once per job, and clear() is called before a job is
// queued, so a request or a choice left over from an earlier job cannot stop or answer the
// next.
class OperationGate final : public adl::arinc615a::RunController {
public:
    // Thread safe. Asks the job that is running (or the next one to start) to abort.
    void request_abort() noexcept { abort_requested_.store(true, std::memory_order_release); }
    // Thread safe. Hands the operator's file choice to the job that is running. The names are
    // copied at once; an empty list is the choice "none".
    void post_selection(QStringList names) noexcept;
    // Forgets a request and a choice. Called before a job is queued, never while one is running.
    void clear() noexcept;
    [[nodiscard]] bool abort_requested() const noexcept {
        return abort_requested_.load(std::memory_order_acquire);
    }
    [[nodiscard]] bool selection_posted() const noexcept {
        return selection_ready_.load(std::memory_order_acquire);
    }

    void on_poll(adl::arinc615a::DataLoader& loader, adl::arinc615a::Instant now) noexcept override;

private:
    // The choice that was posted, once the runner's thread is reading it.
    [[nodiscard]] QStringList take_selection() noexcept;

    std::atomic<bool> abort_requested_{false};
    std::atomic<bool> selection_ready_{false};
    std::mutex selection_mutex_;
    QStringList selection_;
    bool abort_issued_ = false;      // the runner's thread only
    bool selection_issued_ = false;  // the runner's thread only
};

class EngineWorker : public QObject {
    Q_OBJECT

public:
    explicit EngineWorker(HostFactory host_factory, QObject* parent = nullptr);

    // Thread safe. Asks the job that is running (or the next one to start) to abort. A FIND and
    // the reading of a media set have no abort: they end by themselves within seconds.
    void request_abort() noexcept { controls_.request_abort(); }
    // Forgets the abort request and the file choice of a previous job. Called before a job is
    // queued, never while one is running.
    void clear_controls() noexcept { controls_.clear(); }
    // Thread safe. Hands the operator's file choice of an operator defined download to the job
    // that is running. An empty list is the choice "none", which cancels the download.
    void post_selection(QStringList names) noexcept { controls_.post_selection(std::move(names)); }

public Q_SLOTS:
    // FIND: broadcasts (or, for a non-empty dotted address, asks that one host) and collects the
    // answers of the next three seconds.
    void find(const QString& destination);
    // An Information operation (6.3.1) against the target at `address`, whose protocol files are
    // named by `thw_id_pos`. `control_port` is the port the loader listens on (59 unless the
    // operator's preferences moved it, sent to the target as the port option).
    void read_information(const QString& address, const QString& thw_id_pos, quint16 control_port);
    // Reads the media set in the directory and keeps what an upload needs to find its files.
    void read_media_set(const QString& path);
    // Checks the media set in the directory: reads it afresh (the index is rebuilt, so files
    // changed on disk since it was read are what the check and the next upload see) and reports
    // a ValidationReport.
    void validate_media_set(const QString& path);
    // An Upload operation (6.3.2) of `loads`, which have to be loads of the media set read last,
    // to the target at `address`.
    void upload(const QString& address, const QString& thw_id_pos,
                QList<adl::app::LoadSummary> loads, quint16 control_port);
    // A media defined Download (6.3.3): the files `names` name, to the target at `address`. The
    // files land in a new DNLD_DATA_<THW_ID_POS>_<number> directory (5.4.4.3) under
    // `download_directory`. A name that is not a safe file name is refused before the operation
    // starts.
    void download_media(const QString& address, const QString& thw_id_pos, QStringList names,
                        const QString& download_directory, quint16 control_port);
    // An operator defined Download (6.3.4) against the target at `address`. The target's list of
    // files comes back as the download_list signal while the operation runs; the operator's
    // choice reaches the loader through post_selection (an empty choice cancels). The files land
    // in a new DNLD_DATA_<THW_ID_POS>_<number> directory (5.4.4.3) under `download_directory`.
    void download_operator(const QString& address, const QString& thw_id_pos,
                           const QString& download_directory, quint16 control_port);

Q_SIGNALS:
    // A line for the activity log.
    void log_line(adl::app::EventEntry entry);
    void targets_found(QList<adl::app::FoundTarget> targets);
    void configuration_read(QList<adl::app::HardwareRow> rows);
    void media_set_read(adl::app::MediaSetSummary summary);
    // A media set check ended: whether it is valid (it could be read), and what it holds.
    void validation_finished(adl::app::ValidationReport report);
    // An upload status file arrived: how far the list is (percent) and how each load is going.
    void upload_progress(int list_ratio, QList<adl::app::LoadProgressRow> loads);
    // A download status file arrived: how far the list is (percent) and how each file is going.
    void download_progress(int list_ratio, QList<adl::app::FileProgressRow> files);
    // The target's list of an operator defined download has arrived; the operator chooses from
    // it and the choice comes back through post_selection.
    void download_list(QList<adl::app::DownloadFileRow> files);
    // Every job ends with exactly one of these, whatever happened.
    void finished(adl::app::OperationSummary summary);

private:
    // Ends the job with `text` as the outcome, and `hint` (when known) as what the operator can
    // do about it. `download_directory` (when set) is a directory that was made for the job and
    // is reported all the same, so the operator knows where it is.
    void fail(const QString& text, const QString& hint = {},
              const QString& download_directory = {});
    // Runs `operation` against the target and reports how it ended; the part the Information,
    // Upload and Download operations share. `listener`, `source` and `sink` live for the call;
    // `download_path` (when set) is the directory made for a download's files (platform path):
    // it is reported in the summary, or removed when nothing was received.
    // `media` (an upload's source) says if it refused a file that had changed since the check.
    void run_operation(adl::platform::Host& host, const adl::arinc615a::Address& target,
                       const QString& thw_id_pos, const adl::arinc615a::Operation& operation,
                       adl::arinc615a::OperationListener& listener,
                       adl::arinc615a::FileSource* source, adl::arinc615a::FileSink* sink,
                       quint16 control_port, const std::string& download_path = {},
                       const MediaTreeSource* media = nullptr);
    // The part the two Download modes share: the host, the DNLD_DATA directory and the runner.
    void run_download(const adl::arinc615a::Address& target, const QString& thw_id_pos,
                      const adl::arinc615a::Operation& operation, const QString& download_directory,
                      quint16 control_port);

    HostFactory host_factory_;
    // The files of the media set that was read last; used on this worker's thread only.
    std::shared_ptr<const MediaSetIndex> index_;
    OperationGate controls_;
};

}  // namespace adl::app

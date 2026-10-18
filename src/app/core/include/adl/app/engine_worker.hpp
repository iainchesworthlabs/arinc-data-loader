#pragma once

// The engine on its own thread. The 615A runner blocks and is single-threaded, so the window
// never calls it: it queues a job to an EngineWorker that lives on a QThread. The worker runs
// the job to its end and reports through signals, which Qt delivers to the window's thread as
// queued calls, in the order they were emitted. What goes the other way while a job runs is the
// operator's abort, which cannot be a queued call (the worker's thread is busy in the job): it
// is an atomic flag the runner reads between its waits, through the RunController.

#include <atomic>

#include <QList>
#include <QObject>
#include <QString>

#include "adl/app/events.hpp"
#include "adl/app/host_access.hpp"
#include "adl/arinc615a/runner.hpp"

namespace adl::app {

// The operator's abort, handed to the runner as its controller. request() may be called from
// any thread at any time; the runner's thread turns it into one abort of the loader.
class AbortGate final : public adl::arinc615a::RunController {
public:
    void request() noexcept { requested_.store(true, std::memory_order_release); }
    // Forgets a request. Called before a job is queued, never while one is running.
    void clear() noexcept { requested_.store(false, std::memory_order_release); }
    [[nodiscard]] bool requested() const noexcept {
        return requested_.load(std::memory_order_acquire);
    }

    void on_poll(adl::arinc615a::DataLoader& loader, adl::arinc615a::Instant now) noexcept override;

private:
    std::atomic<bool> requested_{false};
    bool issued_ = false;  // the runner's thread only
};

class EngineWorker : public QObject {
    Q_OBJECT

public:
    explicit EngineWorker(HostFactory host_factory, QObject* parent = nullptr);

    // Thread safe. Asks the job that is running (or the next one to start) to abort. A FIND and
    // the reading of a media set have no abort: they end by themselves within seconds.
    void request_abort() noexcept { gate_.request(); }
    void clear_abort() noexcept { gate_.clear(); }

public Q_SLOTS:
    // FIND: broadcasts (or, for a non-empty dotted address, asks that one host) and collects the
    // answers of the next three seconds.
    void find(const QString& destination);
    // An Information operation (6.3.1) against the target at `address`, whose protocol files are
    // named by `thw_id_pos`.
    void read_information(const QString& address, const QString& thw_id_pos);
    void read_media_set(const QString& path);

Q_SIGNALS:
    // A line for the activity log.
    void log_line(adl::app::EventEntry entry);
    void targets_found(QList<adl::app::FoundTarget> targets);
    void configuration_read(QList<adl::app::HardwareRow> rows);
    void media_set_read(adl::app::MediaSetSummary summary);
    // Every job ends with exactly one of these, whatever happened.
    void finished(adl::app::OperationSummary summary);

private:
    void fail(const QString& text);

    HostFactory host_factory_;
    AbortGate gate_;
};

}  // namespace adl::app

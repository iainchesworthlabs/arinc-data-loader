#pragma once

// The generation on its own thread. Building a media set reads every file the operator chose
// into memory and works the ARINC 665 formats over them, which blocks, so the window never calls
// it: it queues a job to a GeneratorWorker that lives on a QThread. The worker runs the job to
// its end and reports through signals, which Qt delivers to the window's thread as queued calls,
// in the order they were emitted. What goes the other way while a job runs is the operator's
// cancel, which cannot be a queued call (the worker's thread is busy in the job): it is an atomic
// flag the worker polls between files and between the job's steps.

#include "adl/ui/message.hpp"

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <atomic>
#include <cstdint>
#include <functional>
#include <string_view>

namespace adl::gen
{

// One file the operator chose, as the worker takes it. `kind` is SourceFileModel's "data" or
// "support"; `check_type` is an index over adl::arinc665::CheckValueType.
struct JobFile
{
    QString source_path; // the full path on disk
    QString file_name;   // the name on the media set
    QString kind;
    int check_type = 0;
};

// Everything the operator decided, copied when the job is queued so the worker never looks at the
// models, which live on the window's thread. All part numbers and names have been validated
// before this is made; the worker still refuses what the formats refuse, and says which file.
struct GenerateJob
{
    QString load_part_number;
    QString media_set_part_number;
    QString batch_part_number; // empty: no batch
    bool include_batch = false;
    QString header_file_name;
    int load_type_index = -1; // -1: no load type
    QStringList target_hardware_ids;
    int member_count = 1;    // 1 to 255
    int file_check_type = 0; // CheckValueType indices
    int files_list_check_type = 0;
    int load_check_type = 0;
    QString output_directory; // the generated set is written below it
    QList<JobFile> files;
};

// How the job ended: 1 the set was written and read back, 2 it was not, 3 the operator cancelled
// it. `findings` are where the written set differs from the standard's recommendations, which do
// not stop a generation.
struct GenerateOutcome
{
    int state = 0;
    adl::ui::Message text;
    adl::ui::Message detail;
    QString output_path;  // where the set was written, when it was
    QStringList findings; // "clause: message"
    int members = 0;
    int data_files = 0;
    int support_files = 0;
    qint64 total_bytes = 0;
};

// The load types the window offers. The Load Type ID is assigned by the system designer, not by
// the standard, so these are the examples the ARINC 665 world uses.
//
// `description` is the English text that is written into the load's header file, whatever
// language the window is in; the window shows it through GeneratorWorker::tr().
struct LoadTypeOption
{
    const char* description;
    QString code;
    std::uint16_t id;
};

[[nodiscard]] const QList<LoadTypeOption>& load_type_options();

class GeneratorWorker : public QObject
{
    Q_OBJECT

public:
    explicit GeneratorWorker(QObject* parent = nullptr);

    // Thread safe. Asks the job that is running (or the next one to start) to stop. The worker
    // checks the flag between files and between its steps, and a cancelled job leaves nothing
    // behind: what it had written is removed.
    void request_cancel() noexcept
    {
        cancel_.store(true, std::memory_order_release);
    }
    void clear_cancel() noexcept
    {
        cancel_.store(false, std::memory_order_release);
    }

    // For tests: called with the name of each place where a job can be cancelled ("read",
    // "load", "write", "written", "verified"), and of the allocation of a file's buffer
    // ("allocate"), on the worker's thread. A test cancels from it, or throws std::bad_alloc, to
    // stop a job at exactly that place.
    void set_checkpoint_hook(std::function<void(std::string_view)> hook)
    {
        checkpoint_hook_ = std::move(hook);
    }

public Q_SLOTS:
    void generate(const adl::gen::GenerateJob& job);

Q_SIGNALS:
    // How far the reading of the files has got (percent), or -1 for the steps after it, which
    // report no figure.
    void progress(int percent);
    // Every job ends with exactly one of these, whatever happened.
    void finished(const adl::gen::GenerateOutcome& outcome);

private:
    [[nodiscard]] bool cancelled() const noexcept
    {
        return cancel_.load(std::memory_order_acquire);
    }
    // A place where the job can be stopped: runs the hook, then says whether it was cancelled.
    [[nodiscard]] bool checkpoint(std::string_view name) const
    {
        if (checkpoint_hook_)
        {
            checkpoint_hook_(name);
        }
        return cancelled();
    }
    void run(const GenerateJob& job, GenerateOutcome& outcome);
    void fail(GenerateOutcome& outcome, const adl::ui::Message& text, const adl::ui::Message& detail = {});
    // Ends the job as cancelled: the operator asked for it, so it is not a failure. `detail` is
    // what to say besides; empty, it says that nothing was written.
    void stop(GenerateOutcome& outcome, const adl::ui::Message& detail = {});
    [[nodiscard]] static adl::ui::Message cannot_read(const QString& path);

    std::atomic<bool> cancel_{false};
    std::function<void(std::string_view)> checkpoint_hook_;
};

} // namespace adl::gen

Q_DECLARE_METATYPE(adl::gen::GenerateJob)
Q_DECLARE_METATYPE(adl::gen::GenerateOutcome)

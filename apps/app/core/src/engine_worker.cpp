#include "adl/app/engine_worker.hpp"

#include "adl/app/address.hpp"
#include "adl/app/download_directory.hpp"
#include "adl/app/media_set_summary.hpp"
#include "adl/app/text.hpp"
#include "adl/app/wording.hpp"
#include "adl/arinc615a/data_loader.hpp"
#include "adl/arinc615a/directory_files.hpp"
#include "adl/arinc615a/find_client.hpp"
#include "adl/arinc615a/limits.hpp"
#include "adl/platform/paths.hpp"

#include <adl/profiling.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QSet>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace adl::app
{

namespace
{

using adl::arinc615a::Address;
using adl::arinc615a::Notice;
using adl::arinc615a::OutcomeCode;
using adl::arinc615a::SelectionDecision;

// What the operator can do about a failure the application can recognise. The set is small and
// honest: only these are known well enough to point anywhere.
class Hint
{
    Q_DECLARE_TR_FUNCTIONS(Hint)

public:
    static Message media_set()
    {
        return Message::translated(&Hint::tr, QT_TR_NOOP("Choose the folder the media set was written to, and make "
                                                         "sure every file of the set is still in it."));
    }

    static Message media_set_changed()
    {
        return Message::translated(&Hint::tr,
                                   QT_TR_NOOP("Something in the media set was edited, replaced or removed after "
                                              "it was checked, so the upload was refused or stopped. If it was "
                                              "stopped part way, the target may already have read some of the "
                                              "files. Check the media set again, then upload."));
    }

    static Message host()
    {
        return Message::translated(&Hint::tr, QT_TR_NOOP("The loader could not use the network or its port. Check that "
                                                         "no other copy of the loader is running, and that no other "
                                                         "program holds the control port (Preferences)."));
    }

    static Message address()
    {
        return Message::translated(&Hint::tr, QT_TR_NOOP("Enter the target's IPv4 address as four numbers of 0 to 255 "
                                                         "separated by dots, for example 10.0.0.2."));
    }

    static Message download_directory()
    {
        return Message::translated(&Hint::tr, QT_TR_NOOP("Choose a download folder that exists or can be created, and "
                                                         "make sure it can be written (Preferences)."));
    }

    static Message download()
    {
        return Message::translated(&Hint::tr, QT_TR_NOOP("The target did not give back every file that was asked for. "
                                                         "Check the names against what the target can give, and ask it "
                                                         "why a file failed."));
    }

    static Message thw_id_pos()
    {
        return Message::translated(
            &Hint::tr, QT_TR_NOOP("Use the target's THW_ID_POS as it names itself: 1 to 240 characters, none "
                                  "of them a control character, a slash, a backslash or a character a "
                                  "folder name cannot hold (a colon, an asterisk, a question mark, quotes, "
                                  "angle brackets or a bar)."));
    }

    static Message file_name()
    {
        return Message::translated(&Hint::tr,
                                   QT_TR_NOOP("Name the file as the target knows it: a plain name of at most "
                                              "254 characters, without a path."));
    }

    static Message abort_unconfirmed()
    {
        return Message::translated(&Hint::tr, QT_TR_NOOP("The target may still be loading; ask it what it is doing "
                                                         "before trying again."));
    }
};

// The complaint about an address that is not an IPv4 one.
class AddressProblem
{
    Q_DECLARE_TR_FUNCTIONS(AddressProblem)

public:
    static Message not_an_address(const QString& address)
    {
        return Message::translated(&AddressProblem::tr, QT_TR_NOOP("\"%1\" is not an IPv4 address."),
                                   {Message::verbatim(address)});
    }
};

// A path as the operator typed it (a Qt string, in the platform's own form) as the '/'-separated
// UTF-8 path the platform's FileSystem speaks.
std::string to_platform_path(const QString& text)
{
    return adl::platform::to_platform_path(std::filesystem::path{QDir::toNativeSeparators(text).toStdU16String()});
}

// Turns what the engine reports into lines for the log, rows for the result and the progress of
// an upload or a download. It runs on the worker's thread, inside the runner, and everything it
// is handed is valid only during the call: all of it is copied into Qt strings at once.
class JobListener final : public adl::arinc615a::OperationListener
{
    Q_DECLARE_TR_FUNCTIONS(JobListener)

public:
    explicit JobListener(EngineWorker& worker) : worker_(worker) {}

    void on_initialization(const adl::arinc615a::InitializationFile& file) noexcept override
    {
        std::vector<Message> parts{Message::translated(&JobListener::tr, QT_TR_NOOP("The target answered: %1"),
                                                       {Wording::status(file.status)})};
        if (!file.description.empty())
        {
            parts.push_back(Message::verbatim(QStringLiteral(" (%1)").arg(to_qstring(file.description))));
        }
        const bool accepted = file.status == adl::arinc615a::StatusCode::accepted;
        Q_EMIT worker_.log_line({accepted ? Severity::info : Severity::warning, Message::sequence(std::move(parts))});
    }

    void on_configuration_status(const adl::arinc615a::ConfigurationStatus& status) noexcept override
    {
        std::vector<Message> parts{
            Message::translated(&JobListener::tr, QT_TR_NOOP("Status %1: %2"),
                                {Message::verbatim(QString::number(status.counter)), Wording::status(status.status)})};
        append_time_left(parts, status.estimated_time);
        if (!status.description.empty())
        {
            parts.push_back(Message::verbatim(QStringLiteral(" (%1)").arg(to_qstring(status.description))));
        }
        Q_EMIT worker_.log_line({Severity::info, Message::sequence(std::move(parts))});
    }

    void on_upload_status(const adl::arinc615a::UploadStatus& status) noexcept override
    {
        std::vector<Message> parts{
            Message::translated(&JobListener::tr, QT_TR_NOOP("Status %1: %2, %3% of the list"),
                                {Message::verbatim(QString::number(status.counter)), Wording::status(status.status),
                                 Message::verbatim(QString::number(status.list_ratio))})};
        append_time_left(parts, status.estimated_time);
        if (adl::arinc615a::description_is_meaningful(status.status) && !status.description.empty())
        {
            parts.push_back(Message::verbatim(QStringLiteral(" (%1)").arg(to_qstring(status.description))));
        }
        Q_EMIT worker_.log_line({Severity::info, Message::sequence(std::move(parts))});

        QList<LoadProgressRow> rows;
        for (const adl::arinc615a::LoadProgress& load : status.loads)
        {
            LoadProgressRow row;
            row.header_file = to_qstring(load.header_file_name);
            row.part_number = to_qstring(load.load_part_number_name);
            row.ratio = load.ratio;
            row.status = Wording::status(load.status);
            row.failed = load.status == adl::arinc615a::StatusCode::item_failed;
            if (adl::arinc615a::description_is_meaningful(load.status))
            {
                row.description = to_qstring(load.description);
            }
            if (row.failed && !failed_logged_.contains(row.header_file))
            {
                failed_logged_.append(row.header_file);
                Q_EMIT worker_.log_line(
                    {Severity::error,
                     Message::translated(
                         &JobListener::tr, QT_TR_NOOP("The target failed load %1 (%2)%3"),
                         {Message::verbatim(row.part_number), Message::verbatim(row.header_file),
                          Message::verbatim(row.description.isEmpty() ? QString()
                                                                      : QStringLiteral(": ") + row.description)})});
            }
            rows.append(row);
        }
        Q_EMIT worker_.upload_progress(status.list_ratio, rows);
    }

    void on_download_status(const adl::arinc615a::DownloadStatus& status) noexcept override
    {
        std::vector<Message> parts{
            Message::translated(&JobListener::tr, QT_TR_NOOP("Status %1: %2, %3% of the list"),
                                {Message::verbatim(QString::number(status.counter)), Wording::status(status.status),
                                 Message::verbatim(QString::number(status.list_ratio))})};
        append_time_left(parts, status.estimated_time);
        if (adl::arinc615a::description_is_meaningful(status.status) && !status.description.empty())
        {
            parts.push_back(Message::verbatim(QStringLiteral(" (%1)").arg(to_qstring(status.description))));
        }
        Q_EMIT worker_.log_line({Severity::info, Message::sequence(std::move(parts))});

        QList<FileProgressRow> rows;
        for (const adl::arinc615a::FileProgress& file : status.files)
        {
            FileProgressRow row;
            row.name = to_qstring(file.file_name);
            // The download status has no figure per file: a file is 100% when it has arrived.
            row.ratio = received_.contains(row.name) ? 100 : 0;
            row.status = Wording::status(file.status);
            row.failed = file.status == adl::arinc615a::StatusCode::item_failed;
            if (adl::arinc615a::description_is_meaningful(file.status))
            {
                row.description = to_qstring(file.description);
            }
            if (row.failed && !failed_logged_.contains(row.name))
            {
                failed_logged_.append(row.name);
                Q_EMIT worker_.log_line(
                    {Severity::error,
                     Message::translated(
                         &JobListener::tr, QT_TR_NOOP("The target failed file %1%2"),
                         {Message::verbatim(row.name),
                          Message::verbatim(row.description.isEmpty() ? QString()
                                                                      : QStringLiteral(": ") + row.description)})});
            }
            rows.append(row);
        }
        Q_EMIT worker_.download_progress(status.list_ratio, rows);
    }

    void on_file_sent(std::string_view name, std::uint64_t bytes) noexcept override
    {
        Q_EMIT worker_.log_line(
            {Severity::info,
             Message::translated(&JobListener::tr, QT_TR_NOOP("The target read %1 (%2 bytes)"),
                                 {Message::verbatim(to_qstring(name)), Message::verbatim(QString::number(bytes))})});
    }

    void on_file_received(std::string_view name, const adl::arinc615a::ReceivedFile& file) noexcept override
    {
        const QString text = to_qstring(name);
        received_.insert(text);
        Q_EMIT worker_.log_line(
            {Severity::info,
             Message::translated(&JobListener::tr, QT_TR_NOOP("Received %1 (%2 bytes)"),
                                 {Message::verbatim(text), Message::verbatim(QString::number(file.size))})});
    }

    void on_configuration_list(const adl::arinc615a::ConfigurationList& list) noexcept override
    {
        QList<HardwareRow> rows;
        for (const adl::arinc615a::HardwareItem& item : list.hardware)
        {
            rows.append({HardwareRow::Level::hardware, Message::verbatim(to_qstring(item.literal_name)),
                         Message::translated(&JobListener::tr, QT_TR_NOOP("serial number %1"),
                                             {Message::verbatim(to_qstring(item.serial_number))})});
            for (const adl::arinc615a::HardwarePart& part : item.parts)
            {
                QString title = to_qstring(part.part_number);
                if (!part.amendment.empty())
                {
                    title += QLatin1Char(' ') + to_qstring(part.amendment);
                }
                rows.append({HardwareRow::Level::part, Message::verbatim(title),
                             Message::verbatim(to_qstring(part.designation))});
            }
        }
        Q_EMIT worker_.log_line(
            {Severity::info,
             Message::translated(&JobListener::tr, QT_TR_NOOP("The target listed its configuration."))});
        Q_EMIT worker_.configuration_read(rows);
    }

    SelectionDecision on_download_list(const adl::arinc615a::DownloadList& list,
                                       adl::arinc615a::FileSelection& /*selection*/) noexcept override
    {
        QList<DownloadFileRow> rows;
        for (const adl::arinc615a::FileListing& file : list.files)
        {
            rows.append({to_qstring(file.file_name), to_qstring(file.description)});
        }
        Q_EMIT worker_.log_line(
            {Severity::info,
             Message::translated(&JobListener::tr, QT_TR_N_NOOP("The target listed %n file(s) it can download."), {},
                                 static_cast<int>(rows.size()))});
        Q_EMIT worker_.download_list(rows);
        // The operator chooses in the window; the choice comes back through the gate
        // (DataLoader::provide_selection), which the runner's thread calls on its next poll.
        return SelectionDecision::deferred;
    }

    void on_notice(Notice notice, std::string_view subject, std::uint32_t value) noexcept override
    {
        Q_EMIT worker_.log_line({Severity::notice, Wording::notice(notice, subject, value)});
    }

private:
    // ", about N s left" when the target gave an estimate.
    static void append_time_left(std::vector<Message>& parts, std::uint32_t seconds)
    {
        if (seconds != 0 && seconds != adl::arinc615a::estimated_time_unknown)
        {
            parts.push_back(Message::translated(&JobListener::tr, QT_TR_NOOP(", about %1 s left"),
                                                {Message::verbatim(QString::number(seconds))}));
        }
    }

    EngineWorker& worker_;
    QStringList failed_logged_; // loads or files already reported as failed
    QSet<QString> received_;    // the files of the download that have arrived
};

} // namespace

void OperationGate::post_selection(QStringList names) noexcept
{
    std::lock_guard lock(selection_mutex_);
    selection_ = std::move(names);
    selection_ready_.store(true, std::memory_order_release);
}

void OperationGate::clear() noexcept
{
    abort_requested_.store(false, std::memory_order_release);
    selection_ready_.store(false, std::memory_order_release);
    std::lock_guard lock(selection_mutex_);
    selection_.clear();
    // The runner's thread is idle when clear() runs: the flags of the last job are reset here,
    // never while one is running.
    abort_issued_ = false;
    selection_issued_ = false;
}

QStringList OperationGate::take_selection() noexcept
{
    std::lock_guard lock(selection_mutex_);
    QStringList choice = std::move(selection_);
    selection_.clear();
    return choice;
}

void OperationGate::on_poll(adl::arinc615a::DataLoader& loader, adl::arinc615a::Instant now) noexcept
{
    if (abort_requested() && !abort_issued_ && !loader.done())
    {
        abort_issued_ = true;
        loader.abort(now, adl::arinc615a::StatusCode::aborted_by_operator);
        return;
    }
    if (selection_posted() && !selection_issued_ && !loader.done())
    {
        selection_issued_ = true;
        const QStringList choice = take_selection();
        if (choice.isEmpty())
        {
            // "None": what a listener answering the target's list with
            // SelectionDecision::cancelled would do. The target's next status file confirms
            // the abort, as for the operator's own Abort.
            loader.abort(now, adl::arinc615a::StatusCode::aborted_by_operator);
            return;
        }
        std::vector<std::string> names;
        names.reserve(static_cast<std::size_t>(choice.size()));
        for (const QString& name : choice)
        {
            names.push_back(name.toStdString());
        }
        std::vector<std::string_view> views;
        views.reserve(names.size());
        for (const std::string& name : names)
        {
            views.push_back(name);
        }
        if (!loader.provide_selection(now, views) && !loader.done())
        {
            // The loader is not waiting for a choice (the target's list has not arrived): the
            // choice is not lost, it is offered again at the next poll.
            post_selection(choice);
            selection_issued_ = false;
        }
    }
}

EngineWorker::EngineWorker(HostFactory host_factory, QObject* parent)
    : QObject(parent), host_factory_(std::move(host_factory))
{
    register_metatypes();
}

void EngineWorker::fail(const Message& text, const Message& hint, const QString& download_directory)
{
    Q_EMIT log_line({Severity::error, text});
    OperationSummary summary;
    summary.outcome = text;
    summary.hint = hint;
    summary.download_directory = download_directory;
    Q_EMIT finished(summary);
}

void EngineWorker::find(const QString& destination)
{
    const adl::profiling::Zone zone{"app: find job"};
    std::vector<Address> destinations;
    if (!destination.isEmpty())
    {
        const auto octets = parse_ipv4(destination);
        if (!octets)
        {
            fail(AddressProblem::not_an_address(destination), Hint::address());
            return;
        }
        destinations.push_back(Address{*octets});
    }

    auto lease = host_factory_();
    if (!lease)
    {
        fail(Message::verbatim(lease.error()), Hint::host());
        return;
    }

    Q_EMIT log_line(
        {Severity::info,
         destination.isEmpty()
             ? Message::translated(&EngineWorker::tr, QT_TR_NOOP("Looking for targets (broadcast, three seconds)..."))
             : Message::translated(&EngineWorker::tr, QT_TR_NOOP("Looking for a target at %1 (three seconds)..."),
                                   {Message::verbatim(destination)})});

    adl::arinc615a::FindConfig config;
    config.destinations = destinations;
    std::vector<adl::arinc615a::FindTarget> answers(adl::arinc615a::FindClient::max_targets);
    adl::arinc615a::Runner runner(*lease->host);
    const auto count = runner.find(config, answers);
    if (!count)
    {
        fail(Message::translated(
            &EngineWorker::tr, QT_TR_NOOP("FIND could not run (%1)."),
            {Message::verbatim(QString::fromLatin1(adl::platform::to_string(count.error().code)))}));
        return;
    }

    QList<FoundTarget> found;
    for (const adl::arinc615a::FindTarget& answer : std::span{answers}.first(*count))
    {
        FoundTarget target;
        target.address = format_ipv4(answer.from.address.octets);
        target.hardware_id = to_qstring(answer.hardware_id.view());
        target.type_name = to_qstring(answer.type_name.view());
        target.position = to_qstring(answer.position.view());
        target.literal_name = to_qstring(answer.literal_name.view());
        target.manufacturer_code = to_qstring(answer.manufacturer_code.view());
        found.append(target);
    }
    Q_EMIT targets_found(found);

    OperationSummary summary;
    summary.success = true;
    const int answer_count = static_cast<int>(found.size());
    summary.outcome = Message::translated(&EngineWorker::tr, QT_TR_N_NOOP("%n target(s) answered"), {}, answer_count);
    Q_EMIT log_line({Severity::info, Message::sequence({summary.outcome, Message::verbatim(QStringLiteral("."))})});
    Q_EMIT finished(summary);
}

void EngineWorker::read_information(const QString& address, const QString& thw_id_pos, quint16 control_port)
{
    const adl::profiling::Zone zone{"app: information job"};
    const auto octets = parse_ipv4(address);
    if (!octets)
    {
        fail(AddressProblem::not_an_address(address), Hint::address());
        return;
    }
    auto lease = host_factory_();
    if (!lease)
    {
        fail(Message::verbatim(lease.error()), Hint::host());
        return;
    }
    Q_EMIT log_line(
        {Severity::info, Message::translated(&EngineWorker::tr, QT_TR_NOOP("Asking %1 (%2) for its configuration..."),
                                             {Message::verbatim(address), Message::verbatim(thw_id_pos)})});
    JobListener listener(*this);
    run_operation(*lease->host, Address{*octets}, thw_id_pos, adl::arinc615a::InformationOperation{}, listener, nullptr,
                  nullptr, control_port);
}

void EngineWorker::upload(const QString& address, const QString& thw_id_pos, QList<LoadSummary> loads,
                          quint16 control_port)
{
    const adl::profiling::Zone zone{"app: upload job"};
    const auto octets = parse_ipv4(address);
    if (!octets)
    {
        fail(AddressProblem::not_an_address(address), Hint::address());
        return;
    }
    if (!index_)
    {
        fail(Message::translated(&EngineWorker::tr,
                                 QT_TR_NOOP("No media set has been read, so there is nothing to upload.")));
        return;
    }
    if (loads.isEmpty())
    {
        fail(Message::translated(&EngineWorker::tr, QT_TR_NOOP("No load was chosen.")));
        return;
    }
    // The request names each load by its header file and part number; the strings it views are
    // kept here for the length of the call.
    struct LoadNames
    {
        std::string header_file;
        std::string part_number;
    };
    std::vector<LoadNames> names;
    names.reserve(static_cast<std::size_t>(loads.size()));
    for (const LoadSummary& load : loads)
    {
        names.push_back({load.header_file.toStdString(), load.part_number.toStdString()});
    }
    std::vector<adl::arinc615a::LoadReference> references;
    references.reserve(names.size());
    for (const LoadNames& load : names)
    {
        references.push_back(adl::arinc615a::LoadReference{load.header_file, load.part_number});
    }

    auto lease = host_factory_();
    if (!lease)
    {
        fail(Message::verbatim(lease.error()), Hint::host());
        return;
    }
    // What the operator checked is what is uploaded: a set edited since is refused here, before
    // the target is contacted, and again as the target opens each file.
    if (const auto changed = index_->changed_file(lease->host->files()))
    {
        const Message text =
            Message::translated(&EngineWorker::tr, QT_TR_NOOP("The media set changed after it was checked: %1."),
                                {Message::verbatim(to_qstring(*changed))});
        Q_EMIT log_line({Severity::error, text});
        OperationSummary summary;
        summary.outcome =
            Message::translated(&EngineWorker::tr, QT_TR_NOOP("the media set changed after it was checked"));
        summary.detail = text;
        summary.hint = Hint::media_set_changed();
        summary.media_set_changed = true;
        Q_EMIT finished(summary);
        return;
    }
    MediaTreeSource source(lease->host->files(), *index_);
    JobListener listener(*this);
    Q_EMIT log_line(
        {Severity::info, Message::translated(&EngineWorker::tr, QT_TR_N_NOOP("Uploading %n load(s) to %1 (%2)..."),
                                             {Message::verbatim(address), Message::verbatim(thw_id_pos)},
                                             static_cast<int>(loads.size()))});
    run_operation(*lease->host, Address{*octets}, thw_id_pos,
                  adl::arinc615a::UploadOperation{adl::arinc615a::UploadRequest{references}}, listener, &source,
                  nullptr, control_port, {}, &source);
}

void EngineWorker::download_media(const QString& address, const QString& thw_id_pos, QStringList names,
                                  const QString& download_directory, quint16 control_port)
{
    const adl::profiling::Zone zone{"app: download job"};
    const auto octets = parse_ipv4(address);
    if (!octets)
    {
        fail(AddressProblem::not_an_address(address), Hint::address());
        return;
    }
    if (names.isEmpty())
    {
        fail(Message::translated(&EngineWorker::tr, QT_TR_NOOP("No file was named.")));
        return;
    }
    // The names are checked before the operation starts: the request (and later the sink) would
    // refuse a name that is not a plain file name all the same.
    std::vector<std::string> stored;
    stored.reserve(static_cast<std::size_t>(names.size()));
    for (const QString& name : names)
    {
        stored.push_back(name.toStdString());
        if (!adl::arinc615a::is_safe_file_name(stored.back()))
        {
            fail(Message::translated(&EngineWorker::tr, QT_TR_NOOP("\"%1\" is not a safe file name."),
                                     {Message::verbatim(name)}),
                 Hint::file_name());
            return;
        }
    }
    std::vector<adl::arinc615a::FileRequest> requests;
    requests.reserve(stored.size());
    for (const std::string& name : stored)
    {
        requests.push_back(adl::arinc615a::FileRequest{name, {}});
    }
    Q_EMIT log_line(
        {Severity::info, Message::translated(&EngineWorker::tr, QT_TR_N_NOOP("Downloading %n file(s) from %1 (%2)..."),
                                             {Message::verbatim(address), Message::verbatim(thw_id_pos)},
                                             static_cast<int>(names.size()))});
    run_download(Address{*octets}, thw_id_pos,
                 adl::arinc615a::MediaDownloadOperation{adl::arinc615a::DownloadRequest{requests}}, download_directory,
                 control_port);
}

void EngineWorker::download_operator(const QString& address, const QString& thw_id_pos,
                                     const QString& download_directory, quint16 control_port)
{
    const adl::profiling::Zone zone{"app: download operator job"};
    const auto octets = parse_ipv4(address);
    if (!octets)
    {
        fail(AddressProblem::not_an_address(address), Hint::address());
        return;
    }
    Q_EMIT log_line({Severity::info, Message::translated(&EngineWorker::tr,
                                                         QT_TR_NOOP("Asking %1 (%2) for the files it can download..."),
                                                         {Message::verbatim(address), Message::verbatim(thw_id_pos)})});
    run_download(Address{*octets}, thw_id_pos, adl::arinc615a::OperatorDownloadOperation{}, download_directory,
                 control_port);
}

void EngineWorker::run_download(const Address& target, const QString& thw_id_pos,
                                const adl::arinc615a::Operation& operation, const QString& download_directory,
                                quint16 control_port)
{
    // The id names the directory the files go into, so it is checked before anything is made:
    // the loader would refuse a bad one only after the directory was there.
    if (!can_download_from(thw_id_pos.toStdString()))
    {
        fail(Message::translated(&EngineWorker::tr,
                                 QT_TR_NOOP("\"%1\" is not a THW_ID_POS a download can be made for."),
                                 {Message::verbatim(thw_id_pos)}),
             Hint::thw_id_pos());
        return;
    }
    auto lease = host_factory_();
    if (!lease)
    {
        fail(Message::verbatim(lease.error()), Hint::host());
        return;
    }
    const auto made =
        make_download_directory(lease->host->files(), to_platform_path(download_directory), thw_id_pos.toStdString());
    if (!made)
    {
        fail(Message::translated(&EngineWorker::tr, QT_TR_NOOP("The download folder could not be used: %1."),
                                 {made.error()}),
             Hint::download_directory());
        return;
    }
    adl::arinc615a::DirectoryFileSink sink(lease->host->files(), *made);
    JobListener listener(*this);
    run_operation(*lease->host, target, thw_id_pos, operation, listener, nullptr, &sink, control_port, *made);
}

void EngineWorker::run_operation(adl::platform::Host& host, const Address& target, const QString& thw_id_pos,
                                 const adl::arinc615a::Operation& operation,
                                 adl::arinc615a::OperationListener& listener, adl::arinc615a::FileSource* source,
                                 adl::arinc615a::FileSink* sink, quint16 control_port, const std::string& download_path,
                                 const MediaTreeSource* media)
{
    const std::string thw = thw_id_pos.toStdString();
    adl::arinc615a::LoaderConfig config;
    config.target = adl::arinc615a::Endpoint{target, adl::arinc615a::tftp_control_port};
    config.thw_id_pos = thw;
    // The operator's preference: anything but 59 is offered to the target in the port option of
    // the initialization request, which matters when the target runs on the same machine.
    config.control_port = control_port;

    // The engine allocates nothing: it works in a block of memory we give it, and the loader
    // object itself is too big for a thread's stack to be a good place for it.
    std::vector<std::byte> workspace(adl::arinc615a::DataLoader::workspace_bytes(config));
    adl::arinc615a::LoaderServices services;
    services.listener = &listener;
    services.source = source;
    services.sink = sink;
    auto loader = std::make_unique<adl::arinc615a::DataLoader>(config, workspace, services);

    adl::arinc615a::RunOptions options;
    options.controller = &controls_;
    if (target.is_loopback())
    {
        // A target on this machine is reached over loopback alone, so the sockets need not listen
        // on the network, and then no firewall asks whether to allow the program.
        options.local_address = adl::platform::Ipv4Address::loopback();
    }
    adl::arinc615a::Runner runner(host);
    const auto outcome = runner.run(*loader, operation, options);

    // A download that received nothing (the target said no, never answered, was cancelled, or the
    // loader could not run at all) leaves no directory behind: the one made for it is removed if
    // it is empty, and left alone if anything is in it.
    const bool directory_removed = !download_path.empty() && remove_if_empty(host.files(), download_path);
    const QString download_directory =
        download_path.empty() || directory_removed ? QString{} : QDir::toNativeSeparators(to_qstring(download_path));
    Message removed_note;
    if (directory_removed)
    {
        removed_note = Message::translated(
            &EngineWorker::tr, QT_TR_NOOP("Nothing was received, so the empty download folder %1 was removed."),
            {Message::verbatim(QDir::toNativeSeparators(to_qstring(download_path)))});
        Q_EMIT log_line({Severity::info, removed_note});
    }

    if (!outcome)
    {
        const auto& error = outcome.error();
        Message why;
        Message hint;
        const Message platform_code = Message::verbatim(QString::fromLatin1(adl::platform::to_string(error.platform)));
        switch (error.stage)
        {
        case adl::arinc615a::RunError::Stage::start:
            why = Message::translated(&EngineWorker::tr, QT_TR_NOOP("the request was refused (%1)"),
                                      {Message::verbatim(to_qstring(adl::arinc615a::to_string(error.engine)))});
            break;
        case adl::arinc615a::RunError::Stage::control_socket:
            why = Message::translated(&EngineWorker::tr, QT_TR_NOOP("the loader's port could not be opened (%1)"),
                                      {platform_code});
            hint = Hint::host();
            break;
        case adl::arinc615a::RunError::Stage::wait:
            why = Message::translated(&EngineWorker::tr, QT_TR_NOOP("waiting for the network failed (%1)"),
                                      {platform_code});
            break;
        }
        fail(Message::translated(&EngineWorker::tr, QT_TR_NOOP("The operation could not run: %1."), {why}), hint,
             download_directory);
        return;
    }

    const bool is_download = std::holds_alternative<adl::arinc615a::MediaDownloadOperation>(operation) ||
                             std::holds_alternative<adl::arinc615a::OperatorDownloadOperation>(operation);
    // A download that completed without every asked-for file is as much a failure as one the
    // target reported as failed: the operator asked for files and has not got them all.
    const bool missing_files =
        is_download && outcome->code == OutcomeCode::completed && outcome->files_received < outcome->files_requested;

    OperationSummary summary;
    summary.success = outcome->code == OutcomeCode::completed && outcome->items_failed == 0 && !missing_files;
    summary.outcome = Wording::outcome(outcome->code);
    summary.statuses_received = outcome->statuses_received;
    summary.statuses_lost = outcome->statuses_lost;
    if (outcome->code == OutcomeCode::completed && outcome->items_failed != 0)
    {
        const int failed = static_cast<int>(outcome->items_failed);
        summary.outcome =
            is_download
                ? Message::translated(&EngineWorker::tr, QT_TR_N_NOOP("completed, %n file(s) failed"), {}, failed)
                : Message::translated(&EngineWorker::tr, QT_TR_N_NOOP("completed, %n load(s) failed"), {}, failed);
        if (is_download)
        {
            summary.hint = Hint::download();
        }
    }
    if (missing_files)
    {
        const int missing = static_cast<int>(outcome->files_requested - outcome->files_received);
        summary.outcome =
            Message::translated(&EngineWorker::tr, QT_TR_N_NOOP("completed, %n file(s) missing"), {}, missing);
        summary.hint = Hint::download();
    }
    if (outcome->code == OutcomeCode::aborted_by_operator)
    {
        summary.detail =
            outcome->abort_confirmed
                ? Message::translated(&EngineWorker::tr, QT_TR_NOOP("The target confirmed that it stopped."))
                : Message::translated(&EngineWorker::tr, QT_TR_NOOP("The target did not confirm that it stopped."));
        if (!outcome->abort_confirmed)
        {
            summary.hint = Hint::abort_unconfirmed();
        }
    }
    if (media != nullptr && media->changed_file())
    {
        // The target was refused a file that is not what was checked; that, and not what the
        // target made of the refusal, is why the upload did not complete.
        const QString changed = to_qstring(*media->changed_file());
        summary.success = false;
        summary.media_set_changed = true;
        summary.detail = Message::translated(
            &EngineWorker::tr, QT_TR_NOOP("The media set changed after it was checked: %1. (The upload ended: %2.)"),
            {Message::verbatim(changed), summary.outcome});
        summary.outcome =
            Message::translated(&EngineWorker::tr, QT_TR_NOOP("the media set changed after it was checked"));
        summary.hint = Hint::media_set_changed();
    }
    else if (std::holds_alternative<adl::arinc615a::UploadOperation>(operation))
    {
        const Message served = Message::translated(&EngineWorker::tr, QT_TR_N_NOOP("The target read %n file(s)."), {},
                                                   static_cast<int>(outcome->files_served));
        summary.detail = summary.detail.isEmpty()
                             ? served
                             : Message::sequence({served, Message::verbatim(QStringLiteral(" ")), summary.detail});
    }
    if (is_download)
    {
        summary.files_requested = outcome->files_requested;
        summary.files_received = outcome->files_received;
        summary.download_directory = download_directory;
        Message received = Message::translated(&EngineWorker::tr, QT_TR_NOOP("Files received: %1 of %2 requested."),
                                               {Message::verbatim(QString::number(outcome->files_received)),
                                                Message::verbatim(QString::number(outcome->files_requested))});
        if (!removed_note.isEmpty())
        {
            received = Message::sequence({received, Message::verbatim(QStringLiteral(" ")), removed_note});
        }
        summary.detail = summary.detail.isEmpty()
                             ? received
                             : Message::sequence({received, Message::verbatim(QStringLiteral(" ")), summary.detail});
    }
    Q_EMIT log_line({summary.success ? Severity::info : Severity::error,
                     Message::translated(&EngineWorker::tr, QT_TR_NOOP("Finished: %1."), {summary.outcome})});
    Q_EMIT finished(summary);
}

void EngineWorker::read_media_set(const QString& path)
{
    const adl::profiling::Zone zone{"app: read media set job"};
    Q_EMIT log_line(
        {Severity::info, Message::translated(&EngineWorker::tr, QT_TR_NOOP("Reading the media set in %1..."),
                                             {Message::verbatim(path)})});
    const MediaSetSummary summary = read_media_set_summary(path, &index_);
    Q_EMIT media_set_read(summary);

    OperationSummary result;
    result.success = summary.error.isEmpty();
    if (result.success)
    {
        result.outcome = Message::translated(&EngineWorker::tr, QT_TR_NOOP("media set read"));
        const int loads = static_cast<int>(summary.loads.size());
        Q_EMIT log_line(
            {Severity::info, Message::translated(&EngineWorker::tr, QT_TR_N_NOOP("Read media set %1: %n load(s)."),
                                                 {Message::verbatim(summary.part_number)}, loads)});
    }
    else
    {
        result.outcome = Message::verbatim(summary.error);
        result.hint = Hint::media_set();
        Q_EMIT log_line(
            {Severity::error, Message::translated(&EngineWorker::tr, QT_TR_NOOP("The media set could not be read: %1."),
                                                  {Message::verbatim(summary.error)})});
    }
    Q_EMIT finished(result);
}

void EngineWorker::validate_media_set(const QString& path)
{
    const adl::profiling::Zone zone{"app: check media set job"};
    Q_EMIT log_line(
        {Severity::info, Message::translated(&EngineWorker::tr, QT_TR_NOOP("Checking the media set in %1..."),
                                             {Message::verbatim(path)})});
    // Read afresh, with a new index: files changed on disk since the set was read are what the
    // check (and the next upload) see.
    const MediaSetSummary summary = read_media_set_summary(path, &index_);
    Q_EMIT media_set_read(summary);

    ValidationReport report;
    report.path = path;
    report.error = summary.error;
    report.loads = static_cast<int>(summary.loads.size());
    report.files = static_cast<int>(summary.files.size());
    report.findings = summary.findings;
    Q_EMIT validation_finished(report);

    // The read verifies every file and its check values, so a set that reads is structurally
    // valid; its findings are deviations from recommendations, which are reported as warnings
    // and do not stop an upload. A read error makes the set invalid, and uploads stay refused
    // with the reason shown.
    OperationSummary result;
    result.success = report.error.isEmpty();
    if (result.success)
    {
        result.outcome = Message::translated(&EngineWorker::tr, QT_TR_NOOP("media set checked"));
        // A sentence can carry only one count, so the loads and the files are phrases of their
        // own and the sentence holds them.
        Q_EMIT log_line({Severity::info,
                         Message::translated(
                             &EngineWorker::tr, QT_TR_NOOP("Checked media set %1: %2, %3."),
                             {Message::verbatim(summary.part_number),
                              Message::translated(&EngineWorker::tr, QT_TR_N_NOOP("%n load(s)"), {}, report.loads),
                              Message::translated(&EngineWorker::tr, QT_TR_N_NOOP("%n file(s)"), {}, report.files)})});
    }
    else
    {
        result.outcome = Message::verbatim(summary.error);
        result.hint = Hint::media_set();
        Q_EMIT log_line({Severity::error,
                         Message::translated(&EngineWorker::tr, QT_TR_NOOP("The media set could not be checked: %1."),
                                             {Message::verbatim(summary.error)})});
    }
    Q_EMIT finished(result);
}

} // namespace adl::app

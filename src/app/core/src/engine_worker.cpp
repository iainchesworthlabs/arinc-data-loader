#include "adl/app/engine_worker.hpp"

#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <QDir>
#include <QSet>

#include "adl/app/address.hpp"
#include "adl/app/media_set_summary.hpp"
#include "adl/app/notice_text.hpp"
#include "adl/app/text.hpp"
#include "adl/arinc615a/data_loader.hpp"

#include <adl/profiling.hpp>
#include "adl/arinc615a/directory_files.hpp"
#include "adl/arinc615a/find_client.hpp"
#include "adl/arinc615a/limits.hpp"

namespace adl::app {

namespace {

using adl::arinc615a::Address;
using adl::arinc615a::Notice;
using adl::arinc615a::OutcomeCode;
using adl::arinc615a::SelectionDecision;

// What the operator can do about a failure the application can recognise. The set is small and
// honest: only these are known well enough to point anywhere.
QString media_set_hint() {
    return QObject::tr(
        "Choose the directory the media set was written to, and make sure every "
        "file of the set is still in it.");
}

QString media_set_changed_hint() {
    return QObject::tr(
        "Something in the media set was edited, replaced or removed after it was checked, so "
        "nothing was uploaded. Check the media set again, then upload.");
}

QString host_hint() {
    return QObject::tr(
        "The loader could not use the network or its port. Check that no other "
        "copy of the loader is running, and that no other program holds the "
        "control port (Preferences).");
}

QString address_hint() {
    return QObject::tr(
        "Enter the target's IPv4 address as four numbers of 0 to 255 separated "
        "by dots, for example 10.0.0.2.");
}

QString download_directory_hint() {
    return QObject::tr(
        "Choose a download directory that exists or can be created, and make "
        "sure it can be written (Preferences).");
}

QString download_hint() {
    return QObject::tr(
        "The target did not give back every file that was asked for. Check the "
        "names against what the target can give, and ask it why a file failed.");
}

QString file_name_hint() {
    return QObject::tr(
        "Name the file as the target knows it: a plain name of at most 254 "
        "characters, without a path.");
}

// Makes `path` a directory, creating the directories on the way: the platform's
// make_directory wants the parent to exist, and the operator's download directory may be a
// path that is not there yet (the default Documents/ArincDownloads is made on demand).
// Returns what to tell the operator, or nothing.
std::optional<QString> make_directory_chain(adl::platform::FileSystem& files,
                                            std::string_view path) {
    using adl::platform::ErrorCode;
    const auto existing = files.stat(path);
    if (existing) {
        if (existing->kind != adl::platform::FileKind::directory) {
            return QObject::tr("%1 is not a directory").arg(to_qstring(path));
        }
        return std::nullopt;
    }
    if (existing.error().code != ErrorCode::not_found) {
        return QObject::tr("cannot look at %1 (%2)")
            .arg(to_qstring(path),
                 QString::fromLatin1(adl::platform::to_string(existing.error().code)));
    }
    const std::size_t cut = path.rfind('/');
    if (cut != std::string_view::npos && cut != 0) {
        if (const auto problem = make_directory_chain(files, path.substr(0, cut))) {
            return problem;
        }
    }
    const auto made = files.make_directory(path);
    if (!made) {
        return QObject::tr("cannot create %1 (%2)")
            .arg(to_qstring(path),
                 QString::fromLatin1(adl::platform::to_string(made.error().code)));
    }
    return std::nullopt;
}

// The directory a download is written into: DNLD_DATA_<THW_ID_POS>_<number> under the operator's
// download directory (5.4.4.3), the <number> being the lowest that does not exist yet, starting
// at 1. The download directory itself is created when it is not there. Returns the platform path
// (UTF-8, '/' separated) of the new directory, or what to tell the operator.
std::expected<std::string, QString> make_download_directory(adl::platform::FileSystem& files,
                                                            std::string_view directory,
                                                            std::string_view thw_id_pos) {
    using adl::platform::ErrorCode;
    if (const auto problem = make_directory_chain(files, directory)) {
        return std::unexpected(*problem);
    }
    for (unsigned number = 1;; ++number) {
        const std::string name =
            "DNLD_DATA_" + std::string{thw_id_pos} + "_" + std::to_string(number);
        const std::string path = std::string{directory} + "/" + name;
        const auto taken = files.stat(path);
        if (taken) {
            continue;  // whatever it is, the number is not free
        }
        if (taken.error().code != ErrorCode::not_found) {
            return std::unexpected(
                QObject::tr("cannot look at %1 (%2)")
                    .arg(to_qstring(path),
                         QString::fromLatin1(adl::platform::to_string(taken.error().code))));
        }
        const auto made = files.make_directory(path);
        if (!made) {
            return std::unexpected(
                QObject::tr("cannot create %1 (%2)")
                    .arg(to_qstring(path),
                         QString::fromLatin1(adl::platform::to_string(made.error().code))));
        }
        return path;
    }
}

// A path as the operator typed it (a Qt string, in the platform's own form) as the '/'-separated
// UTF-8 path the platform's FileSystem speaks.
std::string to_platform_path(const QString& text) {
    const std::filesystem::path path{QDir::toNativeSeparators(text).toStdU16String()};
    const std::u8string generic = path.generic_u8string();
    return {reinterpret_cast<const char*>(generic.data()), generic.size()};
}

// Turns what the engine reports into lines for the log, rows for the result and the progress of
// an upload or a download. It runs on the worker's thread, inside the runner, and everything it
// is handed is valid only during the call: all of it is copied into Qt strings at once.
class JobListener final : public adl::arinc615a::OperationListener {
public:
    explicit JobListener(EngineWorker& worker) : worker_(worker) {}

    void on_initialization(const adl::arinc615a::InitializationFile& file) noexcept override {
        QString text = QObject::tr("The target answered: %1")
                           .arg(to_qstring(adl::arinc615a::to_string(file.status)));
        if (!file.description.empty()) {
            text += QStringLiteral(" (%1)").arg(to_qstring(file.description));
        }
        const bool accepted = file.status == adl::arinc615a::StatusCode::accepted;
        Q_EMIT worker_.log_line({accepted ? Severity::info : Severity::warning, text});
    }

    void on_configuration_status(
        const adl::arinc615a::ConfigurationStatus& status) noexcept override {
        QString text = QObject::tr("Status %1: %2")
                           .arg(status.counter)
                           .arg(to_qstring(adl::arinc615a::to_string(status.status)));
        if (status.estimated_time != 0 &&
            status.estimated_time != adl::arinc615a::estimated_time_unknown) {
            text += QObject::tr(", about %1 s left").arg(status.estimated_time);
        }
        if (!status.description.empty()) {
            text += QStringLiteral(" (%1)").arg(to_qstring(status.description));
        }
        Q_EMIT worker_.log_line({Severity::info, text});
    }

    void on_upload_status(const adl::arinc615a::UploadStatus& status) noexcept override {
        QString text = QObject::tr("Status %1: %2, %3% of the list")
                           .arg(status.counter)
                           .arg(to_qstring(adl::arinc615a::to_string(status.status)))
                           .arg(status.list_ratio);
        if (status.estimated_time != 0 &&
            status.estimated_time != adl::arinc615a::estimated_time_unknown) {
            text += QObject::tr(", about %1 s left").arg(status.estimated_time);
        }
        if (adl::arinc615a::description_is_meaningful(status.status) &&
            !status.description.empty()) {
            text += QStringLiteral(" (%1)").arg(to_qstring(status.description));
        }
        Q_EMIT worker_.log_line({Severity::info, text});

        QList<LoadProgressRow> rows;
        for (const adl::arinc615a::LoadProgress& load : status.loads) {
            LoadProgressRow row;
            row.header_file = to_qstring(load.header_file_name);
            row.part_number = to_qstring(load.load_part_number_name);
            row.ratio = load.ratio;
            row.status = to_qstring(adl::arinc615a::to_string(load.status));
            row.failed = load.status == adl::arinc615a::StatusCode::item_failed;
            if (adl::arinc615a::description_is_meaningful(load.status)) {
                row.description = to_qstring(load.description);
            }
            if (row.failed && !failed_logged_.contains(row.header_file)) {
                failed_logged_.append(row.header_file);
                Q_EMIT worker_.log_line(
                    {Severity::error,
                     QObject::tr("The target failed load %1 (%2)%3")
                         .arg(row.part_number, row.header_file,
                              row.description.isEmpty() ? QString()
                                                        : QStringLiteral(": ") + row.description)});
            }
            rows.append(row);
        }
        Q_EMIT worker_.upload_progress(status.list_ratio, rows);
    }

    void on_download_status(const adl::arinc615a::DownloadStatus& status) noexcept override {
        QString text = QObject::tr("Status %1: %2, %3% of the list")
                           .arg(status.counter)
                           .arg(to_qstring(adl::arinc615a::to_string(status.status)))
                           .arg(status.list_ratio);
        if (status.estimated_time != 0 &&
            status.estimated_time != adl::arinc615a::estimated_time_unknown) {
            text += QObject::tr(", about %1 s left").arg(status.estimated_time);
        }
        if (adl::arinc615a::description_is_meaningful(status.status) &&
            !status.description.empty()) {
            text += QStringLiteral(" (%1)").arg(to_qstring(status.description));
        }
        Q_EMIT worker_.log_line({Severity::info, text});

        QList<FileProgressRow> rows;
        for (const adl::arinc615a::FileProgress& file : status.files) {
            FileProgressRow row;
            row.name = to_qstring(file.file_name);
            // The download status has no figure per file: a file is 100% when it has arrived.
            row.ratio = received_.contains(row.name) ? 100 : 0;
            row.status = to_qstring(adl::arinc615a::to_string(file.status));
            row.failed = file.status == adl::arinc615a::StatusCode::item_failed;
            if (adl::arinc615a::description_is_meaningful(file.status)) {
                row.description = to_qstring(file.description);
            }
            if (row.failed && !failed_logged_.contains(row.name)) {
                failed_logged_.append(row.name);
                Q_EMIT worker_.log_line(
                    {Severity::error,
                     QObject::tr("The target failed file %1%2")
                         .arg(row.name, row.description.isEmpty()
                                            ? QString()
                                            : QStringLiteral(": ") + row.description)});
            }
            rows.append(row);
        }
        Q_EMIT worker_.download_progress(status.list_ratio, rows);
    }

    void on_file_sent(std::string_view name, std::uint64_t bytes) noexcept override {
        Q_EMIT worker_.log_line(
            {Severity::info,
             QObject::tr("The target read %1 (%2 bytes)").arg(to_qstring(name)).arg(bytes)});
    }

    void on_file_received(std::string_view name,
                          const adl::arinc615a::ReceivedFile& file) noexcept override {
        const QString text = to_qstring(name);
        received_.insert(text);
        Q_EMIT worker_.log_line(
            {Severity::info, QObject::tr("Received %1 (%2 bytes)").arg(text).arg(file.size)});
    }

    void on_configuration_list(const adl::arinc615a::ConfigurationList& list) noexcept override {
        QList<HardwareRow> rows;
        for (const adl::arinc615a::HardwareItem& item : list.hardware) {
            rows.append({HardwareRow::Level::hardware, to_qstring(item.literal_name),
                         QObject::tr("serial number %1").arg(to_qstring(item.serial_number))});
            for (const adl::arinc615a::HardwarePart& part : item.parts) {
                QString title = to_qstring(part.part_number);
                if (!part.amendment.empty()) {
                    title += QLatin1Char(' ') + to_qstring(part.amendment);
                }
                rows.append({HardwareRow::Level::part, title, to_qstring(part.designation)});
            }
        }
        Q_EMIT worker_.log_line(
            {Severity::info, QObject::tr("The target listed its configuration.")});
        Q_EMIT worker_.configuration_read(rows);
    }

    SelectionDecision on_download_list(
        const adl::arinc615a::DownloadList& list,
        adl::arinc615a::FileSelection& /*selection*/) noexcept override {
        QList<DownloadFileRow> rows;
        for (const adl::arinc615a::FileListing& file : list.files) {
            rows.append({to_qstring(file.file_name), to_qstring(file.description)});
        }
        Q_EMIT worker_.log_line(
            {Severity::info,
             rows.size() == 1
                 ? QObject::tr("The target listed 1 file it can download.")
                 : QObject::tr("The target listed %1 files it can download.").arg(rows.size())});
        Q_EMIT worker_.download_list(rows);
        // The operator chooses in the window; the choice comes back through the gate
        // (DataLoader::provide_selection), which the runner's thread calls on its next poll.
        return SelectionDecision::deferred;
    }

    void on_notice(Notice notice, std::string_view subject, std::uint32_t value) noexcept override {
        Q_EMIT worker_.log_line({Severity::notice, notice_text(notice, subject, value)});
    }

private:
    EngineWorker& worker_;
    QStringList failed_logged_;  // loads or files already reported as failed
    QSet<QString> received_;     // the files of the download that have arrived
};

}  // namespace

void OperationGate::post_selection(QStringList names) noexcept {
    std::lock_guard lock(selection_mutex_);
    selection_ = std::move(names);
    selection_ready_.store(true, std::memory_order_release);
}

void OperationGate::clear() noexcept {
    abort_requested_.store(false, std::memory_order_release);
    selection_ready_.store(false, std::memory_order_release);
    std::lock_guard lock(selection_mutex_);
    selection_.clear();
    // The runner's thread is idle when clear() runs: the flags of the last job are reset here,
    // never while one is running.
    abort_issued_ = false;
    selection_issued_ = false;
}

QStringList OperationGate::take_selection() noexcept {
    std::lock_guard lock(selection_mutex_);
    QStringList choice = std::move(selection_);
    selection_.clear();
    return choice;
}

void OperationGate::on_poll(adl::arinc615a::DataLoader& loader,
                            adl::arinc615a::Instant now) noexcept {
    if (abort_requested() && !abort_issued_ && !loader.done()) {
        abort_issued_ = true;
        loader.abort(now, adl::arinc615a::StatusCode::aborted_by_operator);
        return;
    }
    if (selection_posted() && !selection_issued_ && !loader.done()) {
        selection_issued_ = true;
        const QStringList choice = take_selection();
        if (choice.isEmpty()) {
            // "None": what a listener answering the target's list with
            // SelectionDecision::cancelled would do. The target's next status file confirms
            // the abort, as for the operator's own Abort.
            loader.abort(now, adl::arinc615a::StatusCode::aborted_by_operator);
            return;
        }
        std::vector<std::string> names;
        names.reserve(static_cast<std::size_t>(choice.size()));
        for (const QString& name : choice) {
            names.push_back(name.toStdString());
        }
        std::vector<std::string_view> views;
        views.reserve(names.size());
        for (const std::string& name : names) {
            views.push_back(name);
        }
        static_cast<void>(loader.provide_selection(now, views));
    }
}

EngineWorker::EngineWorker(HostFactory host_factory, QObject* parent)
    : QObject(parent), host_factory_(std::move(host_factory)) {
    register_metatypes();
}

void EngineWorker::fail(const QString& text, const QString& hint,
                        const QString& download_directory) {
    Q_EMIT log_line({Severity::error, text});
    OperationSummary summary;
    summary.outcome = text;
    summary.hint = hint;
    summary.download_directory = download_directory;
    Q_EMIT finished(summary);
}

void EngineWorker::find(const QString& destination) {
    const adl::profiling::Zone zone{"app: find job"};
    std::vector<Address> destinations;
    if (!destination.isEmpty()) {
        const auto octets = parse_ipv4(destination);
        if (!octets) {
            fail(tr("\"%1\" is not an IPv4 address.").arg(destination), address_hint());
            return;
        }
        destinations.push_back(Address{*octets});
    }

    auto lease = host_factory_();
    if (!lease) {
        fail(lease.error(), host_hint());
        return;
    }

    Q_EMIT log_line({Severity::info,
                     destination.isEmpty()
                         ? tr("Looking for targets (broadcast, three seconds)...")
                         : tr("Looking for a target at %1 (three seconds)...").arg(destination)});

    adl::arinc615a::FindConfig config;
    config.destinations = destinations;
    std::vector<adl::arinc615a::FindTarget> answers(adl::arinc615a::FindClient::max_targets);
    adl::arinc615a::Runner runner(*lease->host);
    const auto count = runner.find(config, answers);
    if (!count) {
        fail(tr("FIND could not run (%1).")
                 .arg(QString::fromLatin1(adl::platform::to_string(count.error().code))));
        return;
    }

    QList<FoundTarget> found;
    for (std::size_t i = 0; i < *count; ++i) {
        const adl::arinc615a::FindTarget& answer = answers[i];
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
    summary.outcome =
        answer_count == 1 ? tr("1 target answered") : tr("%1 targets answered").arg(answer_count);
    Q_EMIT log_line({Severity::info, summary.outcome + QLatin1Char('.')});
    Q_EMIT finished(summary);
}

void EngineWorker::read_information(const QString& address, const QString& thw_id_pos,
                                    quint16 control_port) {
    const adl::profiling::Zone zone{"app: information job"};
    const auto octets = parse_ipv4(address);
    if (!octets) {
        fail(tr("\"%1\" is not an IPv4 address.").arg(address), address_hint());
        return;
    }
    auto lease = host_factory_();
    if (!lease) {
        fail(lease.error(), host_hint());
        return;
    }
    Q_EMIT log_line(
        {Severity::info, tr("Asking %1 (%2) for its configuration...").arg(address, thw_id_pos)});
    JobListener listener(*this);
    run_operation(*lease->host, Address{*octets}, thw_id_pos,
                  adl::arinc615a::InformationOperation{}, listener, nullptr, nullptr, control_port);
}

void EngineWorker::upload(const QString& address, const QString& thw_id_pos,
                          QList<LoadSummary> loads, quint16 control_port) {
    const adl::profiling::Zone zone{"app: upload job"};
    const auto octets = parse_ipv4(address);
    if (!octets) {
        fail(tr("\"%1\" is not an IPv4 address.").arg(address), address_hint());
        return;
    }
    if (!index_) {
        fail(tr("No media set has been read, so there is nothing to upload."));
        return;
    }
    if (loads.isEmpty()) {
        fail(tr("No load was chosen."));
        return;
    }
    // The request names each load by its header file and part number; the strings it views are
    // kept here for the length of the call.
    std::vector<std::string> names;
    names.reserve(static_cast<std::size_t>(loads.size()) * 2);
    for (const LoadSummary& load : loads) {
        names.push_back(load.header_file.toStdString());
        names.push_back(load.part_number.toStdString());
    }
    std::vector<adl::arinc615a::LoadReference> references;
    references.reserve(static_cast<std::size_t>(loads.size()));
    for (std::size_t i = 0; i < names.size(); i += 2) {
        references.push_back(adl::arinc615a::LoadReference{names[i], names[i + 1]});
    }

    auto lease = host_factory_();
    if (!lease) {
        fail(lease.error(), host_hint());
        return;
    }
    // What the operator checked is what is uploaded: a set edited since is refused here, before
    // the target is contacted, and again as the target opens each file.
    if (const auto changed = index_->changed_file(lease->host->files())) {
        const QString text =
            tr("The media set changed after it was checked: %1.").arg(to_qstring(*changed));
        Q_EMIT log_line({Severity::error, text});
        OperationSummary summary;
        summary.outcome = tr("the media set changed after it was checked");
        summary.detail = text;
        summary.hint = media_set_changed_hint();
        summary.media_set_changed = true;
        Q_EMIT finished(summary);
        return;
    }
    MediaTreeSource source(lease->host->files(), *index_);
    JobListener listener(*this);
    Q_EMIT log_line(
        {Severity::info,
         loads.size() == 1
             ? tr("Uploading 1 load to %1 (%2)...").arg(address, thw_id_pos)
             : tr("Uploading %1 loads to %2 (%3)...").arg(loads.size()).arg(address, thw_id_pos)});
    run_operation(*lease->host, Address{*octets}, thw_id_pos,
                  adl::arinc615a::UploadOperation{adl::arinc615a::UploadRequest{references}},
                  listener, &source, nullptr, control_port, {}, &source);
}

void EngineWorker::download_media(const QString& address, const QString& thw_id_pos,
                                  QStringList names, const QString& download_directory,
                                  quint16 control_port) {
    const adl::profiling::Zone zone{"app: download job"};
    const auto octets = parse_ipv4(address);
    if (!octets) {
        fail(tr("\"%1\" is not an IPv4 address.").arg(address), address_hint());
        return;
    }
    if (names.isEmpty()) {
        fail(tr("No file was named."));
        return;
    }
    // The names are checked before the operation starts: the request (and later the sink) would
    // refuse a name that is not a plain file name all the same.
    std::vector<std::string> stored;
    stored.reserve(static_cast<std::size_t>(names.size()));
    for (const QString& name : names) {
        stored.push_back(name.toStdString());
        if (!adl::arinc615a::is_safe_file_name(stored.back())) {
            fail(tr("\"%1\" is not a safe file name.").arg(name), file_name_hint());
            return;
        }
    }
    std::vector<adl::arinc615a::FileRequest> requests;
    requests.reserve(stored.size());
    for (const std::string& name : stored) {
        requests.push_back(adl::arinc615a::FileRequest{name, {}});
    }
    Q_EMIT log_line(
        {Severity::info, names.size() == 1
                             ? tr("Downloading 1 file from %1 (%2)...").arg(address, thw_id_pos)
                             : tr("Downloading %1 files from %2 (%3)...")
                                   .arg(names.size())
                                   .arg(address, thw_id_pos)});
    run_download(Address{*octets}, thw_id_pos,
                 adl::arinc615a::MediaDownloadOperation{adl::arinc615a::DownloadRequest{requests}},
                 download_directory, control_port);
}

void EngineWorker::download_operator(const QString& address, const QString& thw_id_pos,
                                     const QString& download_directory, quint16 control_port) {
    const adl::profiling::Zone zone{"app: download operator job"};
    const auto octets = parse_ipv4(address);
    if (!octets) {
        fail(tr("\"%1\" is not an IPv4 address.").arg(address), address_hint());
        return;
    }
    Q_EMIT log_line(
        {Severity::info,
         tr("Asking %1 (%2) for the files it can download...").arg(address, thw_id_pos)});
    run_download(Address{*octets}, thw_id_pos, adl::arinc615a::OperatorDownloadOperation{},
                 download_directory, control_port);
}

void EngineWorker::run_download(const Address& target, const QString& thw_id_pos,
                                const adl::arinc615a::Operation& operation,
                                const QString& download_directory, quint16 control_port) {
    auto lease = host_factory_();
    if (!lease) {
        fail(lease.error(), host_hint());
        return;
    }
    const auto made = make_download_directory(
        lease->host->files(), to_platform_path(download_directory), thw_id_pos.toStdString());
    if (!made) {
        fail(tr("The download directory could not be used: %1.").arg(made.error()),
             download_directory_hint());
        return;
    }
    adl::arinc615a::DirectoryFileSink sink(lease->host->files(), *made);
    JobListener listener(*this);
    run_operation(*lease->host, target, thw_id_pos, operation, listener, nullptr, &sink,
                  control_port, QDir::toNativeSeparators(to_qstring(*made)));
}

void EngineWorker::run_operation(adl::platform::Host& host, const Address& target,
                                 const QString& thw_id_pos,
                                 const adl::arinc615a::Operation& operation,
                                 adl::arinc615a::OperationListener& listener,
                                 adl::arinc615a::FileSource* source, adl::arinc615a::FileSink* sink,
                                 quint16 control_port, const QString& download_directory,
                                 const MediaTreeSource* media) {
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
    if (target.is_loopback()) {
        // A target on this machine is reached over loopback alone, so the sockets need not listen
        // on the network, and then no firewall asks whether to allow the program.
        options.local_address = adl::platform::Ipv4Address::loopback();
    }
    adl::arinc615a::Runner runner(host);
    const auto outcome = runner.run(*loader, operation, options);
    if (!outcome) {
        const auto& error = outcome.error();
        QString why;
        QString hint;
        switch (error.stage) {
            case adl::arinc615a::RunError::Stage::start:
                why = tr("the request was refused (%1)")
                          .arg(to_qstring(adl::arinc615a::to_string(error.engine)));
                break;
            case adl::arinc615a::RunError::Stage::control_socket:
                why = tr("the loader's port could not be opened (%1)")
                          .arg(QString::fromLatin1(adl::platform::to_string(error.platform)));
                hint = host_hint();
                break;
            case adl::arinc615a::RunError::Stage::wait:
                why = tr("waiting for the network failed (%1)")
                          .arg(QString::fromLatin1(adl::platform::to_string(error.platform)));
                break;
        }
        fail(tr("The operation could not run: %1.").arg(why), hint, download_directory);
        return;
    }

    const bool is_download =
        std::holds_alternative<adl::arinc615a::MediaDownloadOperation>(operation) ||
        std::holds_alternative<adl::arinc615a::OperatorDownloadOperation>(operation);
    // A download that completed without every asked-for file is as much a failure as one the
    // target reported as failed: the operator asked for files and has not got them all.
    const bool missing_files = is_download && outcome->code == OutcomeCode::completed &&
                               outcome->files_received < outcome->files_requested;

    OperationSummary summary;
    summary.success =
        outcome->code == OutcomeCode::completed && outcome->items_failed == 0 && !missing_files;
    summary.outcome = to_qstring(adl::arinc615a::to_string(outcome->code));
    summary.statuses_received = outcome->statuses_received;
    summary.statuses_lost = outcome->statuses_lost;
    if (outcome->code == OutcomeCode::completed && outcome->items_failed != 0) {
        const int failed = static_cast<int>(outcome->items_failed);
        summary.outcome = is_download
                              ? (failed == 1 ? tr("completed, 1 file failed")
                                             : tr("completed, %1 files failed").arg(failed))
                              : (failed == 1 ? tr("completed, 1 load failed")
                                             : tr("completed, %1 loads failed").arg(failed));
        if (is_download) {
            summary.hint = download_hint();
        }
    }
    if (missing_files) {
        const int missing = static_cast<int>(outcome->files_requested - outcome->files_received);
        summary.outcome = missing == 1 ? tr("completed, 1 file missing")
                                       : tr("completed, %1 files missing").arg(missing);
        summary.hint = download_hint();
    }
    if (outcome->code == OutcomeCode::aborted_by_operator) {
        summary.detail = outcome->abort_confirmed
                             ? tr("The target confirmed that it stopped.")
                             : tr("The target did not confirm that it stopped.");
        if (!outcome->abort_confirmed) {
            summary.hint =
                tr("The target may still be loading; ask it what it is doing before trying "
                   "again.");
        }
    }
    if (media != nullptr && media->changed_file()) {
        // The target was refused a file that is not what was checked; that, and not what the
        // target made of the refusal, is why the upload did not complete.
        const QString changed = to_qstring(*media->changed_file());
        summary.success = false;
        summary.media_set_changed = true;
        summary.detail = tr("The media set changed after it was checked: %1. (The upload ended: %2.)")
                             .arg(changed, summary.outcome);
        summary.outcome = tr("the media set changed after it was checked");
        summary.hint = media_set_changed_hint();
    } else if (std::holds_alternative<adl::arinc615a::UploadOperation>(operation)) {
        const int served_count = static_cast<int>(outcome->files_served);
        const QString served = served_count == 1
                                   ? tr("The target read 1 file.")
                                   : tr("The target read %1 files.").arg(served_count);
        summary.detail =
            summary.detail.isEmpty() ? served : served + QLatin1Char(' ') + summary.detail;
    }
    if (is_download) {
        summary.files_requested = outcome->files_requested;
        summary.files_received = outcome->files_received;
        summary.download_directory = download_directory;
        const QString received = tr("Files received: %1 of %2 requested.")
                                     .arg(outcome->files_received)
                                     .arg(outcome->files_requested);
        summary.detail =
            summary.detail.isEmpty() ? received : received + QLatin1Char(' ') + summary.detail;
    }
    Q_EMIT log_line({summary.success ? Severity::info : Severity::error,
                     tr("Finished: %1.").arg(summary.outcome)});
    Q_EMIT finished(summary);
}

void EngineWorker::read_media_set(const QString& path) {
    const adl::profiling::Zone zone{"app: read media set job"};
    Q_EMIT log_line({Severity::info, tr("Reading the media set in %1...").arg(path)});
    const MediaSetSummary summary = read_media_set_summary(path, &index_);
    Q_EMIT media_set_read(summary);

    OperationSummary result;
    result.success = summary.error.isEmpty();
    if (result.success) {
        result.outcome = tr("media set read");
        const int loads = static_cast<int>(summary.loads.size());
        Q_EMIT log_line(
            {Severity::info,
             loads == 1 ? tr("Read media set %1: 1 load.").arg(summary.part_number)
                        : tr("Read media set %1: %2 loads.").arg(summary.part_number).arg(loads)});
    } else {
        result.outcome = summary.error;
        result.hint = media_set_hint();
        Q_EMIT log_line(
            {Severity::error, tr("The media set could not be read: %1.").arg(summary.error)});
    }
    Q_EMIT finished(result);
}

void EngineWorker::validate_media_set(const QString& path) {
    const adl::profiling::Zone zone{"app: check media set job"};
    Q_EMIT log_line({Severity::info, tr("Checking the media set in %1...").arg(path)});
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
    if (result.success) {
        result.outcome = tr("media set checked");
        Q_EMIT log_line(
            {Severity::info,
             report.loads == 1 && report.files == 1
                 ? tr("Checked media set %1: 1 load, 1 file.").arg(summary.part_number)
                 : tr("Checked media set %1: %2, %3.")
                       .arg(summary.part_number)
                       .arg(report.loads == 1 ? tr("1 load") : tr("%1 loads").arg(report.loads))
                       .arg(report.files == 1 ? tr("1 file") : tr("%1 files").arg(report.files))});
    } else {
        result.outcome = summary.error;
        result.hint = media_set_hint();
        Q_EMIT log_line(
            {Severity::error, tr("The media set could not be checked: %1.").arg(summary.error)});
    }
    Q_EMIT finished(result);
}

}  // namespace adl::app

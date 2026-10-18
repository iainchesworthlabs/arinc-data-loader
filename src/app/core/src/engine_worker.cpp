#include "adl/app/engine_worker.hpp"

#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "adl/app/address.hpp"
#include "adl/app/media_set_summary.hpp"
#include "adl/app/notice_text.hpp"
#include "adl/app/text.hpp"
#include "adl/arinc615a/data_loader.hpp"
#include "adl/arinc615a/find_client.hpp"
#include "adl/arinc615a/limits.hpp"

namespace adl::app {

namespace {

using adl::arinc615a::Address;
using adl::arinc615a::Notice;
using adl::arinc615a::OutcomeCode;

// What the operator can do about a failure the application can recognise. The set is small and
// honest: only these are known well enough to point anywhere.
QString media_set_hint() {
    return QObject::tr(
        "Choose the directory the media set was written to, and make sure every "
        "file of the set is still in it.");
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

// Turns what the engine reports into lines for the log, rows for the result and the progress of
// an upload. It runs on the worker's thread, inside the runner, and everything it is handed is
// valid only during the call: all of it is copied into Qt strings at once.
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

    void on_file_sent(std::string_view name, std::uint64_t bytes) noexcept override {
        Q_EMIT worker_.log_line(
            {Severity::info,
             QObject::tr("The target read %1 (%2 bytes)").arg(to_qstring(name)).arg(bytes)});
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

    void on_notice(Notice notice, std::string_view subject, std::uint32_t value) noexcept override {
        Q_EMIT worker_.log_line({Severity::notice, notice_text(notice, subject, value)});
    }

private:
    EngineWorker& worker_;
    QStringList failed_logged_;  // headers of the loads already reported as failed
};

}  // namespace

void AbortGate::on_poll(adl::arinc615a::DataLoader& loader, adl::arinc615a::Instant now) noexcept {
    if (requested() && !issued_ && !loader.done()) {
        issued_ = true;
        loader.abort(now, adl::arinc615a::StatusCode::aborted_by_operator);
    }
}

EngineWorker::EngineWorker(HostFactory host_factory, QObject* parent)
    : QObject(parent), host_factory_(std::move(host_factory)) {
    register_metatypes();
}

void EngineWorker::fail(const QString& text, const QString& hint) {
    Q_EMIT log_line({Severity::error, text});
    OperationSummary summary;
    summary.outcome = text;
    summary.hint = hint;
    Q_EMIT finished(summary);
}

void EngineWorker::find(const QString& destination) {
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
    summary.outcome = tr("%n target(s) answered", "", static_cast<int>(found.size()));
    Q_EMIT log_line({Severity::info, summary.outcome + QLatin1Char('.')});
    Q_EMIT finished(summary);
}

void EngineWorker::read_information(const QString& address, const QString& thw_id_pos,
                                    quint16 control_port) {
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
                  adl::arinc615a::InformationOperation{}, listener, nullptr, control_port);
}

void EngineWorker::upload(const QString& address, const QString& thw_id_pos,
                          QList<LoadSummary> loads, quint16 control_port) {
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
    MediaTreeSource source(lease->host->files(), *index_);
    JobListener listener(*this);
    Q_EMIT log_line({Severity::info,
                     tr("Uploading %n load(s) to %1 (%2)...", "", static_cast<int>(loads.size()))
                         .arg(address, thw_id_pos)});
    run_operation(*lease->host, Address{*octets}, thw_id_pos,
                  adl::arinc615a::UploadOperation{adl::arinc615a::UploadRequest{references}},
                  listener, &source, control_port);
}

void EngineWorker::run_operation(adl::platform::Host& host, const Address& target,
                                 const QString& thw_id_pos,
                                 const adl::arinc615a::Operation& operation,
                                 adl::arinc615a::OperationListener& listener,
                                 adl::arinc615a::FileSource* source, quint16 control_port) {
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
    auto loader = std::make_unique<adl::arinc615a::DataLoader>(config, workspace, services);

    adl::arinc615a::RunOptions options;
    options.controller = &gate_;
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
        fail(tr("The operation could not run: %1.").arg(why), hint);
        return;
    }

    OperationSummary summary;
    summary.success = outcome->code == OutcomeCode::completed && outcome->items_failed == 0;
    summary.outcome = to_qstring(adl::arinc615a::to_string(outcome->code));
    summary.statuses_received = outcome->statuses_received;
    summary.statuses_lost = outcome->statuses_lost;
    if (outcome->code == OutcomeCode::completed && outcome->items_failed != 0) {
        summary.outcome =
            tr("completed, %n load(s) failed", "", static_cast<int>(outcome->items_failed));
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
    if (std::holds_alternative<adl::arinc615a::UploadOperation>(operation)) {
        const QString served =
            tr("The target read %n file(s).", "", static_cast<int>(outcome->files_served));
        summary.detail =
            summary.detail.isEmpty() ? served : served + QLatin1Char(' ') + summary.detail;
    }
    Q_EMIT log_line({summary.success ? Severity::info : Severity::error,
                     tr("Finished: %1.").arg(summary.outcome)});
    Q_EMIT finished(summary);
}

void EngineWorker::read_media_set(const QString& path) {
    Q_EMIT log_line({Severity::info, tr("Reading the media set in %1...").arg(path)});
    const MediaSetSummary summary = read_media_set_summary(path, &index_);
    Q_EMIT media_set_read(summary);

    OperationSummary result;
    result.success = summary.error.isEmpty();
    if (result.success) {
        result.outcome = tr("media set read");
        Q_EMIT log_line({Severity::info, tr("Read media set %1: %n load(s).", "",
                                            static_cast<int>(summary.loads.size()))
                                             .arg(summary.part_number)});
    } else {
        result.outcome = summary.error;
        result.hint = media_set_hint();
        Q_EMIT log_line(
            {Severity::error, tr("The media set could not be read: %1.").arg(summary.error)});
    }
    Q_EMIT finished(result);
}

void EngineWorker::validate_media_set(const QString& path) {
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
        Q_EMIT log_line({Severity::info, tr("Checked media set %1: %2 load(s), %3 file(s).")
                                             .arg(summary.part_number)
                                             .arg(report.loads)
                                             .arg(report.files)});
    } else {
        result.outcome = summary.error;
        result.hint = media_set_hint();
        Q_EMIT log_line(
            {Severity::error, tr("The media set could not be checked: %1.").arg(summary.error)});
    }
    Q_EMIT finished(result);
}

}  // namespace adl::app

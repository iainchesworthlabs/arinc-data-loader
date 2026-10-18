#include "adl/app/engine_worker.hpp"

#include <memory>
#include <string>
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

// Turns what the engine reports into lines for the log and rows for the result. It runs on the
// worker's thread, inside the runner, and everything it is handed is valid only during the call:
// all of it is copied into Qt strings at once.
class InformationListener final : public adl::arinc615a::OperationListener {
public:
    explicit InformationListener(EngineWorker& worker) : worker_(worker) {}

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

void EngineWorker::fail(const QString& text) {
    Q_EMIT log_line({Severity::error, text});
    OperationSummary summary;
    summary.outcome = text;
    Q_EMIT finished(summary);
}

void EngineWorker::find(const QString& destination) {
    std::vector<Address> destinations;
    if (!destination.isEmpty()) {
        const auto octets = parse_ipv4(destination);
        if (!octets) {
            fail(tr("\"%1\" is not an IPv4 address.").arg(destination));
            return;
        }
        destinations.push_back(Address{*octets});
    }

    auto lease = host_factory_();
    if (!lease) {
        fail(lease.error());
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

void EngineWorker::read_information(const QString& address, const QString& thw_id_pos) {
    const auto octets = parse_ipv4(address);
    if (!octets) {
        fail(tr("\"%1\" is not an IPv4 address.").arg(address));
        return;
    }
    const std::string thw = thw_id_pos.toStdString();

    auto lease = host_factory_();
    if (!lease) {
        fail(lease.error());
        return;
    }

    adl::arinc615a::LoaderConfig config;
    config.target = adl::arinc615a::Endpoint{Address{*octets}, adl::arinc615a::tftp_control_port};
    config.thw_id_pos = thw;

    // The engine allocates nothing: it works in a block of memory we give it, and the loader
    // object itself is too big for a thread's stack to be a good place for it.
    std::vector<std::byte> workspace(adl::arinc615a::DataLoader::workspace_bytes(config));
    InformationListener listener(*this);
    adl::arinc615a::LoaderServices services;
    services.listener = &listener;
    auto loader = std::make_unique<adl::arinc615a::DataLoader>(config, workspace, services);

    Q_EMIT log_line(
        {Severity::info, tr("Asking %1 (%2) for its configuration...").arg(address, thw_id_pos)});

    adl::arinc615a::RunOptions options;
    options.controller = &gate_;
    adl::arinc615a::Runner runner(*lease->host);
    const auto outcome = runner.run(*loader, adl::arinc615a::InformationOperation{}, options);
    if (!outcome) {
        const auto& error = outcome.error();
        QString why;
        switch (error.stage) {
            case adl::arinc615a::RunError::Stage::start:
                why = tr("the request was refused (%1)")
                          .arg(to_qstring(adl::arinc615a::to_string(error.engine)));
                break;
            case adl::arinc615a::RunError::Stage::control_socket:
                why = tr("the loader's port could not be opened (%1)")
                          .arg(QString::fromLatin1(adl::platform::to_string(error.platform)));
                break;
            case adl::arinc615a::RunError::Stage::wait:
                why = tr("waiting for the network failed (%1)")
                          .arg(QString::fromLatin1(adl::platform::to_string(error.platform)));
                break;
        }
        fail(tr("The operation could not run: %1.").arg(why));
        return;
    }

    OperationSummary summary;
    summary.success = outcome->code == OutcomeCode::completed;
    summary.outcome = to_qstring(adl::arinc615a::to_string(outcome->code));
    summary.statuses_received = outcome->statuses_received;
    summary.statuses_lost = outcome->statuses_lost;
    if (outcome->code == OutcomeCode::aborted_by_operator) {
        summary.detail = outcome->abort_confirmed
                             ? tr("The target confirmed that it stopped.")
                             : tr("The target did not confirm that it stopped.");
    }
    Q_EMIT log_line({summary.success ? Severity::info : Severity::error,
                     tr("Finished: %1.").arg(summary.outcome)});
    Q_EMIT finished(summary);
}

void EngineWorker::read_media_set(const QString& path) {
    Q_EMIT log_line({Severity::info, tr("Reading the media set in %1...").arg(path)});
    const MediaSetSummary summary = read_media_set_summary(path);
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
        Q_EMIT log_line(
            {Severity::error, tr("The media set could not be read: %1.").arg(summary.error)});
    }
    Q_EMIT finished(result);
}

}  // namespace adl::app

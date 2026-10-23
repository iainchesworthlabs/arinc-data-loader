#include "adl/app/wording.hpp"

#include <utility>
#include <vector>

#include "adl/app/text.hpp"

namespace adl::app {

using adl::arinc615a::Notice;
using adl::arinc615a::OutcomeCode;
using adl::arinc615a::StatusCode;
using adl::arinc615a::TransferOutcome;

Message Wording::outcome(OutcomeCode code) {
    switch (code) {
        case OutcomeCode::running:
            return Message::translated(&Wording::tr, QT_TR_NOOP("running"));
        case OutcomeCode::completed:
            return Message::translated(&Wording::tr, QT_TR_NOOP("completed"));
        case OutcomeCode::denied:
            return Message::translated(&Wording::tr, QT_TR_NOOP("denied by the target"));
        case OutcomeCode::not_supported:
            return Message::translated(&Wording::tr, QT_TR_NOOP("not supported by the target"));
        case OutcomeCode::aborted_by_operator:
            return Message::translated(&Wording::tr, QT_TR_NOOP("aborted by the operator"));
        case OutcomeCode::aborted_by_data_loader:
            return Message::translated(&Wording::tr, QT_TR_NOOP("aborted by the data loader"));
        case OutcomeCode::aborted_by_target:
            return Message::translated(&Wording::tr, QT_TR_NOOP("aborted by the target"));
        case OutcomeCode::unsupported_protocol_version:
            return Message::translated(&Wording::tr, QT_TR_NOOP("unsupported protocol version"));
        case OutcomeCode::transfer_failed:
            return Message::translated(&Wording::tr, QT_TR_NOOP("a file transfer failed"));
        case OutcomeCode::dlp_timed_out:
            return Message::translated(&Wording::tr, QT_TR_NOOP("data load protocol time-out"));
        case OutcomeCode::exception_timer_expired:
            return Message::translated(
                &Wording::tr, QT_TR_NOOP("the target was silent longer than its exception timer"));
        case OutcomeCode::protocol_error:
            return Message::translated(&Wording::tr, QT_TR_NOOP("protocol error"));
        case OutcomeCode::local_error:
            return Message::translated(&Wording::tr, QT_TR_NOOP("local error"));
    }
    return Message::translated(&Wording::tr, QT_TR_NOOP("unknown"));
}

Message Wording::status(StatusCode code) {
    switch (code) {
        case StatusCode::accepted:
            return Message::translated(&Wording::tr, QT_TR_NOOP("accepted"));
        case StatusCode::in_progress:
            return Message::translated(&Wording::tr, QT_TR_NOOP("in progress"));
        case StatusCode::completed:
            return Message::translated(&Wording::tr, QT_TR_NOOP("completed"));
        case StatusCode::in_progress_with_text:
            return Message::translated(&Wording::tr, QT_TR_NOOP("in progress, with details"));
        case StatusCode::operation_denied:
            return Message::translated(&Wording::tr, QT_TR_NOOP("denied"));
        case StatusCode::operation_not_supported:
            return Message::translated(&Wording::tr, QT_TR_NOOP("not supported"));
        case StatusCode::aborted_by_target:
            return Message::translated(&Wording::tr, QT_TR_NOOP("aborted by the target"));
        case StatusCode::aborted_by_data_loader:
            return Message::translated(&Wording::tr, QT_TR_NOOP("aborted by the data loader"));
        case StatusCode::aborted_by_operator:
            return Message::translated(&Wording::tr, QT_TR_NOOP("aborted by the operator"));
        case StatusCode::item_failed:
            return Message::translated(&Wording::tr, QT_TR_NOOP("failed"));
    }
    return Message::translated(&Wording::tr, QT_TR_NOOP("unknown status code"));
}

Message Wording::transfer(TransferOutcome outcome) {
    switch (outcome) {
        case TransferOutcome::running:
            return Message::translated(&Wording::tr, QT_TR_NOOP("running"));
        case TransferOutcome::succeeded:
            return Message::translated(&Wording::tr, QT_TR_NOOP("succeeded"));
        case TransferOutcome::timed_out:
            return Message::translated(&Wording::tr, QT_TR_NOOP("timed out"));
        case TransferOutcome::peer_error:
            return Message::translated(&Wording::tr, QT_TR_NOOP("the peer sent an error"));
        case TransferOutcome::peer_abort:
            return Message::translated(&Wording::tr, QT_TR_NOOP("the peer aborted"));
        case TransferOutcome::wait_requested:
            return Message::translated(&Wording::tr, QT_TR_NOOP("the peer asked to wait"));
        case TransferOutcome::option_refused:
            return Message::translated(&Wording::tr, QT_TR_NOOP("options refused"));
        case TransferOutcome::protocol_error:
            return Message::translated(&Wording::tr, QT_TR_NOOP("protocol error"));
        case TransferOutcome::size_mismatch:
            return Message::translated(&Wording::tr, QT_TR_NOOP("transfer size mismatch"));
        case TransferOutcome::local_error:
            return Message::translated(&Wording::tr, QT_TR_NOOP("local file error"));
        case TransferOutcome::aborted:
            return Message::translated(&Wording::tr, QT_TR_NOOP("aborted"));
    }
    return Message::translated(&Wording::tr, QT_TR_NOOP("unknown"));
}

Message Wording::notice_phrase(Notice notice) {
    switch (notice) {
        case Notice::status_counter_gap:
            return Message::translated(&Wording::tr, QT_TR_NOOP("status files lost"));
        case Notice::status_duplicate:
            return Message::translated(&Wording::tr, QT_TR_NOOP("status file repeated"));
        case Notice::status_stale:
            return Message::translated(&Wording::tr, QT_TR_NOOP("status file out of order"));
        case Notice::status_file_failed:
            return Message::translated(&Wording::tr, QT_TR_NOOP("status file transfer failed"));
        case Notice::checksum_mismatch:
            return Message::translated(&Wording::tr, QT_TR_NOOP("checksum did not match"));
        case Notice::checksum_unsupported:
            return Message::translated(&Wording::tr,
                                       QT_TR_NOOP("checksum algorithm not supported"));
        case Notice::file_not_available:
            return Message::translated(&Wording::tr, QT_TR_NOOP("file not available"));
        case Notice::request_refused:
            return Message::translated(&Wording::tr, QT_TR_NOOP("request refused"));
        case Notice::transfer_failed:
            return Message::translated(&Wording::tr, QT_TR_NOOP("transfer failed"));
        case Notice::waiting:
            return Message::translated(&Wording::tr, QT_TR_NOOP("target asked to wait"));
        case Notice::option_fallback:
            return Message::translated(&Wording::tr,
                                       QT_TR_NOOP("options refused, asked again without them"));
        case Notice::port_option_ignored:
            return Message::translated(&Wording::tr,
                                       QT_TR_NOOP("port option not taken by the target"));
        case Notice::files_missing:
            return Message::translated(&Wording::tr, QT_TR_NOOP("requested files missing"));
    }
    return Message::translated(&Wording::tr, QT_TR_NOOP("unknown"));
}

Message Wording::notice(Notice notice, std::string_view subject, std::uint32_t value) {
    std::vector<Message> parts;
    parts.push_back(notice_phrase(notice));
    if (!subject.empty()) {
        parts.push_back(Message::verbatim(QStringLiteral(": ") + to_qstring(subject)));
    }
    switch (notice) {
        case Notice::status_counter_gap:
        case Notice::files_missing:
            if (value != 0) {
                parts.push_back(Message::verbatim(QStringLiteral(" (%1)").arg(value)));
            }
            break;
        case Notice::waiting:
        case Notice::file_not_available:
            parts.push_back(Message::translated(&Wording::tr, QT_TR_NOOP(" (%1 s)"),
                                                {Message::verbatim(QString::number(value))}));
            break;
        case Notice::port_option_ignored:
            parts.push_back(Message::translated(&Wording::tr, QT_TR_NOOP(" (port %1)"),
                                                {Message::verbatim(QString::number(value))}));
            break;
        case Notice::transfer_failed:
        case Notice::status_file_failed:
            // The value is how the transfer ended (a TransferOutcome).
            if (value <= static_cast<std::uint32_t>(TransferOutcome::aborted)) {
                parts.push_back(Message::verbatim(QStringLiteral(" (")));
                parts.push_back(transfer(static_cast<TransferOutcome>(value)));
                parts.push_back(Message::verbatim(QStringLiteral(")")));
            }
            break;
        case Notice::status_duplicate:
        case Notice::status_stale:
        case Notice::checksum_mismatch:
        case Notice::checksum_unsupported:
        case Notice::request_refused:
        case Notice::option_fallback:
            break;
    }
    return Message::sequence(std::move(parts));
}

}  // namespace adl::app

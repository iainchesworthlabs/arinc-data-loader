#include "adl/app/notice_text.hpp"

#include <QObject>

#include "adl/app/text.hpp"

namespace adl::app {

using adl::arinc615a::Notice;

QString notice_text(Notice notice, std::string_view subject, std::uint32_t value) {
    QString text = to_qstring(adl::arinc615a::to_string(notice));
    if (!subject.empty()) {
        text += QStringLiteral(": ") + to_qstring(subject);
    }
    switch (notice) {
        case Notice::status_counter_gap:
        case Notice::files_missing:
            if (value != 0) {
                text += QStringLiteral(" (%1)").arg(value);
            }
            break;
        case Notice::waiting:
        case Notice::file_not_available:
            text += QObject::tr(" (%1 s)").arg(value);
            break;
        case Notice::port_option_ignored:
            text += QObject::tr(" (port %1)").arg(value);
            break;
        case Notice::transfer_failed:
        case Notice::status_file_failed:
            // The value is how the transfer ended (a TransferOutcome).
            if (value <= static_cast<std::uint32_t>(adl::arinc615a::TransferOutcome::aborted)) {
                text += QStringLiteral(" (%1)").arg(to_qstring(adl::arinc615a::to_string(
                    static_cast<adl::arinc615a::TransferOutcome>(value))));
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
    return text;
}

}  // namespace adl::app

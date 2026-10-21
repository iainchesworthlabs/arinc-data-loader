#pragma once

// The status codes of ARINC 615A-3 Table 6.4.10-1, and which of them each status
// field of the protocol files may hold.

#include <cstdint>
#include <string_view>

namespace adl::arinc615a {

// The status codes of Table 6.4.10-1, as written to the protocol files and, in four
// hexadecimal characters, to an ABORT message. The table shares 0x1007 between the load
// of a header file (.LUS) and the download of a file (.LNS).
enum class StatusCode : std::uint16_t {
    accepted = 0x0001,                 // accepted, not yet started
    in_progress = 0x0002,              // in progress
    completed = 0x0003,                // completed without error
    in_progress_with_text = 0x0004,    // in progress, the description says more
    operation_denied = 0x1000,         // not accepted; the description says why
    operation_not_supported = 0x1002,  // the target does not support the operation
    aborted_by_target = 0x1003,        // the description says why
    aborted_by_data_loader = 0x1004,   // the target got an ABORT message from the protocol
    aborted_by_operator = 0x1005,      // the target got an ABORT message from the operator
    item_failed = 0x1007,              // one load, or one file, failed; the description says why
};

// Which of the sets of codes a field of the protocol files may hold (sections 6.4.1 to
// 6.4.7).
enum class StatusField {
    acceptance,  // the answer to an initialisation: .LCI .LUI .LND .LNO
    operation,   // the status of the whole operation: .LCS, .LUS, .LNS
    item,        // the status of one load or one file: the entries of .LUS and .LNS
};

[[nodiscard]] constexpr bool is_known(StatusCode code) noexcept {
    switch (code) {
        case StatusCode::accepted:
        case StatusCode::in_progress:
        case StatusCode::completed:
        case StatusCode::in_progress_with_text:
        case StatusCode::operation_denied:
        case StatusCode::operation_not_supported:
        case StatusCode::aborted_by_target:
        case StatusCode::aborted_by_data_loader:
        case StatusCode::aborted_by_operator:
        case StatusCode::item_failed:
            return true;
    }
    return false;
}

[[nodiscard]] constexpr bool allowed_in(StatusField field, StatusCode code) noexcept {
    switch (field) {
        case StatusField::acceptance:
            return code == StatusCode::accepted || code == StatusCode::operation_denied ||
                   code == StatusCode::operation_not_supported;
        case StatusField::operation:
            return code == StatusCode::accepted || code == StatusCode::in_progress ||
                   code == StatusCode::completed || code == StatusCode::in_progress_with_text ||
                   code == StatusCode::aborted_by_target ||
                   code == StatusCode::aborted_by_data_loader ||
                   code == StatusCode::aborted_by_operator;
        case StatusField::item:
            return allowed_in(StatusField::operation, code) || code == StatusCode::item_failed;
    }
    return false;
}

// Whether the standard says the description that goes with the code carries the reason
// or the details; with any other code it is ignored (and may hold left-over text).
[[nodiscard]] constexpr bool description_is_meaningful(StatusCode code) noexcept {
    return code == StatusCode::in_progress_with_text || code == StatusCode::operation_denied ||
           code == StatusCode::aborted_by_target || code == StatusCode::item_failed;
}

[[nodiscard]] constexpr std::string_view to_string(StatusCode code) noexcept {
    switch (code) {
        case StatusCode::accepted:
            return "accepted";
        case StatusCode::in_progress:
            return "in progress";
        case StatusCode::completed:
            return "completed";
        case StatusCode::in_progress_with_text:
            return "in progress, with details";
        case StatusCode::operation_denied:
            return "denied";
        case StatusCode::operation_not_supported:
            return "not supported";
        case StatusCode::aborted_by_target:
            return "aborted by the target";
        case StatusCode::aborted_by_data_loader:
            return "aborted by the data loader";
        case StatusCode::aborted_by_operator:
            return "aborted by the operator";
        case StatusCode::item_failed:
            return "failed";
    }
    return "unknown status code";
}

}  // namespace adl::arinc615a

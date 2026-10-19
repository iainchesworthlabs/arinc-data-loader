// The operations of section 5.4 on top of the slots of data_loader.cpp: what the initialization
// file, the status files, the lists and the files of a download mean, when the data load protocol
// time-out has run out, and how an operation is interrupted (5.4.5.2, 6.3.5).

#include <algorithm>
#include <cstring>

#include "adl/arinc615a/data_loader.hpp"

namespace adl::arinc615a {
namespace {

[[nodiscard]] StatusCode abort_status_for(OutcomeCode code) noexcept {
    switch (code) {
        case OutcomeCode::aborted_by_target:
            return StatusCode::aborted_by_target;
        case OutcomeCode::aborted_by_operator:
            return StatusCode::aborted_by_operator;
        default:
            return StatusCode::aborted_by_data_loader;
    }
}

}  // namespace

// ---- Ends of transfers -------------------------------------------------------------------------

void DataLoader::on_slot_finished(Instant now, std::size_t index) noexcept {
    Slot& slot = slots_[index];
    if (slot.transfer.is_client()) {
        on_client_finished(now, slot);
    } else {
        on_server_finished(now, slot);
    }
}

void DataLoader::on_client_finished(Instant now, Slot& slot) noexcept {
    const TransferResult& result = slot.transfer.result();
    end_suspension(now, slot);
    if (done()) {
        return;
    }
    switch (result.outcome) {
        case TransferOutcome::succeeded:
            if (slot.purpose == Purpose::init_file) {
                on_init_file(now, slot);
            } else {
                on_list_written(now);
            }
            return;
        case TransferOutcome::aborted:
        case TransferOutcome::running:
            return;
        case TransferOutcome::wait_requested:
            // The target asks for a pause (its mode change, section 6.3.1): the request is made
            // again after it. That is neither an error nor a DLP retry.
            notice(Notice::waiting, slot.name_view(), result.wait_seconds);
            if (++slot.waits > config_.max_waits) {
                outcome_.failed_transfer = result.outcome;
                finish_operation(now, OutcomeCode::transfer_failed);
                return;
            }
            slot.retry_pending = true;
            slot.retry_at = now + whole_seconds(result.wait_seconds);
            return;
        case TransferOutcome::peer_abort:
            finish_operation(now, result.abort_status == StatusCode::aborted_by_operator
                                      ? OutcomeCode::aborted_by_operator
                                  : result.abort_status == StatusCode::aborted_by_data_loader
                                      ? OutcomeCode::aborted_by_data_loader
                                      : OutcomeCode::aborted_by_target);
            return;
        case TransferOutcome::local_error:
            finish_operation(now, OutcomeCode::local_error);
            return;
        case TransferOutcome::timed_out:
        case TransferOutcome::peer_error:
        case TransferOutcome::option_refused:
        case TransferOutcome::protocol_error:
        case TransferOutcome::size_mismatch:
            break;
    }
    // A target may not take the options we asked for (error 8, or an OACK it should not send):
    // the request is made again without any, which costs no DLP retry (5.3.2.2: a transfer
    // should not fail because an option is not implemented).
    const bool refused = result.outcome == TransferOutcome::option_refused ||
                         (result.outcome == TransferOutcome::peer_error &&
                          result.error_code == TftpErrorCode::option_refused);
    if (refused && !slot.plain) {
        slot.plain = true;
        notice(Notice::option_fallback, slot.name_view(), 0);
        slot.retry_pending = true;
        slot.retry_at = now;
        return;
    }
    // Attachment 4, 4-4.2: a file transfer that failed is tried again, once.
    if (slot.attempts < config_.dlp_retries) {
        ++slot.attempts;
        notice(Notice::transfer_failed, slot.name_view(),
               static_cast<std::uint32_t>(result.outcome));
        slot.retry_pending = true;
        slot.retry_at = now;
        return;
    }
    outcome_.failed_transfer = result.outcome;
    finish_operation(now, OutcomeCode::transfer_failed);
}

void DataLoader::on_server_finished(Instant now, Slot& slot) noexcept {
    const TransferResult& result = slot.transfer.result();
    const bool ok = result.outcome == TransferOutcome::succeeded;
    const bool silent = result.outcome == TransferOutcome::aborted;
    switch (slot.purpose) {
        case Purpose::data_out:
            services_.source->close(slot.io.source_handle);
            if (ok && !done()) {
                ++outcome_.files_served;
                if (services_.listener != nullptr) {
                    services_.listener->on_file_sent(slot.name_view(), result.bytes);
                }
            } else if (!ok && !silent) {
                notice(Notice::transfer_failed, slot.name_view(),
                       static_cast<std::uint32_t>(result.outcome));
            }
            break;
        case Purpose::data_in:
            if (ok && (!done() || outcome_.code == OutcomeCode::completed)) {
                on_data_received(now, slot);
            } else {
                services_.sink->discard(slot.io.sink_handle);
                if (slot.expected_index != no_index) {
                    expected_[slot.expected_index].in_progress = false;
                }
                if (!ok && !silent) {
                    notice(Notice::transfer_failed, slot.name_view(),
                           static_cast<std::uint32_t>(result.outcome));
                }
            }
            break;
        case Purpose::status_file:
            if (ok) {
                if (!done()) {
                    on_status_file(now, slot);
                }
            } else if (!silent) {
                notice(Notice::status_file_failed, slot.name_view(),
                       static_cast<std::uint32_t>(result.outcome));
            }
            break;
        case Purpose::config_list:
            if (ok) {
                if (!done()) {
                    on_configuration_list(now, slot);
                }
            } else if (!silent) {
                notice(Notice::transfer_failed, slot.name_view(),
                       static_cast<std::uint32_t>(result.outcome));
            }
            break;
        case Purpose::download_list:
            if (ok) {
                if (!done()) {
                    on_download_list(now, slot);
                }
            } else if (!silent) {
                notice(Notice::transfer_failed, slot.name_view(),
                       static_cast<std::uint32_t>(result.outcome));
            }
            break;
        case Purpose::none:
        case Purpose::init_file:
        case Purpose::list_write:
            break;
    }
    end_suspension(now, slot);
}

// ---- The initialization file
// ---------------------------------------------------------------------

void DataLoader::on_init_file(Instant now, Slot& slot) noexcept {
    const std::span<const std::byte> bytes =
        std::span<const std::byte>{slot.file}.first(slot.io.length);
    if (config_.control_port != tftp_control_port && !slot.transfer.result().port_acknowledged) {
        notice(Notice::port_option_ignored, slot.name_view(), config_.control_port);
    }
    const auto version = peek_protocol_version(bytes);
    if (!version) {
        finish_operation(now, OutcomeCode::protocol_error);
        return;
    }
    if (std::string_view{version->data(), version->size()} != protocol_version) {
        // 6.4: the data loader that does not know the target's protocol version aborts the
        // operation by answering the first status file write request with an ABORT message.
        abort_status_ = StatusCode::aborted_by_data_loader;
        abort_outcome_ = OutcomeCode::unsupported_protocol_version;
        arm_watchdog(now, config_.dlp_timeout, false);
        return;
    }
    const auto file = decode_initialization_file(bytes);
    if (!file) {
        finish_operation(now, OutcomeCode::protocol_error);
        return;
    }
    if (services_.listener != nullptr) {
        services_.listener->on_initialization(*file);
    }
    switch (file->status) {
        case StatusCode::accepted:
            accepted_ = true;
            // From here the target owes a status file (or, for information, its list) within the
            // data load protocol time-out of the end of this transfer (Attachment 4, 4-4.1).
            arm_watchdog(now, config_.dlp_timeout, false);
            return;
        case StatusCode::operation_denied:
            finish_operation(now, OutcomeCode::denied);
            return;
        case StatusCode::operation_not_supported:
            finish_operation(now, OutcomeCode::not_supported);
            return;
        default:
            finish_operation(now, OutcomeCode::protocol_error);
            return;
    }
}

// ---- Lists --------------------------------------------------------------------------------------

void DataLoader::send_list(Instant now) noexcept {
    if (list_started_ ||
        (kind_ != OperationKind::upload && kind_ != OperationKind::download_media)) {
        return;
    }
    list_started_ = true;
    const std::size_t index = allocate_slot();
    if (index == no_index) {
        finish_operation(now, OutcomeCode::local_error);
        return;
    }
    Slot& slot = slots_[index];
    slot.used = true;
    slot.purpose = Purpose::list_write;
    const std::string_view extension_text =
        extension(kind_ == OperationKind::upload ? ProtocolFileKind::lur : ProtocolFileKind::lnr);
    slot.name_length = config_.thw_id_pos.size() + extension_text.size();
    std::memcpy(slot.name.data(), config_.thw_id_pos.data(), config_.thw_id_pos.size());
    std::memcpy(slot.name.data() + config_.thw_id_pos.size(), extension_text.data(),
                extension_text.size());
    begin_suspension(slot);
    start_client(now, index);
    pump(now, index);
}

void DataLoader::send_selection(Instant now, std::span<const std::string_view> names) noexcept {
    for (const std::string_view name : names) {
        if (const auto expected = expect_file(name); !expected) {
            finish_operation(now, OutcomeCode::local_error);
            return;
        }
    }
    outcome_.files_requested = static_cast<std::uint32_t>(expected_count_);
    const auto size =
        encode_download_answer(DownloadAnswer{List<std::string_view>{names}}, outbound_);
    if (!size) {
        finish_operation(now, OutcomeCode::local_error);
        return;
    }
    outbound_length_ = *size;
    list_started_ = true;
    const std::size_t index = allocate_slot();
    if (index == no_index) {
        finish_operation(now, OutcomeCode::local_error);
        return;
    }
    Slot& slot = slots_[index];
    slot.used = true;
    slot.purpose = Purpose::list_write;
    const std::string_view extension_text = extension(ProtocolFileKind::lna);
    slot.name_length = config_.thw_id_pos.size() + extension_text.size();
    std::memcpy(slot.name.data(), config_.thw_id_pos.data(), config_.thw_id_pos.size());
    std::memcpy(slot.name.data() + config_.thw_id_pos.size(), extension_text.data(),
                extension_text.size());
    begin_suspension(slot);
    start_client(now, index);
    pump(now, index);
}

Result<void> DataLoader::provide_selection(Instant now,
                                           std::span<const std::string_view> names) noexcept {
    begin_calls();
    if (!selection_pending_ || done()) {
        return std::unexpected(Error::invalid_value);
    }
    selection_pending_ = false;
    send_selection(now, names);
    end_calls(now);
    return {};
}

void DataLoader::on_list_written(Instant /*now*/) noexcept {
    list_sent_ = true;
}

void DataLoader::on_configuration_list(Instant now, Slot& slot) noexcept {
    const auto list =
        decode_configuration_list(std::span<const std::byte>{slot.file}.first(slot.io.length));
    if (!list) {
        finish_operation(now, OutcomeCode::protocol_error);
        return;
    }
    list_received_ = true;
    if (services_.listener != nullptr) {
        services_.listener->on_configuration_list(*list);
    }
    // 6.3.1: if the list came before any status file did, the target may send no status file at
    // all, and the operation is over with the list. If it has been sending status files, the
    // last of them (0x0003) closes it.
    if (!have_status_) {
        completing_ = true;
    }
}

void DataLoader::on_download_list(Instant now, Slot& slot) noexcept {
    const auto list =
        decode_download_list(std::span<const std::byte>{slot.file}.first(slot.io.length));
    if (!list) {
        finish_operation(now, OutcomeCode::protocol_error);
        return;
    }
    list_received_ = true;
    FileSelection selection;
    SelectionDecision decision = SelectionDecision::cancelled;
    if (services_.listener != nullptr) {
        decision = services_.listener->on_download_list(*list, selection);
    }
    switch (decision) {
        case SelectionDecision::selected:
            send_selection(now, selection.names());
            return;
        case SelectionDecision::deferred:
            selection_pending_ = true;
            return;
        case SelectionDecision::cancelled:
            do_abort(now, StatusCode::aborted_by_operator);
            return;
    }
}

// ---- Files of a download
// -------------------------------------------------------------------------

Result<void> DataLoader::expect_file(std::string_view name) noexcept {
    if (find_expected(name) != no_index) {
        return {};
    }
    if (expected_count_ == expected_.size() || name.size() > names_.size() - names_used_) {
        return std::unexpected(Error::field_too_large);
    }
    Expected& entry = expected_[expected_count_++];
    entry.name_offset = names_used_;
    entry.name_length = name.size();
    entry.received = false;
    entry.in_progress = false;
    if (!name.empty()) {
        std::memcpy(names_.data() + names_used_, name.data(), name.size());
    }
    names_used_ += name.size();
    return {};
}

std::size_t DataLoader::find_expected(std::string_view name) const noexcept {
    for (std::size_t i = 0; i < expected_count_; ++i) {
        const Expected& entry = expected_[i];
        if (std::string_view{names_.data() + entry.name_offset, entry.name_length} == name) {
            return i;
        }
    }
    return no_index;
}

void DataLoader::on_data_received(Instant now, Slot& slot) noexcept {
    const TransferResult& result = slot.transfer.result();
    Expected& entry = expected_[slot.expected_index];
    entry.in_progress = false;
    const ReceivedFile file{result.bytes, result.checksum};
    if (const auto finished = services_.sink->finish(slot.io.sink_handle, file); !finished) {
        finish_operation(now, OutcomeCode::local_error);
        return;
    }
    entry.received = true;
    ++outcome_.files_received;
    // The checksum is for the operator, never a reason to fail or to throw the file away
    // (5.3.2.3.8.5).
    if (result.checksum == ChecksumVerdict::mismatched) {
        ++outcome_.checksum_mismatches;
        notice(Notice::checksum_mismatch, slot.name_view(), 0);
    } else if (result.checksum == ChecksumVerdict::unsupported) {
        notice(Notice::checksum_unsupported, slot.name_view(), 0);
    }
    if (services_.listener != nullptr) {
        services_.listener->on_file_received(slot.name_view(), file);
    }
}

std::size_t DataLoader::active_data_transfers() const noexcept {
    std::size_t count = 0;
    for (std::size_t i = 0; i < slot_count_; ++i) {
        const Slot& slot = slots_[i];
        const bool data = slot.purpose == Purpose::config_list ||
                          slot.purpose == Purpose::download_list ||
                          slot.purpose == Purpose::data_in || slot.purpose == Purpose::data_out;
        if (slot.used && data && slot.transfer.active() && !slot.transfer.lingering()) {
            ++count;
        }
    }
    return count;
}

// ---- Status files
// --------------------------------------------------------------------------------

// Section 6.4.3: the counter starts at 0, goes up by one with each status file the target sends
// and wraps from 0xFFFF to 0, "to detect the duplication of status files or the loss of status
// files". False for a status file that is a repeat (the same counter again) or older than the
// last (reordered): its content is not acted on a second time.
bool DataLoader::take_counter(std::uint16_t counter) noexcept {
    if (!have_status_) {
        // The text says the initial value is 0 and that the target increments before sending;
        // either 0 or 1 is taken as the first file, and a larger number says files were lost.
        have_status_ = true;
        last_counter_ = counter;
        ++outcome_.statuses_received;
        if (counter > 1) {
            outcome_.statuses_lost += counter - 1U;
            notice(Notice::status_counter_gap, {}, counter - 1U);
        }
        return true;
    }
    const auto distance = static_cast<std::uint16_t>(counter - last_counter_);
    if (distance == 0) {
        ++outcome_.statuses_repeated;
        notice(Notice::status_duplicate, {}, counter);
        return false;
    }
    if (distance >= 0x8000U) {
        ++outcome_.statuses_repeated;
        notice(Notice::status_stale, {}, counter);
        return false;
    }
    if (distance > 1) {
        outcome_.statuses_lost += distance - 1U;
        notice(Notice::status_counter_gap, {}, distance - 1U);
    }
    last_counter_ = counter;
    ++outcome_.statuses_received;
    return true;
}

void DataLoader::on_status_file(Instant now, Slot& slot) noexcept {
    const std::span<const std::byte> bytes =
        std::span<const std::byte>{slot.file}.first(slot.io.length);
    switch (kind_) {
        case OperationKind::information: {
            const auto status = decode_configuration_status(bytes);
            if (!status) {
                finish_operation(now, OutcomeCode::protocol_error);
                return;
            }
            if (!take_counter(status->counter)) {
                return;
            }
            if (services_.listener != nullptr) {
                services_.listener->on_configuration_status(*status);
            }
            apply_status(now, status->status, status->exception_timer);
            return;
        }
        case OperationKind::upload: {
            const auto status = decode_upload_status(bytes);
            if (!status) {
                finish_operation(now, OutcomeCode::protocol_error);
                return;
            }
            if (!take_counter(status->counter)) {
                return;
            }
            outcome_.items_failed = 0;
            for (const LoadProgress& load : status->loads) {
                if (load.status == StatusCode::item_failed) {
                    ++outcome_.items_failed;
                }
            }
            if (services_.listener != nullptr) {
                services_.listener->on_upload_status(*status);
            }
            apply_status(now, status->status, status->exception_timer);
            return;
        }
        case OperationKind::download_media:
        case OperationKind::download_operator: {
            const auto status = decode_download_status(bytes);
            if (!status) {
                finish_operation(now, OutcomeCode::protocol_error);
                return;
            }
            if (!take_counter(status->counter)) {
                return;
            }
            outcome_.items_failed = 0;
            for (const FileProgress& file : status->files) {
                if (file.status == StatusCode::item_failed) {
                    ++outcome_.items_failed;
                }
            }
            if (services_.listener != nullptr) {
                services_.listener->on_download_status(*status);
            }
            apply_status(now, status->status, status->exception_timer);
            return;
        }
    }
}

void DataLoader::apply_status(Instant now, StatusCode status,
                              std::uint16_t exception_timer) noexcept {
    last_status_ = status;
    outcome_.last_status = status;
    switch (status) {
        case StatusCode::accepted:
            arm_watchdog(now, config_.dlp_timeout, false);
            // 6.2.7.3: the list goes after the status file that says "accepted, not yet
            // started", which is what synchronises the two ends.
            if (!abort_status_) {
                send_list(now);
            }
            return;
        case StatusCode::in_progress:
        case StatusCode::in_progress_with_text: {
            // The exception timer says how long the target will keep silent (6.3.1); the
            // data loader waits that long for the next status file if it is longer than the
            // data load protocol time-out.
            const Duration silent = whole_seconds(exception_timer);
            const bool longer = silent > config_.dlp_timeout;
            arm_watchdog(now, longer ? silent : config_.dlp_timeout, longer);
            return;
        }
        case StatusCode::completed:
            if ((kind_ == OperationKind::upload || kind_ == OperationKind::download_media) &&
                !list_started_) {
                // A target that completes before it has been given the list has broken the
                // protocol.
                finish_operation(now, OutcomeCode::protocol_error);
                return;
            }
            completing_ = true;
            return;
        case StatusCode::aborted_by_target:
            finish_operation(now, OutcomeCode::aborted_by_target);
            return;
        case StatusCode::aborted_by_data_loader:
        case StatusCode::aborted_by_operator:
            if (abort_status_) {
                // The target's confirmation of the abort we asked for (6.3.5).
                outcome_.abort_confirmed = true;
                finish_aborted(now, true);
            } else {
                finish_operation(now, status == StatusCode::aborted_by_operator
                                          ? OutcomeCode::aborted_by_operator
                                          : OutcomeCode::aborted_by_data_loader);
            }
            return;
        default:
            // The decoders allow no other code in the status of an operation.
            finish_operation(now, OutcomeCode::protocol_error);
            return;
    }
}

// ---- The data load protocol time-out, completion, abort
// ------------------------------------------

void DataLoader::arm_watchdog(Instant now, Duration window, bool from_exception) noexcept {
    watching_ = true;
    watch_window_ = window;
    watch_from_exception_ = from_exception;
    watch_deadline_ = suspend_count_ == 0 ? now + window : never;
}

void DataLoader::check_watchdog(Instant now) noexcept {
    if (!watching_ || done() || suspend_count_ != 0 || now < watch_deadline_) {
        return;
    }
    if (abort_status_) {
        // The target never confirmed (or never wrote a status file at all).
        finish_aborted(now, false);
        return;
    }
    finish_operation(now, watch_from_exception_ ? OutcomeCode::exception_timer_expired
                                                : OutcomeCode::dlp_timed_out);
}

void DataLoader::check_completion(Instant now) noexcept {
    if (!completing_ || done() || active_data_transfers() != 0) {
        return;
    }
    switch (kind_) {
        case OperationKind::information:
        case OperationKind::download_operator:
            if (!list_received_) {
                finish_operation(now, OutcomeCode::protocol_error);
                return;
            }
            break;
        case OperationKind::upload:
        case OperationKind::download_media:
            break;
    }
    if (outcome_.files_requested > outcome_.files_received) {
        notice(Notice::files_missing, {}, outcome_.files_requested - outcome_.files_received);
    }
    finish_operation(now, OutcomeCode::completed);
}

void DataLoader::abort_active(Instant now, StatusCode status) noexcept {
    for (std::size_t i = 0; i < slot_count_; ++i) {
        Slot& slot = slots_[i];
        if (!slot.used) {
            continue;
        }
        slot.retry_pending = false;
        if (slot.transfer.active()) {
            slot.transfer.abort(status);
        }
        pump(now, i);
    }
}

void DataLoader::finish_operation(Instant now, OutcomeCode code) noexcept {
    if (done()) {
        return;
    }
    outcome_.code = code;
    outcome_.last_status = last_status_;
    watching_ = false;
    watch_deadline_ = never;
    selection_pending_ = false;
    completing_ = false;
    const bool clean = code == OutcomeCode::completed || code == OutcomeCode::denied ||
                       code == OutcomeCode::not_supported;
    if (!clean) {
        // 6.3.5: whatever else is open is ended with an ABORT message that says why.
        abort_active(now, abort_status_for(code));
    }
}

void DataLoader::finish_aborted(Instant now, bool confirmed) noexcept {
    outcome_.abort_confirmed = confirmed;
    if (abort_outcome_) {
        finish_operation(now, *abort_outcome_);
        return;
    }
    finish_operation(now, abort_status_ == StatusCode::aborted_by_operator
                              ? OutcomeCode::aborted_by_operator
                              : OutcomeCode::aborted_by_data_loader);
}

void DataLoader::abort(Instant now, StatusCode status) noexcept {
    begin_calls();
    do_abort(now, status);
    end_calls(now);
}

void DataLoader::do_abort(Instant now, StatusCode status) noexcept {
    if (!started_ || done() || abort_status_) {
        return;
    }
    const StatusCode abort_status = status == StatusCode::aborted_by_operator
                                        ? StatusCode::aborted_by_operator
                                        : StatusCode::aborted_by_data_loader;
    abort_status_ = abort_status;
    if (!accepted_) {
        // The target has not started anything yet, so it has nothing to confirm.
        finish_aborted(now, false);
        return;
    }
    // 6.3.5: the transfers that are open are ended with the ABORT message. That informs the
    // target as much as answering its next status file request with one does, so when any of
    // them carried it the status file after that is the target's confirmation; if none did, the
    // next request for a status file is answered with the message.
    for (std::size_t i = 0; i < slot_count_; ++i) {
        Slot& slot = slots_[i];
        if (!slot.used) {
            continue;
        }
        slot.retry_pending = false;
        if (slot.transfer.active() && !slot.transfer.lingering() &&
            slot.transfer.abort(abort_status)) {
            abort_answered_ = true;
        }
        pump(now, i);
    }
    arm_watchdog(now, config_.dlp_timeout, false);
}

}  // namespace adl::arinc615a

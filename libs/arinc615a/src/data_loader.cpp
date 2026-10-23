#include "adl/arinc615a/data_loader.hpp"

#include <adl/profiling.hpp>

#include <algorithm>
#include <cstring>
#include <variant>

namespace adl::arinc615a
{
namespace
{

constexpr std::size_t longest_name = 254;

[[nodiscard]] std::size_t transfer_buffer_bytes(const LoaderConfig& config) noexcept
{
    return std::max(config.max_block_size + 4, TftpTransfer::min_buffer_bytes);
}

[[nodiscard]] std::size_t slots_for(const LoaderConfig& config) noexcept
{
    return std::clamp<std::size_t>(config.max_transfers, 2, DataLoader::max_slots);
}

} // namespace

bool is_valid_thw_id_pos(std::string_view id) noexcept
{
    return !id.empty() && id.size() <= max_thw_id_pos_length &&
           std::ranges::none_of(id, [](char c)
                                { return static_cast<unsigned char>(c) < 0x20 || c == '/' || c == '\\' || c == 0x7F; });
}

// ---- FileSelection ---------------------------------------------------------------------------

bool FileSelection::select(std::string_view name) noexcept
{
    if (count_ == capacity || name.empty() || name.size() > longest_name)
    {
        return false;
    }
    names_[count_++] = name;
    return true;
}

// ---- SlotIo ----------------------------------------------------------------------------------

IoResult<void> DataLoader::SlotIo::write(std::span<const std::byte> block) noexcept
{
    switch (mode)
    {
    case Mode::memory_sink:
        if (block.size() > buffer.size() - length)
        {
            return std::unexpected(IoFailure{IoError::no_space, 0});
        }
        if (!block.empty())
        {
            std::memcpy(buffer.data() + length, block.data(), block.size());
        }
        length += block.size();
        return {};
    case Mode::file_sink:
        return sink->write(sink_handle, block);
    case Mode::none:
    case Mode::memory_source:
    case Mode::file_source:
        break;
    }
    return std::unexpected(IoFailure{IoError::io_error, 0});
}

std::uint64_t DataLoader::SlotIo::size() const noexcept
{
    switch (mode)
    {
    case Mode::memory_source:
        return data.size();
    case Mode::file_source:
        return source_size;
    case Mode::none:
    case Mode::memory_sink:
    case Mode::file_sink:
        break;
    }
    return 0;
}

IoResult<std::size_t> DataLoader::SlotIo::read(std::uint64_t offset, std::span<std::byte> out) noexcept
{
    switch (mode)
    {
    case Mode::memory_source:
    {
        if (offset >= data.size())
        {
            return std::size_t{0};
        }
        const std::size_t count = std::min<std::size_t>(out.size(), data.size() - static_cast<std::size_t>(offset));
        if (count != 0)
        {
            std::memcpy(out.data(), data.data() + offset, count);
        }
        return count;
    }
    case Mode::file_source:
        return source->read(source_handle, offset, out);
    case Mode::none:
    case Mode::memory_sink:
    case Mode::file_sink:
        break;
    }
    return std::unexpected(IoFailure{IoError::io_error, 0});
}

// ---- Set-up ----------------------------------------------------------------------------------

std::size_t DataLoader::workspace_bytes(const LoaderConfig& config) noexcept
{
    return slots_for(config) * (transfer_buffer_bytes(config) + config.max_protocol_file_bytes) +
           config.max_protocol_file_bytes + config.name_pool_bytes;
}

DataLoader::DataLoader(const LoaderConfig& config, std::span<std::byte> workspace,
                       const LoaderServices& services) noexcept
    : config_(config), services_(services), workspace_(workspace)
{
}

Result<void> DataLoader::start(Instant now, const Operation& operation) noexcept
{
    const adl::profiling::Zone zone{"615a: start operation"};
    if (started_)
    {
        return std::unexpected(Error::invalid_value);
    }
    const std::string_view id = config_.thw_id_pos;
    if (!is_valid_thw_id_pos(id) || config_.control_port == 0 || config_.max_block_size < tftp_min_block_size ||
        config_.max_block_size > tftp_max_block_size || config_.preferred_block_size < tftp_min_block_size ||
        config_.preferred_block_size > config_.max_block_size || config_.max_protocol_file_bytes < 64)
    {
        return std::unexpected(Error::invalid_value);
    }
    if (workspace_.size() < workspace_bytes(config_))
    {
        return std::unexpected(Error::buffer_too_small);
    }

    // Lay the workspace out: each slot's datagram buffer and protocol file buffer, the buffer
    // the outgoing list is encoded into, and the pool of names.
    slot_count_ = slots_for(config_);
    std::size_t offset = 0;
    const std::size_t tx_bytes = transfer_buffer_bytes(config_);
    for (std::size_t i = 0; i < slot_count_; ++i)
    {
        slots_[i].transfer.set_buffer(workspace_.subspan(offset, tx_bytes));
        offset += tx_bytes;
        slots_[i].file = workspace_.subspan(offset, config_.max_protocol_file_bytes);
        offset += config_.max_protocol_file_bytes;
    }
    outbound_ = workspace_.subspan(offset, config_.max_protocol_file_bytes);
    offset += config_.max_protocol_file_bytes;
    names_ = std::span<char>{reinterpret_cast<char*>(workspace_.data() + offset), config_.name_pool_bytes};
    begin_calls();

    ProtocolFileKind init_kind = ProtocolFileKind::lci;
    if (const auto* upload = std::get_if<UploadOperation>(&operation))
    {
        kind_ = OperationKind::upload;
        init_kind = ProtocolFileKind::lui;
        const auto size = encode_upload_request(upload->loads, outbound_);
        if (!size)
        {
            return std::unexpected(size.error());
        }
        outbound_length_ = *size;
    }
    else if (const auto* media = std::get_if<MediaDownloadOperation>(&operation))
    {
        kind_ = OperationKind::download_media;
        init_kind = ProtocolFileKind::lnd;
        const auto size = encode_download_request(media->files, outbound_);
        if (!size)
        {
            return std::unexpected(size.error());
        }
        outbound_length_ = *size;
        for (const FileRequest& file : media->files.files)
        {
            if (const auto expected = expect_file(file.file_name); !expected)
            {
                return std::unexpected(expected.error());
            }
        }
    }
    else if (std::holds_alternative<OperatorDownloadOperation>(operation))
    {
        kind_ = OperationKind::download_operator;
        init_kind = ProtocolFileKind::lno;
    }
    else
    {
        kind_ = OperationKind::information;
    }
    outcome_.files_requested = static_cast<std::uint32_t>(expected_count_);

    const std::string_view extension_text = extension(init_kind);
    std::memcpy(init_name_, id.data(), id.size());
    std::memcpy(init_name_ + id.size(), extension_text.data(), extension_text.size());

    started_ = true;
    const std::size_t index = allocate_slot();
    Slot& slot = slots_[index];
    slot.used = true;
    slot.purpose = Purpose::init_file;
    slot.name_length = id.size() + extension_text.size();
    std::memcpy(slot.name.data(), init_name_, slot.name_length);
    start_client(now, index);
    pump(now, index);
    return {};
}

// ---- Commands, slots ---------------------------------------------------------------------------

void DataLoader::begin_calls() noexcept
{
    command_count_ = 0;
}

void DataLoader::push(const Command& command) noexcept
{
    if (command_count_ < commands_.size())
    {
        commands_[command_count_++] = command;
    }
}

TransferSettings DataLoader::transfer_settings() const noexcept
{
    TransferSettings settings;
    settings.max_block_size = config_.max_block_size;
    settings.timeout = config_.tftp_timeout;
    settings.retries = config_.tftp_retries;
    settings.dally = config_.dally;
    return settings;
}

std::size_t DataLoader::allocate_slot() noexcept
{
    for (std::size_t i = 0; i < slot_count_; ++i)
    {
        if (!slots_[i].used)
        {
            Slot& slot = slots_[i];
            slot.transfer.reset();
            slot.io = SlotIo{};
            slot.purpose = Purpose::none;
            slot.used = false;
            slot.handled = false;
            slot.suspends = false;
            slot.retry_pending = false;
            slot.plain = false;
            slot.attempts = 0;
            slot.waits = 0;
            slot.retry_at = never;
            slot.expected_index = no_index;
            slot.name_length = 0;
            return i;
        }
    }
    return no_index;
}

void DataLoader::release_slot(std::size_t index) noexcept
{
    Slot& slot = slots_[index];
    slot.used = false;
    slot.purpose = Purpose::none;
    slot.transfer.reset();
    slot.io = SlotIo{};
    Command command;
    command.kind = Command::Kind::release_slot;
    command.slot = static_cast<std::uint8_t>(index);
    push(command);
}

void DataLoader::begin_suspension(Slot& slot) noexcept
{
    if (!slot.suspends)
    {
        slot.suspends = true;
        ++suspend_count_;
        watch_deadline_ = never;
    }
}

void DataLoader::end_suspension(Instant now, Slot& slot) noexcept
{
    if (!slot.suspends)
    {
        return;
    }
    slot.suspends = false;
    --suspend_count_;
    if (suspend_count_ == 0 && watching_ && !done())
    {
        watch_deadline_ = now + watch_window_;
    }
}

void DataLoader::notice(Notice what, std::string_view subject, std::uint32_t value) noexcept
{
    if (services_.listener != nullptr)
    {
        services_.listener->on_notice(what, subject, value);
    }
}

bool DataLoader::name_matches(std::string_view stem) const noexcept
{
    return stem == config_.thw_id_pos;
}

// Starts (or starts again) the transfer a client slot is for.
void DataLoader::start_client(Instant now, std::size_t index) noexcept
{
    Slot& slot = slots_[index];
    slot.retry_pending = false;
    slot.retry_at = never;
    slot.handled = false;
    ClientOptions options;
    Result<void> started;
    if (slot.purpose == Purpose::init_file)
    {
        slot.io = SlotIo{};
        slot.io.mode = SlotIo::Mode::memory_sink;
        slot.io.buffer = slot.file;
        if (!slot.plain)
        {
            options.block_size = config_.preferred_block_size;
            options.transfer_size = 0;
            if (config_.control_port != tftp_control_port)
            {
                options.port = config_.control_port;
            }
        }
        started =
            slot.transfer.start_read(now, config_.target, slot.name_view(), options, slot.io, transfer_settings());
    }
    else
    {
        slot.io = SlotIo{};
        slot.io.mode = SlotIo::Mode::memory_source;
        slot.io.data = std::span<const std::byte>{outbound_}.first(outbound_length_);
        if (!slot.plain)
        {
            options.block_size = config_.preferred_block_size;
        }
        started =
            slot.transfer.start_write(now, config_.target, slot.name_view(), options, slot.io, transfer_settings());
    }
    if (!started)
    {
        finish_operation(now, OutcomeCode::local_error);
    }
}

// Collects what a slot's transfer has to send and deals with its ending.
void DataLoader::pump(Instant now, std::size_t index) noexcept
{
    Slot& slot = slots_[index];
    if (!slot.used)
    {
        return;
    }
    if (const auto outgoing = slot.transfer.take_outgoing())
    {
        Command command;
        command.kind = Command::Kind::send_from_slot;
        command.slot = static_cast<std::uint8_t>(index);
        command.to = outgoing->to;
        command.bytes = outgoing->bytes;
        push(command);
    }
    if (!slot.handled && slot.transfer.finished())
    {
        slot.handled = true;
        on_slot_finished(now, index);
    }
    if (slot.used && slot.handled && !slot.transfer.active() && !slot.retry_pending)
    {
        release_slot(index);
    }
}

void DataLoader::run_retries(Instant now) noexcept
{
    for (std::size_t i = 0; i < slot_count_; ++i)
    {
        Slot& slot = slots_[i];
        if (slot.used && slot.retry_pending && now >= slot.retry_at)
        {
            if (done())
            {
                slot.retry_pending = false;
            }
            else
            {
                start_client(now, i);
            }
            pump(now, i);
        }
    }
}

void DataLoader::end_calls(Instant now) noexcept
{
    check_watchdog(now);
    check_completion(now);
}

// ---- Driving ---------------------------------------------------------------------------------

void DataLoader::on_transfer_datagram(Instant now, std::size_t index, const Endpoint& from,
                                      std::span<const std::byte> bytes) noexcept
{
    begin_calls();
    if (!started_ || index >= slot_count_ || !slots_[index].used)
    {
        return;
    }
    slots_[index].transfer.on_datagram(now, from, bytes);
    pump(now, index);
    end_calls(now);
}

void DataLoader::on_timer(Instant now) noexcept
{
    begin_calls();
    if (!started_)
    {
        return;
    }
    for (std::size_t i = 0; i < slot_count_; ++i)
    {
        if (slots_[i].used && !slots_[i].retry_pending)
        {
            slots_[i].transfer.on_timer(now);
            pump(now, i);
        }
    }
    run_retries(now);
    end_calls(now);
}

Instant DataLoader::next_wake() const noexcept
{
    Instant wake = never;
    for (std::size_t i = 0; i < slot_count_; ++i)
    {
        const Slot& slot = slots_[i];
        if (!slot.used)
        {
            continue;
        }
        wake = earlier_of(wake, slot.retry_pending ? slot.retry_at : slot.transfer.next_wake());
    }
    if (!done())
    {
        wake = earlier_of(wake, watch_deadline_);
    }
    return wake;
}

bool DataLoader::idle() const noexcept
{
    if (!done())
    {
        return false;
    }
    return std::ranges::none_of(slots_, [](const Slot& slot) { return slot.used; });
}

// ---- Requests from the target ------------------------------------------------------------------

void DataLoader::refuse(const Endpoint& to, TftpErrorCode code, std::string_view text) noexcept
{
    if (const auto size = encode_error(TftpError{code, text}, reply_))
    {
        Command command;
        command.kind = Command::Kind::send_from_control;
        command.to = to;
        command.bytes = std::span<const std::byte>{reply_}.first(*size);
        push(command);
    }
}

void DataLoader::refuse_wait(const Endpoint& to, std::uint16_t seconds) noexcept
{
    if (const auto size = encode_wait(WaitMessage{seconds}, reply_))
    {
        Command command;
        command.kind = Command::Kind::send_from_control;
        command.to = to;
        command.bytes = std::span<const std::byte>{reply_}.first(*size);
        push(command);
    }
}

void DataLoader::refuse_abort(const Endpoint& to) noexcept
{
    const StatusCode status = abort_status_.value_or(StatusCode::aborted_by_data_loader);
    if (const auto size = encode_abort(AbortMessage{status}, reply_))
    {
        Command command;
        command.kind = Command::Kind::send_from_control;
        command.to = to;
        command.bytes = std::span<const std::byte>{reply_}.first(*size);
        push(command);
    }
}

void DataLoader::on_control_datagram(Instant now, const Endpoint& from, std::span<const std::byte> bytes) noexcept
{
    begin_calls();
    if (!started_ || done() || from.address != config_.target.address)
    {
        return;
    }
    const auto packet = decode_packet(bytes);
    if (!packet)
    {
        return;
    }
    const auto* request = std::get_if<TftpRequest>(&*packet);
    if (request == nullptr)
    {
        return;
    }
    // A repeat of a request already being served (our first answer was lost) is answered by the
    // transfer, not started again.
    for (std::size_t i = 0; i < slot_count_; ++i)
    {
        Slot& slot = slots_[i];
        if (slot.used && !slot.transfer.is_client() && slot.transfer.active() && !slot.transfer.lingering() &&
            slot.transfer.peer() == from)
        {
            slot.transfer.on_repeated_request(now);
            pump(now, i);
            return;
        }
    }
    handle_request(now, from, *request);
    end_calls(now);
}

void DataLoader::handle_request(Instant now, const Endpoint& from, const TftpRequest& request) noexcept
{
    const std::string_view name = request.file_name;
    const bool is_write = request.kind == TftpRequest::Kind::write;
    const auto kind = kind_of_file_name(name);
    const bool protocol_name = kind.has_value() && name_matches(name.substr(0, name.size() - 4));
    const bool status_name =
        protocol_name && is_write &&
        (*kind == ProtocolFileKind::lcs || *kind == ProtocolFileKind::lus || *kind == ProtocolFileKind::lns);

    // While an abort is in progress everything but the target's status files is stopped, and
    // the first status file is answered with the ABORT message (6.2.17, 6.3.5).
    if (abort_status_)
    {
        if (!status_name)
        {
            refuse_abort(from);
            return;
        }
        if (!abort_answered_)
        {
            abort_answered_ = true;
            refuse_abort(from);
            if (abort_outcome_)
            {
                finish_aborted(now, false);
            }
            else
            {
                arm_watchdog(now, config_.dlp_timeout, false);
            }
            return;
        }
    }
    if (!accepted_)
    {
        refuse(from, TftpErrorCode::illegal_operation, "the operation has not been started");
        notice(Notice::request_refused, name, 0);
        return;
    }

    const auto settings = transfer_settings();

    if (status_name)
    {
        const ProtocolFileKind wanted = kind_ == OperationKind::information ? ProtocolFileKind::lcs
                                        : kind_ == OperationKind::upload    ? ProtocolFileKind::lus
                                                                            : ProtocolFileKind::lns;
        if (*kind != wanted)
        {
            refuse(from, TftpErrorCode::access_violation, "not the status file of this operation");
            notice(Notice::request_refused, name, 0);
            return;
        }
        // A newer request for the status file replaces one still open: the target gave up on
        // that one and will not be back.
        for (std::size_t i = 0; i < slot_count_; ++i)
        {
            Slot& other = slots_[i];
            if (other.used && other.purpose == Purpose::status_file && other.transfer.active())
            {
                other.transfer.drop();
                pump(now, i);
            }
        }
        const std::size_t index = allocate_slot();
        if (index == no_index)
        {
            refuse(from, TftpErrorCode::disk_full, "no transfer available");
            notice(Notice::request_refused, name, 0);
            return;
        }
        Slot& slot = slots_[index];
        slot.used = true;
        slot.purpose = Purpose::status_file;
        slot.name_length = std::min(name.size(), slot.name.size());
        std::memcpy(slot.name.data(), name.data(), slot.name_length);
        slot.io.mode = SlotIo::Mode::memory_sink;
        slot.io.buffer = slot.file;
        begin_suspension(slot);
        slot.transfer.accept_write(now, from, request, slot.io, settings);
        pump(now, index);
        return;
    }

    if (is_write && protocol_name && (*kind == ProtocolFileKind::lcl || *kind == ProtocolFileKind::lnl))
    {
        const bool right_one = (*kind == ProtocolFileKind::lcl && kind_ == OperationKind::information) ||
                               (*kind == ProtocolFileKind::lnl && kind_ == OperationKind::download_operator);
        if (!right_one)
        {
            refuse(from, TftpErrorCode::access_violation, "not a file of this operation");
            notice(Notice::request_refused, name, 0);
            return;
        }
        if (list_received_)
        {
            refuse(from, TftpErrorCode::file_already_exists, "already received");
            notice(Notice::request_refused, name, 0);
            return;
        }
        for (std::size_t i = 0; i < slot_count_; ++i)
        {
            Slot& other = slots_[i];
            if (other.used && (other.purpose == Purpose::config_list || other.purpose == Purpose::download_list) &&
                other.transfer.active())
            {
                other.transfer.drop();
                pump(now, i);
            }
        }
        const std::size_t index = allocate_slot();
        if (index == no_index)
        {
            refuse(from, TftpErrorCode::disk_full, "no transfer available");
            notice(Notice::request_refused, name, 0);
            return;
        }
        Slot& slot = slots_[index];
        slot.used = true;
        slot.purpose = *kind == ProtocolFileKind::lcl ? Purpose::config_list : Purpose::download_list;
        slot.name_length = std::min(name.size(), slot.name.size());
        std::memcpy(slot.name.data(), name.data(), slot.name_length);
        slot.io.mode = SlotIo::Mode::memory_sink;
        slot.io.buffer = slot.file;
        // The target's list is part of the list transfer step. While it comes, and while there
        // has been no status file to be silent about, nothing else is waited for.
        if (*kind == ProtocolFileKind::lnl || !have_status_)
        {
            begin_suspension(slot);
        }
        slot.transfer.accept_write(now, from, request, slot.io, settings);
        pump(now, index);
        return;
    }

    if (is_write && !protocol_name)
    {
        // A file of a download.
        if (kind_ != OperationKind::download_media && kind_ != OperationKind::download_operator)
        {
            refuse(from, TftpErrorCode::access_violation, "no file may be written now");
            notice(Notice::request_refused, name, 0);
            return;
        }
        const std::size_t wanted = find_expected(name);
        if (wanted == no_index)
        {
            refuse(from, TftpErrorCode::access_violation, "file not asked for");
            notice(Notice::request_refused, name, 0);
            return;
        }
        if (expected_[wanted].received)
        {
            refuse(from, TftpErrorCode::file_already_exists, "already received");
            notice(Notice::request_refused, name, 0);
            return;
        }
        if (kind_ == OperationKind::download_media && !list_sent_)
        {
            // The list is what tells the target which files to send; a file before it has
            // gone is the target's error.
            refuse(from, TftpErrorCode::illegal_operation, "no list has been sent");
            notice(Notice::request_refused, name, 0);
            return;
        }
        if (services_.sink == nullptr)
        {
            refuse(from, TftpErrorCode::access_violation, "no place to put files");
            notice(Notice::request_refused, name, 0);
            return;
        }
        // The same file again while an earlier try is still open: the newer one wins.
        for (std::size_t i = 0; i < slot_count_; ++i)
        {
            Slot& other = slots_[i];
            if (other.used && other.purpose == Purpose::data_in && other.expected_index == wanted &&
                other.transfer.active())
            {
                other.transfer.drop();
                pump(now, i);
            }
        }
        const std::size_t index = allocate_slot();
        if (index == no_index)
        {
            refuse(from, TftpErrorCode::disk_full, "no transfer available");
            notice(Notice::request_refused, name, 0);
            return;
        }
        const auto options = read_transfer_options(request.options);
        const auto handle =
            services_.sink->open(name, options ? options->transfer_size : std::optional<std::uint64_t>{});
        if (!handle)
        {
            const IoFailure& failure = handle.error();
            refuse(from,
                   failure.error == IoError::no_space ? TftpErrorCode::disk_full : TftpErrorCode::access_violation,
                   to_string(failure.error));
            notice(Notice::request_refused, name, 0);
            return;
        }
        Slot& slot = slots_[index];
        slot.used = true;
        slot.purpose = Purpose::data_in;
        slot.name_length = std::min(name.size(), slot.name.size());
        std::memcpy(slot.name.data(), name.data(), slot.name_length);
        slot.expected_index = wanted;
        expected_[wanted].in_progress = true;
        slot.io.mode = SlotIo::Mode::file_sink;
        slot.io.sink = services_.sink;
        slot.io.sink_handle = *handle;
        slot.transfer.accept_write(now, from, request, slot.io, settings);
        pump(now, index);
        return;
    }

    if (!is_write && !protocol_name)
    {
        // A read of a file of the load.
        if (kind_ != OperationKind::upload || services_.source == nullptr)
        {
            refuse(from, TftpErrorCode::access_violation, "no file may be read now");
            notice(Notice::request_refused, name, 0);
            return;
        }
        const auto options = read_transfer_options(request.options);
        const std::optional<ChecksumOption> checksum = options ? options->checksum : std::optional<ChecksumOption>{};
        const auto file = services_.source->open(name, checksum);
        if (!file)
        {
            const IoFailure& failure = file.error();
            if (failure.error == IoError::not_available)
            {
                // [File_Not_Available]: the target is told to wait and ask again (6.2.8).
                notice(Notice::file_not_available, name, failure.wait_seconds);
                refuse_wait(from, failure.wait_seconds);
            }
            else
            {
                refuse(from,
                       failure.error == IoError::not_found       ? TftpErrorCode::file_not_found
                       : failure.error == IoError::access_denied ? TftpErrorCode::access_violation
                                                                 : TftpErrorCode::not_defined,
                       to_string(failure.error));
                notice(Notice::request_refused, name, 0);
            }
            return;
        }
        const std::size_t index = allocate_slot();
        if (index == no_index)
        {
            services_.source->close(file->handle);
            refuse(from, TftpErrorCode::disk_full, "no transfer available");
            notice(Notice::request_refused, name, 0);
            return;
        }
        Slot& slot = slots_[index];
        slot.used = true;
        slot.purpose = Purpose::data_out;
        slot.name_length = std::min(name.size(), slot.name.size());
        std::memcpy(slot.name.data(), name.data(), slot.name_length);
        slot.io.mode = SlotIo::Mode::file_source;
        slot.io.source = services_.source;
        slot.io.source_handle = file->handle;
        slot.io.source_size = file->size;
        slot.transfer.accept_read(now, from, request, slot.io, settings);
        pump(now, index);
        return;
    }

    refuse(from, TftpErrorCode::access_violation, "not a file this operation uses");
    notice(Notice::request_refused, name, 0);
}

} // namespace adl::arinc615a

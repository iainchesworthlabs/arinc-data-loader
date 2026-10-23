#include "adl/arinc615a/tftp_transfer.hpp"

#include <adl/profiling.hpp>

#include <algorithm>
#include <variant>

namespace adl::arinc615a
{
namespace
{

constexpr std::size_t data_header_bytes = 4;

[[nodiscard]] std::size_t usable_block_size(const TransferSettings& settings, std::size_t buffer_bytes) noexcept
{
    const std::size_t room = buffer_bytes > data_header_bytes ? buffer_bytes - data_header_bytes : 0;
    return std::min(settings.max_block_size, room);
}

[[nodiscard]] TftpErrorCode error_code_for(IoError error) noexcept
{
    switch (error)
    {
    case IoError::not_found:
        return TftpErrorCode::file_not_found;
    case IoError::access_denied:
        return TftpErrorCode::access_violation;
    case IoError::no_space:
        return TftpErrorCode::disk_full;
    case IoError::not_available:
    case IoError::io_error:
        return TftpErrorCode::not_defined;
    }
    return TftpErrorCode::not_defined;
}

} // namespace

void TftpTransfer::reset() noexcept
{
    state_ = State::idle;
    client_ = false;
    receiving_ = false;
    peer_locked_ = false;
    first_exchange_ = true;
    last_block_sent_ = false;
    sink_ = nullptr;
    source_ = nullptr;
    settings_ = TransferSettings{};
    timeout_ = tftp_timeout;
    peer_ = Endpoint{};
    deadline_ = never;
    attempts_ = 0;
    block_size_ = tftp_default_block_size;
    block_ = 0;
    offset_ = 0;
    expected_size_.reset();
    asked_ = ClientOptions{};
    verifier_ = ChecksumVerifier{};
    result_ = TransferResult{};
    out_length_ = 0;
    retransmit_length_ = 0;
    out_pending_ = false;
    peer_text_length_ = 0;
}

void TftpTransfer::begin(const TransferSettings& settings, bool client, bool receiving, const Endpoint& peer) noexcept
{
    reset();
    settings_ = settings;
    settings_.max_block_size = usable_block_size(settings, tx_.size());
    timeout_ = settings.timeout;
    client_ = client;
    receiving_ = receiving;
    peer_ = peer;
    peer_locked_ = !client; // a server's peer is the one that asked; a client's is learned
}

void TftpTransfer::arm(Instant now) noexcept
{
    deadline_ = now + timeout_;
    attempts_ = 1;
}

void TftpTransfer::queue(std::size_t length) noexcept
{
    out_length_ = length;
    retransmit_length_ = length;
    out_pending_ = true;
}

void TftpTransfer::finish(TransferOutcome outcome) noexcept
{
    result_.outcome = outcome;
    result_.bytes = offset_;
    state_ = State::done;
    deadline_ = never;
}

void TftpTransfer::remember_peer_text(std::string_view text) noexcept
{
    peer_text_length_ = std::min(text.size(), peer_text_.size());
    std::copy_n(text.begin(), peer_text_length_, peer_text_.begin());
}

void TftpTransfer::fail_with_error(TransferOutcome outcome, TftpErrorCode code, std::string_view text) noexcept
{
    if (peer_locked_)
    {
        if (const auto size = encode_error(TftpError{code, text}, tx_))
        {
            out_length_ = *size;
            out_pending_ = true;
        }
    }
    finish(outcome);
}

void TftpTransfer::fail_with_io(const IoFailure& failure) noexcept
{
    result_.io_error = failure.error;
    fail_with_error(TransferOutcome::local_error, error_code_for(failure.error), to_string(failure.error));
}

// ---- Starting ------------------------------------------------------------------------------

Result<void> TftpTransfer::start_read(Instant now, const Endpoint& server, std::string_view file_name,
                                      const ClientOptions& options, BlockSink& sink,
                                      const TransferSettings& settings) noexcept
{
    const adl::profiling::Zone zone{"615a: tftp read start"};
    begin(settings, true, true, server);
    sink_ = &sink;
    asked_ = options;
    OptionBuilder builder;
    if (options.block_size != 0)
    {
        if (auto added = builder.add_block_size(
                static_cast<std::uint16_t>(std::min(options.block_size, settings_.max_block_size)));
            !added)
        {
            return std::unexpected(added.error());
        }
        asked_.block_size = std::min(options.block_size, settings_.max_block_size);
    }
    if (options.transfer_size)
    {
        if (auto added = builder.add_transfer_size(*options.transfer_size); !added)
        {
            return std::unexpected(added.error());
        }
    }
    if (options.port)
    {
        if (auto added = builder.add_port(*options.port); !added)
        {
            return std::unexpected(added.error());
        }
    }
    const auto size = encode_request(TftpRequest{TftpRequest::Kind::read, file_name, builder.list()}, tx_);
    if (!size)
    {
        state_ = State::idle;
        return std::unexpected(size.error());
    }
    state_ = State::await_reply;
    queue(*size);
    arm(now);
    return {};
}

Result<void> TftpTransfer::start_write(Instant now, const Endpoint& server, std::string_view file_name,
                                       const ClientOptions& options, BlockSource& source,
                                       const TransferSettings& settings) noexcept
{
    const adl::profiling::Zone zone{"615a: tftp write start"};
    begin(settings, true, false, server);
    source_ = &source;
    asked_ = options;
    OptionBuilder builder;
    if (options.block_size != 0)
    {
        if (auto added = builder.add_block_size(
                static_cast<std::uint16_t>(std::min(options.block_size, settings_.max_block_size)));
            !added)
        {
            return std::unexpected(added.error());
        }
        asked_.block_size = std::min(options.block_size, settings_.max_block_size);
    }
    // A write always says how big the file is: it is cheap, and a target can refuse a file
    // that will not fit before the first block.
    const std::uint64_t file_size = source.size();
    asked_.transfer_size = file_size;
    if (auto added = builder.add_transfer_size(file_size); !added)
    {
        return std::unexpected(added.error());
    }
    if (options.port)
    {
        if (auto added = builder.add_port(*options.port); !added)
        {
            return std::unexpected(added.error());
        }
    }
    const auto size = encode_request(TftpRequest{TftpRequest::Kind::write, file_name, builder.list()}, tx_);
    if (!size)
    {
        state_ = State::idle;
        return std::unexpected(size.error());
    }
    state_ = State::await_reply;
    queue(*size);
    arm(now);
    return {};
}

void TftpTransfer::accept_write(Instant now, const Endpoint& client, const TftpRequest& request, BlockSink& sink,
                                const TransferSettings& settings) noexcept
{
    begin(settings, false, true, client);
    sink_ = &sink;
    const auto options = read_transfer_options(request.options);
    if (!options)
    {
        fail_with_error(TransferOutcome::option_refused, TftpErrorCode::option_refused, "invalid option");
        return;
    }
    OptionBuilder oack;
    bool refused = false;
    if (options->block_size)
    {
        block_size_ = std::min<std::size_t>(*options->block_size, settings_.max_block_size);
        refused = refused || !oack.add_block_size(static_cast<std::uint16_t>(block_size_));
    }
    if (options->transfer_size)
    {
        expected_size_ = options->transfer_size;
        result_.announced_size = expected_size_;
        refused = refused || !oack.add_transfer_size(*options->transfer_size);
    }
    if (options->timeout)
    {
        timeout_ = whole_seconds(*options->timeout);
        refused = refused || !oack.add_timeout(*options->timeout);
    }
    if (options->checksum)
    {
        verifier_.begin(options->checksum);
        if (can_calculate(*options->checksum))
        {
            refused = refused || !oack.add_checksum(*options->checksum);
        }
    }
    if (refused)
    {
        fail_with_error(TransferOutcome::option_refused, TftpErrorCode::option_refused, "options do not fit");
        return;
    }
    std::size_t length = 0;
    if (oack.count() != 0)
    {
        const auto size = encode_option_ack(TftpOptionAck{oack.list()}, tx_);
        if (!size)
        {
            fail_with_error(TransferOutcome::protocol_error, TftpErrorCode::not_defined, "cannot answer");
            return;
        }
        length = *size;
    }
    else
    {
        length = encode_ack(TftpAck{0}, tx_).value_or(0);
    }
    state_ = State::receiving;
    block_ = 0;
    queue(length);
    arm(now);
}

void TftpTransfer::accept_read(Instant now, const Endpoint& client, const TftpRequest& request, BlockSource& source,
                               const TransferSettings& settings) noexcept
{
    begin(settings, false, false, client);
    source_ = &source;
    const auto options = read_transfer_options(request.options);
    if (!options)
    {
        fail_with_error(TransferOutcome::option_refused, TftpErrorCode::option_refused, "invalid option");
        return;
    }
    OptionBuilder oack;
    bool refused = false;
    if (options->block_size)
    {
        block_size_ = std::min<std::size_t>(*options->block_size, settings_.max_block_size);
        refused = refused || !oack.add_block_size(static_cast<std::uint16_t>(block_size_));
    }
    if (options->transfer_size)
    {
        refused = refused || !oack.add_transfer_size(source.size());
    }
    if (options->timeout)
    {
        timeout_ = whole_seconds(*options->timeout);
        refused = refused || !oack.add_timeout(*options->timeout);
    }
    // The checksum of a read request is how the target picks one of two files of a name; the
    // source was handed it when it opened the file. It is acknowledged when it is the 16-bit
    // CRC, which is the one the standard has the target send for that.
    if (options->checksum && options->checksum->type == adl::arinc665::CheckValueType::crc16)
    {
        refused = refused || !oack.add_checksum(*options->checksum);
    }
    if (refused)
    {
        fail_with_error(TransferOutcome::option_refused, TftpErrorCode::option_refused, "options do not fit");
        return;
    }
    block_ = 0;
    offset_ = 0;
    last_block_sent_ = false;
    if (oack.count() != 0)
    {
        const auto size = encode_option_ack(TftpOptionAck{oack.list()}, tx_);
        if (!size)
        {
            fail_with_error(TransferOutcome::protocol_error, TftpErrorCode::not_defined, "cannot answer");
            return;
        }
        state_ = State::sending; // waiting for ACK 0
        queue(*size);
        arm(now);
        return;
    }
    send_next(now);
}

// ---- Sending -------------------------------------------------------------------------------

void TftpTransfer::start_sending(Instant now) noexcept
{
    block_ = 0;
    offset_ = 0;
    last_block_sent_ = false;
    send_next(now);
}

void TftpTransfer::send_next(Instant now) noexcept
{
    const std::uint16_t block = next_block(block_);
    const auto room = tx_.subspan(data_header_bytes, block_size_);
    const auto read = source_->read(offset_, room);
    if (!read)
    {
        fail_with_io(read.error());
        return;
    }
    // The header is written by hand because the block is read straight into the datagram's
    // place; encode_data would copy it onto itself. The tests compare the result with
    // encode_data's.
    tx_[0] = std::byte{0};
    tx_[1] = static_cast<std::byte>(Opcode::data);
    tx_[2] = static_cast<std::byte>(block >> 8);
    tx_[3] = static_cast<std::byte>(block & 0xFF);
    block_ = block;
    offset_ += *read;
    last_block_sent_ = *read < block_size_;
    state_ = State::sending;
    queue(data_header_bytes + *read);
    arm(now);
}

void TftpTransfer::on_ack(Instant now, const TftpAck& ack) noexcept
{
    if (state_ != State::sending)
    {
        return;
    }
    // Anything but the ACK of the block in flight is a repeat or a stray. A repeat is never a
    // reason to send the block again: that is the Sorcerer's Apprentice (Attachment 4, 4-3.3),
    // where each duplicate ACK buys a duplicate block and so another duplicate ACK, for good.
    if (ack.block != block_)
    {
        return;
    }
    first_exchange_ = false;
    if (last_block_sent_)
    {
        finish(TransferOutcome::succeeded);
        return;
    }
    send_next(now);
}

// ---- Receiving -----------------------------------------------------------------------------

void TftpTransfer::queue_ack(std::uint16_t block) noexcept
{
    if (const auto size = encode_ack(TftpAck{block}, tx_))
    {
        queue(*size);
    }
}

void TftpTransfer::on_data(Instant now, const TftpData& data) noexcept
{
    if (state_ != State::receiving && state_ != State::lingering)
    {
        return;
    }
    if (data.block == block_ && block_ != 0)
    {
        // The block we already have: our ACK did not arrive. Answer it again, without
        // moving the time-out, so that a flood of repeats cannot keep a transfer alive.
        queue_ack(block_);
        return;
    }
    if (state_ == State::lingering || data.block != next_block(block_))
    {
        return; // a block from the future or the past: the sender will send what we need
    }
    if (data.data.size() > block_size_)
    {
        fail_with_error(TransferOutcome::protocol_error, TftpErrorCode::illegal_operation,
                        "block larger than the block size");
        return;
    }
    if (const auto written = sink_->write(data.data); !written)
    {
        fail_with_io(written.error());
        return;
    }
    verifier_.update(data.data);
    offset_ += data.data.size();
    block_ = data.block;
    first_exchange_ = false;
    if (data.data.size() < block_size_)
    {
        if (expected_size_ && *expected_size_ != offset_)
        {
            // Section 5.3.2.3.8.2: a transfer size that does not match fails the transfer.
            fail_with_error(TransferOutcome::size_mismatch, TftpErrorCode::not_defined, "transfer size mismatch");
            return;
        }
        result_.outcome = TransferOutcome::succeeded;
        result_.bytes = offset_;
        result_.checksum = verifier_.finish();
        queue_ack(block_);
        state_ = State::lingering;
        deadline_ = now + settings_.dally;
        return;
    }
    queue_ack(block_);
    arm(now);
}

// ---- Errors and the client's first reply -----------------------------------------------------

void TftpTransfer::on_error_packet(const TftpError& error) noexcept
{
    remember_peer_text(error.message);
    const auto meaning = interpret_error(error);
    if (meaning)
    {
        if (const auto* wait = std::get_if<WaitMessage>(&*meaning))
        {
            if (state_ == State::await_reply)
            {
                result_.wait_seconds = wait->seconds;
                finish(TransferOutcome::wait_requested);
                return;
            }
        }
        else if (const auto* abort_message = std::get_if<AbortMessage>(&*meaning))
        {
            result_.abort_status = abort_message->status;
            finish(TransferOutcome::peer_abort);
            return;
        }
    }
    result_.error_code = error.code;
    finish(TransferOutcome::peer_error);
}

bool TftpTransfer::read_options_from_oack(const TftpOptionAck& oack) noexcept
{
    const auto options = read_transfer_options(oack.options);
    if (!options)
    {
        fail_with_error(TransferOutcome::option_refused, TftpErrorCode::option_refused, "invalid option");
        return false;
    }
    // A server may take an option out of a request, or take it as it is; it may lower the
    // block size, never raise it, and it may not add what was not asked for.
    if (options->block_size)
    {
        if (asked_.block_size == 0 || *options->block_size > asked_.block_size)
        {
            fail_with_error(TransferOutcome::option_refused, TftpErrorCode::option_refused, "block size not offered");
            return false;
        }
        block_size_ = *options->block_size;
    }
    if (options->timeout)
    {
        fail_with_error(TransferOutcome::option_refused, TftpErrorCode::option_refused, "timeout not offered");
        return false;
    }
    if (options->transfer_size && receiving_)
    {
        expected_size_ = options->transfer_size;
        result_.announced_size = expected_size_;
    }
    result_.port_acknowledged = options->port.has_value() && asked_.port.has_value() && *options->port == *asked_.port;
    return true;
}

void TftpTransfer::on_client_reply(Instant now, const Endpoint& from, const TftpPacket& packet) noexcept
{
    if (const auto* error = std::get_if<TftpError>(&packet))
    {
        peer_ = from;
        peer_locked_ = true;
        on_error_packet(*error);
        return;
    }
    if (const auto* oack = std::get_if<TftpOptionAck>(&packet))
    {
        peer_ = from;
        peer_locked_ = true;
        if (!read_options_from_oack(*oack))
        {
            return;
        }
        first_exchange_ = false;
        if (receiving_)
        {
            block_ = 0;
            state_ = State::receiving;
            queue_ack(0);
            arm(now);
        }
        else
        {
            start_sending(now);
        }
        return;
    }
    if (receiving_)
    {
        if (const auto* data = std::get_if<TftpData>(&packet))
        {
            if (data->block != 1)
            {
                return;
            }
            peer_ = from;
            peer_locked_ = true;
            block_size_ = tftp_default_block_size; // no OACK: every option was left out
            state_ = State::receiving;
            block_ = 0;
            on_data(now, *data);
        }
        return;
    }
    if (const auto* ack = std::get_if<TftpAck>(&packet))
    {
        if (ack->block != 0)
        {
            return;
        }
        peer_ = from;
        peer_locked_ = true;
        block_size_ = tftp_default_block_size;
        first_exchange_ = false;
        start_sending(now);
    }
}

// ---- Driving -------------------------------------------------------------------------------

void TftpTransfer::on_datagram(Instant now, const Endpoint& from, std::span<const std::byte> bytes) noexcept
{
    if (!active() || from.address != peer_.address)
    {
        return;
    }
    if (peer_locked_ && from.port != peer_.port)
    {
        return; // another transfer's port (an unknown transfer ID): not ours to answer
    }
    // A datagram that does not decode is treated as lost: the sender's time-out repeats it.
    const auto packet = decode_packet(bytes);
    if (!packet)
    {
        return;
    }
    if (state_ == State::await_reply)
    {
        on_client_reply(now, from, *packet);
        return;
    }
    if (const auto* error = std::get_if<TftpError>(&*packet))
    {
        if (state_ != State::lingering)
        {
            on_error_packet(*error);
        }
        return;
    }
    if (const auto* data = std::get_if<TftpData>(&*packet))
    {
        on_data(now, *data);
    }
    else if (const auto* ack = std::get_if<TftpAck>(&*packet))
    {
        on_ack(now, *ack);
    }
}

void TftpTransfer::on_timer(Instant now) noexcept
{
    if (!active() || now < deadline_)
    {
        return;
    }
    if (state_ == State::lingering)
    {
        state_ = State::done;
        deadline_ = never;
        return;
    }
    if (attempts_ < 1 + settings_.retries)
    {
        ++attempts_;
        deadline_ = now + timeout_;
        out_length_ = retransmit_length_;
        out_pending_ = true;
        return;
    }
    finish(TransferOutcome::timed_out);
}

bool TftpTransfer::abort(StatusCode status) noexcept
{
    if (!active())
    {
        return false;
    }
    if (state_ == State::lingering)
    {
        state_ = State::done;
        deadline_ = never;
        return false;
    }
    if (peer_locked_)
    {
        if (const auto size = encode_abort(AbortMessage{status}, tx_))
        {
            out_length_ = *size;
            out_pending_ = true;
        }
    }
    finish(TransferOutcome::aborted);
    return out_pending_;
}

void TftpTransfer::drop() noexcept
{
    if (!active())
    {
        return;
    }
    if (state_ == State::lingering)
    {
        state_ = State::done;
        deadline_ = never;
        return;
    }
    finish(TransferOutcome::aborted);
}

void TftpTransfer::on_repeated_request(Instant /*now*/) noexcept
{
    if (!active() || client_ || !first_exchange_ || state_ == State::lingering)
    {
        return;
    }
    out_length_ = retransmit_length_;
    out_pending_ = true;
}

std::optional<Outgoing> TftpTransfer::take_outgoing() noexcept
{
    if (!out_pending_)
    {
        return std::nullopt;
    }
    out_pending_ = false;
    return Outgoing{peer_, std::span<const std::byte>{tx_}.first(out_length_)};
}

Instant TftpTransfer::next_wake() const noexcept
{
    return active() ? deadline_ : never;
}

} // namespace adl::arinc615a

#pragma once

// One TFTP file transfer, as a state machine that is driven and does nothing of its own: it
// is told the time and the datagrams that arrive, and answers with the one datagram to send
// next and the time it wants to be called again. No socket, thread, clock or file system is in
// it. Both ends of a transfer and both directions are the one class:
//
//   start_read()    we send a read request and receive the file    (client, receiving)
//   start_write()   we send a write request and send the file      (client, sending)
//   accept_write()  a peer asked to write a file to us             (server, receiving)
//   accept_read()   a peer asked to read a file from us            (server, sending)
//
// What it holds to: lock-step blocks numbered from 1 and wrapping 65535 to 1 (section
// 5.3.2.3.9); a time-out of two seconds with one retry per exchange (Attachment 4, 4-3); the
// fix for the Sorcerer's Apprentice (a sender never resends a block because it saw a duplicate
// ACK, 4-3.3); the options of 5.3.2.3.8 (blksize, tsize, timeout, port, checksum_<n>) and
// ignoring the ones it does not know; WAIT and ABORT; and a final dally so that a repeat of the
// last block is answered. A received file whose checksum does not match is *not* a failed
// transfer (5.3.2.3.8.5): the verdict is reported and the file is kept.
//
// The bytes of a transfer live in a buffer the caller gives it (`tx`): one datagram, large
// enough for the block size plus the header and for a request. It is also what a datagram
// returned by take_outgoing() points into, so it stays valid until the transfer is next called.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "adl/arinc615a/checksum.hpp"
#include "adl/arinc615a/endpoint.hpp"
#include "adl/arinc615a/file_io.hpp"
#include "adl/arinc615a/limits.hpp"
#include "adl/arinc615a/status_code.hpp"
#include "adl/arinc615a/tftp.hpp"
#include "adl/arinc615a/time.hpp"

namespace adl::arinc615a {

// Where the blocks of a file being received go.
class BlockSink {
public:
    virtual ~BlockSink() = default;
    [[nodiscard]] virtual IoResult<void> write(std::span<const std::byte> data) noexcept = 0;
};

// Where the blocks of a file being sent come from.
class BlockSource {
public:
    virtual ~BlockSource() = default;
    [[nodiscard]] virtual std::uint64_t size() const noexcept = 0;
    [[nodiscard]] virtual IoResult<std::size_t> read(std::uint64_t offset,
                                                     std::span<std::byte> out) noexcept = 0;
};

enum class TransferOutcome : std::uint8_t {
    running,         // not over (also: a transfer that was never started)
    succeeded,       // the whole file went across and the last block was acknowledged
    timed_out,       // no answer after the retry
    peer_error,      // the peer sent an ordinary error packet
    peer_abort,      // the peer sent ABORT:XXXX
    wait_requested,  // the peer answered the request with WAIT:x; ask again after x seconds
    option_refused,  // the options could not be agreed
    protocol_error,  // a block too long, or another breach of the rules
    size_mismatch,   // the file's size was not the tsize the peer announced
    local_error,     // the file could not be read or written here
    aborted,         // abort() was called
};

[[nodiscard]] constexpr const char* to_string(TransferOutcome outcome) noexcept {
    switch (outcome) {
        case TransferOutcome::running:
            return "running";
        case TransferOutcome::succeeded:
            return "succeeded";
        case TransferOutcome::timed_out:
            return "timed out";
        case TransferOutcome::peer_error:
            return "the peer sent an error";
        case TransferOutcome::peer_abort:
            return "the peer aborted";
        case TransferOutcome::wait_requested:
            return "the peer asked to wait";
        case TransferOutcome::option_refused:
            return "options refused";
        case TransferOutcome::protocol_error:
            return "protocol error";
        case TransferOutcome::size_mismatch:
            return "transfer size mismatch";
        case TransferOutcome::local_error:
            return "local file error";
        case TransferOutcome::aborted:
            return "aborted";
    }
    return "unknown";
}

// How a transfer ended and what is known of it: the outcome and the detail that goes
// with it, the bytes moved and the verdict on the checksum. See TftpTransfer::result().
struct TransferResult {
    TransferOutcome outcome = TransferOutcome::running;
    std::uint16_t wait_seconds = 0;                                // wait_requested
    StatusCode abort_status = StatusCode::aborted_by_data_loader;  // peer_abort
    TftpErrorCode error_code = TftpErrorCode::not_defined;         // peer_error
    IoError io_error = IoError::io_error;                          // local_error
    std::uint64_t bytes = 0;  // bytes received, or sent (and acknowledged, once succeeded)
    ChecksumVerdict checksum = ChecksumVerdict::not_given;
    // The peer took the "port" option we sent. A peer that leaves it out of its OACK has
    // refused it (5.3.2.3.8.6), and will go on using port 59 for its own requests.
    bool port_acknowledged = false;
    std::optional<std::uint64_t> announced_size;  // the tsize the peer gave, if it did
};

// The limits a transfer works within: the largest block size it agrees to, the
// time-out and retries of each exchange, and how long it lingers after the last block.
struct TransferSettings {
    // The largest block size this end will agree to or ask for; the buffer must hold it and 4.
    std::size_t max_block_size = 1024;
    Duration timeout = tftp_timeout;
    unsigned retries = tftp_retries;
    Duration dally = tftp_dally;
};

// The options our own request carries.
struct ClientOptions {
    std::size_t block_size = 0;                  // 0: do not ask
    std::optional<std::uint64_t> transfer_size;  // a write: the size; a read: 0 asks for it
    std::optional<std::uint16_t> port;           // the "port" option (initialization requests)
};

// A datagram waiting to be sent: where to, and the bytes, which are in the
// transfer's own buffer. See TftpTransfer::take_outgoing().
struct Outgoing {
    Endpoint to;
    std::span<const std::byte> bytes;
};

class TftpTransfer {
public:
    // The smallest buffer that can hold a request with a long file name and its options.
    static constexpr std::size_t min_buffer_bytes = 1024;

    TftpTransfer() noexcept = default;
    explicit TftpTransfer(std::span<std::byte> tx) noexcept : tx_(tx) {}

    // Gives an idle transfer its datagram buffer (for one made without one).
    void set_buffer(std::span<std::byte> tx) noexcept { tx_ = tx; }

    // ---- starting -------------------------------------------------------------------------

    // Sends a request to `server`'s control port. `sink`/`source` must outlive the transfer.
    // Error::buffer_too_small if the request does not fit the buffer.
    [[nodiscard]] Result<void> start_read(Instant now, const Endpoint& server,
                                          std::string_view file_name, const ClientOptions& options,
                                          BlockSink& sink,
                                          const TransferSettings& settings) noexcept;
    [[nodiscard]] Result<void> start_write(Instant now, const Endpoint& server,
                                           std::string_view file_name, const ClientOptions& options,
                                           BlockSource& source,
                                           const TransferSettings& settings) noexcept;

    // The peer's request has arrived (and been decoded; the views need live only during the
    // call). The transfer answers it. If the options cannot be agreed it ends at once with
    // option_refused, with the error packet waiting to be taken.
    void accept_write(Instant now, const Endpoint& client, const TftpRequest& request,
                      BlockSink& sink, const TransferSettings& settings) noexcept;
    void accept_read(Instant now, const Endpoint& client, const TftpRequest& request,
                     BlockSource& source, const TransferSettings& settings) noexcept;

    // ---- driving --------------------------------------------------------------------------

    void on_datagram(Instant now, const Endpoint& from, std::span<const std::byte> bytes) noexcept;
    void on_timer(Instant now) noexcept;

    // Ends the transfer with an ABORT:XXXX error packet to the peer (section 5.3.2.3.6), if the
    // peer is known. A transfer that is already over is left alone. True if a packet is waiting
    // to be sent, that is, if the peer is to learn of the abort from this transfer.
    bool abort(StatusCode status) noexcept;

    // Ends the transfer without a word to the peer (it will find out by its own time-out): for
    // a transfer that a newer one from the same peer replaces.
    void drop() noexcept;

    // The peer repeated its request (our first answer was lost or late): the first answer is
    // sent again. Only meaningful, and only done, before the exchange has moved on.
    void on_repeated_request(Instant now) noexcept;

    // The datagram to send now, if there is one; asking for it takes it.
    [[nodiscard]] std::optional<Outgoing> take_outgoing() noexcept;

    // When on_timer() should next be called, or `never`.
    [[nodiscard]] Instant next_wake() const noexcept;

    // ---- state ----------------------------------------------------------------------------

    // Not over: running, or lingering to answer a repeat of the last block.
    [[nodiscard]] bool active() const noexcept {
        return state_ != State::idle && state_ != State::done;
    }
    // The outcome is known (succeeded or not). A receiver that succeeded may still linger.
    [[nodiscard]] bool finished() const noexcept {
        return result_.outcome != TransferOutcome::running;
    }
    [[nodiscard]] bool lingering() const noexcept { return state_ == State::lingering; }
    [[nodiscard]] const TransferResult& result() const noexcept { return result_; }
    [[nodiscard]] const Endpoint& peer() const noexcept { return peer_; }
    [[nodiscard]] bool is_receiving() const noexcept { return receiving_; }
    [[nodiscard]] bool is_client() const noexcept { return client_; }
    // The peer's error text, when it sent one (up to 80 characters).
    [[nodiscard]] std::string_view peer_text() const noexcept {
        return {peer_text_.data(), peer_text_length_};
    }

    // Makes the object ready to start again.
    void reset() noexcept;

private:
    enum class State : std::uint8_t {
        idle,
        await_reply,  // a client's request is out: waiting for the first answer
        receiving,    // waiting for the next block of a file coming in
        sending,      // waiting for the ACK of the block (or OACK) just sent
        lingering,    // the file is in; the last ACK may have to be repeated
        done,
    };

    void begin(const TransferSettings& settings, bool client, bool receiving,
               const Endpoint& peer) noexcept;
    void arm(Instant now) noexcept;
    void queue(std::size_t length) noexcept;
    void finish(TransferOutcome outcome) noexcept;
    void fail_with_error(TransferOutcome outcome, TftpErrorCode code,
                         std::string_view text) noexcept;
    void fail_with_io(const IoFailure& failure) noexcept;
    void remember_peer_text(std::string_view text) noexcept;

    void on_client_reply(Instant now, const Endpoint& from, const TftpPacket& packet) noexcept;
    void on_error_packet(const TftpError& error) noexcept;
    void on_data(Instant now, const TftpData& data) noexcept;
    void on_ack(Instant now, const TftpAck& ack) noexcept;
    void queue_ack(std::uint16_t block) noexcept;
    void start_sending(Instant now) noexcept;
    void send_next(Instant now) noexcept;
    bool read_options_from_oack(const TftpOptionAck& oack) noexcept;

    std::span<std::byte> tx_;
    BlockSink* sink_ = nullptr;
    BlockSource* source_ = nullptr;

    State state_ = State::idle;
    bool client_ = false;
    bool receiving_ = false;
    bool peer_locked_ = false;
    bool first_exchange_ = true;
    bool last_block_sent_ = false;

    TransferSettings settings_;
    Duration timeout_ = tftp_timeout;
    Endpoint peer_;
    Instant deadline_ = never;
    unsigned attempts_ = 0;

    std::size_t block_size_ = tftp_default_block_size;
    std::uint16_t block_ = 0;   // receiving: the last block acknowledged; sending: the last sent
    std::uint64_t offset_ = 0;  // bytes received so far, or the offset of the next block to send
    std::optional<std::uint64_t> expected_size_;

    // What our own request asked for, to check the OACK against.
    ClientOptions asked_;

    ChecksumVerifier verifier_;
    TransferResult result_;

    std::size_t out_length_ = 0;
    std::size_t retransmit_length_ = 0;  // the packet in tx that a time-out sends again
    bool out_pending_ = false;

    std::array<char, 80> peer_text_{};
    std::size_t peer_text_length_ = 0;
};

}  // namespace adl::arinc615a

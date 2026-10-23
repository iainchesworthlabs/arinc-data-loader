// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The TFTP packets ARINC 615A-3 uses (section 5.3.2): RFC 1350 with the option
// extension of RFCs 2347 to 2349, octet mode only, and the two messages 615A hides in
// error packets, WAIT and ABORT. Nothing here sends or receives: a packet is a view over
// bytes the caller holds, or is written into a buffer the caller supplies.
//
// What a decoded packet views (a file name, an option, the data of a block) lives in the
// bytes that were decoded, so those bytes must outlive it.

#include "adl/arinc615a/error.hpp"
#include "adl/arinc615a/list.hpp"
#include "adl/arinc615a/status_code.hpp"
#include "adl/arinc665/check_value_type.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <variant>

namespace adl::arinc615a
{

enum class Opcode : std::uint16_t
{
    read_request = 1,
    write_request = 2,
    data = 3,
    ack = 4,
    error = 5,
    option_ack = 6,
};

// RFC 1350's error codes (section 5.3.2.3.3 of 615A-3 repeats them). Code 0 carries the
// WAIT and ABORT messages. A code outside the table decodes unchanged.
enum class TftpErrorCode : std::uint16_t
{
    not_defined = 0,
    file_not_found = 1,
    access_violation = 2,
    disk_full = 3,
    illegal_operation = 4,
    unknown_transfer_id = 5,
    file_already_exists = 6,
    no_such_user = 7,
    option_refused = 8,
};

// One option of a request or of an option acknowledgement: a name and a value, each a
// text on the wire. Names compare without regard to case (RFC 2347).
struct TftpOption
{
    std::string_view name;
    std::string_view value;

    friend bool operator==(const TftpOption&, const TftpOption&) = default;
};

using OptionList = List<TftpOption>;

// A read request (RRQ) or a write request (WRQ). The transfer mode is always "octet",
// the only one 615A uses (section 5.3.2.2), so it is not a field: it is written, and
// checked on decoding, case-insensitively as RFC 1350 says.
struct TftpRequest
{
    enum class Kind
    {
        read,
        write
    };

    Kind kind = Kind::read;
    std::string_view file_name;
    OptionList options;

    friend bool operator==(const TftpRequest&, const TftpRequest&) = default;
};

// A block of a file. The first block is 1; a WRQ, or an OACK to a WRQ, is acknowledged
// with block 0. Block numbers go 65535, 1, 2 (section 5.3.2.3.9): see next_block().
struct TftpData
{
    std::uint16_t block = 0;
    std::span<const std::byte> data; // 0 to the block size; a short block ends the file
};

struct TftpAck
{
    std::uint16_t block = 0;

    friend bool operator==(const TftpAck&, const TftpAck&) = default;
};

struct TftpError
{
    TftpErrorCode code = TftpErrorCode::not_defined;
    std::string_view message; // ASCII, without the terminator

    friend bool operator==(const TftpError&, const TftpError&) = default;
};

// The server's answer to the options of a request it accepts, naming the ones it takes.
// An acknowledgement of no options is not a packet (RFC 2347).
struct TftpOptionAck
{
    OptionList options;

    friend bool operator==(const TftpOptionAck&, const TftpOptionAck&) = default;
};

using TftpPacket = std::variant<TftpRequest, TftpData, TftpAck, TftpError, TftpOptionAck>;

// The block after `block` in a transfer: one more, but 65535 is followed by 1 and never 0
// (section 5.3.2.3.9, where RFC 1350 says nothing).
[[nodiscard]] constexpr std::uint16_t next_block(std::uint16_t block) noexcept
{
    return block == 0xFFFFU ? std::uint16_t{1} : static_cast<std::uint16_t>(block + 1);
}

// The largest packet for a block size: opcode, block number, and the data.
[[nodiscard]] constexpr std::size_t max_packet_size(std::size_t block_size) noexcept
{
    return block_size + 4;
}

// What the first two bytes of a datagram say it is.
[[nodiscard]] Result<Opcode> peek_opcode(std::span<const std::byte> bytes) noexcept;

// Decodes a datagram, checking that its fields are whole and end where the datagram
// does. Options are checked to be well-formed pairs; what they mean is read by
// read_transfer_options().
[[nodiscard]] Result<TftpPacket> decode_packet(std::span<const std::byte> bytes) noexcept;

// Each encoder writes the packet at the start of `out` and returns how many bytes it
// took; Error::buffer_too_small if they do not fit (what was written is then unspecified).
[[nodiscard]] Result<std::size_t> encode_request(const TftpRequest& packet, std::span<std::byte> out) noexcept;
[[nodiscard]] Result<std::size_t> encode_data(const TftpData& packet, std::span<std::byte> out) noexcept;
[[nodiscard]] Result<std::size_t> encode_ack(const TftpAck& packet, std::span<std::byte> out) noexcept;
[[nodiscard]] Result<std::size_t> encode_error(const TftpError& packet, std::span<std::byte> out) noexcept;
[[nodiscard]] Result<std::size_t> encode_option_ack(const TftpOptionAck& packet, std::span<std::byte> out) noexcept;
[[nodiscard]] Result<std::size_t> encode_packet(const TftpPacket& packet, std::span<std::byte> out) noexcept;

// How many bytes the encoding takes, or why it cannot be made.
[[nodiscard]] Result<std::size_t> encoded_size(const TftpPacket& packet) noexcept;

// ---- Options ---------------------------------------------------------------------------

// The check value algorithm and value of a "checksum_<n>" option (section 5.3.2.3.8.5):
// <n> is the number of the algorithm in ARINC 665-3's Table 5.1 (adl::arinc665::
// CheckValueType), and the value is the check value as that table stores it (a 16-bit CRC
// is two bytes), written as hexadecimal characters.
struct ChecksumOption
{
    adl::arinc665::CheckValueType type = adl::arinc665::CheckValueType::crc16;
    std::array<std::byte, 20> value{};
    std::uint8_t length = 0; // bytes of `value` in use

    [[nodiscard]] std::span<const std::byte> bytes() const noexcept
    {
        return std::span<const std::byte>{value}.first(length);
    }
    friend bool operator==(const ChecksumOption&, const ChecksumOption&) = default;
};

// The options this library knows, read from the list of a request or an OACK. Options it
// does not know are ignored, as the standard says (section 5.3.2.3.8).
struct TransferOptions
{
    std::optional<std::uint16_t> block_size;    // "blksize", 8 to 65464
    std::optional<std::uint64_t> transfer_size; // "tsize"; a request for a read says 0
    std::optional<std::uint8_t> timeout;        // "timeout" in seconds, 1 to 255
    std::optional<std::uint16_t> port;          // "port", 1 to 65535 (615A-3 5.3.2.3.8.6)
    std::optional<ChecksumOption> checksum;     // "checksum_<n>"

    friend bool operator==(const TransferOptions&, const TransferOptions&) = default;
};

// Error::bad_option for a known option that is repeated, malformed or out of range, and
// for more than one checksum option.
[[nodiscard]] Result<TransferOptions> read_transfer_options(const OptionList& options) noexcept;

// The value of the option named `name` (compared without regard to case), if there is one.
[[nodiscard]] std::optional<std::string_view> find_option(const OptionList& options, std::string_view name) noexcept;

// Builds the options of a request or acknowledgement in a buffer of its own, with no
// allocation: the numbers are formatted into it. list() views the buffer, so it is valid
// until the builder is changed, moved or destroyed.
class OptionBuilder
{
public:
    // Any option. Error::bad_option if the name is empty or either text holds a NUL, and
    // Error::buffer_too_small if the options no longer fit the builder.
    [[nodiscard]] Result<void> add(std::string_view name, std::string_view value) noexcept;

    [[nodiscard]] Result<void> add_block_size(std::uint16_t size) noexcept;
    [[nodiscard]] Result<void> add_transfer_size(std::uint64_t size) noexcept;
    [[nodiscard]] Result<void> add_timeout(std::uint8_t seconds) noexcept;
    [[nodiscard]] Result<void> add_port(std::uint16_t port) noexcept;
    [[nodiscard]] Result<void> add_checksum(const ChecksumOption& checksum) noexcept;

    [[nodiscard]] OptionList list() const noexcept;
    [[nodiscard]] std::size_t count() const noexcept
    {
        return count_;
    }

    static constexpr std::size_t capacity = 256;

private:
    std::array<char, capacity> text_{};
    std::size_t size_ = 0;
    std::size_t count_ = 0;
};

// ---- The messages hidden in error packets ------------------------------------------------

// "WAIT:x" with error code 0 (section 5.3.2.3.4): the unit that sent it cannot take the
// request now; the receiver stops this transfer and asks again after x seconds.
struct WaitMessage
{
    std::uint16_t seconds = 0;

    friend bool operator==(const WaitMessage&, const WaitMessage&) = default;
};

// "ABORT:XXXX" with error code 0 (section 5.3.2.3.6): the data loader ends the operation.
// XXXX is the status code of Table 6.4.10-1 that says why, as four hexadecimal characters.
struct AbortMessage
{
    StatusCode status = StatusCode::aborted_by_data_loader;

    friend bool operator==(const AbortMessage&, const AbortMessage&) = default;
};

[[nodiscard]] Result<std::size_t> encode_wait(const WaitMessage& message, std::span<std::byte> out) noexcept;
[[nodiscard]] Result<std::size_t> encode_abort(const AbortMessage& message, std::span<std::byte> out) noexcept;

// What an error packet means to 615A.
using ErrorMeaning = std::variant<WaitMessage, AbortMessage, TftpError>;

// A packet with error code 0 whose text starts "WAIT:" or "ABORT:" is the message, and is
// Error::invalid_value when the rest is not a number of seconds (0 to 65535, decimal) or
// four hexadecimal characters. Anything else, including those texts under another error
// code, is an ordinary error, returned as it came.
[[nodiscard]] Result<ErrorMeaning> interpret_error(const TftpError& error) noexcept;

} // namespace adl::arinc615a

// SPDX-License-Identifier: GPL-3.0-or-later
// The TFTP packets of ARINC 615A-3 section 5.3.2: RFC 1350 with options, octet mode only,
// block numbers that wrap to 1, and a WRQ acknowledged with block 0.

#include "adl/arinc615a/limits.hpp"
#include "adl/arinc615a/tftp.hpp"
#include "bytes.hpp"
#include "codec_615a.hpp"
#include "sample_615a.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string>
#include <vector>

using namespace adl::arinc615a;
using adl::test::bytes_of;
using adl::test::from_hex;
using adl::test::to_bytes;

namespace
{

template <typename T>
const T& as(const TftpPacket& packet)
{
    REQUIRE(std::holds_alternative<T>(packet));
    return std::get<T>(packet);
}

std::vector<TftpOption> collect(const OptionList& options)
{
    return {options.begin(), options.end()};
}

} // namespace

TEST_CASE("a read request is opcode 1, the file name, octet and the options", "[tftp]")
{
    // RFC 1350's layout: 01, name, 0, mode, 0, then name and value pairs (RFC 2347).
    const auto bytes = to_bytes(adl::test::sample::rrq_lui());

    CHECK(bytes == from_hex("0001 484e50464d535f4c2e4c5549 00 6f6374657400 626c6b73697a6500 3130323400 "
                            "7473697a6500 3000 706f727400 3136313500"));
    CHECK(bytes.size() == 2 + 13 + 6 + 8 + 5 + 6 + 2 + 5 + 5);

    const auto decoded = decode_packet(bytes);
    REQUIRE(decoded.has_value());
    const auto& request = as<TftpRequest>(*decoded);
    CHECK(request.kind == TftpRequest::Kind::read);
    CHECK(request.file_name == "HNPFMS_L.LUI");
    const auto options = collect(request.options);
    REQUIRE(options.size() == 3);
    CHECK(options[0] == TftpOption{"blksize", "1024"});
    CHECK(options[1] == TftpOption{"tsize", "0"});
    CHECK(options[2] == TftpOption{"port", "1615"});
}

TEST_CASE("a write request is opcode 2, and needs no options", "[tftp]")
{
    const auto bytes = to_bytes(adl::test::sample::wrq_plain());

    CHECK(bytes == from_hex("0002 484e50464d535f4c2e4c4353 00 6f6374657400"));
    const auto decoded = decode_packet(bytes);
    REQUIRE(decoded.has_value());
    CHECK(as<TftpRequest>(*decoded).kind == TftpRequest::Kind::write);
    CHECK(as<TftpRequest>(*decoded).options.empty());
}

TEST_CASE("the transfer mode is octet in any case, and nothing else", "[tftp]")
{
    for (const char* mode : {"octet", "OCTET", "Octet", "oCtEt"})
    {
        CAPTURE(mode);
        auto bytes = bytes_of(std::string("\0\1a\0", 4) + mode + std::string(1, '\0'));
        CHECK(decode_packet(bytes).has_value());
    }
    for (const char* mode : {"netascii", "mail", "", "octets", "octe"})
    {
        CAPTURE(mode);
        auto bytes = bytes_of(std::string("\0\1a\0", 4) + mode + std::string(1, '\0'));
        const auto decoded = decode_packet(bytes);
        REQUIRE_FALSE(decoded.has_value());
        CHECK(decoded.error() == Error::unsupported_mode);
    }
}

TEST_CASE("a request with no file name is refused both ways", "[tftp]")
{
    CHECK(decode_packet(from_hex("0001 00 6f6374657400")).error() == Error::invalid_value);

    std::array<std::byte, 64> buffer{};
    CHECK(encode_request({TftpRequest::Kind::read, "", {}}, buffer).error() == Error::invalid_value);
}

TEST_CASE("a request that ends before its mode, or inside an option, is truncated", "[tftp]")
{
    CHECK(decode_packet(from_hex("0001 6162")).error() == Error::truncated);
    CHECK(decode_packet(from_hex("0001 6162 00 6f63")).error() == Error::truncated);
    // A name with no value.
    CHECK(decode_packet(from_hex("0001 6162 00 6f6374657400 626c6b73697a6500")).error() == Error::truncated);
    // An option with no name.
    CHECK(decode_packet(from_hex("0001 6162 00 6f6374657400 00 3100")).error() == Error::bad_option);
}

TEST_CASE("an ACK is opcode 4 and a block number, and a WRQ is acknowledged with block 0", "[tftp]")
{
    CHECK(to_bytes(TftpAck{0}) == from_hex("0004 0000"));
    CHECK(to_bytes(TftpAck{65535}) == from_hex("0004 ffff"));

    const auto decoded = decode_packet(from_hex("0004 0000"));
    REQUIRE(decoded.has_value());
    CHECK(as<TftpAck>(*decoded).block == 0);
}

TEST_CASE("an ACK is exactly four bytes", "[tftp]")
{
    CHECK(decode_packet(from_hex("0004")).error() == Error::truncated);
    CHECK(decode_packet(from_hex("0004 00")).error() == Error::truncated);
    CHECK(decode_packet(from_hex("0004 0001 00")).error() == Error::bad_length);
}

TEST_CASE("a DATA packet is opcode 3, a block number and up to a block of data", "[tftp]")
{
    const auto payload = adl::test::sample::payload32();
    const auto bytes = to_bytes(TftpData{1, payload});

    CHECK(bytes == from_hex("0003 0001 000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"));
    const auto decoded = decode_packet(bytes);
    REQUIRE(decoded.has_value());
    CHECK(as<TftpData>(*decoded).block == 1);
    CHECK(std::ranges::equal(as<TftpData>(*decoded).data, payload));
}

TEST_CASE("a DATA packet with no data is the end of a file that fills its last block", "[tftp]")
{
    const auto bytes = to_bytes(TftpData{7, {}});

    CHECK(bytes == from_hex("0003 0007"));
    const auto decoded = decode_packet(bytes);
    REQUIRE(decoded.has_value());
    CHECK(as<TftpData>(*decoded).data.empty());
}

TEST_CASE("a block holds at most 65464 bytes, the largest block size of RFC 2348", "[tftp]")
{
    std::vector<std::byte> largest(max_packet_size(tftp_max_block_size), std::byte{0x5A});
    largest[0] = std::byte{0};
    largest[1] = std::byte{3};
    largest[2] = std::byte{0xFF};
    largest[3] = std::byte{0xFF};

    const auto decoded = decode_packet(largest);
    REQUIRE(decoded.has_value());
    CHECK(as<TftpData>(*decoded).data.size() == 65464);

    largest.push_back(std::byte{0});
    CHECK(decode_packet(largest).error() == Error::bad_length);

    std::vector<std::byte> data(tftp_max_block_size + 1);
    std::vector<std::byte> out(70000);
    CHECK(encode_data({1, data}, out).error() == Error::field_too_large);
    CHECK(encode_data({1, std::span<const std::byte>{data}.first(tftp_max_block_size)}, out).value() == 65468);
}

TEST_CASE("block numbers go from 65535 to 1, never to 0", "[tftp]")
{
    CHECK(next_block(0) == 1);
    CHECK(next_block(1) == 2);
    CHECK(next_block(65534) == 65535);
    CHECK(next_block(65535) == 1);

    std::uint16_t block = 65530;
    std::vector<std::uint16_t> seen;
    for (int i = 0; i < 10; ++i)
    {
        seen.push_back(block);
        block = next_block(block);
    }
    CHECK(seen == std::vector<std::uint16_t>{65530, 65531, 65532, 65533, 65534, 65535, 1, 2, 3, 4});
}

TEST_CASE("the largest packet is four bytes more than the block size", "[tftp]")
{
    CHECK(max_packet_size(tftp_default_block_size) == 516);
    CHECK(max_packet_size(tftp_max_block_size) == 65468);
}

TEST_CASE("an error packet is opcode 5, a code and a text ending in a NUL", "[tftp]")
{
    CHECK(to_bytes(TftpError{TftpErrorCode::file_not_found, "File not found"}) ==
          from_hex("0005 0001 46696c65206e6f7420666f756e64 00"));

    const auto bytes = from_hex("0005 0003 4469736b2066756c6c 00");
    const auto decoded = decode_packet(bytes);
    REQUIRE(decoded.has_value());
    CHECK(as<TftpError>(*decoded) == TftpError{TftpErrorCode::disk_full, "Disk full"});
}

TEST_CASE("an error packet with no text is just the terminator", "[tftp]")
{
    const auto decoded = decode_packet(from_hex("0005 0000 00"));
    REQUIRE(decoded.has_value());
    CHECK(as<TftpError>(*decoded).message.empty());
}

TEST_CASE("an error code outside RFC 1350's table is kept as it came", "[tftp]")
{
    const auto decoded = decode_packet(from_hex("0005 0063 6200"));
    REQUIRE(decoded.has_value());
    CHECK(static_cast<std::uint16_t>(as<TftpError>(*decoded).code) == 99);
}

TEST_CASE("an error packet must end with its terminator and nothing after it", "[tftp]")
{
    CHECK(decode_packet(from_hex("0005 0001")).error() == Error::truncated);
    CHECK(decode_packet(from_hex("0005 0001 6162")).error() == Error::truncated);
    CHECK(decode_packet(from_hex("0005 0001 6162 00 00")).error() == Error::bad_length);
    CHECK(decode_packet(from_hex("0005 0001 61 0a 62 00")).error() == Error::malformed_text);
}

TEST_CASE("an option acknowledgement is opcode 6 and the options accepted", "[tftp]")
{
    const auto bytes = to_bytes(adl::test::sample::oack());

    CHECK(bytes == from_hex("0006 626c6b73697a6500 3130323400 636865636b73756d5f3300 4445414442454546 00"));
    const auto decoded = decode_packet(bytes);
    REQUIRE(decoded.has_value());
    const auto options = collect(as<TftpOptionAck>(*decoded).options);
    REQUIRE(options.size() == 2);
    CHECK(options[1] == TftpOption{"checksum_3", "DEADBEEF"});
}

TEST_CASE("an option acknowledgement of no options is not a packet", "[tftp]")
{
    CHECK(decode_packet(from_hex("0006")).error() == Error::bad_option);

    std::array<std::byte, 16> buffer{};
    CHECK(encode_option_ack({}, buffer).error() == Error::bad_option);
}

TEST_CASE("an unknown opcode is refused", "[tftp]")
{
    for (const auto* hex : {"0000", "0007", "0100", "ffff", "0008 00"})
    {
        CAPTURE(hex);
        CHECK(decode_packet(from_hex(hex)).error() == Error::unknown_opcode);
        CHECK(peek_opcode(from_hex(hex)).error() == Error::unknown_opcode);
    }
    CHECK(decode_packet({}).error() == Error::truncated);
    CHECK(decode_packet(from_hex("00")).error() == Error::truncated);
}

TEST_CASE("the opcode of a datagram can be read without decoding it", "[tftp]")
{
    CHECK(peek_opcode(from_hex("0001")).value() == Opcode::read_request);
    CHECK(peek_opcode(from_hex("0002 ff")).value() == Opcode::write_request);
    CHECK(peek_opcode(from_hex("0003")).value() == Opcode::data);
    CHECK(peek_opcode(from_hex("0004")).value() == Opcode::ack);
    CHECK(peek_opcode(from_hex("0005")).value() == Opcode::error);
    CHECK(peek_opcode(from_hex("0006")).value() == Opcode::option_ack);
}

TEST_CASE("every packet decodes to what was encoded", "[tftp]")
{
    const std::vector<TftpPacket> packets = {
        adl::test::sample::rrq_lui(),
        adl::test::sample::rrq_checksum(),
        adl::test::sample::wrq_lus(),
        adl::test::sample::wrq_plain(),
        adl::test::sample::oack(),
        TftpAck{0},
        TftpAck{12345},
        TftpData{1, adl::test::sample::payload32()},
        TftpData{65535, {}},
        TftpError{TftpErrorCode::not_defined, "WAIT:50"},
        TftpError{TftpErrorCode::option_refused, ""},
    };
    for (const TftpPacket& packet : packets)
    {
        std::vector<std::byte> bytes(*encoded_size(packet));
        REQUIRE(encode_packet(packet, bytes).value() == bytes.size());

        const auto decoded = decode_packet(bytes);
        REQUIRE(decoded.has_value());
        CHECK(decoded->index() == packet.index());

        // Encoding what was decoded gives the same bytes.
        std::vector<std::byte> again(bytes.size());
        REQUIRE(encode_packet(*decoded, again).value() == bytes.size());
        CHECK(again == bytes);
    }
}

TEST_CASE("an encoder reports a buffer that is too small and writes nothing past it", "[tftp]")
{
    const std::vector<TftpPacket> packets = {adl::test::sample::rrq_lui(), adl::test::sample::oack(), TftpAck{3},
                                             TftpData{9, adl::test::sample::payload32()},
                                             TftpError{TftpErrorCode::file_not_found, "x"}};
    for (const TftpPacket& packet : packets)
    {
        const std::size_t size = *encoded_size(packet);
        for (std::size_t room = 0; room < size; ++room)
        {
            // A guard of 8 bytes after the room the encoder is given.
            std::vector<std::byte> memory(room + 8, std::byte{0xAA});
            const auto result = encode_packet(packet, std::span<std::byte>{memory}.first(room));
            REQUIRE_FALSE(result.has_value());
            CHECK(result.error() == Error::buffer_too_small);
            for (std::size_t i = room; i < memory.size(); ++i)
            {
                REQUIRE(memory[i] == std::byte{0xAA});
            }
        }
    }
}

TEST_CASE("text in a packet may not hold a NUL or a control character", "[tftp]")
{
    std::array<std::byte, 64> buffer{};
    CHECK(encode_request({TftpRequest::Kind::read, std::string_view{"a\0b", 3}, {}}, buffer).error() ==
          Error::malformed_text);
    CHECK(encode_request({TftpRequest::Kind::read, "a\nb", {}}, buffer).error() == Error::malformed_text);
    CHECK(encode_error({TftpErrorCode::not_defined, std::string_view{"x\0", 2}}, buffer).error() ==
          Error::malformed_text);
}

TEST_CASE("an option with no name cannot be sent", "[tftp]")
{
    std::array<std::byte, 64> buffer{};
    const std::array options = {TftpOption{"", "1"}};
    CHECK(encode_request({TftpRequest::Kind::read, "f", options}, buffer).error() == Error::bad_option);
}

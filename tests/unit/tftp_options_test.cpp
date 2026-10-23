// SPDX-License-Identifier: GPL-3.0-or-later
// The TFTP options of RFCs 2347 to 2349 and the ones 615A adds (section 5.3.2.3.8), and the
// WAIT and ABORT messages carried in error packets (sections 5.3.2.3.4 and 5.3.2.3.6).

#include "adl/arinc615a/limits.hpp"
#include "adl/arinc615a/tftp.hpp"
#include "bytes.hpp"
#include "codec_615a.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string>
#include <utility>
#include <vector>

using namespace adl::arinc615a;
using adl::arinc665::CheckValueType;
using adl::test::from_hex;
using adl::test::to_bytes;

namespace
{

Result<TransferOptions> read_one(std::string_view name, std::string_view value)
{
    const std::array options = {TftpOption{name, value}};
    return read_transfer_options(options);
}

bool refused(std::string_view name, std::string_view value)
{
    const auto result = read_one(name, value);
    return !result.has_value() && result.error() == Error::bad_option;
}

} // namespace

TEST_CASE("the block size option takes 8 to 65464, as RFC 2348 says", "[tftp][options]")
{
    CHECK(read_one("blksize", "8")->block_size == 8);
    CHECK(read_one("blksize", "512")->block_size == 512);
    CHECK(read_one("blksize", "1468")->block_size == 1468);
    CHECK(read_one("blksize", "65464")->block_size == 65464);

    CHECK(refused("blksize", "7"));
    CHECK(refused("blksize", "0"));
    CHECK(refused("blksize", "65465"));
    CHECK(refused("blksize", "99999999999"));
}

TEST_CASE("the transfer size option takes any size, and a read request asks with 0", "[tftp][options]")
{
    CHECK(read_one("tsize", "0")->transfer_size == 0);
    CHECK(read_one("tsize", "87")->transfer_size == 87);
    CHECK(read_one("tsize", "4294967296")->transfer_size == 4294967296ULL);
    CHECK(read_one("tsize", "18446744073709551615")->transfer_size == 18446744073709551615ULL);

    CHECK(refused("tsize", "18446744073709551616"));
    CHECK(refused("tsize", "-1"));
}

TEST_CASE("the timeout option takes 1 to 255 seconds", "[tftp][options]")
{
    CHECK(read_one("timeout", "1")->timeout == 1);
    CHECK(read_one("timeout", "2")->timeout == 2);
    CHECK(read_one("timeout", "255")->timeout == 255);

    CHECK(refused("timeout", "0"));
    CHECK(refused("timeout", "256"));
}

TEST_CASE("the port option takes any port, though 615A-3 would have 49152 or more", "[tftp][options]")
{
    // The standard's own example (Appendix G) asks for port 1615.
    CHECK(read_one("port", "1615")->port == 1615);
    CHECK(read_one("port", "49152")->port == first_private_port);
    CHECK(read_one("port", "65535")->port == 65535);
    CHECK(read_one("port", "1")->port == 1);

    CHECK(refused("port", "0"));
    CHECK(refused("port", "65536"));
}

TEST_CASE("a number option holds digits and nothing else", "[tftp][options]")
{
    for (const char* name : {"blksize", "tsize", "timeout", "port"})
    {
        for (const char* value : {"", " 5", "5 ", "+5", "-5", "5x", "0x10", "5.0", "abc"})
        {
            CAPTURE(name, value);
            CHECK(refused(name, value));
        }
    }
}

TEST_CASE("option names compare without regard to case", "[tftp][options]")
{
    CHECK(read_one("BLKSIZE", "512")->block_size == 512);
    CHECK(read_one("TSize", "9")->transfer_size == 9);
    CHECK(read_one("Port", "1615")->port == 1615);
    CHECK(read_one("CHECKSUM_2", "ABCD")->checksum.has_value());

    const std::array options = {TftpOption{"BlkSize", "1024"}};
    CHECK(find_option(options, "blksize") == "1024");
    CHECK(find_option(options, "BLKSIZE") == "1024");
    CHECK_FALSE(find_option(options, "blksize2").has_value());
    CHECK_FALSE(find_option(options, "tsize").has_value());
}

TEST_CASE("options the library does not know are ignored", "[tftp][options]")
{
    const std::array options = {TftpOption{"zeroize", "yes"}, TftpOption{"blksize", "1024"},
                                TftpOption{"checksum", "nothing"}};
    const auto result = read_transfer_options(options);
    REQUIRE(result.has_value());
    CHECK(result->block_size == 1024);
    CHECK_FALSE(result->port.has_value());
}

TEST_CASE("a known option given twice is refused", "[tftp][options]")
{
    for (const char* name : {"blksize", "tsize", "timeout", "port"})
    {
        CAPTURE(name);
        const std::array options = {TftpOption{name, "9"}, TftpOption{name, "9"}};
        CHECK(read_transfer_options(options).error() == Error::bad_option);
    }
    const std::array same_name_other_case = {TftpOption{"port", "2000"}, TftpOption{"PORT", "2000"}};
    CHECK(read_transfer_options(same_name_other_case).error() == Error::bad_option);
}

TEST_CASE("no options is a valid set of options", "[tftp][options]")
{
    const auto result = read_transfer_options({});
    REQUIRE(result.has_value());
    CHECK(*result == TransferOptions{});
}

TEST_CASE("the checksum option is named for the algorithm's number in ARINC 665 Table 5.1", "[tftp][options]")
{
    const auto crc16 = read_one("checksum_2", "B1C2");
    REQUIRE(crc16.has_value());
    REQUIRE(crc16->checksum.has_value());
    CHECK(crc16->checksum->type == CheckValueType::crc16);
    CHECK(std::ranges::equal(crc16->checksum->bytes(), from_hex("B1C2")));

    const auto crc8 = read_one("checksum_1", "00A5"); // stored in a word, like 665's check values
    REQUIRE(crc8.has_value());
    CHECK(crc8->checksum->type == CheckValueType::crc8);
    CHECK(crc8->checksum->length == 2);

    const auto crc32 = read_one("checksum_3", "deadBEEF");
    REQUIRE(crc32.has_value());
    CHECK(std::ranges::equal(crc32->checksum->bytes(), from_hex("DEADBEEF")));

    const auto md5 = read_one("checksum_4", "00112233445566778899AABBCCDDEEFF");
    REQUIRE(md5.has_value());
    CHECK(md5->checksum->length == 16);

    const auto sha1 = read_one("checksum_5", "00112233445566778899AABBCCDDEEFF00112233");
    REQUIRE(sha1.has_value());
    CHECK(sha1->checksum->length == 20);
}

TEST_CASE("a checksum value must be hexadecimal of the algorithm's length", "[tftp][options]")
{
    CHECK(refused("checksum_2", "B1C"));    // odd
    CHECK(refused("checksum_2", "B1C2D3")); // too long
    CHECK(refused("checksum_2", "B1"));     // too short
    CHECK(refused("checksum_2", ""));       // empty
    CHECK(refused("checksum_2", "B1G2"));   // not hex
    CHECK(refused("checksum_2", "0xB1C2")); // not bare hex
    CHECK(refused("checksum_3", "B1C2"));   // a CRC-32 needs eight digits
    CHECK(refused("checksum_4", "00112233"));
}

TEST_CASE("a checksum for an algorithm the library does not know is an unknown option", "[tftp][options]")
{
    // 0 is "none" in this library's CheckValueType, and the table has nothing above 5.
    CHECK(read_one("checksum_0", "1234")->checksum == std::nullopt);
    CHECK(read_one("checksum_6", "1234")->checksum == std::nullopt);
    CHECK(read_one("checksum_99", "zz")->checksum == std::nullopt);
    // But a name with no number is not a checksum option the standard defines either way.
    CHECK(refused("checksum_", "1234"));
    CHECK(refused("checksum_x", "1234"));
}

TEST_CASE("only one checksum option is accepted", "[tftp][options]")
{
    const std::array options = {TftpOption{"checksum_2", "B1C2"}, TftpOption{"checksum_3", "DEADBEEF"}};
    CHECK(read_transfer_options(options).error() == Error::bad_option);

    const std::array twice = {TftpOption{"checksum_2", "B1C2"}, TftpOption{"checksum_2", "B1C2"}};
    CHECK(read_transfer_options(twice).error() == Error::bad_option);
}

TEST_CASE("the option builder writes the options of a request with no allocation", "[tftp][options]")
{
    OptionBuilder builder;
    REQUIRE(builder.add_block_size(1024).has_value());
    REQUIRE(builder.add_transfer_size(0).has_value());
    REQUIRE(builder.add_timeout(2).has_value());
    REQUIRE(builder.add_port(1615).has_value());
    ChecksumOption checksum;
    checksum.type = CheckValueType::crc16;
    checksum.length = 2;
    checksum.value[0] = std::byte{0xB1};
    checksum.value[1] = std::byte{0xC2};
    REQUIRE(builder.add_checksum(checksum).has_value());
    CHECK(builder.count() == 5);

    const std::vector<TftpOption> options{builder.list().begin(), builder.list().end()};
    REQUIRE(options.size() == 5);
    CHECK(options[0] == TftpOption{"blksize", "1024"});
    CHECK(options[1] == TftpOption{"tsize", "0"});
    CHECK(options[2] == TftpOption{"timeout", "2"});
    CHECK(options[3] == TftpOption{"port", "1615"});
    CHECK(options[4] == TftpOption{"checksum_2", "B1C2"});

    const auto read = read_transfer_options(builder.list());
    REQUIRE(read.has_value());
    CHECK(read->block_size == 1024);
    CHECK(read->checksum == checksum);
}

TEST_CASE("options built with the builder travel in a request", "[tftp][options]")
{
    OptionBuilder builder;
    REQUIRE(builder.add_block_size(1400).has_value());
    REQUIRE(builder.add_port(49152).has_value());

    const auto bytes = to_bytes(TftpRequest{TftpRequest::Kind::write, "T_L.LUS", builder.list()});
    const auto decoded = decode_packet(bytes);
    REQUIRE(decoded.has_value());
    const auto options = read_transfer_options(std::get<TftpRequest>(*decoded).options);
    REQUIRE(options.has_value());
    CHECK(options->block_size == 1400);
    CHECK(options->port == 49152);
}

TEST_CASE("the option builder refuses what is not an option", "[tftp][options]")
{
    OptionBuilder builder;
    CHECK(builder.add("", "1").error() == Error::bad_option);
    CHECK(builder.add(std::string_view{"a\0b", 3}, "1").error() == Error::bad_option);
    CHECK(builder.add("a", std::string_view{"1\0", 2}).error() == Error::bad_option);
    CHECK(builder.add_block_size(7).error() == Error::bad_option);
    CHECK(builder.add_block_size(65465).error() == Error::bad_option);
    CHECK(builder.add_timeout(0).error() == Error::bad_option);
    CHECK(builder.add_port(0).error() == Error::bad_option);

    ChecksumOption wrong_length;
    wrong_length.type = CheckValueType::crc32;
    wrong_length.length = 2;
    CHECK(builder.add_checksum(wrong_length).error() == Error::bad_option);
    ChecksumOption none;
    none.type = CheckValueType::none;
    CHECK(builder.add_checksum(none).error() == Error::bad_option);
    CHECK(builder.count() == 0);
}

TEST_CASE("the option builder says when it is full and keeps what it had", "[tftp][options]")
{
    OptionBuilder builder;
    int added = 0;
    while (builder.add("name", "0123456789"))
    {
        ++added;
    }
    CHECK(added == static_cast<int>(OptionBuilder::capacity / 16));
    CHECK(builder.count() == static_cast<std::size_t>(added));
    CHECK(builder.add("name", "0123456789").error() == Error::buffer_too_small);
    CHECK(builder.list().size() == static_cast<std::size_t>(added));
}

TEST_CASE("the builder holds the options a load would ask for with room to spare", "[tftp][options]")
{
    OptionBuilder builder;
    ChecksumOption sha1;
    sha1.type = CheckValueType::sha1;
    sha1.length = 20;
    REQUIRE(builder.add_block_size(65464).has_value());
    REQUIRE(builder.add_transfer_size(18446744073709551615ULL).has_value());
    REQUIRE(builder.add_timeout(255).has_value());
    REQUIRE(builder.add_port(65535).has_value());
    REQUIRE(builder.add_checksum(sha1).has_value());
}

// ---- WAIT and ABORT --------------------------------------------------------------------

TEST_CASE("a wait message is an error packet, code 0, with the text WAIT:50", "[tftp][wait]")
{
    // The standard's own example (5.3.2.3.4): 0x0005, 0x0000, "WAIT:50", 0x00.
    std::array<std::byte, 32> buffer{};
    const auto size = encode_wait(WaitMessage{50}, buffer);

    REQUIRE(size.has_value());
    CHECK(std::vector<std::byte>(buffer.begin(), buffer.begin() + static_cast<long>(*size)) ==
          from_hex("0005 0000 574149543a3530 00"));
}

TEST_CASE("an abort message is an error packet, code 0, with ABORT: and four hex digits", "[tftp][abort]")
{
    // 5.3.2.3.6: the Load Status Code of Table 6.4.10-1, here 0x1004.
    std::array<std::byte, 32> buffer{};
    const auto size = encode_abort(AbortMessage{StatusCode::aborted_by_data_loader}, buffer);

    REQUIRE(size.has_value());
    CHECK(std::vector<std::byte>(buffer.begin(), buffer.begin() + static_cast<long>(*size)) ==
          from_hex("0005 0000 41424f52543a31303034 00"));
}

TEST_CASE("the wait time written is decimal with no leading zeros", "[tftp][wait]")
{
    std::array<std::byte, 32> buffer{};
    using Case = std::pair<std::uint16_t, std::string_view>;
    for (const auto& [seconds, text] : std::array{Case{0, "WAIT:0"}, Case{5, "WAIT:5"}, Case{65535, "WAIT:65535"}})
    {
        CAPTURE(seconds);
        const auto size = encode_wait(WaitMessage{seconds}, buffer);
        REQUIRE(size.has_value());
        const auto decoded = decode_packet(std::span<const std::byte>{buffer}.first(*size));
        REQUIRE(decoded.has_value());
        CHECK(std::get<TftpError>(*decoded).message == text);
    }
}

TEST_CASE("every wait time from 0 to 65535 survives encoding and interpreting", "[tftp][wait]")
{
    std::array<std::byte, 32> buffer{};
    for (std::uint32_t seconds = 0; seconds <= 65535; ++seconds)
    {
        const auto size = encode_wait(WaitMessage{static_cast<std::uint16_t>(seconds)}, buffer);
        REQUIRE(size.has_value());
        const auto decoded = decode_packet(std::span<const std::byte>{buffer}.first(*size));
        REQUIRE(decoded.has_value());
        const auto meaning = interpret_error(std::get<TftpError>(*decoded));
        REQUIRE(meaning.has_value());
        REQUIRE(std::holds_alternative<WaitMessage>(*meaning));
        REQUIRE(std::get<WaitMessage>(*meaning).seconds == seconds);
    }
}

TEST_CASE("every abort status code survives encoding and interpreting", "[tftp][abort]")
{
    std::array<std::byte, 32> buffer{};
    for (std::uint32_t code = 0; code <= 0xFFFF; ++code)
    {
        const AbortMessage message{static_cast<StatusCode>(code)};
        const auto size = encode_abort(message, buffer);
        REQUIRE(size.has_value());
        const auto decoded = decode_packet(std::span<const std::byte>{buffer}.first(*size));
        REQUIRE(decoded.has_value());
        const auto meaning = interpret_error(std::get<TftpError>(*decoded));
        REQUIRE(meaning.has_value());
        REQUIRE(std::holds_alternative<AbortMessage>(*meaning));
        REQUIRE(std::get<AbortMessage>(*meaning) == message);
    }
}

TEST_CASE("an abort status code is written in capitals", "[tftp][abort]")
{
    std::array<std::byte, 32> buffer{};
    const auto size = encode_abort(AbortMessage{static_cast<StatusCode>(0xABCD)}, buffer);
    REQUIRE(size.has_value());
    const auto decoded = decode_packet(std::span<const std::byte>{buffer}.first(*size));
    CHECK(std::get<TftpError>(*decoded).message == "ABORT:ABCD");
}

TEST_CASE("a wait or abort message is read from error code 0 only", "[tftp][wait][abort]")
{
    const auto meaning = interpret_error({TftpErrorCode::not_defined, "WAIT:30"});
    REQUIRE(meaning.has_value());
    CHECK(std::get<WaitMessage>(*meaning).seconds == 30);

    // Under any other code the same text is an ordinary error.
    const TftpError other{TftpErrorCode::access_violation, "WAIT:30"};
    const auto plain = interpret_error(other);
    REQUIRE(plain.has_value());
    CHECK(std::get<TftpError>(*plain) == other);
}

TEST_CASE("text that only looks like a wait or abort message is an ordinary error", "[tftp][wait][abort]")
{
    for (const char* text : {"", "WAIT", "WAI", "wait:5", "Wait:5", "WAITING:5", " WAIT:5", "ABORT", "abort:1004",
                             "ABORTED:1004", "File not found"})
    {
        CAPTURE(text);
        const TftpError error{TftpErrorCode::not_defined, text};
        const auto meaning = interpret_error(error);
        REQUIRE(meaning.has_value());
        REQUIRE(std::holds_alternative<TftpError>(*meaning));
        CHECK(std::get<TftpError>(*meaning) == error);
    }
}

TEST_CASE("a wait or abort message with a bad number is an invalid value", "[tftp][wait][abort]")
{
    for (const char* text :
         {"WAIT:", "WAIT:-1", "WAIT:+1", "WAIT:65536", "WAIT:100000", "WAIT:1 ", "WAIT: 1", "WAIT:1s", "WAIT:0x10",
          "WAIT:1.5", "ABORT:", "ABORT:100", "ABORT:10044", "ABORT:G004", "ABORT:0x10", "ABORT: 104", "ABORT:1004 "})
    {
        CAPTURE(text);
        const auto meaning = interpret_error({TftpErrorCode::not_defined, text});
        REQUIRE_FALSE(meaning.has_value());
        CHECK(meaning.error() == Error::invalid_value);
    }
}

TEST_CASE("an abort status code may be written in either case", "[tftp][abort]")
{
    const auto lower = interpret_error({TftpErrorCode::not_defined, "ABORT:abcd"});
    const auto upper = interpret_error({TftpErrorCode::not_defined, "ABORT:ABCD"});
    REQUIRE(lower.has_value());
    REQUIRE(upper.has_value());
    CHECK(*lower == *upper);
}

TEST_CASE("wait and abort messages are refused by a buffer that is too small", "[tftp][wait]")
{
    std::array<std::byte, 8> small{};
    CHECK(encode_wait(WaitMessage{50}, small).error() == Error::buffer_too_small);
    CHECK(encode_abort(AbortMessage{}, small).error() == Error::buffer_too_small);
    std::array<std::byte, 0> none{};
    CHECK(encode_wait(WaitMessage{50}, none).error() == Error::buffer_too_small);
}

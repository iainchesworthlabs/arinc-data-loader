// SPDX-License-Identifier: GPL-3.0-or-later
// The protocol files of ARINC 615A-3 section 6.4: their layouts, round trips, the choices
// docs/arinc615a-codec.md records, and each way a file can be damaged.

#include "adl/arinc615a/limits.hpp"
#include "adl/arinc615a/protocol_files.hpp"
#include "bytes.hpp"
#include "codec_615a.hpp"
#include "edit.hpp"
#include "sample_615a.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <string>
#include <utility>
#include <vector>

using namespace adl::arinc615a;
using adl::test::bytes_of;
using adl::test::from_hex;
using adl::test::to_bytes;
namespace sample = adl::test::sample;

namespace
{

std::vector<LoadReference> many_loads(std::size_t count)
{
    return std::vector<LoadReference>(count, LoadReference{"a", "b"});
}

// A .LCI with the given bytes after the status code, and the file length put right.
std::vector<std::byte> lci_with(std::string_view hex_after_status)
{
    auto body = from_hex(hex_after_status);
    std::vector<std::byte> file = from_hex("00000000 4134 0001");
    file.insert(file.end(), body.begin(), body.end());
    adl::test::put_u32(file, 0, static_cast<std::uint32_t>(file.size()));
    return file;
}

} // namespace

// ---- Layout ----------------------------------------------------------------------------

TEST_CASE("every protocol file starts with its length and the version A4", "[protocol-files]")
{
    // Section 6.4: File Length is 32 bits, in 8-bit words, counting itself; the Protocol
    // Version is the next two bytes, "A4" for Supplement 3.
    CHECK(to_bytes(sample::initialization_accepted()) == from_hex("00000009 4134 0001 00"));
    CHECK(protocol_version == "A4");
}

TEST_CASE("an .LCS holds the counter, status, timers and description in that order", "[protocol-files]")
{
    // Table 6.4.3-1: counter, status code, exception timer, estimated time, description.
    CHECK(to_bytes(sample::configuration_status_idle()) == from_hex("0000000F 4134 0000 0001 0000 0000 00"));
    CHECK(to_bytes(sample::configuration_status()) ==
          from_hex("0000001d 4134 0007 0004 001e 0078 0e 4572617369 6e6720666c617368 00"));
}

TEST_CASE("an .LUS puts the status first and the counter after its description", "[protocol-files]")
{
    // Table 6.4.5-1: status, description, counter, timers, list ratio, then the loads.
    CHECK(to_bytes(sample::upload_status_start()) == from_hex("00000014 4134 0001 00 0000 0000 0000 202030 0000"));
}

TEST_CASE("an .LUR is a count and a header file name and part number for each load", "[protocol-files]")
{
    const std::array one = {LoadReference{"A.LUH", "P"}};
    CHECK(to_bytes(UploadRequest{one}) == from_hex("00000012 4134 0001 06 412e4c5548 00 02 50 00"));
}

TEST_CASE("a ratio is three characters, right adjusted with blanks", "[protocol-files]")
{
    std::array<std::byte, 64> out{};
    using Case = std::pair<int, std::string_view>;
    for (const auto& [ratio, text] : std::array{Case{0, "  0"}, Case{7, "  7"}, Case{10, " 10"}, Case{45, " 45"},
                                                Case{99, " 99"}, Case{100, "100"}})
    {
        CAPTURE(ratio);
        UploadStatus status = sample::upload_status_start();
        status.list_ratio = static_cast<std::uint8_t>(ratio);
        const auto bytes = to_bytes(status);
        // The ratio follows: 4 + 2 + 2 + 1 + 6 bytes.
        CHECK(std::string(reinterpret_cast<const char*>(bytes.data()) + 15, 3) == text);
    }
    UploadStatus too_big = sample::upload_status_start();
    too_big.list_ratio = 101;
    CHECK(encode_upload_status(too_big, out).error() == Error::invalid_value);
    too_big.list_ratio = 255;
    CHECK(encoded_size(too_big).error() == Error::invalid_value);
}

TEST_CASE("a ratio is read from blanks and digits only, and is at most 100", "[protocol-files]")
{
    // The ratio of the start-up .LUS sits at bytes 15 to 17.
    const auto good = to_bytes(sample::upload_status_start());
    const auto with_ratio = [&](std::string_view text)
    {
        auto bytes = good;
        for (std::size_t i = 0; i < 3; ++i)
        {
            bytes[15 + i] = static_cast<std::byte>(text[i]);
        }
        return decode_upload_status(bytes);
    };
    CHECK(with_ratio("  0")->list_ratio == 0);
    CHECK(with_ratio(" 45")->list_ratio == 45);
    CHECK(with_ratio("100")->list_ratio == 100);
    CHECK(with_ratio("045")->list_ratio == 45); // leading zeros are digits too
    CHECK(with_ratio("  5")->list_ratio == 5);

    for (const char* text : {"101", "999", "   ", "1 0", "5  ", " -5", "  x", "0x1", "\0\0\0"})
    {
        CAPTURE(text);
        CHECK(with_ratio(std::string_view{text, 3}).error() == Error::invalid_value);
    }
}

// ---- Round trips -----------------------------------------------------------------------

TEST_CASE("each protocol file decodes to what was encoded, and encodes back to the same bytes", "[protocol-files]")
{
    SECTION("initialization")
    {
        for (const auto& file : {sample::initialization_accepted(), sample::initialization_denied()})
        {
            const auto bytes = to_bytes(file);
            const auto decoded = decode_initialization_file(bytes);
            REQUIRE(decoded.has_value());
            CHECK(*decoded == file);
            CHECK(to_bytes(*decoded) == bytes);
        }
    }
    SECTION("configuration list")
    {
        const auto bytes = to_bytes(sample::configuration_list());
        const auto decoded = decode_configuration_list(bytes);
        REQUIRE(decoded.has_value());
        CHECK(*decoded == sample::configuration_list());
        CHECK(to_bytes(*decoded) == bytes);

        REQUIRE(decoded->hardware.size() == 2);
        const auto first = *decoded->hardware.begin();
        CHECK(first.literal_name == "FMS LEFT");
        CHECK(first.serial_number == "SN0001");
        REQUIRE(first.parts.size() == 2);
        const auto second_part = *std::next(first.parts.begin());
        CHECK(second_part.part_number == "ACM47-1234-5678");
        CHECK(second_part.amendment.empty());
        CHECK(second_part.designation == "FMS 1 OPS");
    }
    SECTION("configuration status")
    {
        for (const auto& file : {sample::configuration_status(), sample::configuration_status_idle()})
        {
            const auto bytes = to_bytes(file);
            const auto decoded = decode_configuration_status(bytes);
            REQUIRE(decoded.has_value());
            CHECK(*decoded == file);
            CHECK(to_bytes(*decoded) == bytes);
        }
    }
    SECTION("upload request")
    {
        const auto bytes = to_bytes(sample::upload_request());
        const auto decoded = decode_upload_request(bytes);
        REQUIRE(decoded.has_value());
        CHECK(*decoded == sample::upload_request());
        CHECK(to_bytes(*decoded) == bytes);
    }
    SECTION("upload status")
    {
        for (const auto& file : {sample::upload_status(), sample::upload_status_start()})
        {
            const auto bytes = to_bytes(file);
            const auto decoded = decode_upload_status(bytes);
            REQUIRE(decoded.has_value());
            CHECK(*decoded == file);
            CHECK(to_bytes(*decoded) == bytes);
        }
        const auto bytes = to_bytes(sample::upload_status());
        const auto decoded = decode_upload_status(bytes);
        CHECK(decoded->counter == 65535);
        CHECK(decoded->estimated_time == estimated_time_unknown);
        CHECK(decoded->list_ratio == 45);
        REQUIRE(decoded->loads.size() == 2);
        CHECK((*std::next(decoded->loads.begin())).description == "Erasing");
    }
    SECTION("download request")
    {
        const auto bytes = to_bytes(sample::download_request());
        const auto decoded = decode_download_request(bytes);
        REQUIRE(decoded.has_value());
        CHECK(*decoded == sample::download_request());
        CHECK(to_bytes(*decoded) == bytes);
        const auto first = *decoded->files.begin();
        CHECK(first.file_name == "LOG1.BIN");
        CHECK(std::ranges::equal(first.user_defined_data, from_hex("DEADBEEF")));
    }
    SECTION("download status")
    {
        const auto bytes = to_bytes(sample::download_status());
        const auto decoded = decode_download_status(bytes);
        REQUIRE(decoded.has_value());
        CHECK(*decoded == sample::download_status());
        CHECK(to_bytes(*decoded) == bytes);
    }
    SECTION("download list")
    {
        const auto bytes = to_bytes(sample::download_list());
        const auto decoded = decode_download_list(bytes);
        REQUIRE(decoded.has_value());
        CHECK(*decoded == sample::download_list());
        CHECK(to_bytes(*decoded) == bytes);
    }
    SECTION("download answer")
    {
        const auto bytes = to_bytes(sample::download_answer());
        const auto decoded = decode_download_answer(bytes);
        REQUIRE(decoded.has_value());
        CHECK(*decoded == sample::download_answer());
        CHECK(to_bytes(*decoded) == bytes);
    }
}

TEST_CASE("files with no entries are whole files", "[protocol-files]")
{
    CHECK(decode_configuration_list(to_bytes(ConfigurationList{})).value().hardware.empty());
    CHECK(decode_download_request(to_bytes(DownloadRequest{})).value().files.empty());
    CHECK(decode_download_status(to_bytes(DownloadStatus{})).value().files.empty());
    CHECK(decode_download_list(to_bytes(DownloadList{})).value().files.empty());
    CHECK(decode_download_answer(to_bytes(DownloadAnswer{})).value().file_names.empty());
    CHECK(decode_upload_status(to_bytes(UploadStatus{})).value().loads.empty());
}

TEST_CASE("an upload request holds at least one load", "[protocol-files]")
{
    // 6.4.4: "Any .LUR file should contain at least one Header File."
    std::array<std::byte, 64> out{};
    CHECK(encode_upload_request(UploadRequest{}, out).error() == Error::invalid_value);
    CHECK(decode_upload_request(from_hex("00000008 4134 0000")).error() == Error::invalid_value);
}

TEST_CASE("a list holds up to 65535 entries", "[protocol-files]")
{
    const auto loads = many_loads(65535);
    const UploadRequest full{loads};
    const auto bytes = to_bytes(full);
    CHECK(bytes.size() == 8 + 65535 * 6);

    const auto decoded = decode_upload_request(bytes);
    REQUIRE(decoded.has_value());
    CHECK(decoded->loads.size() == 65535);
    CHECK(std::distance(decoded->loads.begin(), decoded->loads.end()) == 65535);

    const auto too_many = many_loads(65536);
    CHECK(encoded_size(UploadRequest{too_many}).error() == Error::field_too_large);
}

TEST_CASE("a count larger than the file can hold is truncated, whatever it claims", "[protocol-files]")
{
    CHECK(decode_upload_request(from_hex("0000000b 4134 ffff 0261 00")).error() == Error::truncated);
    CHECK(decode_download_list(from_hex("00000008 4134 ffff")).error() == Error::truncated);
    CHECK(decode_download_answer(from_hex("00000008 4134 0001")).error() == Error::truncated);
}

TEST_CASE("nested entries are counted separately", "[protocol-files]")
{
    // A hardware item that claims more parts than the file holds.
    auto bytes = to_bytes(sample::configuration_list());
    // Literal name "FMS LEFT" (10 bytes with its length and NUL) and the serial number (8)
    // come first; the part count follows the item's two names.
    const std::size_t parts_at = 6 + 2 + 10 + 8;
    REQUIRE(adl::test::get_u16(bytes, parts_at / 2) == 2);
    adl::test::put_u16(bytes, parts_at / 2, 3);
    CHECK_FALSE(decode_configuration_list(bytes).has_value());
}

// ---- The frame -------------------------------------------------------------------------

TEST_CASE("a file length longer than the bytes is truncated, and shorter leaves bytes over", "[protocol-files]")
{
    auto bytes = to_bytes(sample::initialization_accepted());
    adl::test::put_u32(bytes, 0, 10);
    CHECK(decode_initialization_file(bytes).error() == Error::truncated);
    adl::test::put_u32(bytes, 0, 8);
    CHECK(decode_initialization_file(bytes).error() == Error::bad_length);
    adl::test::put_u32(bytes, 0, 0);
    CHECK(decode_initialization_file(bytes).error() == Error::bad_length);
    adl::test::put_u32(bytes, 0, 0xFFFFFFFFU);
    CHECK(decode_initialization_file(bytes).error() == Error::truncated);
}

TEST_CASE("bytes after the end of the file are refused", "[protocol-files]")
{
    for (int extra = 1; extra < 5; ++extra)
    {
        auto bytes = to_bytes(sample::upload_status());
        bytes.insert(bytes.end(), static_cast<std::size_t>(extra), std::byte{0});
        CHECK(decode_upload_status(bytes).error() == Error::bad_length);
    }
}

TEST_CASE("the protocol version must be A4", "[protocol-files]")
{
    for (const char* version : {"4134", "a134", "4234", "4133", "0000", "4141", "4433"})
    {
        CAPTURE(version);
        auto bytes = to_bytes(sample::initialization_accepted());
        const auto replaced = from_hex(version);
        bytes[4] = replaced[0];
        bytes[5] = replaced[1];
        if (std::string_view{version} == "4134")
        {
            CHECK(decode_initialization_file(bytes).has_value());
        }
        else
        {
            CHECK(decode_initialization_file(bytes).error() == Error::bad_version);
        }
    }
}

TEST_CASE("the version is read from a file whose layout the library may not know", "[protocol-files]")
{
    // A target on another protocol version: the data loader reads the version to refuse it
    // with an ABORT (section 6.4), even though the rest of the file is not for it.
    const auto other = from_hex("00000099 4235 aabbccdd");
    const auto version = peek_protocol_version(other);
    REQUIRE(version.has_value());
    CHECK(*version == std::array<char, 2>{'B', '5'});
    CHECK(decode_initialization_file(other).error() == Error::bad_version);

    CHECK(peek_protocol_version(from_hex("00000009 41")).error() == Error::truncated);
    CHECK(peek_protocol_version({}).error() == Error::truncated);
    CHECK(*peek_protocol_version(from_hex("00000009 4134")) == std::array<char, 2>{'A', '4'});
}

TEST_CASE("a file shorter than the frame is truncated", "[protocol-files]")
{
    for (std::size_t length = 0; length < 6; ++length)
    {
        const auto bytes = from_hex("0000000a 4134");
        CHECK(decode_upload_status(std::span<const std::byte>{bytes}.first(length)).error() == Error::truncated);
    }
}

// ---- Status codes ----------------------------------------------------------------------

TEST_CASE("an initialization answer is accepted, denied or not supported", "[protocol-files][status]")
{
    for (const auto code : {StatusCode::accepted, StatusCode::operation_denied, StatusCode::operation_not_supported})
    {
        const auto bytes = to_bytes(InitializationFile{code, ""});
        CHECK(decode_initialization_file(bytes).value().status == code);
    }
    for (const auto code : {StatusCode::in_progress, StatusCode::completed, StatusCode::in_progress_with_text,
                            StatusCode::aborted_by_target, StatusCode::aborted_by_data_loader,
                            StatusCode::aborted_by_operator, StatusCode::item_failed, static_cast<StatusCode>(0),
                            static_cast<StatusCode>(0x1001), static_cast<StatusCode>(0x1006)})
    {
        CAPTURE(static_cast<unsigned>(code));
        std::array<std::byte, 32> out{};
        CHECK(encode_initialization_file(InitializationFile{code, ""}, out).error() == Error::invalid_value);
        auto bytes = to_bytes(sample::initialization_accepted());
        adl::test::put_u16(bytes, 3, static_cast<std::uint16_t>(code));
        CHECK(decode_initialization_file(bytes).error() == Error::invalid_value);
    }
}

TEST_CASE("a status file's operation status is one of seven codes", "[protocol-files][status]")
{
    const std::array allowed = {StatusCode::accepted,           StatusCode::in_progress,
                                StatusCode::completed,          StatusCode::in_progress_with_text,
                                StatusCode::aborted_by_target,  StatusCode::aborted_by_data_loader,
                                StatusCode::aborted_by_operator};
    for (std::uint32_t value = 0; value <= 0x1FFF; ++value)
    {
        const auto code = static_cast<StatusCode>(value);
        const bool ok = std::ranges::find(allowed, code) != allowed.end();
        ConfigurationStatus file = sample::configuration_status_idle();
        file.status = code;
        std::array<std::byte, 64> out{};
        REQUIRE(encode_configuration_status(file, out).has_value() == ok);
        REQUIRE(allowed_in(StatusField::operation, code) == ok);
    }
}

TEST_CASE("a load or a file may also have failed (0x1007), which a whole operation cannot", "[protocol-files][status]")
{
    DownloadStatus status = sample::download_status();
    CHECK(encoded_size(status).has_value()); // LOG2.BIN is item_failed
    status.status = StatusCode::item_failed;
    CHECK(encoded_size(status).error() == Error::invalid_value);

    UploadStatus upload = sample::upload_status();
    upload.status = StatusCode::item_failed;
    CHECK(encoded_size(upload).error() == Error::invalid_value);

    // And an item cannot be denied or unsupported.
    const std::array denied = {FileProgress{"f", StatusCode::operation_denied, ""}};
    DownloadStatus bad = sample::download_status();
    bad.files = denied;
    CHECK(encoded_size(bad).error() == Error::invalid_value);
}

TEST_CASE("the known status codes are the ten of Table 6.4.10-1", "[protocol-files][status]")
{
    std::size_t known = 0;
    for (std::uint32_t value = 0; value <= 0xFFFF; ++value)
    {
        known += is_known(static_cast<StatusCode>(value)) ? 1U : 0U;
    }
    CHECK(known == 10);
    CHECK(static_cast<unsigned>(StatusCode::accepted) == 0x0001);
    CHECK(static_cast<unsigned>(StatusCode::aborted_by_data_loader) == 0x1004);
    CHECK(static_cast<unsigned>(StatusCode::item_failed) == 0x1007);
    CHECK(description_is_meaningful(StatusCode::operation_denied));
    CHECK(description_is_meaningful(StatusCode::item_failed));
    CHECK_FALSE(description_is_meaningful(StatusCode::completed));
    CHECK_FALSE(to_string(StatusCode::completed).empty());
}

TEST_CASE("the timers are carried as found, even where the standard has them zero", "[protocol-files][status]")
{
    // 6.4.3: for any status but 2 and 4 the exception timer and estimated time are 0. A
    // decoder is no place to refuse a target that leaves other figures there.
    ConfigurationStatus file{9, StatusCode::completed, 77, 88, ""};
    const auto decoded = decode_configuration_status(to_bytes(file));
    REQUIRE(decoded.has_value());
    CHECK(decoded->exception_timer == 77);
    CHECK(decoded->estimated_time == 88);
}

TEST_CASE("the counter and timers use their whole 16 bits", "[protocol-files]")
{
    ConfigurationStatus file{65535, StatusCode::in_progress, 65535, estimated_time_unknown, ""};
    const auto decoded = decode_configuration_status(to_bytes(file));
    REQUIRE(decoded.has_value());
    CHECK(*decoded == file);
}

// ---- Text ------------------------------------------------------------------------------

TEST_CASE("a text field's length counts the characters and the terminator", "[protocol-files][text]")
{
    // 6.4's commentary: the length is the size of the field, which the text need not fill;
    // an implementer may set it to the text plus its terminator, or to zero for no text.
    CHECK(to_bytes(InitializationFile{StatusCode::operation_denied, "Busy"}) ==
          from_hex("0000000e 4134 1000 05 4275737900"));
    CHECK(to_bytes(InitializationFile{StatusCode::operation_denied, ""}) == from_hex("00000009 4134 1000 00"));
}

TEST_CASE("a text is read up to its NUL and the rest of the field is padding", "[protocol-files][text]")
{
    const auto padded_bytes = lci_with("0a 616200 78787878787878");
    const auto padded = decode_initialization_file(padded_bytes);
    REQUIRE(padded.has_value());
    CHECK(padded->description == "ab");

    // The text may fill the field with no terminator at all.
    const auto unterminated_bytes = lci_with("03 616263");
    const auto unterminated = decode_initialization_file(unterminated_bytes);
    REQUIRE(unterminated.has_value());
    CHECK(unterminated->description == "abc");

    // A length of 1 holding only the terminator, and a length of 0, are both no text.
    CHECK(decode_initialization_file(lci_with("01 00"))->description.empty());
    CHECK(decode_initialization_file(lci_with("00"))->description.empty());
}

TEST_CASE("a text ends where the field does", "[protocol-files][text]")
{
    // The length says 5; only 2 bytes remain.
    CHECK(decode_initialization_file(lci_with("05 6162")).error() == Error::truncated);
    // The text is 3 long and 1 byte is left over after the file: a different length.
    CHECK(decode_initialization_file(lci_with("02 6100 00")).error() == Error::bad_length);
    CHECK(decode_initialization_file(lci_with("")).error() == Error::truncated);
}

TEST_CASE("a text of 254 characters fits its field and 255 does not", "[protocol-files][text]")
{
    const std::string longest(254, 'x');
    const auto bytes = to_bytes(InitializationFile{StatusCode::operation_denied, longest});
    CHECK(bytes.size() == 9 + 254 + 1);
    CHECK(bytes[8] == std::byte{255});
    CHECK(decode_initialization_file(bytes).value().description == longest);

    const std::string too_long(255, 'x');
    std::array<std::byte, 512> out{};
    CHECK(encode_initialization_file({StatusCode::operation_denied, too_long}, out).error() == Error::field_too_large);
    CHECK(max_text_chars == 254);
    CHECK(max_field_bytes == 255);
}

TEST_CASE("a field of 255 bytes with no terminator is 255 characters on the way in", "[protocol-files][text]")
{
    const std::string text(255, 'y');
    const auto bytes = lci_with("ff " + adl::test::to_hex(bytes_of(text)));
    const auto decoded = decode_initialization_file(bytes);
    REQUIRE(decoded.has_value());
    CHECK(decoded->description.size() == 255);
}

TEST_CASE("a required text may not be empty", "[protocol-files][text]")
{
    // 6.4.4: a header file name is 1 to 255 characters.
    std::array<std::byte, 64> out{};
    const std::array empty_name = {LoadReference{"", "P"}};
    CHECK(encode_upload_request(UploadRequest{empty_name}, out).error() == Error::invalid_value);
    const std::array empty_part_number = {LoadReference{"A.LUH", ""}};
    CHECK(encode_upload_request(UploadRequest{empty_part_number}, out).error() == Error::invalid_value);

    CHECK(decode_upload_request(from_hex("0000000c 4134 0001 00 0261 00")).error() == Error::invalid_value);
    CHECK(decode_upload_request(from_hex("0000000d 4134 0001 0100 0261 00")).error() == Error::invalid_value);
    CHECK(decode_upload_request(from_hex("0000000c 4134 0001 0261 00 00")).error() == Error::invalid_value);
}

TEST_CASE("an optional text may be empty", "[protocol-files][text]")
{
    const std::array part = {HardwarePart{"PN", "", "D"}};
    const std::array hardware = {HardwareItem{"L", "S", part}};
    CHECK(decode_configuration_list(to_bytes(ConfigurationList{hardware})).has_value());

    const std::array files = {FileListing{"F", ""}};
    CHECK(decode_download_list(to_bytes(DownloadList{files})).has_value());
}

TEST_CASE("a text holds no control character", "[protocol-files][text]")
{
    std::array<std::byte, 64> out{};
    for (const char c : {'\1', '\t', '\n', '\r', '\x1b', '\x1f', '\x7f'})
    {
        CAPTURE(static_cast<int>(c));
        const std::string text = std::string("a") + c + "b";
        CHECK(encode_initialization_file({StatusCode::operation_denied, text}, out).error() == Error::malformed_text);
        CHECK(decode_initialization_file(lci_with("04 61" + adl::test::to_hex(bytes_of(std::string(1, c))) + "62 00"))
                  .error() == Error::malformed_text);
    }
    // A NUL is the terminator, so the encoder cannot be given one inside the text.
    CHECK(encode_initialization_file({StatusCode::operation_denied, std::string_view{"a\0b", 3}}, out).error() ==
          Error::malformed_text);
    // Space and 8-bit characters are text.
    CHECK(decode_initialization_file(lci_with("05 6120e9fe 00")).has_value());
}

TEST_CASE("what follows the terminator in a padded field is not examined", "[protocol-files][text]")
{
    CHECK(decode_initialization_file(lci_with("05 61 00 01 02 03")).has_value());
}

// ---- Entries ---------------------------------------------------------------------------

TEST_CASE("user defined data is up to 255 bytes after its length", "[protocol-files]")
{
    const std::vector<std::byte> data(255, std::byte{0x42});
    const std::array longest = {FileRequest{"f", data}};
    const auto bytes = to_bytes(DownloadRequest{longest});
    const auto decoded = decode_download_request(bytes);
    REQUIRE(decoded.has_value());
    CHECK((*decoded->files.begin()).user_defined_data.size() == 255);

    const std::vector<std::byte> too_long(256, std::byte{0x42});
    const std::array bad = {FileRequest{"f", too_long}};
    CHECK(encoded_size(DownloadRequest{bad}).error() == Error::field_too_large);

    // A length that runs past the end of the file.
    CHECK(decode_download_request(from_hex("0000000e 4134 0001 02 6600 05 0102")).error() == Error::truncated);
}

TEST_CASE("user defined data is bytes, not text", "[protocol-files]")
{
    const std::array odd = {std::byte{0}, std::byte{1}, std::byte{0x10}, std::byte{0xFF}};
    const std::array files = {FileRequest{"f", odd}};
    const auto bytes = to_bytes(DownloadRequest{files});
    const auto decoded = decode_download_request(bytes);
    REQUIRE(decoded.has_value());
    CHECK(std::ranges::equal((*decoded->files.begin()).user_defined_data, odd));
}

TEST_CASE("a hardware item reports its parts under it", "[protocol-files]")
{
    const auto bytes = to_bytes(sample::configuration_list());
    const auto decoded = decode_configuration_list(bytes);
    REQUIRE(decoded.has_value());
    std::vector<std::string> numbers;
    for (const HardwareItem item : decoded->hardware)
    {
        for (const HardwarePart part : item.parts)
        {
            numbers.emplace_back(part.part_number);
        }
    }
    CHECK(numbers == std::vector<std::string>{"HNP-HW-0001", "ACM47-1234-5678", "HNP-HW-0001"});
}

TEST_CASE("a list can be walked more than once", "[protocol-files]")
{
    const auto bytes = to_bytes(sample::upload_request());
    const auto decoded = decode_upload_request(bytes);
    REQUIRE(decoded.has_value());
    for (int pass = 0; pass < 3; ++pass)
    {
        std::size_t count = 0;
        for (const LoadReference load : decoded->loads)
        {
            CHECK_FALSE(load.header_file_name.empty());
            ++count;
        }
        CHECK(count == 2);
    }
}

// ---- Buffers ---------------------------------------------------------------------------

TEST_CASE("every encoder reports a buffer that is too small and writes nothing past it", "[protocol-files]")
{
    const auto check = [](const auto& file, auto encode)
    {
        const std::size_t size = *encoded_size(file);
        REQUIRE(size > 0);
        for (std::size_t room = 0; room < size; ++room)
        {
            std::vector<std::byte> memory(room + 8, std::byte{0xAA});
            const auto result = encode(file, std::span<std::byte>{memory}.first(room));
            REQUIRE_FALSE(result.has_value());
            REQUIRE(result.error() == Error::buffer_too_small);
            for (std::size_t i = room; i < memory.size(); ++i)
            {
                REQUIRE(memory[i] == std::byte{0xAA});
            }
        }
    };
    check(sample::initialization_denied(), encode_initialization_file);
    check(sample::configuration_list(), encode_configuration_list);
    check(sample::configuration_status(), encode_configuration_status);
    check(sample::upload_request(), encode_upload_request);
    check(sample::upload_status(), encode_upload_status);
    check(sample::download_request(), encode_download_request);
    check(sample::download_status(), encode_download_status);
    check(sample::download_list(), encode_download_list);
    check(sample::download_answer(), encode_download_answer);
}

TEST_CASE("an encoder that fails for another reason reports that, not the buffer", "[protocol-files]")
{
    UploadStatus bad = sample::upload_status();
    bad.list_ratio = 200;
    std::array<std::byte, 4> tiny{};
    CHECK(encode_upload_status(bad, tiny).error() == Error::invalid_value);
}

// ---- Names -----------------------------------------------------------------------------

TEST_CASE("a protocol file is named for its extension, case sensitively", "[protocol-files][names]")
{
    const std::array<std::pair<std::string_view, ProtocolFileKind>, 12> names = {{
        {"HNPFMS_L.LCI", ProtocolFileKind::lci},
        {"HNPFMS_L.LCL", ProtocolFileKind::lcl},
        {"HNPFMS_L.LCS", ProtocolFileKind::lcs},
        {"HNPFMS_L.LUI", ProtocolFileKind::lui},
        {"HNPFMS_L.LUR", ProtocolFileKind::lur},
        {"HNPFMS_L.LUS", ProtocolFileKind::lus},
        {"HNPFMS_L.LND", ProtocolFileKind::lnd},
        {"HNPFMS_L.LNO", ProtocolFileKind::lno},
        {"HNPFMS_L.LNR", ProtocolFileKind::lnr},
        {"HNPFMS_L.LNS", ProtocolFileKind::lns},
        {"HNPFMS_L.LNL", ProtocolFileKind::lnl},
        {"HNPFMS_L.LNA", ProtocolFileKind::lna},
    }};
    for (const auto& [name, kind] : names)
    {
        CAPTURE(name);
        CHECK(kind_of_file_name(name) == kind);
        CHECK(name.ends_with(extension(kind)));
    }
    for (const char* name : {"HNPFMS_L.lci", "HNPFMS_L.Lci", "HNPFMS_L.LUH", "HNPFMS_L", ".LCI", "HNPFMS_L.LCIX",
                             "HNPFMS_L.LC", "", "LCI", "HNPFMS_L.LUP"})
    {
        CAPTURE(name);
        CHECK_FALSE(kind_of_file_name(name).has_value());
    }
}

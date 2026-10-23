// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/arinc665/header_file.hpp"
#include "bytes.hpp"
#include "edit.hpp"
#include "fixtures.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace adl::arinc665;
using adl::test::from_hex;
using adl::test::get_u16;
using adl::test::get_u32;
using adl::test::put_u16;
using adl::test::put_u32;

namespace
{

// The smallest header file there can be: one data file, nothing optional.
HeaderFile minimal_header()
{
    HeaderFile header;
    header.load_part_number = PartNumber{"ACM47-1234-5678"};
    header.target_hardware_ids = {"ACME"};
    DataFile file;
    file.file_name = "A.LUP";
    file.length = 4;
    file.crc = 0x1234;
    header.data_files.push_back(file);
    return header;
}

bool mentions(const Findings& findings, std::string_view clause)
{
    return std::any_of(findings.begin(), findings.end(),
                       [&](const Finding& finding) { return finding.clause == clause; });
}

} // namespace

TEST_CASE("a header file has the fixed layout of Figure C-1", "[header]")
{
    const auto bytes = encode_header_file(minimal_header());
    REQUIRE(bytes.has_value());

    CHECK(get_u32(*bytes, 0) == bytes->size() / 2); // Header File Length, in words, itself included
    CHECK(get_u16(*bytes, 2) == 0x8004);            // Load File Format Version
    CHECK(get_u16(*bytes, 3) == 0x0000);            // Part Flags
    CHECK(get_u32(*bytes, 4) == 20);                // Pointer to Load PN Length: section 1.4.4's example
}

TEST_CASE("the pointers of a header file with nothing optional say so", "[header]")
{
    const auto bytes = encode_header_file(minimal_header());
    REQUIRE(bytes.has_value());

    CHECK(get_u32(*bytes, 10) == 0); // Pointer to Number of Support Files
    CHECK(get_u32(*bytes, 12) == 0); // Pointer to User Defined Data
    CHECK(get_u32(*bytes, 14) == 0); // Pointer to Load Type Description Length
    CHECK(get_u32(*bytes, 16) == 0); // Pointer to Number of Target HW ID with Positions
    CHECK(get_u32(*bytes, 18) != 0); // Pointer to Load Check Value Length is always there
}

TEST_CASE("the download flag is written to the part flags", "[header]")
{
    auto header = minimal_header();
    header.part_flags.is_download = true;

    const auto bytes = encode_header_file(header);
    REQUIRE(bytes.has_value());
    CHECK(get_u16(*bytes, 3) == 0x0001);

    const auto decoded = decode_header_file(*bytes);
    REQUIRE(decoded.has_value());
    CHECK(decoded->part_flags.is_download);
}

TEST_CASE("a header file with nothing optional reads back as written", "[header]")
{
    const auto header = minimal_header();

    const auto decoded = decode_header_file(encode_header_file(header).value());

    REQUIRE(decoded.has_value());
    CHECK(*decoded == header);
}

TEST_CASE("a header file with every part reads back as written", "[header]")
{
    const auto header = adl::test::sample_load().header;

    const auto decoded = decode_header_file(encode_header_file(header).value());

    REQUIRE(decoded.has_value());
    CHECK(*decoded == header);
}

TEST_CASE("an odd-length text field is padded with a NUL to a whole word", "[header]")
{
    auto header = minimal_header();
    header.load_part_number = PartNumber{"ABC"}; // three characters

    const auto bytes = encode_header_file(header);
    REQUIRE(bytes.has_value());

    // Pointer to Load PN Length is word 20: the length, 'A' 'B', 'C' NUL.
    CHECK(get_u16(*bytes, 20) == 3);
    CHECK(get_u16(*bytes, 21) == 0x4142);
    CHECK(get_u16(*bytes, 22) == 0x4300);
}

TEST_CASE("the pointer of a list entry is the distance to the next, and zero in the last", "[header]")
{
    auto header = minimal_header();
    DataFile second;
    second.file_name = "BB.LUP";
    header.data_files.push_back(second);

    const auto bytes = encode_header_file(header);
    REQUIRE(bytes.has_value());
    const std::size_t numbers = get_u32(*bytes, 8); // Pointer to Number of Data Files

    CHECK(get_u16(*bytes, numbers) == 2);
    const std::size_t first_entry = numbers + 1;
    const std::size_t distance = get_u16(*bytes, first_entry);
    // "A.LUP": 1 + 3 words of name; empty PN: 1; 2 length, 1 CRC, 4 length in bytes, 1 check
    // value length; plus the pointer itself.
    CHECK(distance == 1 + 4 + 1 + 2 + 1 + 4 + 1);
    CHECK(get_u16(*bytes, first_entry + distance) == 0);
}

TEST_CASE("a data file's length is given in words and in bytes", "[header]")
{
    auto header = minimal_header();
    header.data_files[0].length = 5; // a half word at the end counts as a word

    const auto bytes = encode_header_file(header);
    REQUIRE(bytes.has_value());
    const std::size_t entry = get_u32(*bytes, 8) + 1;
    // pointer, name "A.LUP" (1 + 3), part number (1), then the length in words
    const std::size_t length = entry + 1 + 4 + 1;

    CHECK(get_u32(*bytes, length) == 3);
    CHECK(get_u16(*bytes, length + 2) == 0x1234); // the CRC
    CHECK(get_u32(*bytes, length + 3) == 0);      // the length in bytes, most significant words
    CHECK(get_u32(*bytes, length + 5) == 5);
}

TEST_CASE("a header file needs a data file and whole words of user data", "[header]")
{
    auto none = minimal_header();
    none.data_files.clear();
    CHECK(encode_header_file(none).error() == Error::invalid_value);

    auto odd = minimal_header();
    odd.user_defined_data = {std::byte{1}, std::byte{2}, std::byte{3}};
    CHECK(encode_header_file(odd).error() == Error::invalid_value);

    auto odd_check = minimal_header();
    odd_check.load_check_value = CheckValue{CheckValueType::crc16, {std::byte{1}}};
    CHECK(encode_header_file(odd_check).error() == Error::invalid_value);
}

TEST_CASE("a header file refuses what does not fit its fields", "[header]")
{
    auto header = minimal_header();
    header.load_part_number = PartNumber{std::string(70'000, 'A')};
    CHECK(encode_header_file(header).error() == Error::field_too_large);

    auto many = minimal_header();
    many.target_hardware_ids.assign(70'000, "ACME");
    CHECK(encode_header_file(many).error() == Error::field_too_large);
}

TEST_CASE("a header file is read by its pointers, so what a later version adds is skipped", "[header]")
{
    // A later format version of the load class adds fields at the expansion points
    // (section 1.4.3): here two words after the first data file's check value (Expansion
    // Point No. 6), which only the pointer to the next entry steps over.
    auto header = minimal_header();
    DataFile second;
    second.file_name = "BB.LUP";
    second.length = 2;
    header.data_files.push_back(second);

    auto bytes = encode_header_file(header).value();
    const std::size_t numbers = get_u32(bytes, 8);
    const std::size_t first_entry = numbers + 1;
    const std::size_t distance = get_u16(bytes, first_entry);
    const std::size_t after_first = first_entry + distance;

    adl::test::insert_words(bytes, after_first, 2);
    put_u16(bytes, first_entry, static_cast<std::uint16_t>(distance + 2));
    // Everything after the insertion has moved two words.
    for (const std::size_t field : {10U, 12U, 18U})
    { // support files, user data, check value
        if (const auto pointer = get_u32(bytes, field); pointer > after_first)
        {
            put_u32(bytes, field, pointer + 2);
        }
    }
    put_u32(bytes, 0, static_cast<std::uint32_t>(bytes.size() / 2));
    put_u16(bytes, 2, 0x8005); // a later version
    adl::test::refresh_header_crc(bytes);

    const auto decoded = decode_header_file(bytes);

    REQUIRE(decoded.has_value());
    CHECK(decoded->format_version == static_cast<FileFormatVersion>(0x8005));
    CHECK(decoded->data_files == header.data_files);
}

TEST_CASE("a header file that is damaged is refused for the right reason", "[header]")
{
    const auto good = adl::test::sample_load().header_bytes;
    const std::size_t words = good.size() / 2;

    SECTION("whole words")
    {
        auto bytes = good;
        bytes.pop_back();
        CHECK(decode_header_file(bytes).error() == Error::bad_length);
    }
    SECTION("nothing at all")
    {
        CHECK(decode_header_file({}).error() == Error::truncated);
    }
    SECTION("cut short")
    {
        const auto shorter = std::span<const std::byte>{good}.first(good.size() - 2);
        CHECK(decode_header_file(shorter).error() == Error::truncated);
    }
    SECTION("longer than it says")
    {
        auto bytes = good;
        bytes.push_back(std::byte{0});
        bytes.push_back(std::byte{0});
        CHECK(decode_header_file(bytes).error() == Error::bad_length);
    }
    SECTION("the format version of another class")
    {
        for (const std::uint16_t version : std::vector<std::uint16_t>{0x9004, 0xA004, 0x7004, 0x0004})
        {
            auto bytes = good;
            put_u16(bytes, 2, version);
            adl::test::refresh_header_crc(bytes);
            CHECK(decode_header_file(bytes).error() == Error::bad_version);
        }
    }
    SECTION("an earlier version, which had no pointer to the Load Check Value")
    {
        auto bytes = good;
        put_u16(bytes, 2, 0x8003);
        adl::test::refresh_header_crc(bytes);
        CHECK(decode_header_file(bytes).error() == Error::bad_version);
    }
    SECTION("a Header File CRC that is wrong")
    {
        auto bytes = good;
        bytes[2 * (words - 3)] ^= std::byte{0x01};
        CHECK(decode_header_file(bytes).error() == Error::bad_crc);
    }
    SECTION("a change to any byte before the CRC")
    {
        auto bytes = good;
        bytes[40] ^= std::byte{0x10};
        CHECK(decode_header_file(bytes).error() == Error::bad_crc);
    }
    SECTION("a pointer that points outside the body")
    {
        for (const std::size_t field : {4U, 6U, 8U, 18U})
        {
            for (const std::uint32_t value : {0U, 19U, static_cast<std::uint32_t>(words - 2)})
            {
                auto bytes = good;
                put_u32(bytes, field, value);
                adl::test::refresh_header_crc(bytes);
                CHECK(decode_header_file(bytes).error() == Error::bad_pointer);
            }
        }
        for (const std::size_t field : {10U, 12U, 14U, 16U})
        { // these may be 0, not wild
            auto bytes = good;
            put_u32(bytes, field, static_cast<std::uint32_t>(words));
            adl::test::refresh_header_crc(bytes);
            CHECK(decode_header_file(bytes).error() == Error::bad_pointer);
        }
    }
    SECTION("no data files")
    {
        auto bytes = good;
        put_u16(bytes, get_u32(bytes, 8), 0);
        adl::test::refresh_header_crc(bytes);
        CHECK(decode_header_file(bytes).error() == Error::invalid_value);
    }
    SECTION("a data file whose length in words is not its length in bytes")
    {
        auto bytes = good;
        const std::size_t entry = get_u32(bytes, 8) + 1;
        // pointer, "APP.LUP" (1 + 4 words), "ACM4F-0001-0001" (1 + 8 words), then the length
        const std::size_t length = entry + 1 + 5 + 9;
        REQUIRE(get_u32(bytes, length) == 8);
        put_u32(bytes, length, 9);
        adl::test::refresh_header_crc(bytes);
        CHECK(decode_header_file(bytes).error() == Error::bad_length);
    }
    SECTION("a last list entry that points on")
    {
        auto bytes = good;
        const std::size_t numbers = get_u32(bytes, 8);
        std::size_t entry = numbers + 1;
        entry += get_u16(bytes, entry); // the second, last, entry
        put_u16(bytes, entry, 4);
        adl::test::refresh_header_crc(bytes);
        CHECK(decode_header_file(bytes).error() == Error::bad_pointer);
    }
    SECTION("a list entry that points to itself")
    {
        auto bytes = good;
        const std::size_t first_entry = get_u32(bytes, 8) + 1;
        put_u16(bytes, first_entry, 1);
        adl::test::refresh_header_crc(bytes);
        CHECK(decode_header_file(bytes).error() == Error::bad_pointer);
    }
    SECTION("a check value of impossible length")
    {
        auto bytes = good;
        const std::size_t check = get_u32(bytes, 18);
        put_u16(bytes, check, 3);
        adl::test::refresh_header_crc(bytes);
        CHECK(decode_header_file(bytes).error() == Error::bad_length);
    }
}

TEST_CASE("a load's header file is checked against the recommendations", "[header]")
{
    SECTION("the sample has one: its second data file is five bytes")
    {
        const auto findings = check_header_file(adl::test::sample_load().header);

        REQUIRE(findings.size() == 1);
        CHECK(findings[0].clause == "2.2.3.2");
    }
    SECTION("a later format version, and no target hardware")
    {
        auto header = adl::test::sample_load().header;
        header.format_version = static_cast<FileFormatVersion>(0x8005);
        header.target_hardware_ids.clear();
        header.target_hardware_ids_with_positions.clear();

        const auto findings = check_header_file(header);
        CHECK(mentions(findings, "1.4.1"));
        CHECK(mentions(findings, "2.2.3.1.20"));
    }
    SECTION("target hardware with positions that is not in the list")
    {
        auto header = adl::test::sample_load().header;
        header.target_hardware_ids_with_positions = {TargetHardware{"ZZZZ", {"L"}}};

        CHECK(mentions(check_header_file(header), "2.2.3.1.26"));
    }
    SECTION("a part number or file name that does not follow the standard")
    {
        auto header = adl::test::sample_load().header;
        header.load_part_number = PartNumber{"nonsense"};
        header.data_files[0].file_name = "APP.LUH";
        header.support_files[0].part_number = PartNumber{"alsobad"};

        const auto findings = check_header_file(header);
        CHECK(mentions(findings, "2.1.1"));
        CHECK(mentions(findings, "3.2.2"));
    }
    SECTION("names and part numbers used twice")
    {
        auto header = adl::test::sample_load().header;
        header.data_files[1].file_name = header.data_files[0].file_name;
        header.data_files[1].part_number = header.data_files[0].part_number;

        const auto findings = check_header_file(header);
        CHECK(mentions(findings, "2.2.1"));
        CHECK(mentions(findings, "2.2.3.1.36"));
    }
}

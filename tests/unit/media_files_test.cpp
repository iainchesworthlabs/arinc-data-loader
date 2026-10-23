// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/arinc665/crc.hpp"
#include "adl/arinc665/media_files.hpp"
#include "bytes.hpp"
#include "edit.hpp"

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

bool mentions(const Findings& findings, std::string_view clause)
{
    return std::any_of(findings.begin(), findings.end(),
                       [&](const Finding& finding) { return finding.clause == clause; });
}

MediaSetIdentity identity(std::uint8_t x = 1, std::uint8_t y = 2)
{
    return MediaSetIdentity{PartNumber{"ACM-SET-0001"}, x, y};
}

LoadsList sample_loads()
{
    LoadsList list;
    list.identity = identity();
    list.loads = {
        LoadsListEntry{PartNumber{"ACM47-1234-5678"}, "ACM47.LUH", 1, {"ACME", "0A2B"}},
        LoadsListEntry{PartNumber{"ACM4F-0001-0001"}, "ACM48.LUH", 2, {"ACME"}},
        LoadsListEntry{PartNumber{"ACM4F-0002-0002"}, "ACM49.LUH", 2, {}},
    };
    return list;
}

FilesList sample_files()
{
    FilesList list;
    list.identity = identity();
    list.files = {
        FilesListEntry{"LOADS.LUM", "\\", 1, 0, std::nullopt},
        FilesListEntry{"ACM47.LUH", "\\ACM4712345678\\", 1, 0x1234,
                       CheckValue{CheckValueType::crc16, from_hex("ABCD")}},
        FilesListEntry{"APP.LUP", "\\ACM4712345678\\disk001\\", 2, 0xFFFF, std::nullopt},
    };
    return list;
}

BatchesList sample_batches()
{
    BatchesList list;
    list.identity = identity();
    list.batches = {BatchesListEntry{PartNumber{"ACM4C-0100-0200"}, "ACM47.LUB", 2},
                    BatchesListEntry{PartNumber{"ACM4D-0100-0300"}, "ACM48.LUB", 1}};
    return list;
}

} // namespace

// ---- LOADS.LUM ----

TEST_CASE("LOADS.LUM has the layout of Table 3.2.3.1", "[media-files]")
{
    const auto bytes = encode_loads_list(sample_loads()).value();

    CHECK(get_u32(bytes, 0) == bytes.size() / 2); // LOADS.LUM File Length
    CHECK(get_u16(bytes, 2) == 0xA004);           // Media File Format Version
    CHECK(get_u16(bytes, 3) == 0x0000);           // Spare
    CHECK(get_u32(bytes, 4) == 10);               // Pointer to Media Set PN Length
    CHECK(get_u32(bytes, 8) == 0);                // no User Defined Data
    // The media set PN (12 characters), then X and Y in one word, then the number of loads.
    CHECK(get_u16(bytes, 10) == 12);
    CHECK(get_u16(bytes, 17) == 0x0102); // X = 1, Y = 2
    CHECK(get_u16(bytes, 18) == 3);      // Number of Loads
    CHECK(get_u32(bytes, 6) == 18);      // Pointer to Number of Loads
}

TEST_CASE("LOADS.LUM reads back as written", "[media-files]")
{
    SECTION("with loads")
    {
        const auto decoded = decode_loads_list(encode_loads_list(sample_loads()).value());
        REQUIRE(decoded.has_value());
        CHECK(*decoded == sample_loads());
    }
    SECTION("with no loads")
    {
        LoadsList list;
        list.identity = identity();
        const auto decoded = decode_loads_list(encode_loads_list(list).value());
        REQUIRE(decoded.has_value());
        CHECK(*decoded == list);
    }
    SECTION("with user defined data")
    {
        auto list = sample_loads();
        list.user_defined_data = from_hex("00 01 02 03 04 05");
        const auto bytes = encode_loads_list(list).value();
        CHECK(get_u32(bytes, 8) != 0);
        const auto decoded = decode_loads_list(bytes);
        REQUIRE(decoded.has_value());
        CHECK(*decoded == list);
    }
}

TEST_CASE("the CRC of the whole of a LOADS.LUM is zero", "[media-files]")
{
    // Section 3.2.3.2.20's commentary: it is how FILES.LUM shows the file is valid.
    const auto bytes = encode_loads_list(sample_loads()).value();

    CHECK(crc_of<Crc16>(bytes) == 0);
}

TEST_CASE("a list is refused for a media sequence number the set cannot have", "[media-files]")
{
    for (const auto& [x, y] : {std::pair<int, int>{0, 1}, {0, 0}, {3, 2}, {255, 254}})
    {
        auto loads = sample_loads();
        loads.identity = identity(static_cast<std::uint8_t>(x), static_cast<std::uint8_t>(y));
        CHECK(encode_loads_list(loads).error() == Error::invalid_value);

        auto files = sample_files();
        files.identity = loads.identity;
        CHECK(encode_files_list(files).error() == Error::invalid_value);

        auto batches = sample_batches();
        batches.identity = loads.identity;
        CHECK(encode_batches_list(batches).error() == Error::invalid_value);
    }
    // The largest set there can be.
    auto largest = sample_loads();
    largest.identity = identity(255, 255);
    CHECK(encode_loads_list(largest).has_value());
}

TEST_CASE("a list is read with the sequence number it carries, and not one that is wrong", "[media-files]")
{
    auto bytes = encode_loads_list(sample_loads()).value();
    put_u16(bytes, 17, 0x0302); // X = 3 of Y = 2
    adl::test::refresh_final_crc(bytes);

    CHECK(decode_loads_list(bytes).error() == Error::invalid_value);

    put_u16(bytes, 17, 0x0002); // X = 0
    adl::test::refresh_final_crc(bytes);
    CHECK(decode_loads_list(bytes).error() == Error::invalid_value);
}

TEST_CASE("user defined data must be whole words", "[media-files]")
{
    auto loads = sample_loads();
    loads.user_defined_data = {std::byte{1}};
    CHECK(encode_loads_list(loads).error() == Error::invalid_value);

    auto files = sample_files();
    files.user_defined_data = {std::byte{1}};
    CHECK(encode_files_list(files).error() == Error::invalid_value);

    auto batches = sample_batches();
    batches.user_defined_data = {std::byte{1}};
    CHECK(encode_batches_list(batches).error() == Error::invalid_value);
}

// ---- FILES.LUM ----

TEST_CASE("FILES.LUM has the layout of Table 3.2.3.2", "[media-files]")
{
    const auto bytes = encode_files_list(sample_files()).value();

    CHECK(get_u32(bytes, 0) == bytes.size() / 2);
    CHECK(get_u16(bytes, 2) == 0xA004);
    CHECK(get_u32(bytes, 4) == 12);                   // Pointer to Media Set PN Length
    CHECK(get_u32(bytes, 8) == 0);                    // Pointer to User Defined Data
    CHECK(get_u32(bytes, 10) > get_u32(bytes, 6));    // Pointer to FILES.LUM File Check Value Length
    CHECK(get_u16(bytes, bytes.size() / 2 - 2) == 0); // no check value: the length word, then the CRC
}

TEST_CASE("FILES.LUM reads back as written", "[media-files]")
{
    SECTION("with files and a check value of its own")
    {
        auto list = sample_files();
        list.check_value = CheckValue{CheckValueType::sha1, std::vector<std::byte>(20, std::byte{0x5A})};
        list.user_defined_data = from_hex("11 22");
        const auto decoded = decode_files_list(encode_files_list(list).value());
        REQUIRE(decoded.has_value());
        CHECK(*decoded == list);
    }
    SECTION("with no check value")
    {
        const auto decoded = decode_files_list(encode_files_list(sample_files()).value());
        REQUIRE(decoded.has_value());
        CHECK(*decoded == sample_files());
        CHECK_FALSE(decoded->check_value.has_value());
    }
}

TEST_CASE("the file name and the pathname are both in a FILES.LUM entry", "[media-files]")
{
    const auto decoded = decode_files_list(encode_files_list(sample_files()).value());

    REQUIRE(decoded.has_value());
    CHECK(decoded->files[2].file_name == "APP.LUP");
    CHECK(decoded->files[2].pathname == "\\ACM4712345678\\disk001\\");
    CHECK(decoded->files[2].member_sequence_number == 2);
    CHECK(decoded->files[2].crc == 0xFFFF);
}

TEST_CASE("the offset of the FILES.LUM check value is where its pointer says", "[media-files]")
{
    auto list = sample_files();
    list.check_value = CheckValue{CheckValueType::crc32, from_hex("01020304")};
    const auto bytes = encode_files_list(list).value();

    const auto offset = files_list_check_value_offset(bytes);

    REQUIRE(offset.has_value());
    CHECK(*offset == get_u32(bytes, 10) * 2);
    CHECK(get_u16(bytes, *offset / 2) == 8); // length 4 + 4
    CHECK(get_u16(bytes, *offset / 2 + 1) == 3);

    CHECK(files_list_check_value_offset({}).error() == Error::truncated);
}

// ---- BATCHES.LUM ----

TEST_CASE("BATCHES.LUM has the layout of Table 3.2.3.3-1", "[media-files]")
{
    const auto bytes = encode_batches_list(sample_batches()).value();

    CHECK(get_u32(bytes, 0) == bytes.size() / 2);
    CHECK(get_u16(bytes, 2) == 0xA004);
    CHECK(get_u32(bytes, 4) == 10);
    CHECK(get_u16(bytes, 17) == 0x0102);
    CHECK(get_u16(bytes, 18) == 2); // Number of Batches
    CHECK(crc_of<Crc16>(bytes) == 0);
}

TEST_CASE("BATCHES.LUM reads back as written", "[media-files]")
{
    auto list = sample_batches();
    list.user_defined_data = from_hex("AA BB CC DD");

    const auto decoded = decode_batches_list(encode_batches_list(list).value());

    REQUIRE(decoded.has_value());
    CHECK(*decoded == list);
}

// ---- Damage, for every list ----

namespace
{

struct ListKind
{
    const char* name;
    std::vector<std::byte> good;
    std::size_t first_pointer; // the word of the pointer to the entries
    bool (*decodes)(std::span<const std::byte>);
    Error (*error)(std::span<const std::byte>);
};

bool loads_decodes(std::span<const std::byte> bytes)
{
    return decode_loads_list(bytes).has_value();
}
bool files_decodes(std::span<const std::byte> bytes)
{
    return decode_files_list(bytes).has_value();
}
bool batches_decodes(std::span<const std::byte> bytes)
{
    return decode_batches_list(bytes).has_value();
}
Error loads_error(std::span<const std::byte> bytes)
{
    return decode_loads_list(bytes).error();
}
Error files_error(std::span<const std::byte> bytes)
{
    return decode_files_list(bytes).error();
}
Error batches_error(std::span<const std::byte> bytes)
{
    return decode_batches_list(bytes).error();
}

std::vector<ListKind> list_kinds()
{
    return {
        {"LOADS.LUM", encode_loads_list(sample_loads()).value(), 6, loads_decodes, loads_error},
        {"FILES.LUM", encode_files_list(sample_files()).value(), 6, files_decodes, files_error},
        {"BATCHES.LUM", encode_batches_list(sample_batches()).value(), 6, batches_decodes, batches_error},
    };
}

} // namespace

TEST_CASE("a list file that is damaged is refused for the right reason", "[media-files]")
{
    for (const ListKind& kind : list_kinds())
    {
        CAPTURE(kind.name);
        const std::size_t words = kind.good.size() / 2;

        SECTION("not whole words")
        {
            auto bytes = kind.good;
            bytes.pop_back();
            CHECK(kind.error(bytes) == Error::bad_length);
        }
        SECTION("nothing at all")
        {
            CHECK(kind.error({}) == Error::truncated);
        }
        SECTION("cut short")
        {
            CHECK(kind.error(std::span<const std::byte>{kind.good}.first(kind.good.size() - 2)) == Error::truncated);
        }
        SECTION("a length that is not the file's")
        {
            auto bytes = kind.good;
            put_u32(bytes, 0, static_cast<std::uint32_t>(words + 1));
            CHECK(kind.error(bytes) == Error::truncated);
            put_u32(bytes, 0, static_cast<std::uint32_t>(words - 1));
            CHECK(kind.error(bytes) == Error::bad_length);
        }
        SECTION("the format version of another class")
        {
            for (const std::uint16_t version : std::vector<std::uint16_t>{0x8004, 0x9004, 0xA003, 0x0000})
            {
                auto bytes = kind.good;
                put_u16(bytes, 2, version);
                adl::test::refresh_final_crc(bytes);
                CHECK(kind.error(bytes) == Error::bad_version);
            }
        }
        SECTION("a CRC that is wrong")
        {
            auto bytes = kind.good;
            bytes.back() ^= std::byte{0x01};
            CHECK(kind.error(bytes) == Error::bad_crc);
        }
        SECTION("a changed byte")
        {
            auto bytes = kind.good;
            bytes[30] ^= std::byte{0x01};
            CHECK(kind.error(bytes) == Error::bad_crc);
        }
        SECTION("a pointer outside the body")
        {
            for (const std::uint32_t value : {0U, 5U, static_cast<std::uint32_t>(words)})
            {
                auto bytes = kind.good;
                put_u32(bytes, 4, value); // Pointer to Media Set PN Length
                adl::test::refresh_final_crc(bytes);
                CHECK(kind.error(bytes) == Error::bad_pointer);
            }
        }
        SECTION("a last entry that points on")
        {
            auto bytes = kind.good;
            const std::size_t entries = get_u32(bytes, kind.first_pointer);
            const std::size_t count = get_u16(bytes, entries);
            std::size_t entry = entries + 1;
            for (std::size_t i = 1; i < count; ++i)
            {
                entry += get_u16(bytes, entry);
            }
            put_u16(bytes, entry, 2);
            adl::test::refresh_final_crc(bytes);
            CHECK(kind.error(bytes) == Error::bad_pointer);
        }
    }
}

TEST_CASE("a later media format version is read by its pointers", "[media-files]")
{
    for (const ListKind& kind : list_kinds())
    {
        CAPTURE(kind.name);
        auto bytes = kind.good;
        put_u16(bytes, 2, 0xA005);
        adl::test::refresh_final_crc(bytes);

        CHECK(kind.decodes(bytes));
    }
}

TEST_CASE("a FILES.LUM check value that has been cut away is refused", "[media-files]")
{
    auto bytes = encode_files_list(sample_files()).value();
    // Point the check value at the CRC word, so there is no room for the check value.
    put_u32(bytes, 10, static_cast<std::uint32_t>(bytes.size() / 2 - 1));
    adl::test::refresh_final_crc(bytes);

    CHECK(decode_files_list(bytes).error() == Error::bad_pointer);
}

// ---- Findings ----

TEST_CASE("the list files are checked against the recommendations", "[media-files]")
{
    CHECK(check_loads_list(sample_loads()).empty());
    CHECK(check_batches_list(sample_batches()).empty());
    CHECK(check_files_list(sample_files()).empty());

    SECTION("a media set part number and sequence number")
    {
        auto loads = sample_loads();
        loads.identity.media_set_part_number = PartNumber{"TOO-LONG-FOR-A-MEDIA-SET"};
        loads.identity.media_sequence_number = 5;
        const auto findings = check_loads_list(loads);
        CHECK(mentions(findings, "3.1"));
        CHECK(mentions(findings, "3.2.3.1.10"));
    }
    SECTION("a load listed twice, on a member that is not there, with a bad name")
    {
        auto loads = sample_loads();
        loads.loads.push_back(loads.loads[0]);
        loads.loads[1].member_sequence_number = 9;
        loads.loads[1].header_file_name = "ACM48.LUP";
        const auto findings = check_loads_list(loads);
        CHECK(mentions(findings, "3.2.3.1.12"));
        CHECK(mentions(findings, "3.2.3.1.18"));
        CHECK(mentions(findings, "2.2.2.1"));
    }
    SECTION("a file that FILES.LUM should not list, and a bad pathname")
    {
        auto files = sample_files();
        files.files.push_back(FilesListEntry{"FILES.LUM", "\\", 1, 0, std::nullopt});
        files.files.push_back(FilesListEntry{"X.BIN", "no-leading-backslash", 7, 0, std::nullopt});
        const auto findings = check_files_list(files);
        CHECK(mentions(findings, "3.2.3.2.13"));
        CHECK(mentions(findings, "3.2.3.2.18"));
        CHECK(mentions(findings, "3.2.3.2.19"));
    }
    SECTION("a batch with a bad name and member")
    {
        auto batches = sample_batches();
        batches.batches[0].batch_file_name = "ACM47.LUH";
        batches.batches[1].member_sequence_number = 9;
        const auto findings = check_batches_list(batches);
        CHECK(mentions(findings, "2.3.1"));
        CHECK(mentions(findings, "3.2.3.3.18"));
    }
    SECTION("the wrong format version")
    {
        auto loads = sample_loads();
        loads.format_version = static_cast<FileFormatVersion>(0xA005);
        CHECK(mentions(check_loads_list(loads), "1.4.1"));
    }
}

#include <algorithm>
#include <catch2/catch_test_macros.hpp>

#include "adl/arinc665/batch_file.hpp"
#include "edit.hpp"
#include "fixtures.hpp"

using namespace adl::arinc665;
using adl::test::get_u16;
using adl::test::get_u32;
using adl::test::put_u16;
using adl::test::put_u32;

namespace {

bool mentions(const Findings& findings, std::string_view clause) {
    return std::any_of(findings.begin(), findings.end(),
                       [&](const Finding& finding) { return finding.clause == clause; });
}

}  // namespace

TEST_CASE("a batch file has the layout of Table 2.3.1-1", "[batch]") {
    const auto bytes = encode_batch_file(adl::test::sample_batch());
    REQUIRE(bytes.has_value());

    CHECK(get_u32(*bytes, 0) == bytes->size() / 2);  // Batch File Length
    CHECK(get_u16(*bytes, 2) == 0x9004);             // Batch File Format Version
    CHECK(get_u16(*bytes, 3) == 0x0000);             // Spare
    CHECK(get_u32(*bytes, 4) == 8);                  // Pointer to Batch File PN Length
    CHECK(get_u32(*bytes, 6) > 8);    // Pointer to Number of Target HW ID Load-List Blocks
    CHECK(get_u16(*bytes, 8) == 15);  // Batch File PN Length
}

TEST_CASE("a batch file reads back as written", "[batch]") {
    const auto batch = adl::test::sample_batch();

    const auto decoded = decode_batch_file(encode_batch_file(batch).value());

    REQUIRE(decoded.has_value());
    CHECK(*decoded == batch);
}

TEST_CASE("a batch with no comment or no target reads back as written", "[batch]") {
    BatchFile batch;
    batch.batch_part_number = PartNumber{"ACM4C-0100-0200"};

    SECTION("no targets, no comment") {
        const auto decoded = decode_batch_file(encode_batch_file(batch).value());
        REQUIRE(decoded.has_value());
        CHECK(*decoded == batch);
        CHECK(decoded->comment.empty());
    }
    SECTION("a target with no loads") {
        batch.targets.push_back(BatchTarget{"ACME-L", {}});
        const auto decoded = decode_batch_file(encode_batch_file(batch).value());
        REQUIRE(decoded.has_value());
        CHECK(*decoded == batch);
    }
}

TEST_CASE("an empty comment is a single zero word", "[batch]") {
    BatchFile batch;
    batch.batch_part_number = PartNumber{"ABCD"};  // even length: no padding
    const auto bytes = encode_batch_file(batch).value();

    // length 2 words, version, spare, 2 pointers = 8; PN 1 + 2; comment length 1; count 1; CRC 1
    CHECK(bytes.size() / 2 == 8 + 3 + 1 + 1 + 1);
    CHECK(get_u16(bytes, 11) == 0);  // Comment Length
}

TEST_CASE("the pointer of a load-list block is the distance to the next, and zero in the last",
          "[batch]") {
    const auto bytes = encode_batch_file(adl::test::sample_batch()).value();
    const std::size_t count = get_u32(bytes, 6);

    CHECK(get_u16(bytes, count) == 2);
    const std::size_t first_block = count + 1;
    const std::size_t distance = get_u16(bytes, first_block);
    // pointer 1; "ACME-L" 1 + 3; number of loads 1; "ACM47.LUH" 1 + 5; load PN (15 chars) 1 + 8
    CHECK(distance == 1 + 4 + 1 + 6 + 9);
    // the second, last, block's pointer
    CHECK(get_u16(bytes, first_block + distance) == 0);
}

TEST_CASE("a batch file that is damaged is refused for the right reason", "[batch]") {
    const auto good = encode_batch_file(adl::test::sample_batch()).value();
    const std::size_t words = good.size() / 2;

    SECTION("not whole words") {
        auto bytes = good;
        bytes.pop_back();
        CHECK(decode_batch_file(bytes).error() == Error::bad_length);
    }
    SECTION("nothing at all") {
        CHECK(decode_batch_file({}).error() == Error::truncated);
    }
    SECTION("cut short") {
        CHECK(decode_batch_file(std::span<const std::byte>{good}.first(good.size() - 2)).error() ==
              Error::truncated);
    }
    SECTION("a length that is not the file's") {
        auto bytes = good;
        put_u32(bytes, 0, static_cast<std::uint32_t>(words - 1));
        CHECK(decode_batch_file(bytes).error() == Error::bad_length);
    }
    SECTION("another class of format version") {
        for (const std::uint16_t version :
             std::vector<std::uint16_t>{0x8004, 0xA004, 0x9003, 0x0000}) {
            auto bytes = good;
            put_u16(bytes, 2, version);
            adl::test::refresh_final_crc(bytes);
            CHECK(decode_batch_file(bytes).error() == Error::bad_version);
        }
    }
    SECTION("a wrong CRC") {
        auto bytes = good;
        bytes.back() ^= std::byte{0x01};
        CHECK(decode_batch_file(bytes).error() == Error::bad_crc);
    }
    SECTION("any changed byte") {
        auto bytes = good;
        bytes[20] ^= std::byte{0x01};
        CHECK(decode_batch_file(bytes).error() == Error::bad_crc);
    }
    SECTION("a pointer that points outside the file") {
        for (const std::size_t field : {4U, 6U}) {
            for (const std::uint32_t value : {0U, 7U, static_cast<std::uint32_t>(words)}) {
                auto bytes = good;
                put_u32(bytes, field, value);
                adl::test::refresh_final_crc(bytes);
                CHECK(decode_batch_file(bytes).error() == Error::bad_pointer);
            }
        }
    }
    SECTION("a last block that points on") {
        auto bytes = good;
        const std::size_t first_block = get_u32(bytes, 6) + 1;
        const std::size_t last_block = first_block + get_u16(bytes, first_block);
        put_u16(bytes, last_block, 3);
        adl::test::refresh_final_crc(bytes);
        CHECK(decode_batch_file(bytes).error() == Error::bad_pointer);
    }
    SECTION("a block that does not point on") {
        auto bytes = good;
        const std::size_t first_block = get_u32(bytes, 6) + 1;
        put_u16(bytes, first_block, 0);
        adl::test::refresh_final_crc(bytes);
        CHECK(decode_batch_file(bytes).error() == Error::bad_pointer);
    }
}

TEST_CASE("a later batch format version is read by its pointers", "[batch]") {
    auto bytes = encode_batch_file(adl::test::sample_batch()).value();
    put_u16(bytes, 2, 0x9005);
    adl::test::refresh_final_crc(bytes);

    const auto decoded = decode_batch_file(bytes);

    REQUIRE(decoded.has_value());
    CHECK(decoded->format_version == static_cast<FileFormatVersion>(0x9005));
    CHECK(decoded->targets == adl::test::sample_batch().targets);
}

TEST_CASE("a batch file is checked against the recommendations", "[batch]") {
    CHECK(check_batch_file(adl::test::sample_batch()).empty());

    auto batch = adl::test::sample_batch();
    batch.format_version = static_cast<FileFormatVersion>(0x9005);
    batch.batch_part_number = PartNumber{"nonsense"};
    batch.targets[0].loads[0].header_file_name = "ACM47.LUP";
    batch.targets[0].loads[0].load_part_number = PartNumber{"nonsense"};

    const auto findings = check_batch_file(batch);
    CHECK(mentions(findings, "1.4.1"));
    CHECK(mentions(findings, "2.1.1"));
    CHECK(mentions(findings, "2.2.2.1"));
}

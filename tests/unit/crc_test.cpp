#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <span>

#include "adl/arinc665/crc.hpp"
#include "bytes.hpp"

namespace {

using adl::arinc665::Crc16;
using adl::arinc665::Crc32;
using adl::arinc665::Crc8;
using adl::arinc665::crc_of;
using adl::test::ascending;
using adl::test::bytes_of;
using adl::test::repeated;

// The 256-byte test file of ARINC 665-3 Tables 4.3-1 to 4.3-3: 00 01 02 ... FF.
constexpr std::array<std::byte, 256> ascending_bytes() {
    std::array<std::byte, 256> bytes{};
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        bytes[i] = static_cast<std::byte>(i);
    }
    return bytes;
}

constexpr auto test_file = ascending_bytes();

// The check values in the standard's tables, worked out at compile time too.
static_assert(crc_of<Crc8>(test_file) == 0x00);
static_assert(crc_of<Crc16>(test_file) == 0x3FBD);
static_assert(crc_of<Crc32>(test_file) == 0xB6B5EE95U);

}  // namespace

TEST_CASE("the CRCs match the check values in ARINC 665-3 section 4.3", "[crc]") {
    CHECK(crc_of<Crc8>(test_file) == 0x00);
    CHECK(crc_of<Crc16>(test_file) == 0x3FBD);
    CHECK(crc_of<Crc32>(test_file) == 0xB6B5EE95U);
}

// Appendix L lists the CRC of each reference file. These are the ones whose
// content the table describes exactly (the others are random values, posted
// separately by the publisher).
TEST_CASE("the CRCs match the Appendix L reference files", "[crc]") {
    const auto t01 = std::vector<std::byte>{};        // empty
    const auto t02 = repeated(std::byte{0xFF}, 128);  // all 0xFF
    const auto t03 = repeated(std::byte{0x00}, 100);  // all 0x00
    const std::array<std::byte, 2> aa55{std::byte{0xAA}, std::byte{0x55}};
    const auto t04 = repeated(aa55, 128);  // 128 x 0xAA55
    const auto t11 = ascending();          // 0x00 to 0xFF
    const auto t12 = bytes_of("ACM12345678");
    const auto t13 = bytes_of("ABCDEFGHIJKLMNO");

    SECTION("Table L-2.1, 8-bit") {
        CHECK(crc_of<Crc8>(t01) == 0x00);
        CHECK(crc_of<Crc8>(t02) == 0x00);
        CHECK(crc_of<Crc8>(t03) == 0x00);
        CHECK(crc_of<Crc8>(t12) == 0x47);
        CHECK(crc_of<Crc8>(t13) == 0x40);
    }
    SECTION("Table L-2.2, 16-bit") {
        CHECK(crc_of<Crc16>(t01) == 0xFFFF);
        CHECK(crc_of<Crc16>(t02) == 0x1DA3);
        CHECK(crc_of<Crc16>(t03) == 0x4634);
        CHECK(crc_of<Crc16>(t04) == 0x1D7E);
        CHECK(crc_of<Crc16>(t11) == 0x3FBD);
    }
    SECTION("Table L-2.3, 32-bit") {
        CHECK(crc_of<Crc32>(t01) == 0x00000000U);
        CHECK(crc_of<Crc32>(t02) == 0x322AB4A6U);
        CHECK(crc_of<Crc32>(t03) == 0x53631199U);
        CHECK(crc_of<Crc32>(t04) == 0xC2F270BCU);
        CHECK(crc_of<Crc32>(t11) == 0xB6B5EE95U);
    }
}

TEST_CASE("the CRCs match the published results for the standard test string", "[crc]") {
    // "123456789" is the conventional test input. CRC-8 with generator 0x01 is
    // the exclusive-or of the bytes. The 16-bit CRC is CRC-16/IBM-3740 and the
    // 32-bit one CRC-32/BZIP2 in the Rocksoft catalogue: same parameters.
    const auto text = bytes_of("123456789");
    CHECK(crc_of<Crc8>(text) == 0x31);
    CHECK(crc_of<Crc16>(text) == 0x29B1);
    CHECK(crc_of<Crc32>(text) == 0xFC891918U);
}

TEST_CASE("a CRC of no data is the initial value with the final exclusive-or applied", "[crc]") {
    CHECK(Crc8{}.value() == 0x00);
    CHECK(Crc16{}.value() == 0xFFFF);
    CHECK(Crc32{}.value() == 0x00000000U);
}

TEST_CASE("feeding data in pieces gives the same CRC as feeding it whole", "[crc]") {
    const std::span<const std::byte> whole{test_file};

    Crc32 pieces;
    pieces.update(whole.first(1));
    pieces.update(whole.subspan(1, 100));
    pieces.update(whole.subspan(101));

    CHECK(pieces.value() == crc_of<Crc32>(whole));
}

TEST_CASE("reset starts a CRC again", "[crc]") {
    Crc16 crc;
    crc.update(bytes_of("something else"));
    crc.reset();
    crc.update(bytes_of("123456789"));

    CHECK(crc.value() == 0x29B1);
}

// A file that carries its own 16-bit CRC at the end, most significant byte
// first, has a CRC of zero over the whole of it. Section 3.2.3.2.20 relies on
// this for LOADS.LUM and BATCHES.LUM.
TEST_CASE("a 16-bit CRC appended big-endian makes the CRC of the whole zero", "[crc]") {
    auto data = bytes_of("some file contents");
    const auto crc = crc_of<Crc16>(data);
    data.push_back(static_cast<std::byte>(crc >> 8));
    data.push_back(static_cast<std::byte>(crc & 0xFFU));

    CHECK(crc_of<Crc16>(data) == 0x0000);
}

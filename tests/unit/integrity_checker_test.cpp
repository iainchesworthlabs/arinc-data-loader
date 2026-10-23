// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/arinc665/check_value.hpp"
#include "adl/arinc665/integrity_checker.hpp"
#include "bytes.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>

namespace
{

using adl::arinc665::CheckValue;
using adl::arinc665::CheckValueType;
using adl::arinc665::Error;
using adl::arinc665::IntegrityChecker;
using adl::test::bytes_of;
using adl::test::from_hex;

constexpr auto text = "123456789";

} // namespace

// Section 5.1: a check value is stored big-endian in whole 16-bit words, and a
// value of an odd number of bytes is preceded by a zero byte.
TEST_CASE("each check value type is stored in the form section 5.1 gives", "[integrity]")
{
    const auto data = bytes_of(text);

    SECTION("an 8-bit CRC is a zero byte then the CRC")
    {
        const auto value = adl::arinc665::compute_check_value(CheckValueType::crc8, data);
        REQUIRE(value.has_value());
        CHECK(value->type == CheckValueType::crc8);
        CHECK(value->bytes == from_hex("00 31"));
    }
    SECTION("a 16-bit CRC is two bytes, most significant first")
    {
        const auto value = adl::arinc665::compute_check_value(CheckValueType::crc16, data);
        REQUIRE(value.has_value());
        CHECK(value->bytes == from_hex("29 B1"));
    }
    SECTION("a 32-bit CRC is four bytes, most significant first")
    {
        const auto value = adl::arinc665::compute_check_value(CheckValueType::crc32, data);
        REQUIRE(value.has_value());
        CHECK(value->bytes == from_hex("FC 89 19 18"));
    }
    SECTION("MD5 is the 16-byte digest")
    {
        const auto value = adl::arinc665::compute_check_value(CheckValueType::md5, bytes_of("abc"));
        REQUIRE(value.has_value());
        CHECK(value->bytes == from_hex("90 01 50 98 3c d2 4f b0 d6 96 3f 7d 28 e1 7f 72"));
    }
    SECTION("SHA-1 is the 20-byte digest")
    {
        const auto value = adl::arinc665::compute_check_value(CheckValueType::sha1, bytes_of("abc"));
        REQUIRE(value.has_value());
        CHECK(value->bytes == from_hex("a9 99 3e 36 47 06 81 6a ba 3e 25 71 78 50 c2 6c 9c d0 d8 9d"));
    }
}

TEST_CASE("the stored length of each check value is the one in Table 5.1", "[integrity]")
{
    for (const auto type : {CheckValueType::crc8, CheckValueType::crc16, CheckValueType::crc32, CheckValueType::md5,
                            CheckValueType::sha1})
    {
        const auto value = adl::arinc665::compute_check_value(type, bytes_of(text));
        REQUIRE(value.has_value());
        CHECK(value->bytes.size() == adl::arinc665::check_value_length(type));
    }
}

TEST_CASE("a checker reports its type and its Table 5.1 title", "[integrity]")
{
    const auto checker = IntegrityChecker::make(CheckValueType::crc16);

    REQUIRE(checker.has_value());
    CHECK(checker->type() == CheckValueType::crc16);
    CHECK(checker->title() == "16-bit CRC");
}

TEST_CASE("feeding a checker in pieces gives the same value as feeding it whole", "[integrity]")
{
    const auto data = bytes_of("the quick brown fox jumps over the lazy dog");
    const std::span<const std::byte> all{data};

    for (const auto type : {CheckValueType::crc8, CheckValueType::crc16, CheckValueType::crc32, CheckValueType::md5,
                            CheckValueType::sha1})
    {
        auto pieces = IntegrityChecker::make(type);
        REQUIRE(pieces.has_value());
        pieces->update(all.first(7));
        pieces->update(all.subspan(7));

        const auto whole = adl::arinc665::compute_check_value(type, all);
        REQUIRE(whole.has_value());
        CHECK(pieces->check_value() == *whole);
    }
}

TEST_CASE("reset returns a checker to its initial state", "[integrity]")
{
    auto checker = IntegrityChecker::make(CheckValueType::sha1);
    REQUIRE(checker.has_value());
    const auto fresh = checker->check_value();

    checker->update(bytes_of(text));
    REQUIRE(checker->check_value() != fresh);

    checker->reset();
    CHECK(checker->check_value() == fresh);
}

TEST_CASE("there is no checker for none or for a type the standard has not defined", "[integrity]")
{
    for (const auto type : {CheckValueType::none, static_cast<CheckValueType>(6), static_cast<CheckValueType>(0x00FF)})
    {
        const auto checker = IntegrityChecker::make(type);

        REQUIRE_FALSE(checker.has_value());
        CHECK(checker.error() == Error::unsupported_check_type);
    }
}

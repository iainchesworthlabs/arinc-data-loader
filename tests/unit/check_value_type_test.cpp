#include <catch2/catch_test_macros.hpp>
#include <cstdint>

#include "adl/arinc665/check_value_type.hpp"
#include "adl/arinc665/error.hpp"

using adl::arinc665::CheckValueType;

TEST_CASE("the check value types have the values Table 5.1 gives them", "[check-value]") {
    CHECK(static_cast<std::uint16_t>(CheckValueType::none) == 0);
    CHECK(static_cast<std::uint16_t>(CheckValueType::crc8) == 1);
    CHECK(static_cast<std::uint16_t>(CheckValueType::crc16) == 2);
    CHECK(static_cast<std::uint16_t>(CheckValueType::crc32) == 3);
    CHECK(static_cast<std::uint16_t>(CheckValueType::md5) == 4);
    CHECK(static_cast<std::uint16_t>(CheckValueType::sha1) == 5);
}

TEST_CASE("the check value lengths are the ones in Table 5.1", "[check-value]") {
    CHECK(adl::arinc665::check_value_length(CheckValueType::none) == 0);
    CHECK(adl::arinc665::check_value_length(CheckValueType::crc8) == 2);
    CHECK(adl::arinc665::check_value_length(CheckValueType::crc16) == 2);
    CHECK(adl::arinc665::check_value_length(CheckValueType::crc32) == 4);
    CHECK(adl::arinc665::check_value_length(CheckValueType::md5) == 16);
    CHECK(adl::arinc665::check_value_length(CheckValueType::sha1) == 20);
}

TEST_CASE("the check value types and errors can be named", "[check-value]") {
    CHECK(adl::arinc665::to_string(CheckValueType::crc32) == "32-bit CRC");
    CHECK(adl::arinc665::to_string(CheckValueType::sha1) == "SHA-1");
    CHECK(adl::arinc665::to_string(adl::arinc665::Error::bad_crc) == "CRC mismatch");
    CHECK(adl::arinc665::to_string(adl::arinc665::Error::unsupported_check_type) ==
          "unsupported check value type");
}

// SPDX-License-Identifier: GPL-3.0-or-later
// The words and the default names of a plan.

#include "adl/set_maker/plan.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <vector>

using namespace adl::set_maker;
using adl::arinc665::CheckValueType;

TEST_CASE("plan: the names a file gets when none is given", "[plan]")
{
    CHECK(default_header_file_name("ACM47-1234-5678") == "ACM.LUH");
    CHECK(default_batch_file_name("ACM4C-0100-0200") == "ACM.LUB");
    CHECK(default_header_file_name("AB") == "AB.LUH"); // not a part number: nothing to cut
    CHECK(part_root_name("ACM47-1234-5678") == "ACM4712345678");
    CHECK(part_root_directory("ACM47-1234-5678") == "\\ACM4712345678\\");
}

TEST_CASE("plan: a batch gets each target once, with the loads that name it, in order", "[plan]")
{
    const auto load = [](const char* part_number, std::vector<std::string> ids)
    {
        PlannedLoad planned;
        planned.part_number = part_number;
        planned.target_hardware_ids = std::move(ids);
        return planned;
    };
    const std::vector<PlannedLoad> loads = {load("ACM47-1234-5678", {"ACME", "ACMEFMS"}),
                                            load("ACM4C-0100-0200", {"ACMEFMS", "ACME", "ACME"}),
                                            load("ACM8E-0100-0300", {})};
    const auto targets = derive_batch_targets(loads);
    REQUIRE(targets.size() == 2);
    CHECK(targets[0].id == "ACME");
    CHECK(targets[0].load_part_numbers == std::vector<std::string>{"ACM47-1234-5678", "ACM4C-0100-0200"});
    CHECK(targets[1].id == "ACMEFMS");
    CHECK(targets[1].load_part_numbers == std::vector<std::string>{"ACM47-1234-5678", "ACM4C-0100-0200"});
    CHECK(derive_batch_targets({}).empty());
}

TEST_CASE("plan: the load types of the window", "[plan]")
{
    REQUIRE(load_type_codes().size() == 4);
    CHECK(load_type_from_code("OPS")->description == "Operational software");
    CHECK(load_type_from_code("OPS")->id == 1);
    CHECK(load_type_from_code("NDB")->id == 2);
    CHECK(load_type_from_code("ADB")->id == 3);
    CHECK(load_type_from_code("TST")->description == "Test software");
    CHECK(load_type_from_code("TST")->id == 4);
    CHECK_FALSE(load_type_from_code("ops").has_value());
    CHECK_FALSE(load_type_from_code("").has_value());
}

TEST_CASE("plan: check value types as words", "[plan]")
{
    for (const CheckValueType type : {CheckValueType::none, CheckValueType::crc8, CheckValueType::crc16,
                                      CheckValueType::crc32, CheckValueType::md5, CheckValueType::sha1})
    {
        CHECK(check_type_from_word(check_type_word(type)) == type);
    }
    CHECK(check_type_word(CheckValueType::crc8) == "crc8");
    CHECK(check_type_word(CheckValueType::sha1) == "sha1");
    CHECK(check_type_word(static_cast<CheckValueType>(9)) == "type_9");
    CHECK_FALSE(check_type_from_word("type_9").has_value());
    CHECK_FALSE(check_type_from_word("SHA1").has_value());
    CHECK_FALSE(check_type_from_word("").has_value());
}

TEST_CASE("plan: hexadecimal is upper case and whole bytes", "[plan]")
{
    const std::vector<std::byte> bytes = {std::byte{0x00}, std::byte{0x0A}, std::byte{0xFF}};
    CHECK(hex_of(bytes) == "000AFF");
    CHECK(hex_of({}).empty());
    CHECK(hex4_of(0x0000) == "0000");
    CHECK(hex4_of(0x1a2b) == "1A2B");
    CHECK(hex4_of(0xFFFF) == "FFFF");
}

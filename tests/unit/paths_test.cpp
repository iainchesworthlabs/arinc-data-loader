// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/platform/paths.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>

TEST_CASE("the executable path names the running test program", "[platform]")
{
    const auto path = adl::platform::executable_path();

    REQUIRE_FALSE(path.empty());
    CHECK(path.is_absolute());
    CHECK(std::filesystem::is_regular_file(path));
    CHECK(path.filename().string().find("adl_unit_tests") != std::string::npos);
}

TEST_CASE("the computer name is text a licence can be compared with", "[platform]")
{
    const std::string name = adl::platform::computer_name();

    REQUIRE_FALSE(name.empty());
    CHECK(name.find('\0') == std::string::npos);
    CHECK(name.find_first_of("\r\n") == std::string::npos);
}

TEST_CASE("the user data directory is an absolute path", "[platform]")
{
    const auto path = adl::platform::user_data_directory();

    REQUIRE_FALSE(path.empty());
    CHECK(path.is_absolute());
}

TEST_CASE("the machine configuration directory is an absolute folder that is there", "[platform]")
{
    const auto path = adl::platform::machine_config_directory();

    REQUIRE_FALSE(path.empty());
    CHECK(path.is_absolute());
    CHECK(std::filesystem::is_directory(path));
}

TEST_CASE("the two data folders are different folders", "[platform]")
{
    CHECK(adl::platform::user_data_directory() != adl::platform::machine_config_directory());
}

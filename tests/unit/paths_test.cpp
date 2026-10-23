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

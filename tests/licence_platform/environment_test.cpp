// SPDX-License-Identifier: GPL-3.0-or-later
// The reader of an environment variable that the programs use for ADL_SETTINGS_DIR (adl/licence_platform/
// environment.hpp). The variables it reads are given to the test by CMake (tests/licence_platform/CMakeLists.txt),
// the same way on every system; this file holds no operating system code. The bytes of the text outside ASCII are
// escapes, so that no compiler setting or code page decides what the test means.

#include "adl/licence_platform/environment.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

using adl::licence_platform::environment_variable;

TEST_CASE("a variable that is set is read as it is", "[licence_platform][environment]")
{
    const auto value = environment_variable("ADL_ENVIRONMENT_TEST_PLAIN");
    REQUIRE(value.has_value());
    CHECK(*value == "a plain value with spaces");
}

TEST_CASE("a variable with text outside ASCII is read as UTF-8", "[licence_platform][environment]")
{
    const auto value = environment_variable("ADL_ENVIRONMENT_TEST_UTF8");
    REQUIRE(value.has_value());
    // an e with an acute accent, a space and two Japanese characters
    CHECK(*value == "caf\xC3\xA9 \xE6\x97\xA5\xE6\x9C\xAC");
}

TEST_CASE("a variable that is not set is nothing", "[licence_platform][environment]")
{
    CHECK_FALSE(environment_variable("ADL_ENVIRONMENT_TEST_IS_NOT_SET").has_value());
}

TEST_CASE("a name is looked up as it is spelt", "[licence_platform][environment]")
{
    // The name of a variable that is set, with a letter dropped, is another variable.
    CHECK_FALSE(environment_variable("ADL_ENVIRONMENT_TEST_PLAI").has_value());
    CHECK_FALSE(environment_variable("").has_value());
}

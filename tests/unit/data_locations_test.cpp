// SPDX-License-Identifier: GPL-3.0-or-later
// The rules that decide where a user's data goes on Linux and macOS, as functions of the text the
// operating system gave. They read no environment, so every case runs on every host.
#include "detail/data_locations.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

using adl::platform::detail::is_absolute_posix_path;
using adl::platform::detail::join_posix_path;
using adl::platform::detail::macos_user_data_directory;
using adl::platform::detail::xdg_user_data_directory;

TEST_CASE("only a path that starts at the root is absolute", "[platform][locations]")
{
    CHECK(is_absolute_posix_path("/"));
    CHECK(is_absolute_posix_path("/home/ada"));
    CHECK(is_absolute_posix_path("/home/ada/"));

    CHECK_FALSE(is_absolute_posix_path(""));
    CHECK_FALSE(is_absolute_posix_path("home/ada"));
    CHECK_FALSE(is_absolute_posix_path("./data"));
    CHECK_FALSE(is_absolute_posix_path("../data"));
    CHECK_FALSE(is_absolute_posix_path("~/data"));
    CHECK_FALSE(is_absolute_posix_path("C:/Users/ada"));
}

TEST_CASE("an absolute path with a NUL in it is not a path", "[platform][locations]")
{
    using namespace std::string_view_literals;
    CHECK_FALSE(is_absolute_posix_path("/home\0/ada"sv));
}

TEST_CASE("a path is joined with one separator", "[platform][locations]")
{
    CHECK(join_posix_path("/home/ada", "data") == "/home/ada/data");
    CHECK(join_posix_path("/home/ada/", "data") == "/home/ada/data");
    CHECK(join_posix_path("/", "data") == "/data");
    CHECK(join_posix_path("/home/ada", "a/b") == "/home/ada/a/b");
}

TEST_CASE("XDG_DATA_HOME wins when it is an absolute path", "[platform][locations]")
{
    CHECK(xdg_user_data_directory("/srv/data", "/home/ada") == "/srv/data");
    CHECK(xdg_user_data_directory("/srv/data", "") == "/srv/data");
    // The text is used as it was given: a trailing separator is not the rule's business.
    CHECK(xdg_user_data_directory("/srv/data/", "/home/ada") == "/srv/data/");
}

TEST_CASE("an unset, empty or relative XDG_DATA_HOME falls back to the home directory", "[platform][locations]")
{
    const std::string expected = "/home/ada/.local/share";

    CHECK(xdg_user_data_directory("", "/home/ada") == expected);
    CHECK(xdg_user_data_directory("data", "/home/ada") == expected);
    CHECK(xdg_user_data_directory("./data", "/home/ada") == expected);
    CHECK(xdg_user_data_directory("~/data", "/home/ada") == expected);
    CHECK(xdg_user_data_directory("", "/home/ada/") == expected);
}

TEST_CASE("the home directory of the root of a file system still gives a path", "[platform][locations]")
{
    CHECK(xdg_user_data_directory("", "/") == "/.local/share");
}

TEST_CASE("with neither a usable XDG_DATA_HOME nor a usable home there is no folder", "[platform][locations]")
{
    CHECK(xdg_user_data_directory("", "").empty());
    CHECK(xdg_user_data_directory("relative", "also/relative").empty());
    CHECK(xdg_user_data_directory("", "home/ada").empty());
}

TEST_CASE("macOS keeps a user's data in Application Support below the home directory", "[platform][locations]")
{
    CHECK(macos_user_data_directory("/Users/ada") == "/Users/ada/Library/Application Support");
    CHECK(macos_user_data_directory("/Users/ada/") == "/Users/ada/Library/Application Support");
}

TEST_CASE("macOS has no user data folder without an absolute home directory", "[platform][locations]")
{
    CHECK(macos_user_data_directory("").empty());
    CHECK(macos_user_data_directory("Users/ada").empty());
    CHECK(macos_user_data_directory("~").empty());
}

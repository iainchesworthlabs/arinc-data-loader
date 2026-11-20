// SPDX-License-Identifier: GPL-3.0-or-later
// Where the licence folders are: the product's folder names on each system, the folders the platform gives,
// and what ADL_SETTINGS_DIR moves.

#include "adl/licence_platform/locations.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>
#include <string_view>

using namespace adl::licence_platform;

namespace
{

// Roots with the fields a test cares about. Every field is set, so that no compiler sees one left out.
Roots roots_of(std::string_view host, std::string user = {}, std::string machine = {}, std::string settings = {})
{
    Roots roots;
    roots.settings_directory = std::move(settings);
    roots.user_data_directory = std::move(user);
    roots.machine_config_directory = std::move(machine);
    roots.host_name = std::string{host};
    return roots;
}

} // namespace

TEST_CASE("licence folders: the product folder is spelled for the system", "[licence_platform][locations]")
{
    REQUIRE(product_folder_name("linux") == "arinc-data-loader");
    REQUIRE(product_folder_name("windows") == "ARINC Data Loader");
    REQUIRE(product_folder_name("macos") == "ARINC Data Loader");
    REQUIRE(product_folder_name("fake") == "ARINC Data Loader");
    REQUIRE(product_folder_name("") == "ARINC Data Loader");
}

TEST_CASE("licence folders: a user and a machine folder below the platform's folders", "[licence_platform][locations]")
{
    const auto linux_folders = folders_for(roots_of("linux", "/home/pat/.local/share", "/etc"));
    REQUIRE(linux_folders.user == "/home/pat/.local/share/arinc-data-loader/licences");
    REQUIRE(linux_folders.machine == "/etc/arinc-data-loader/licences");
    REQUIRE(linux_folders.clock_file == "/home/pat/.local/share/arinc-data-loader/licence-clock.json");

    const auto windows_folders = folders_for(roots_of("windows", "C:/Users/pat/AppData/Local", "C:/ProgramData"));
    REQUIRE(windows_folders.user == "C:/Users/pat/AppData/Local/ARINC Data Loader/licences");
    REQUIRE(windows_folders.machine == "C:/ProgramData/ARINC Data Loader/licences");
    REQUIRE(windows_folders.clock_file == "C:/Users/pat/AppData/Local/ARINC Data Loader/licence-clock.json");

    const auto macos_folders =
        folders_for(roots_of("macos", "/Users/pat/Library/Application Support", "/Library/Application Support"));
    REQUIRE(macos_folders.user == "/Users/pat/Library/Application Support/ARINC Data Loader/licences");
    REQUIRE(macos_folders.machine == "/Library/Application Support/ARINC Data Loader/licences");

    // A base that ends with a separator is joined once.
    REQUIRE(folders_for(roots_of("linux", "/data/", "/etc/")).user == "/data/arinc-data-loader/licences");
}

TEST_CASE("licence folders: a base the platform does not have leaves its folder empty", "[licence_platform][locations]")
{
    const auto none = folders_for(roots_of("linux"));
    REQUIRE(none.user.empty());
    REQUIRE(none.machine.empty());
    REQUIRE(none.clock_file.empty());

    const auto no_user = folders_for(roots_of("linux", "", "/etc"));
    REQUIRE(no_user.user.empty());
    REQUIRE(no_user.clock_file.empty());
    REQUIRE(no_user.machine == "/etc/arinc-data-loader/licences");

    const auto no_machine = folders_for(roots_of("linux", "/home/pat/.local/share"));
    REQUIRE(no_machine.machine.empty());
    REQUIRE(no_machine.user == "/home/pat/.local/share/arinc-data-loader/licences");
}

TEST_CASE("licence folders: ADL_SETTINGS_DIR moves all three, and the machine folder with them",
          "[licence_platform][locations]")
{
    const auto moved = folders_for(roots_of("linux", "/home/pat/.local/share", "/etc", "/tmp/settings"));
    REQUIRE(moved.user == "/tmp/settings/licences");
    REQUIRE(moved.machine == "/tmp/settings/machine-licences");
    REQUIRE(moved.clock_file == "/tmp/settings/licence-clock.json");

    // Even where the platform has no folders at all.
    const auto bare = folders_for(roots_of("fake", "", "", "/tmp/settings/"));
    REQUIRE(bare.user == "/tmp/settings/licences");
    REQUIRE(bare.machine == "/tmp/settings/machine-licences");
}

TEST_CASE("licence folders: the roots of this computer name the settings folder as a platform path",
          "[licence_platform][locations]")
{
    const auto without = current_roots("fake", {});
    REQUIRE(without.settings_directory.empty());
    REQUIRE(without.host_name == "fake");
    // The platform's folders are absolute paths with '/' or empty; they are never relative.
    for (const std::string& base : {without.user_data_directory, without.machine_config_directory})
    {
        REQUIRE((base.empty() || std::filesystem::path{base}.is_absolute()));
        REQUIRE(base.find('\\') == std::string::npos);
    }

    const auto with = current_roots("fake", std::filesystem::path{"scratch"} / "settings");
    REQUIRE(with.settings_directory == "scratch/settings");
}

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Where ARINC Data Loader keeps its licence files and its clock file. This is the product's part of the
// store: the folder names, and the variable that moves them for a test.
//
//   per user     <user data folder>/<product folder>/licences     imports go here; removal works here
//   per machine  <machine folder>/<product folder>/licences       read only; an administrator puts a file here
//   clock file   <user data folder>/<product folder>/licence-clock.json
//
// The product folder is "ARINC Data Loader" on Windows and macOS and "arinc-data-loader" on Linux. The two
// base folders come from adl::platform (user_data_directory and machine_config_directory).
//
// ADL_SETTINGS_DIR, which moves the programs' preferences so that a test never touches a real one, moves these
// too, and the machine folder with them, so that a test run never reads a real licence:
//
//   <ADL_SETTINGS_DIR>/licences, <ADL_SETTINGS_DIR>/machine-licences, <ADL_SETTINGS_DIR>/licence-clock.json
//
// This code reads no environment. The program that has the variable passes its value in.

#include "adl/licence/store.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace adl::licence_platform
{

// The text the folders are made from.
struct Roots
{
    // The value of ADL_SETTINGS_DIR as a platform path (UTF-8, '/'), or empty when the variable is not set.
    std::string settings_directory;
    // adl::platform::user_data_directory() and machine_config_directory() as platform paths; empty when
    // the platform has none.
    std::string user_data_directory;
    std::string machine_config_directory;
    // adl::platform::HostInfo::name of the host: "windows", "linux", "macos" or "fake". It only decides how
    // the product folder is spelled.
    std::string host_name;
};

// The folder the product keeps its data in, below a base folder: "arinc-data-loader" for "linux",
// "ARINC Data Loader" otherwise.
[[nodiscard]] std::string_view product_folder_name(std::string_view host_name) noexcept;

// The licence folders and the clock file for these roots. A folder whose base is not known is empty: the store
// then reads nothing from it, and an import with no user folder is refused.
[[nodiscard]] licence::store::Folders folders_for(const Roots& roots);

// The roots of this computer for the host named `host_name`, and the settings folder given (empty for none).
// The platform is asked for its two base folders.
[[nodiscard]] Roots current_roots(std::string_view host_name, const std::filesystem::path& settings_directory);

} // namespace adl::licence_platform

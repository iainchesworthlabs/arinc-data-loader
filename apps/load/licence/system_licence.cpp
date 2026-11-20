// SPDX-License-Identifier: GPL-3.0-or-later
// The licence source of a desktop program: this computer's licence files and the keys of the release. Compiled on a
// desktop; apps/load/licence/no_system_licence.cpp is the one for the other platforms.

#include "adl/load/system_licence.hpp"

#include "adl/licence/signed_file.hpp"
#include "adl/licence_platform/environment.hpp"
#include "adl/load/licence_files.hpp"

#include <string>

namespace adl::load
{

LicenceSource system_licence()
{
    // ADL_SETTINGS_DIR moves the licence folders and the clock file, as it moves the settings of the other programs,
    // so that a test never reads a real licence.
    std::string settings = adl::licence_platform::environment_variable("ADL_SETTINGS_DIR").value_or(std::string{});
    return licence_from_files(std::move(settings), adl::licence::signed_file::product_trust_list());
}

} // namespace adl::load

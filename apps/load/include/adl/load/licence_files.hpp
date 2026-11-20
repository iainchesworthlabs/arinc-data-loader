// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// A licence source that reads licence files (licence.hpp), on a desktop: the folders of the product, or the ones
// `settings_directory` moves (the value of ADL_SETTINGS_DIR as UTF-8 text, or empty), judged against `keys`. It reads
// the files and the clock file and writes nothing. The tests give it the published test key; the program gives it the
// keys of its release (system_licence.hpp).

#include "adl/licence/signed_file.hpp"
#include "adl/load/licence.hpp"

#include <string>
#include <vector>

namespace adl::load
{

[[nodiscard]] LicenceSource licence_from_files(std::string settings_directory,
                                               std::vector<adl::licence::signed_file::TrustedKey> keys);

} // namespace adl::load

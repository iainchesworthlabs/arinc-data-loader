// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The licence source of the program on the computer it runs on (licence.hpp): the licence files of this computer, in
// the folders the other programs read, judged against the keys of this release. On a platform with no file system to
// keep licences in (the real-time ones) it reads none, and every record says `null`. CMake compiles one of the two
// (apps/load/CMakeLists.txt), as it does for the platform layer.

#include "adl/load/licence.hpp"

namespace adl::load
{

[[nodiscard]] LicenceSource system_licence();

} // namespace adl::load

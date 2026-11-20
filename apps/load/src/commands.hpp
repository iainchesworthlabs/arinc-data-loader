// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The commands, one function each. They take the options that were accepted and write their events; they
// return the exit code. Wrong usage that only shows once the media set has been read (a part number it does
// not hold) is told on `err` and is exit code 2 with no event written.

#include "adl/load/events.hpp"
#include "adl/load/options.hpp"
#include "adl/platform/host.hpp"

#include <iosfwd>

namespace adl::load
{

// information, upload and download: one operation on one target.
[[nodiscard]] int run_operation_command(adl::platform::Host& host, const Options& options, EventWriter& events,
                                        std::ostream& err);

// find.
[[nodiscard]] int run_find_command(adl::platform::Host& host, const Options& options, EventWriter& events);

} // namespace adl::load

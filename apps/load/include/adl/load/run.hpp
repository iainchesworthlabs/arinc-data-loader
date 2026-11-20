// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The program, as a function: the arguments of the command line (not the program's name) and the two
// streams the program writes to, on the host it runs on. main makes the real host and standard output
// and error; a test gives the fake platform and streams of its own and runs a whole command in one
// process. Returns the exit code.

#include "adl/platform/host.hpp"

#include <iosfwd>
#include <span>
#include <string_view>

namespace adl::load
{

[[nodiscard]] int run(adl::platform::Host& host, std::span<const std::string_view> arguments, std::ostream& out,
                      std::ostream& err);

} // namespace adl::load

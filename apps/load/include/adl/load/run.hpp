// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The program, as a function: the arguments of the command line (not the program's name) and the three streams the
// program reads and writes, on the host it runs on. main makes the real host and standard input, output and
// error; a test gives the fake platform and streams of its own and runs a whole command in one process. Returns
// the exit code. Only `serve` reads its input, which is where it takes its requests from.

#include "adl/load/licence.hpp"
#include "adl/platform/host.hpp"

#include <iosfwd>
#include <span>
#include <string_view>

namespace adl::load
{

// `licence` says how each operation's record gets the licence it ran under (licence.hpp); a program that is given
// none writes `null` there.
[[nodiscard]] int run(adl::platform::Host& host, std::span<const std::string_view> arguments, std::istream& in,
                      std::ostream& out, std::ostream& err, const LicenceSource& licence = {});

// The same with an input that has nothing in it: for the commands that read none. `serve` run this way takes the
// end of its input as its first request.
[[nodiscard]] int run(adl::platform::Host& host, std::span<const std::string_view> arguments, std::ostream& out,
                      std::ostream& err, const LicenceSource& licence = {});

} // namespace adl::load

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// adl_mock as a function: the arguments in, the exit code out, the events on the stream given. main()
// is only a call to it, so the tests can start the program in a thread of their own and run the
// loader against it.

#include <iosfwd>
#include <span>
#include <string_view>

namespace adl::mock::cli
{

// The exit codes. They are the ones the page "The mock target" lists; 1 is the run loop failing (waiting on the
// network failed, or the platform would not start).
inline constexpr int exit_ok = 0;       // stopped as asked: --operations reached, --seconds passed, or --help
inline constexpr int exit_failed = 1;   // the run failed
inline constexpr int exit_usage = 2;    // wrong options
inline constexpr int exit_folder = 3;   // a folder cannot be used (--holds, --storage)
inline constexpr int exit_port = 4;     // a port cannot be opened
inline constexpr int exit_scenario = 5; // the scenario file (--scenario) cannot be read or is refused

// Runs the program with `arguments` (the command line without the program's name). Events and the
// result of --help and --version go to `out`; what is not a result (a usage error, a problem storing
// a file, why a run failed) goes to `err`. Blocks until the target has stopped; the options say when.
[[nodiscard]] int run_command(std::span<const std::string_view> arguments, std::ostream& out, std::ostream& err);

} // namespace adl::mock::cli

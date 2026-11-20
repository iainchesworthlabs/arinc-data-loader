// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// adl_make_set as a function, so that the tests can run it on temporary folders and read what it writes.
// main() is only a call to run_command(). Nothing here handles a signal.

#include <iosfwd>
#include <span>
#include <string>
#include <string_view>

namespace adl::make_set
{

// The exit codes (docs/user-guide/command-line.md). A program that starts adl_make_set reads them with the
// `reason` of the last event.
inline constexpr int exit_ok = 0;            // done (a finding of the standard's recommendations does not stop it)
inline constexpr int exit_set_invalid = 1;   // the set does not read, or does not verify
inline constexpr int exit_usage = 2;         // the command line cannot be used
inline constexpr int exit_input = 3;         // a description, a source file or a folder given cannot be used
inline constexpr int exit_output_exists = 4; // the output folder is there already and is not empty
inline constexpr int exit_local = 5;         // memory, a disk, or standard output

// How a command ended, as `finished` and the inspection document say it.
struct Verdict
{
    bool completed = false;
    std::string reason; // one word for a program to read
    std::string detail; // one English sentence for a person
    int exit_code = exit_ok;
};

// Runs the command named by `arguments` (not including the program's name) and returns the exit code.
// Standard output goes to `out`, and the usage errors and, without --json, the sentences of a failure
// to `err`.
[[nodiscard]] int run_command(std::span<const std::string_view> arguments, std::ostream& out, std::ostream& err);

} // namespace adl::make_set

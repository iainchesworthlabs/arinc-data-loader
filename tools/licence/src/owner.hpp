// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
#pragma once

// The owner's tool for ARINC Data Loader licence files: make a key, issue a licence, check one, write a
// revocation list. It is built with the tools and never installed (docs/licence-owner-tool.md). This is the
// whole program but its entry point: the entry points (os/*/entry.cpp) turn the command line into UTF-8 text
// and call `run`.

#include <iosfwd>
#include <span>
#include <string>

namespace adl::licence_owner
{

inline constexpr int exit_ok = 0;
inline constexpr int exit_failed = 1;
inline constexpr int exit_usage = 2;

// Where the tool writes. A test passes its own.
struct Console
{
    std::ostream& out;
    std::ostream& err;
};

// Runs one command line. `arguments` are what follows the program's name, as UTF-8. Returns the exit code.
[[nodiscard]] int run(std::span<const std::string> arguments, const Console& console);

// What `--help` prints.
[[nodiscard]] std::string usage_text();

} // namespace adl::licence_owner

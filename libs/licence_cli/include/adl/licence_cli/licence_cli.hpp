// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The commands of adl_licence: show the licence in force and the files behind it, import a file, remove one,
// and judge a file without keeping it (docs/user-guide/command-line.md). This is the whole program but the
// system it runs on: `run` is given the command line as UTF-8 text and an Environment, and writes to two
// streams, so a test gives it files in memory, a clock it sets and the published test key.
//
// The commands read and write the licence store (adl/licence/store.hpp) and nothing else. They use no
// network. `show` and `verify` write nothing. Nothing is printed that came from a file without its control
// characters replaced, and no key, signature or file content is printed.

#include "adl/licence/licence.hpp"
#include "adl/licence/signed_file.hpp"
#include "adl/licence/store.hpp"

#include <functional>
#include <iosfwd>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace adl::licence_cli
{

inline constexpr int exit_ok = 0;
inline constexpr int exit_failed = 1;
inline constexpr int exit_usage = 2;

// Where the program writes. A test passes its own.
struct Console
{
    std::ostream& out;
    std::ostream& err;
};

// What the commands know of the computer they run on. All of it is passed in, so that every state is tried.
struct Environment
{
    // The files of the store, and the files a command names. Not owned; has to outlive `run`.
    licence::store::Files* files = nullptr;
    licence::store::Folders folders;
    // The time now, or nothing when the computer will not say.
    std::function<std::optional<licence::TimePoint>()> now;
    std::string computer_name;
    // The keys this program trusts.
    std::vector<licence::signed_file::TrustedKey> keys;
    // When this program was built: a time that has certainly passed.
    licence::TimePoint build_time = licence::TimePoint::min();
    // What --version prints after the program's name.
    std::string version;
};

// Runs one command line. `arguments` are what follows the program's name, as UTF-8 text. Returns the exit code.
[[nodiscard]] int run(std::span<const std::string> arguments, const Environment& environment, const Console& console);

// What --help prints.
[[nodiscard]] std::string usage_text();

} // namespace adl::licence_cli

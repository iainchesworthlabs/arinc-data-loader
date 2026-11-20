// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
#pragma once

// The five commands. Each takes what follows its name on the command line and returns an exit code.

#include "owner.hpp"

#include <span>
#include <string>
#include <string_view>

namespace adl::licence_owner
{

[[nodiscard]] int inspect_command(std::span<const std::string> arguments, const Console& console);
[[nodiscard]] int verify_command(std::span<const std::string> arguments, const Console& console);
[[nodiscard]] int issue_command(std::span<const std::string> arguments, const Console& console);
[[nodiscard]] int revocations_command(std::span<const std::string> arguments, const Console& console);
[[nodiscard]] int keygen_command(std::span<const std::string> arguments, const Console& console);

// `adl_licence_owner: <text>` on the error stream, and the exit code for a failure or for a usage error (which
// also says where the commands are listed).
[[nodiscard]] int fail(const Console& console, std::string_view text);
[[nodiscard]] int usage_error(const Console& console, std::string_view text);

// Whether a licence number has the form ADL-<year>-<six digits>, and whether it is in the range kept for tests.
[[nodiscard]] bool is_licence_number(std::string_view text) noexcept;
[[nodiscard]] bool is_test_licence_number(std::string_view text) noexcept;

} // namespace adl::licence_owner

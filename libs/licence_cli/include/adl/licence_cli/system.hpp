// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// The Environment of this computer: its file system and clock through the platform layer, the licence folders
// (licence_platform/locations.hpp), the name of the computer and the keys of this release. The program that
// reads ADL_SETTINGS_DIR passes its value in, because the platform has no environment reader
// (adl/licence_platform/environment.hpp).

#include "adl/licence_cli/licence_cli.hpp"
#include "adl/licence_platform/files.hpp"
#include "adl/platform/host.hpp"

#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace adl::licence_cli
{

// The time a commit was made, from the text `git log --format=%cI` gives (2026-11-20T10:15:30+01:00, or a Z for
// UTC), as a time in UTC. Nothing for text of any other form, and for a build that was not made from a
// repository (the text is then empty): the program knows no time that has certainly passed but its clock's.
[[nodiscard]] std::optional<licence::TimePoint> parse_commit_time(std::string_view text);

class SystemEnvironment
{
public:
    // `settings_directory` is the value of ADL_SETTINGS_DIR as UTF-8 text, or empty when the variable is not set.
    // The error is a sentence for the person using the program: the system's file system and clock could not be
    // opened.
    [[nodiscard]] static std::expected<std::unique_ptr<SystemEnvironment>, std::string>
    open(std::string_view settings_directory, std::vector<licence::signed_file::TrustedKey> keys,
         licence::TimePoint build_time, std::string version);

    SystemEnvironment(const SystemEnvironment&) = delete;
    SystemEnvironment& operator=(const SystemEnvironment&) = delete;

    [[nodiscard]] const Environment& environment() const noexcept
    {
        return environment_;
    }

    // For a test that sets the clock, the keys or the computer's name before it runs a command.
    [[nodiscard]] Environment& environment() noexcept
    {
        return environment_;
    }

private:
    SystemEnvironment() = default;

    std::unique_ptr<platform::Host> host_;
    std::unique_ptr<licence_platform::PlatformFiles> files_;
    Environment environment_;
};

} // namespace adl::licence_cli

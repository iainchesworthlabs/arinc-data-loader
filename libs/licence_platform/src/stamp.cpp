// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/licence_platform/stamp.hpp"

#include "adl/licence/words.hpp"
#include "adl/licence_platform/files.hpp"
#include "adl/licence_platform/locations.hpp"
#include "adl/platform/arguments.hpp"
#include "adl/platform/paths.hpp"

#include <chrono>
#include <filesystem>

namespace adl::licence_platform
{

Stamp stamp_for(licence::store::Files& files, const licence::store::Folders& folders, licence::TimePoint now,
                std::string_view computer_name, const std::vector<licence::signed_file::TrustedKey>& keys,
                licence::TimePoint build_time)
{
    namespace lic = licence;
    // Read-only: the clock file is read to see the latest time a program has seen, and is not written back.
    const lic::ClockState clock = lic::store::load_clock(files, folders);
    lic::Context where;
    where.now = now;
    where.latest_seen = clock.latest_seen;
    where.build_time = build_time;
    where.computer_name = std::string{computer_name};
    where.keys = keys;
    const lic::Status best = lic::store::scan(files, folders, where).best;

    Stamp stamp;
    stamp.edition = std::string{lic::edition_name(best)};
    stamp.state = std::string{lic::state_name(best.state)};
    // The number of a licence that is not in force is not stamped: a stamp says what the program ran as.
    if (best.active() && best.value)
    {
        stamp.number = best.value->licence_id;
    }
    return stamp;
}

std::optional<Stamp> stamp_of(platform::Host& host, std::string_view settings_directory,
                              const std::vector<licence::signed_file::TrustedKey>& keys)
{
    const auto seconds = host.clock().wall_time_seconds();
    if (!seconds)
    {
        return std::nullopt;
    }
    std::filesystem::path settings;
    if (!settings_directory.empty())
    {
        settings = platform::path_from_utf8(settings_directory);
    }
    const licence::store::Folders folders = folders_for(current_roots(host.info().name, settings));
    PlatformFiles files{host.files()};
    return stamp_for(files, folders, licence::TimePoint{std::chrono::seconds{*seconds}}, platform::computer_name(),
                     keys);
}

} // namespace adl::licence_platform

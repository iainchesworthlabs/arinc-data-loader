// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/licence_cli/system.hpp"

#include "adl/licence_platform/locations.hpp"
#include "adl/platform/arguments.hpp"
#include "adl/platform/paths.hpp"
#include "messages.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <utility>

namespace adl::licence_cli
{

namespace
{

// The number the `count` characters at `at` spell, or -1 if one of them is not a digit.
[[nodiscard]] int digits_at(std::string_view text, std::size_t at, std::size_t count) noexcept
{
    int value = 0;
    for (std::size_t i = at; i < at + count; ++i)
    {
        if (text[i] < '0' || text[i] > '9')
        {
            return -1;
        }
        value = value * 10 + (text[i] - '0');
    }
    return value;
}

} // namespace

std::optional<licence::TimePoint> parse_commit_time(std::string_view text)
{
    // 2026-11-20T10:15:30Z, or with an offset: 2026-11-20T10:15:30+01:00
    const bool with_offset = text.size() == 25 && (text[19] == '+' || text[19] == '-') && text[22] == ':';
    if (!(text.size() == 20 && text[19] == 'Z') && !with_offset)
    {
        return std::nullopt;
    }
    if (text[4] != '-' || text[7] != '-' || text[10] != 'T' || text[13] != ':' || text[16] != ':')
    {
        return std::nullopt;
    }
    const int year = digits_at(text, 0, 4);
    const int month = digits_at(text, 5, 2);
    const int day = digits_at(text, 8, 2);
    const int hour = digits_at(text, 11, 2);
    const int minute = digits_at(text, 14, 2);
    const int second = digits_at(text, 17, 2);
    int offset_hours = 0;
    int offset_minutes = 0;
    if (with_offset)
    {
        offset_hours = digits_at(text, 20, 2);
        offset_minutes = digits_at(text, 23, 2);
    }
    if (year < 1970 || month < 1 || day < 1 || hour < 0 || minute < 0 || second < 0 || offset_hours < 0 ||
        offset_minutes < 0)
    {
        return std::nullopt;
    }
    const std::chrono::year_month_day date{std::chrono::year{year}, std::chrono::month{static_cast<unsigned>(month)},
                                           std::chrono::day{static_cast<unsigned>(day)}};
    if (!date.ok() || hour > 23 || minute > 59 || second > 59 || offset_hours > 23 || offset_minutes > 59)
    {
        return std::nullopt;
    }
    const std::chrono::minutes offset{offset_hours * 60 + offset_minutes};
    return licence::TimePoint{std::chrono::sys_days{date}} + std::chrono::hours{hour} + std::chrono::minutes{minute} +
           std::chrono::seconds{second} - (text[19] == '-' ? -offset : offset);
}

std::expected<std::unique_ptr<SystemEnvironment>, std::string>
SystemEnvironment::open(std::string_view settings_directory, std::vector<licence::signed_file::TrustedKey> keys,
                        licence::TimePoint build_time, std::string version)
{
    auto host = platform::make_host();
    if (!host)
    {
        return std::unexpected(std::string(message::cannot_start));
    }
    std::unique_ptr<SystemEnvironment> system{new SystemEnvironment};
    system->host_ = std::move(*host);
    system->files_ = std::make_unique<licence_platform::PlatformFiles>(system->host_->files());

    std::filesystem::path settings;
    if (!settings_directory.empty())
    {
        settings = platform::path_from_utf8(settings_directory);
    }
    Environment& environment = system->environment_;
    environment.files = system->files_.get();
    environment.folders =
        licence_platform::folders_for(licence_platform::current_roots(system->host_->info().name, settings));
    platform::Clock* const clock = &system->host_->clock();
    environment.now = [clock]() -> std::optional<licence::TimePoint>
    {
        const auto seconds = clock->wall_time_seconds();
        if (!seconds)
        {
            return std::nullopt;
        }
        return licence::TimePoint{std::chrono::seconds{*seconds}};
    };
    environment.computer_name = platform::computer_name();
    environment.keys = std::move(keys);
    environment.build_time = build_time;
    environment.version = std::move(version);
    return system;
}

} // namespace adl::licence_cli

// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
#include "adl/licence/store.hpp"

#include "adl/licence/product.hpp"
#include "json.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace adl::licence::store
{

namespace
{

enum class Kind
{
    licence,
    revocations,
};

constexpr std::size_t max_name_length = 255;
constexpr std::size_t max_token_length = 64;
constexpr std::size_t time_text_length = 20;

[[nodiscard]] char ascii_lower(char c) noexcept
{
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

[[nodiscard]] bool ends_with_nocase(std::string_view name, std::string_view extension) noexcept
{
    // A name has something before its extension: ".adllic" is not a licence file.
    if (name.size() <= extension.size())
    {
        return false;
    }
    return std::ranges::equal(extension, name.substr(name.size() - extension.size()),
                              [](char a, char b) { return ascii_lower(a) == ascii_lower(b); });
}

// What a file name says it is, by its extension. A file with another extension is not the store's.
[[nodiscard]] std::optional<Kind> kind_of(std::string_view name) noexcept
{
    if (ends_with_nocase(name, licence_extension))
    {
        return Kind::licence;
    }
    if (ends_with_nocase(name, revocations_extension))
    {
        return Kind::revocations;
    }
    return std::nullopt;
}

[[nodiscard]] std::string join(std::string_view folder, std::string_view name)
{
    std::string path{folder};
    if (!path.empty() && path.back() != '/')
    {
        path += '/';
    }
    path += name;
    return path;
}

[[nodiscard]] Status refused_status(Reason why)
{
    Status status;
    status.state = State::invalid;
    status.reason = why;
    return status;
}

// A file name that is one name: no folder, no drive, no control character.
[[nodiscard]] bool is_plain_name(std::string_view name) noexcept
{
    if (name.empty() || name.size() > max_name_length || name.front() == '.')
    {
        return false;
    }
    return std::ranges::none_of(name,
                                [](char c)
                                {
                                    const auto byte = static_cast<unsigned char>(c);
                                    return c == '/' || c == '\\' || c == ':' || byte < 0x20 || byte == 0x7f;
                                });
}

[[nodiscard]] bool is_display_text(std::string_view text) noexcept
{
    return std::ranges::none_of(text,
                                [](char c)
                                {
                                    const auto byte = static_cast<unsigned char>(c);
                                    return byte < 0x20 || byte == 0x7f;
                                });
}

struct Listing
{
    // The files with a licence or revocation extension, in name order, at most `max_files_per_folder`.
    std::vector<std::pair<std::string, Kind>> files;
    // How many such files the folder holds, counting the ones not listed.
    std::size_t total = 0;
    bool unreadable = false;
};

// A folder that is not there lists nothing and is no fault.
[[nodiscard]] Listing list_folder(Files& files, std::string_view folder)
{
    Listing listing;
    if (folder.empty())
    {
        return listing;
    }
    auto names = files.list(folder);
    if (!names)
    {
        listing.unreadable = names.error() != FilesError::not_found;
        return listing;
    }
    std::ranges::sort(*names);
    for (std::string& name : *names)
    {
        if (const auto kind = kind_of(name))
        {
            ++listing.total;
            if (listing.files.size() < max_files_per_folder)
            {
                listing.files.emplace_back(std::move(name), *kind);
            }
        }
    }
    return listing;
}

[[nodiscard]] std::string with_counter(std::string_view stem, std::size_t counter, std::string_view extension)
{
    std::string name{stem};
    if (counter > 1)
    {
        name += '-';
        name += std::to_string(counter);
    }
    name += extension;
    return name;
}

} // namespace

Overview scan(Files& files, const Folders& folders, Context where)
{
    Overview overview;
    overview.revocations = where.revocations != nullptr ? *where.revocations : builtin_revocations();

    struct Found
    {
        Origin origin;
        std::string name;
        // The text, or the reason the file could not be read.
        std::optional<std::string> text;
    };
    std::vector<Found> found;

    const std::pair<Origin, const std::string*> places[] = {{Origin::user, &folders.user},
                                                            {Origin::machine, &folders.machine}};
    for (const auto& [origin, folder] : places)
    {
        const Listing listing = list_folder(files, *folder);
        overview.folder_unreadable = overview.folder_unreadable || listing.unreadable;
        overview.folder_full = overview.folder_full || listing.total > max_files_per_folder;
        for (const auto& [name, kind] : listing.files)
        {
            auto text = files.read(join(*folder, name), signed_file::max_file_size);
            if (kind == Kind::revocations)
            {
                const auto list = text ? read_revocations(*text, where.keys) : std::unexpected(Error::malformed);
                if (list)
                {
                    overview.revocations = newer(overview.revocations, *list);
                    ++overview.revocation_lists_accepted;
                }
                else
                {
                    ++overview.revocation_lists_refused;
                }
                continue;
            }
            found.push_back({origin, name, text ? std::optional<std::string>{std::move(*text)} : std::nullopt});
        }
    }

    // Every list found takes part in the judgement, whichever folder or order it came in.
    where.revocations = &overview.revocations;
    std::vector<Status> statuses;
    for (Found& item : found)
    {
        Status status = item.text ? evaluate(*item.text, where) : refused_status(Reason::malformed);
        statuses.push_back(status);
        overview.licences.push_back({item.origin, std::move(item.name), std::move(status)});
    }
    overview.best = best(statuses);
    return overview;
}

std::expected<Imported, ImportError> import_file(Files& files, const Folders& folders, std::string_view file_text,
                                                 const Context& where)
{
    const auto refuse = [](Reason why) { return std::unexpected(ImportError{ImportFailure::refused, why}); };

    Imported imported;
    std::string stem;
    const auto envelope = signed_file::parse(file_text);
    if (!envelope)
    {
        return refuse(reason_for(envelope.error()));
    }
    if (envelope->purpose == licence_purpose)
    {
        imported.status = evaluate(file_text, where);
        if (imported.status.state == State::invalid || !imported.status.value)
        {
            return refuse(imported.status.reason);
        }
        stem = imported.status.value->licence_id;
    }
    else if (envelope->purpose == revocations_purpose)
    {
        const auto list = read_revocations(file_text, where.keys);
        if (!list)
        {
            return refuse(reason_for(list.error()));
        }
        imported.revocation_list = true;
        stem = "revocations-" + std::to_string(list->sequence);
    }
    else
    {
        return refuse(reason_for(Error::wrong_purpose));
    }

    const auto write_failed = std::unexpected(ImportError{ImportFailure::write_failed, Reason::ok});
    if (folders.user.empty())
    {
        return write_failed;
    }
    const std::string_view extension = imported.revocation_list ? revocations_extension : licence_extension;

    const auto listed = files.list(folders.user);
    if (!listed && listed.error() != FilesError::not_found)
    {
        return write_failed;
    }
    const std::vector<std::string> none;
    const std::vector<std::string>& names = listed ? *listed : none;
    const auto taken = [&](const std::string& name) { return std::ranges::find(names, name) != names.end(); };
    const std::size_t store_files = static_cast<std::size_t>(
        std::ranges::count_if(names, [](const std::string& n) { return kind_of(n).has_value(); }));

    // The first name that is free; a file with the same bytes under an earlier name is the file itself.
    for (std::size_t counter = 1; counter <= max_files_per_folder + 1; ++counter)
    {
        std::string name = with_counter(stem, counter, extension);
        const std::string path = join(folders.user, name);
        if (taken(name))
        {
            const auto existing = files.read(path, signed_file::max_file_size);
            if (existing && *existing == file_text)
            {
                imported.name = std::move(name);
                imported.already_there = true;
                return imported;
            }
            continue;
        }
        if (store_files >= max_files_per_folder)
        {
            return std::unexpected(ImportError{ImportFailure::folder_full, Reason::ok});
        }
        const auto created = files.create(path, file_text);
        if (created)
        {
            imported.name = std::move(name);
            return imported;
        }
        if (created.error() != FilesError::already_exists)
        {
            return write_failed;
        }
        // Taken since the listing, by a file we have not seen: try the next name.
    }
    return std::unexpected(ImportError{ImportFailure::folder_full, Reason::ok});
}

std::expected<void, RemoveError> remove_file(Files& files, const Folders& folders, std::string_view name)
{
    if (!is_plain_name(name) || !kind_of(name))
    {
        return std::unexpected(RemoveError::not_allowed);
    }
    const auto listed = files.list(folders.user);
    if (!listed || std::ranges::find(*listed, name) == listed->end())
    {
        return std::unexpected(listed || listed.error() == FilesError::not_found ? RemoveError::not_found
                                                                                 : RemoveError::failed);
    }
    const auto removed = files.remove(join(folders.user, name));
    if (!removed)
    {
        return std::unexpected(removed.error() == FilesError::not_found ? RemoveError::not_found : RemoveError::failed);
    }
    return {};
}

Result<ClockState> parse_clock(std::string_view text)
{
    if (text.size() > signed_file::max_file_size)
    {
        return std::unexpected(Error::too_large);
    }
    const auto document = detail::parse_strict(text);
    if (!document)
    {
        return std::unexpected(document.error());
    }
    const auto* object = document->as_object();
    if (object == nullptr)
    {
        return std::unexpected(Error::malformed);
    }
    const auto format = detail::required_integer(*object, "format", 0, std::numeric_limits<std::int32_t>::max());
    if (!format)
    {
        return std::unexpected(format.error());
    }
    if (*format > signed_file::current_format)
    {
        return std::unexpected(Error::newer_version);
    }
    if (*format != signed_file::current_format)
    {
        return std::unexpected(Error::malformed);
    }

    ClockState state;
    const auto seen = detail::optional_string(*object, "latest_seen", time_text_length);
    if (!seen)
    {
        return std::unexpected(seen.error());
    }
    if (!seen->empty())
    {
        const auto time = parse_time(*seen);
        if (!time)
        {
            return std::unexpected(time.error());
        }
        state.latest_seen = *time;
    }
    auto resets = detail::string_list(*object, "applied_resets", max_applied_resets, max_token_length);
    if (!resets)
    {
        return std::unexpected(resets.error());
    }
    for (const std::string& token : *resets)
    {
        if (token.empty() || !is_display_text(token))
        {
            return std::unexpected(Error::malformed);
        }
    }
    state.applied_resets = std::move(*resets);
    return state;
}

std::string write_clock(const ClockState& state)
{
    std::string text = "{\"format\": 1";
    if (state.latest_seen != TimePoint::min())
    {
        text += ", \"latest_seen\": " + detail::quote(format_time(state.latest_seen));
    }
    text += ", \"applied_resets\": [";
    bool first = true;
    for (const std::string& token : state.applied_resets)
    {
        text += first ? "" : ", ";
        text += detail::quote(token);
        first = false;
    }
    text += "]}\n";
    return text;
}

ClockState load_clock(Files& files, const Folders& folders)
{
    if (folders.clock_file.empty())
    {
        return {};
    }
    const auto text = files.read(folders.clock_file, signed_file::max_file_size);
    if (!text)
    {
        return {};
    }
    auto state = parse_clock(*text);
    return state ? std::move(*state) : ClockState{};
}

FilesResult<void> save_clock(Files& files, const Folders& folders, const ClockState& state)
{
    if (folders.clock_file.empty())
    {
        return std::unexpected(FilesError::failed);
    }
    return files.replace(folders.clock_file, write_clock(state));
}

} // namespace adl::licence::store

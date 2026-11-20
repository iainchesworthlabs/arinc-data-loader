// SPDX-License-Identifier: GPL-3.0-or-later
#include "commands.hpp"

#include "adl/licence/licence.hpp"
#include "adl/licence/signed_file.hpp"
#include "adl/licence/store.hpp"
#include "adl/licence/words.hpp"
#include "adl/platform/arguments.hpp"
#include "adl/platform/paths.hpp"
#include "messages.hpp"
#include "text.hpp"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace adl::licence_cli
{

namespace lic = adl::licence;
namespace st = adl::licence::store;

namespace
{

// The store as it is, with every write left out: what `import` would do, without doing it. A call that would
// write says it worked and writes nothing.
class DryRunFiles final : public st::Files
{
public:
    explicit DryRunFiles(st::Files& real) noexcept : real_(real) {}

    st::FilesResult<std::vector<std::string>> list(std::string_view folder) override
    {
        return real_.list(folder);
    }
    st::FilesResult<std::string> read(std::string_view path, std::size_t max_bytes) override
    {
        return real_.read(path, max_bytes);
    }
    st::FilesResult<void> create(std::string_view, std::string_view) override
    {
        return {};
    }
    st::FilesResult<void> replace(std::string_view, std::string_view) override
    {
        return {};
    }
    st::FilesResult<void> remove(std::string_view) override
    {
        return {};
    }

private:
    st::Files& real_;
};

[[nodiscard]] std::optional<lic::TimePoint> time_now(const Environment& environment)
{
    return environment.now ? environment.now() : std::nullopt;
}

// What the program knows when it judges a licence.
[[nodiscard]] lic::Context context_for(const Environment& environment, lic::TimePoint now, const lic::ClockState& clock)
{
    lic::Context where;
    where.now = now;
    where.latest_seen = clock.latest_seen;
    where.build_time = environment.build_time;
    where.computer_name = environment.computer_name;
    where.keys = environment.keys;
    return where;
}

// The file a command names, as the platform's file interface takes a name: UTF-8 and '/'.
[[nodiscard]] std::expected<std::string, std::string> read_named(const Environment& environment,
                                                                 std::string_view argument)
{
    const std::string path = platform::to_platform_path(platform::path_from_utf8(argument));
    st::FilesError error = st::FilesError::failed;
    if (!path.empty())
    {
        auto text = environment.files->read(path, lic::signed_file::max_file_size);
        if (text)
        {
            return std::move(*text);
        }
        error = text.error();
    }
    std::string reason;
    switch (error)
    {
    case st::FilesError::not_found:
        reason = std::string(message::reason_missing);
        break;
    case st::FilesError::too_large:
        reason = format(message::reason_too_large, {std::to_string(lic::signed_file::max_file_size)});
        break;
    default:
        reason = std::string(message::reason_unreadable);
        break;
    }
    return std::unexpected(format(message::cannot_read, {plain(argument), reason}));
}

// The sentence for a file that is not taken.
[[nodiscard]] std::string refusal_sentence(const Environment& environment, lic::Reason reason)
{
    switch (reason)
    {
    case lic::Reason::newer_version:
        // A release with no keys of its own checks nothing, so an unknown key is not "a newer version".
        return std::string(environment.keys.empty() ? message::note_no_keys : message::refused_newer);
    case lic::Reason::revoked:
        return std::string(message::refused_revoked);
    case lic::Reason::wrong_computer:
        return format(message::refused_wrong_computer, {plain(environment.computer_name)});
    case lic::Reason::wrong_product:
        return std::string(message::refused_wrong_product);
    default:
        return std::string(message::refused_invalid);
    }
}

[[nodiscard]] std::string_view kind_word(lic::LicenceKind kind) noexcept
{
    switch (kind)
    {
    case lic::LicenceKind::trial:
        return "trial";
    case lic::LicenceKind::annual:
        return "annual";
    case lic::LicenceKind::enterprise:
        return "enterprise";
    }
    return "annual";
}

void print_line(std::ostream& out, std::string_view label, std::string_view value)
{
    out << label << ' ' << value << '\n';
}

// The lines of a licence's state. Every key is the same in every release: a script reads them.
void print_status(std::ostream& out, const lic::Status& status)
{
    print_line(out, message::label_edition, lic::edition_name(status));
    print_line(out, message::label_state, lic::state_name(status.state));
    if (status.reason != lic::Reason::ok)
    {
        print_line(out, message::label_reason, lic::reason_sentence(status.reason));
    }
    if (!status.value)
    {
        return;
    }
    const lic::Licence& value = *status.value;
    print_line(out, message::label_licence, plain(value.licence_id));
    print_line(out, message::label_kind, kind_word(value.kind));
    print_line(out, message::label_licensed_to, plain(value.licensee_name));
    print_line(out, message::label_term,
               format(message::term_range, {date_of(value.starts_at), date_of(value.expires_at)}));
    if (status.state == lic::State::valid || status.state == lic::State::expiring)
    {
        print_line(out, message::label_days_left, std::to_string(status.days_left));
    }
    else if (status.state == lic::State::grace)
    {
        print_line(out, message::label_grace_ends, date_of(status.grace_ends));
    }
    print_line(out, message::label_seats,
               value.seats == 0 ? std::string(message::seats_site) : std::to_string(value.seats));
    print_line(out, message::label_features, joined(value.features, message::none_word));
    print_line(out, message::label_computers, joined(value.machines, message::any_word));
    if (value.updates_until)
    {
        print_line(out, message::label_updates_until, date_of(*value.updates_until));
    }
}

// Whether two statuses are the same one: how `show` finds the file that is in use.
[[nodiscard]] bool same_status(const lic::Status& a, const lic::Status& b)
{
    if (a.state != b.state || a.reason != b.reason || a.key_id != b.key_id || a.days_left != b.days_left ||
        a.value.has_value() != b.value.has_value())
    {
        return false;
    }
    return !a.value || (a.value->licence_id == b.value->licence_id && a.grace_ends == b.grace_ends);
}

[[nodiscard]] int fail(const Console& console, std::string_view command, const std::string& sentence)
{
    console.err << command << ": " << sentence << '\n';
    return exit_failed;
}

// The clock, the files that are needed and the licence the command names. False after saying why.
struct Judging
{
    lic::TimePoint now;
    lic::ClockState clock;
    lic::Context where;
    lic::RevocationList withdrawn;
    st::Overview overview;
};

// Reads the clock file and the folders, and sets up the context to judge in, with the lists of withdrawn
// licences that are in the folders. `where.revocations` points into the result, which has to stay where it is.
[[nodiscard]] std::optional<Judging> judge_setup(const Environment& environment, const Console& console,
                                                 std::string_view command)
{
    const auto now = time_now(environment);
    if (!now)
    {
        static_cast<void>(fail(console, command, std::string(message::clock_unreadable)));
        return std::nullopt;
    }
    Judging setup;
    setup.now = *now;
    setup.clock = st::load_clock(*environment.files, environment.folders);
    setup.where = context_for(environment, setup.now, setup.clock);
    setup.overview = st::scan(*environment.files, environment.folders, setup.where);
    setup.withdrawn = setup.overview.revocations;
    return setup;
}

} // namespace

int command_show(const Environment& environment, const Console& console)
{
    const auto now = time_now(environment);
    if (!now)
    {
        return fail(console, "show", std::string(message::clock_unreadable));
    }
    if (environment.folders.user.empty() && environment.folders.machine.empty())
    {
        return fail(console, "show", std::string(message::no_folder));
    }
    const lic::ClockState clock = st::load_clock(*environment.files, environment.folders);
    const st::Overview overview =
        st::scan(*environment.files, environment.folders, context_for(environment, *now, clock));
    std::ostream& out = console.out;

    print_status(out, overview.best);
    if (overview.licences.empty())
    {
        print_line(out, message::label_files, message::none_word);
    }
    else
    {
        out << message::label_files << '\n';
        bool in_use_marked = false;
        for (const st::Entry& entry : overview.licences)
        {
            out << "  " << plain(entry.name) << " ("
                << (entry.origin == st::Origin::user ? message::file_user : message::file_machine) << ", "
                << lic::state_name(entry.status.state);
            if (entry.status.state == lic::State::invalid)
            {
                out << ", " << lic::reason_sentence(entry.status.reason);
            }
            if (!in_use_marked && same_status(entry.status, overview.best))
            {
                out << ", " << message::file_in_use;
                in_use_marked = true;
            }
            out << ")\n";
        }
    }
    if (overview.revocation_lists_accepted > 0 || overview.revocation_lists_refused > 0)
    {
        print_line(out, message::label_revocation_lists,
                   format(message::lists_counted, {std::to_string(overview.revocation_lists_accepted),
                                                   std::to_string(overview.revocation_lists_refused)}));
    }
    if (!overview.licences.empty() && environment.keys.empty())
    {
        print_line(out, message::note, message::note_no_keys);
    }
    if (overview.folder_full)
    {
        print_line(out, message::note, format(message::note_folder_full, {std::to_string(st::max_files_per_folder)}));
    }
    if (overview.folder_unreadable)
    {
        print_line(out, message::note, message::note_folder_unreadable);
    }
    return exit_ok;
}

int command_import(const Environment& environment, std::string_view file, const Console& console)
{
    if (environment.folders.user.empty())
    {
        return fail(console, "import", std::string(message::no_folder));
    }
    const auto text = read_named(environment, file);
    if (!text)
    {
        return fail(console, "import", text.error());
    }
    auto setup = judge_setup(environment, console, "import");
    if (!setup)
    {
        return exit_failed;
    }
    setup->where.revocations = &setup->withdrawn;

    const auto imported = st::import_file(*environment.files, environment.folders, *text, setup->where);
    if (!imported)
    {
        switch (imported.error().failure)
        {
        case st::ImportFailure::refused:
            return fail(console, "import", refusal_sentence(environment, imported.error().reason));
        case st::ImportFailure::folder_full:
            return fail(console, "import", format(message::folder_full, {std::to_string(st::max_files_per_folder)}));
        case st::ImportFailure::write_failed:
            return fail(console, "import", format(message::write_failed, {plain(environment.folders.user)}));
        }
        return fail(console, "import", format(message::unexpected_failure, {"import"}));
    }

    // A licence that carries a clock reset puts the stored time right, once, and is then judged by that time.
    lic::Status status = imported->status;
    if (status.value)
    {
        const lic::Licence value = *status.value;
        const lic::ClockState reset = lic::apply_reset(setup->clock, value);
        if (!(reset == setup->clock))
        {
            static_cast<void>(st::save_clock(*environment.files, environment.folders, reset));
            setup->where.latest_seen = reset.latest_seen;
            status = lic::evaluate(value, nullptr, setup->where);
        }
    }

    std::ostream& out = console.out;
    const std::string name = plain(imported->name);
    if (imported->already_there)
    {
        out << format(message::already_there, {name}) << '\n';
        return exit_ok;
    }
    if (imported->revocation_list)
    {
        out << format(message::imported_list, {name}) << '\n';
        return exit_ok;
    }
    if (!status.value)
    {
        out << format(message::imported_licence, {std::string("?"), name}) << '\n';
        return exit_ok;
    }
    const lic::Licence& value = *status.value;
    out << format(message::imported_licence, {plain(value.licence_id), name}) << '\n';
    switch (status.state)
    {
    case lic::State::expired:
        out << format(message::ended_imported, {date_of(value.expires_at)}) << '\n';
        break;
    case lic::State::grace:
        out << format(message::grace_imported, {date_of(value.expires_at), date_of(status.grace_ends)}) << '\n';
        break;
    case lic::State::not_yet_valid:
        out << format(message::starts_on, {date_of(value.starts_at)}) << '\n';
        break;
    case lic::State::clock_suspect:
        out << message::clock_looks_wrong << '\n';
        break;
    default:
        out << format(message::now_edition,
                      {value.edition == lic::Edition::enterprise ? message::edition_enterprise : message::edition_pro})
            << '\n';
        break;
    }
    return exit_ok;
}

int command_remove(const Environment& environment, std::string_view name, const Console& console)
{
    if (environment.folders.user.empty())
    {
        return fail(console, "remove", std::string(message::no_folder));
    }
    const std::string shown = plain(name);
    const auto removed = st::remove_file(*environment.files, environment.folders, name);
    if (removed)
    {
        console.out << format(message::removed, {shown}) << '\n';
        return exit_ok;
    }
    switch (removed.error())
    {
    case st::RemoveError::not_allowed:
        return fail(console, "remove", format(message::remove_not_allowed, {shown}));
    case st::RemoveError::not_found:
    {
        // A file the programs read from the folder of this computer is not the person's to remove.
        if (!environment.folders.machine.empty())
        {
            const auto listed = environment.files->list(environment.folders.machine);
            if (listed && std::ranges::find(*listed, name) != listed->end())
            {
                return fail(console, "remove", format(message::remove_in_machine_folder, {shown}));
            }
        }
        return fail(console, "remove", format(message::remove_not_found, {shown}));
    }
    case st::RemoveError::failed:
        return fail(console, "remove", format(message::remove_failed, {shown}));
    }
    return fail(console, "remove", format(message::unexpected_failure, {"remove"}));
}

int command_verify(const Environment& environment, std::string_view file, const Console& console)
{
    if (environment.folders.user.empty())
    {
        return fail(console, "verify", std::string(message::no_folder));
    }
    const auto text = read_named(environment, file);
    if (!text)
    {
        return fail(console, "verify", text.error());
    }
    auto setup = judge_setup(environment, console, "verify");
    if (!setup)
    {
        return exit_failed;
    }
    setup->where.revocations = &setup->withdrawn;

    // The same call as `import` makes, over the same folders, with the writes left out.
    DryRunFiles dry_run{*environment.files};
    const auto taken = st::import_file(dry_run, environment.folders, *text, setup->where);
    std::ostream& out = console.out;
    if (!taken)
    {
        // What the file amounts to, so that the person sees whose it is and why it is not taken.
        const auto envelope = lic::signed_file::parse(*text);
        if (envelope && envelope->purpose == lic::licence_purpose)
        {
            print_status(out, lic::evaluate(*text, setup->where));
        }
        switch (taken.error().failure)
        {
        case st::ImportFailure::refused:
            return fail(console, "verify", refusal_sentence(environment, taken.error().reason));
        case st::ImportFailure::folder_full:
            return fail(console, "verify", format(message::folder_full, {std::to_string(st::max_files_per_folder)}));
        case st::ImportFailure::write_failed:
            return fail(console, "verify", format(message::write_failed, {plain(environment.folders.user)}));
        }
        return fail(console, "verify", format(message::unexpected_failure, {"verify"}));
    }

    if (taken->revocation_list)
    {
        if (const auto list = lic::read_revocations(*text, environment.keys))
        {
            print_line(out, message::label_sequence, std::to_string(list->sequence));
            print_line(out, message::label_licences_withdrawn, std::to_string(list->licence_ids.size()));
            print_line(out, message::label_keys_withdrawn, std::to_string(list->keys.size()));
        }
        out << message::would_accept_list << '\n';
    }
    else
    {
        print_status(out, taken->status);
        out << message::would_accept_licence << '\n';
    }
    if (taken->already_there)
    {
        out << format(message::would_accept_already, {plain(taken->name)}) << '\n';
    }
    return exit_ok;
}

} // namespace adl::licence_cli

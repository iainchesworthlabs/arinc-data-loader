// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
// The licence store: what a scan reads from the two folders and how it judges it, what an import writes and
// refuses, what a removal may touch, and the clock file. The files are held in memory (memory_files.hpp), so a
// folder that cannot be read and a disk that fills are tested like the rest.

#include "adl/licence/licence.hpp"
#include "adl/licence/product.hpp"
#include "adl/licence/store.hpp"
#include "adl/licence/test_keys.hpp"
#include "memory_files.hpp"
#include "support.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

using namespace adl::licence;
using namespace adl::test;
namespace st = adl::licence::store;

namespace
{

const st::Folders folders{"/home/pat/licences", "/etc/licences", "/home/pat/licence-clock.json"};

[[nodiscard]] std::string user_path(std::string_view name)
{
    return std::string{folders.user} + "/" + std::string{name};
}

[[nodiscard]] std::string machine_path(std::string_view name)
{
    return std::string{folders.machine} + "/" + std::string{name};
}

// An annual licence with this number and these dates.
[[nodiscard]] Licence annual_with(std::string_view number, TimePoint starts, TimePoint expires)
{
    Licence value = annual();
    value.licence_id = std::string{number};
    value.starts_at = starts;
    value.expires_at = expires;
    value.issued_at = starts;
    return value;
}

[[nodiscard]] std::string revocation_list_text(std::uint64_t sequence, std::vector<std::string> ids,
                                               std::string_view purpose = revocations_purpose)
{
    RevocationList list;
    list.sequence = sequence;
    list.issued_at = at(2027, 1, 1);
    list.licence_ids = std::move(ids);
    const auto payload = write_revocations(list);
    REQUIRE(payload);
    return signed_payload(*payload, purpose);
}

constexpr auto now = [] { return at(2027, 1, 10); };

} // namespace

TEST_CASE("store: no folder, or a folder that is not there, holds no licence", "[licence][store]")
{
    MemoryFiles files;
    const auto none = st::scan(files, st::Folders{}, on(now()));
    REQUIRE(none.licences.empty());
    REQUIRE(none.best.state == State::none);
    REQUIRE_FALSE(none.folder_full);
    REQUIRE_FALSE(none.folder_unreadable);

    const auto absent = st::scan(files, folders, on(now()));
    REQUIRE(absent.licences.empty());
    REQUIRE(absent.best.state == State::none);
    REQUIRE_FALSE(absent.folder_unreadable);
    // The built-in list is the one in force until a file brings a newer one.
    REQUIRE(absent.revocations == builtin_revocations());
}

TEST_CASE("store: a licence file is read and judged", "[licence][store]")
{
    MemoryFiles files;
    files.put(user_path("ADL-2026-000123.adllic"), signed_text(annual()));

    const auto overview = st::scan(files, folders, on(now()));
    REQUIRE(overview.licences.size() == 1);
    REQUIRE(overview.licences[0].origin == st::Origin::user);
    REQUIRE(overview.licences[0].name == "ADL-2026-000123.adllic");
    REQUIRE(overview.licences[0].status.state == State::valid);
    REQUIRE(overview.best.state == State::valid);
    REQUIRE(overview.best.value->licence_id == "ADL-2026-000123");

    // The same file judged later, in the same folder.
    REQUIRE(st::scan(files, folders, on(at(2028, 1, 1))).best.state == State::expired);
}

TEST_CASE("store: only the files with the store's extensions are read, in name order", "[licence][store]")
{
    MemoryFiles files;
    files.put(user_path("B.adllic"), signed_text(annual_with("ADL-2026-000002", at(2026, 11, 13), at(2027, 11, 13))));
    files.put(user_path("A.ADLLIC"), signed_text(annual_with("ADL-2026-000001", at(2026, 11, 13), at(2027, 11, 13))));
    files.put(user_path("notes.txt"), "not a licence");
    files.put(user_path("C.vsslic"), signed_text(annual()));
    files.put(user_path(".adllic"), signed_text(annual()));

    // The names come in no particular order, so they are tried in two.
    for (const bool reversed : {false, true})
    {
        CAPTURE(reversed);
        files.list_in_reverse = reversed;
        const auto overview = st::scan(files, folders, on(now()));
        REQUIRE(overview.licences.size() == 2);
        // Byte order: upper case sorts before lower case.
        REQUIRE(overview.licences[0].name == "A.ADLLIC");
        REQUIRE(overview.licences[1].name == "B.adllic");
    }
}

TEST_CASE("store: the best of the files in both folders wins", "[licence][store]")
{
    MemoryFiles files;
    // An ended licence in the user's folder, a licence in grace and a valid Pro licence in the machine folder.
    files.put(user_path("ended.adllic"), signed_text(annual_with("ADL-2025-000001", at(2025, 1, 1), at(2026, 1, 1))));
    Licence grace = annual_with("ADL-2026-000002", at(2026, 1, 1), at(2027, 1, 5));
    files.put(machine_path("grace.adllic"), signed_text(grace));
    files.put(machine_path("pro.adllic"),
              signed_text(annual_with("ADL-2026-000003", at(2026, 11, 13), at(2027, 11, 13))));

    auto overview = st::scan(files, folders, on(now()));
    REQUIRE(overview.licences.size() == 3);
    // The user's folder comes first.
    REQUIRE(overview.licences[0].origin == st::Origin::user);
    REQUIRE(overview.licences[1].origin == st::Origin::machine);
    REQUIRE(overview.licences[0].status.state == State::expired);
    REQUIRE(overview.licences[1].status.state == State::grace);
    REQUIRE(overview.licences[2].status.state == State::valid);
    REQUIRE(overview.best.value->licence_id == "ADL-2026-000003");

    // Enterprise beats Pro when both are valid.
    Licence enterprise = full();
    enterprise.machines.clear();
    files.put(machine_path("enterprise.adllic"), signed_text(enterprise));
    overview = st::scan(files, folders, on(now()));
    REQUIRE(overview.best.value->edition == Edition::enterprise);
    REQUIRE(overview.best.value->licence_id == "ADL-2026-000124");

    // The machine folder is read but nothing is written to it.
    REQUIRE(files.creates == 0);
    REQUIRE(files.removes == 0);
}

TEST_CASE("store: a file that cannot be judged is an invalid entry, and does not hide a good one", "[licence][store]")
{
    MemoryFiles files;
    files.put(user_path("a-damaged.adllic"), "this is not a licence");
    files.put(user_path("b-large.adllic"), std::string(signed_file::max_file_size + 1, 'x'));
    files.put(user_path("c-good.adllic"), signed_text(annual()));

    const auto overview = st::scan(files, folders, on(now()));
    REQUIRE(overview.licences.size() == 3);
    REQUIRE(overview.licences[0].status.state == State::invalid);
    REQUIRE(overview.licences[0].status.reason == Reason::malformed);
    REQUIRE(overview.licences[1].status.state == State::invalid);
    REQUIRE(overview.licences[1].status.reason == Reason::malformed);
    REQUIRE(overview.licences[2].status.state == State::valid);
    REQUIRE(overview.best.state == State::valid);

    // A file that cannot be read at all is the same.
    files.read_fails = st::FilesError::failed;
    const auto unreadable = st::scan(files, folders, on(now()));
    REQUIRE(unreadable.licences.size() == 3);
    REQUIRE(unreadable.best.state == State::invalid);
    REQUIRE(unreadable.best.reason == Reason::malformed);
}

TEST_CASE("store: a folder that cannot be listed is reported", "[licence][store]")
{
    MemoryFiles files;
    files.put(machine_path("pro.adllic"), signed_text(annual()));
    files.list_fails = st::FilesError::failed;
    const auto overview = st::scan(files, folders, on(now()));
    REQUIRE(overview.folder_unreadable);
    REQUIRE(overview.licences.empty());
    REQUIRE(overview.best.state == State::none);

    // Not being there is no fault.
    files.list_fails = st::FilesError::not_found;
    REQUIRE_FALSE(st::scan(files, folders, on(now())).folder_unreadable);
}

TEST_CASE("store: at most 64 files are read from a folder", "[licence][store]")
{
    MemoryFiles files;
    for (int i = 1; i <= 70; ++i)
    {
        const std::string number = "ADL-2026-" + std::string(i < 10 ? "00000" : "0000") + std::to_string(i);
        files.put(user_path(number + ".adllic"), signed_text(annual_with(number, at(2026, 11, 13), at(2027, 11, 13))));
    }
    for (const bool reversed : {false, true})
    {
        CAPTURE(reversed);
        files.list_in_reverse = reversed;
        const auto overview = st::scan(files, folders, on(now()));
        REQUIRE(overview.licences.size() == st::max_files_per_folder);
        REQUIRE(overview.folder_full);
        // The first 64 in name order, and not the last.
        REQUIRE(overview.licences.front().name == "ADL-2026-000001.adllic");
        REQUIRE(overview.licences.back().name == "ADL-2026-000064.adllic");
    }

    // Exactly 64 is not too many.
    MemoryFiles exact;
    for (int i = 1; i <= 64; ++i)
    {
        exact.put(user_path("f" + std::to_string(100 + i) + ".adllic"), "x");
    }
    REQUIRE_FALSE(st::scan(exact, folders, on(now())).folder_full);
}

TEST_CASE("store: a signed revocation list in a folder withdraws a licence", "[licence][store][revocation]")
{
    MemoryFiles files;
    files.put(user_path("ADL-2026-000123.adllic"), signed_text(annual()));
    files.put(machine_path("withdrawn.adlrev"), revocation_list_text(3, {"ADL-2026-000123"}));

    const auto overview = st::scan(files, folders, on(now()));
    REQUIRE(overview.revocation_lists_accepted == 1);
    REQUIRE(overview.revocation_lists_refused == 0);
    REQUIRE(overview.revocations.sequence == 3);
    REQUIRE(overview.licences.size() == 1);
    REQUIRE(overview.licences[0].status.state == State::invalid);
    REQUIRE(overview.licences[0].status.reason == Reason::revoked);
    REQUIRE(overview.best.state == State::invalid);

    // The order of the files does not matter: the list sorts after the licence here and before it there.
    files.put(user_path("0000.adlrev"), revocation_list_text(2, {}));
    REQUIRE(st::scan(files, folders, on(now())).revocations.sequence == 3);

    // A list is kept only if it is newer than the one passed in.
    auto where = on(now());
    RevocationList newest;
    newest.sequence = 9;
    newest.issued_at = at(2027, 1, 2);
    where.revocations = &newest;
    const auto kept = st::scan(files, folders, where);
    REQUIRE(kept.revocations.sequence == 9);
    REQUIRE(kept.best.state == State::valid);
}

TEST_CASE("store: a revocation list that cannot be trusted is refused and counted", "[licence][store][revocation]")
{
    MemoryFiles files;
    files.put(user_path("ADL-2026-000123.adllic"), signed_text(annual()));
    files.put(user_path("a.adlrev"), "damaged");
    // Signed for another purpose.
    files.put(user_path("b.adlrev"), revocation_list_text(4, {"ADL-2026-000123"}, licence_purpose));
    // A file that is a list but has the licence extension is judged as a licence, and is not one.
    files.put(user_path("c.adllic"), revocation_list_text(5, {"ADL-2026-000123"}));
    files.read_fails = std::nullopt;

    const auto overview = st::scan(files, folders, on(now()));
    REQUIRE(overview.revocation_lists_accepted == 0);
    REQUIRE(overview.revocation_lists_refused == 2);
    REQUIRE(overview.revocations == builtin_revocations());
    const auto licence = std::ranges::find(overview.licences, "ADL-2026-000123.adllic", &st::Entry::name);
    REQUIRE(licence != overview.licences.end());
    REQUIRE(licence->status.state == State::valid);
    const auto stray = std::ranges::find(overview.licences, "c.adllic", &st::Entry::name);
    REQUIRE(stray != overview.licences.end());
    REQUIRE(stray->status.state == State::invalid);
}

TEST_CASE("store: import copies a licence into the user's folder under its number", "[licence][store][import]")
{
    MemoryFiles files;
    const std::string text = signed_text(annual());
    const auto imported = st::import_file(files, folders, text, on(now()));
    REQUIRE(imported);
    REQUIRE(imported->name == "ADL-2026-000123.adllic");
    REQUIRE_FALSE(imported->revocation_list);
    REQUIRE_FALSE(imported->already_there);
    REQUIRE(imported->status.state == State::valid);
    REQUIRE(imported->status.value->licence_id == "ADL-2026-000123");
    REQUIRE(files.files.at(user_path("ADL-2026-000123.adllic")) == text);
    REQUIRE(files.files.size() == 1);

    // And a scan finds it.
    REQUIRE(st::scan(files, folders, on(now())).best.state == State::valid);
}

TEST_CASE("store: an import never replaces a file", "[licence][store][import]")
{
    MemoryFiles files;
    const std::string first = signed_text(annual());
    // The same licence number with other bytes: a corrected copy.
    Licence other = annual();
    other.licensee_name = "Another Lab Pty Ltd";
    const std::string second = signed_text(other);
    other.licensee_name = "A Third Lab Pty Ltd";
    const std::string third = signed_text(other);
    REQUIRE(first != second);

    REQUIRE(st::import_file(files, folders, first, on(now()))->name == "ADL-2026-000123.adllic");
    const auto again = st::import_file(files, folders, second, on(now()));
    REQUIRE(again);
    REQUIRE(again->name == "ADL-2026-000123-2.adllic");
    REQUIRE(st::import_file(files, folders, third, on(now()))->name == "ADL-2026-000123-3.adllic");
    REQUIRE(files.files.at(user_path("ADL-2026-000123.adllic")) == first);
    REQUIRE(files.files.at(user_path("ADL-2026-000123-2.adllic")) == second);
    REQUIRE(files.files.size() == 3);

    // The same bytes again are the file that is there. Nothing is written.
    const int creates = files.creates;
    const auto repeat = st::import_file(files, folders, second, on(now()));
    REQUIRE(repeat);
    REQUIRE(repeat->already_there);
    REQUIRE(repeat->name == "ADL-2026-000123-2.adllic");
    REQUIRE(files.creates == creates);
    REQUIRE(files.files.size() == 3);

    // A name taken between the listing and the write moves on to the next.
    MemoryFiles racing;
    racing.put(user_path("ADL-2026-000123.adllic"), "x");
    const auto moved = st::import_file(racing, folders, first, on(now()));
    REQUIRE(moved);
    REQUIRE(moved->name == "ADL-2026-000123-2.adllic");
    REQUIRE(racing.files.at(user_path("ADL-2026-000123.adllic")) == "x");
}

TEST_CASE("store: an import refuses what is not a licence a program could use, and writes nothing",
          "[licence][store][import]")
{
    MemoryFiles files;
    const auto refused = [&](const std::string& text, const Context& where, Reason why)
    {
        const auto result = st::import_file(files, folders, text, where);
        REQUIRE_FALSE(result);
        REQUIRE(result.error().failure == st::ImportFailure::refused);
        REQUIRE(result.error().reason == why);
        REQUIRE(files.files.empty());
    };

    refused("this is not a licence", on(now()), Reason::malformed);
    refused(std::string(signed_file::max_file_size + 1, 'x'), on(now()), Reason::malformed);
    refused(signed_text(annual(), revocations_purpose), on(now()), Reason::malformed);

    Licence other_product = annual();
    other_product.product = "vss";
    refused(signed_text(other_product), on(now()), Reason::wrong_product);

    Licence other_computer = annual();
    other_computer.machines = {"OTHER-*"};
    refused(signed_text(other_computer), on(now()), Reason::wrong_computer);

    // A key this version does not know is a licence made for a newer version.
    const auto payload = write_payload(annual());
    REQUIRE(payload);
    refused(signed_payload(*payload, licence_purpose, "lic-2099-a"), on(now()), Reason::newer_version);

    // No trusted keys at all (a list a release does not have, but a build of another product could): every file is
    // refused as made for a newer version.
    auto no_keys = on(now());
    no_keys.keys = {};
    refused(signed_text(annual()), no_keys, Reason::newer_version);

    // Withdrawn by a list the program already has.
    RevocationList list;
    list.sequence = 1;
    list.licence_ids = {"ADL-2026-000123"};
    auto withdrawn = on(now());
    withdrawn.revocations = &list;
    refused(signed_text(annual()), withdrawn, Reason::revoked);

    // A list that cannot be trusted.
    refused(revocation_list_text(1, {}, licence_purpose), on(now()), Reason::malformed);
}

TEST_CASE("store: an import takes a licence that has ended or not started: it is the customer's file",
          "[licence][store][import]")
{
    MemoryFiles files;
    const auto ended = st::import_file(
        files, folders, signed_text(annual_with("ADL-2024-000001", at(2024, 1, 1), at(2025, 1, 1))), on(now()));
    REQUIRE(ended);
    REQUIRE(ended->status.state == State::expired);

    // A renewal sent ahead of time: issued now, and starting in March.
    Licence renewal = annual_with("ADL-2027-000002", at(2027, 3, 1), at(2028, 3, 1));
    renewal.issued_at = at(2027, 1, 1);
    const auto future = st::import_file(files, folders, signed_text(renewal), on(now()));
    REQUIRE(future);
    REQUIRE(future->status.state == State::not_yet_valid);

    // One issued in the future, as a clock that reads too early sees it.
    const auto suspect = st::import_file(
        files, folders, signed_text(annual_with("ADL-2028-000001", at(2028, 1, 1), at(2029, 1, 1))), on(now()));
    REQUIRE(suspect);
    REQUIRE(suspect->status.state == State::clock_suspect);
    REQUIRE(files.files.size() == 3);
}

TEST_CASE("store: an import takes a signed revocation list", "[licence][store][import]")
{
    MemoryFiles files;
    const auto imported = st::import_file(files, folders, revocation_list_text(12, {"ADL-2026-000123"}), on(now()));
    REQUIRE(imported);
    REQUIRE(imported->revocation_list);
    REQUIRE(imported->name == "revocations-12.adlrev");
    REQUIRE(files.has(user_path("revocations-12.adlrev")));

    // Later, a scan applies it.
    files.put(user_path("ADL-2026-000123.adllic"), signed_text(annual()));
    REQUIRE(st::scan(files, folders, on(now())).best.state == State::invalid);
}

TEST_CASE("store: an import into a full folder is refused, and the same file is still there",
          "[licence][store][import]")
{
    MemoryFiles files;
    for (int i = 1; i <= 64; ++i)
    {
        files.put(user_path("f" + std::to_string(100 + i) + ".adllic"), "x");
    }
    const std::string text = signed_text(annual());
    const auto refused = st::import_file(files, folders, text, on(now()));
    REQUIRE_FALSE(refused);
    REQUIRE(refused.error().failure == st::ImportFailure::folder_full);
    REQUIRE(files.files.size() == 64);

    // A file with these bytes is already there, so nothing is needed.
    files.put(user_path("ADL-2026-000123.adllic"), text);
    files.files.erase(user_path("f101.adllic"));
    const auto repeat = st::import_file(files, folders, text, on(now()));
    REQUIRE(repeat);
    REQUIRE(repeat->already_there);

    // Other files in the folder do not count towards the 64.
    MemoryFiles mixed;
    for (int i = 1; i <= 64; ++i)
    {
        mixed.put(user_path("note" + std::to_string(i) + ".txt"), "x");
    }
    REQUIRE(st::import_file(mixed, folders, text, on(now())));
}

TEST_CASE("store: an import says when it could not write", "[licence][store][import]")
{
    MemoryFiles files;
    files.create_fails = st::FilesError::failed;
    const auto result = st::import_file(files, folders, signed_text(annual()), on(now()));
    REQUIRE_FALSE(result);
    REQUIRE(result.error().failure == st::ImportFailure::write_failed);
    REQUIRE(files.files.empty());

    files.create_fails = std::nullopt;
    files.list_fails = st::FilesError::failed;
    const auto unlisted = st::import_file(files, folders, signed_text(annual()), on(now()));
    REQUIRE_FALSE(unlisted);
    REQUIRE(unlisted.error().failure == st::ImportFailure::write_failed);

    // No user folder to write to.
    files.list_fails = std::nullopt;
    const auto nowhere = st::import_file(files, st::Folders{}, signed_text(annual()), on(now()));
    REQUIRE_FALSE(nowhere);
    REQUIRE(nowhere.error().failure == st::ImportFailure::write_failed);
}

TEST_CASE("store: remove deletes one file of the user's folder and nothing else", "[licence][store][remove]")
{
    MemoryFiles files;
    files.put(user_path("a.adllic"), "x");
    files.put(user_path("b.adllic"), "y");
    files.put(machine_path("a.adllic"), "z");
    files.put(machine_path("only-here.adllic"), "w");

    REQUIRE(st::remove_file(files, folders, "a.adllic"));
    REQUIRE_FALSE(files.has(user_path("a.adllic")));
    REQUIRE(files.has(user_path("b.adllic")));
    // The machine folder keeps its file of the same name.
    REQUIRE(files.has(machine_path("a.adllic")));

    // A file the user's folder does not have is not removed from the machine folder.
    const auto missing = st::remove_file(files, folders, "only-here.adllic");
    REQUIRE_FALSE(missing);
    REQUIRE(missing.error() == st::RemoveError::not_found);
    REQUIRE(files.has(machine_path("only-here.adllic")));

    // Case does not change the extension, and a revocation list goes the same way.
    files.put(user_path("list.ADLREV"), "r");
    REQUIRE(st::remove_file(files, folders, "list.ADLREV"));

    files.remove_fails = st::FilesError::failed;
    const auto failed = st::remove_file(files, folders, "b.adllic");
    REQUIRE_FALSE(failed);
    REQUIRE(failed.error() == st::RemoveError::failed);
    REQUIRE(files.has(user_path("b.adllic")));

    files.remove_fails = std::nullopt;
    files.list_fails = st::FilesError::failed;
    REQUIRE(st::remove_file(files, folders, "b.adllic").error() == st::RemoveError::failed);
}

TEST_CASE("store: remove refuses a name that is not a licence file", "[licence][store][remove]")
{
    MemoryFiles files;
    files.put(user_path("notes.txt"), "x");
    files.put(user_path("a.adllic"), "x");
    files.put("/home/pat/secret.adllic", "x");
    files.put("/home/pat/licences/sub/b.adllic", "x");

    for (const std::string_view name : {"", ".adllic", "notes.txt", "../secret.adllic", "sub/b.adllic", "sub\\b.adllic",
                                        "c:a.adllic", "..", ".", "a.adllic\n", "adllic"})
    {
        CAPTURE(name);
        const auto result = st::remove_file(files, folders, name);
        REQUIRE_FALSE(result);
        REQUIRE(result.error() == st::RemoveError::not_allowed);
    }
    REQUIRE(files.removes == 0);
    REQUIRE(files.files.size() == 4);
    // The name of a file that is not there is not allowed to make the folder look empty.
    REQUIRE(st::remove_file(files, folders, "zzz.adllic").error() == st::RemoveError::not_found);
    // No user folder: nothing to remove from.
    REQUIRE(st::remove_file(files, st::Folders{}, "a.adllic").error() == st::RemoveError::not_found);
}

TEST_CASE("store: names with letters outside ASCII pass through as the bytes they are", "[licence][store][utf8]")
{
    // "e acute, space, Japanese" and "u umlaut" and an emoji, as UTF-8 bytes written out, so that the test does
    // not depend on how the compiler reads this file. The store sorts, matches and passes names as bytes.
    const std::string outside = "\xC3\xA9 \xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E";
    const st::Folders wide{"/home/pat/" + outside + "/licences", "/etc/" + outside,
                           "/home/pat/" + outside + "/clock.json"};
    const std::string name = "M\xC3\xBC"
                             "ller \xF0\x9F\x98\x80.adllic";
    const auto in_user = [&](const std::string& file) { return wide.user + "/" + file; };

    MemoryFiles files;
    files.put(in_user(name), signed_text(annual()));
    files.put(wide.machine + "/" + name,
              signed_text(annual_with("ADL-2026-000009", at(2026, 11, 13), at(2027, 11, 13))));
    for (const bool reversed : {false, true})
    {
        files.list_in_reverse = reversed;
        const auto overview = st::scan(files, wide, on(now()));
        REQUIRE(overview.licences.size() == 2);
        REQUIRE(overview.licences[0].name == name);
        REQUIRE(overview.licences[0].origin == st::Origin::user);
        REQUIRE(overview.licences[1].origin == st::Origin::machine);
        REQUIRE(overview.best.state == State::valid);
    }

    // An import goes in under its number, a clock file is kept in the folder with the long name, and a file with such a
    // name is removed by that name and no other.
    const auto imported = st::import_file(
        files, wide, signed_text(annual_with("ADL-2026-000010", at(2026, 11, 13), at(2027, 11, 13))), on(now()));
    REQUIRE(imported);
    REQUIRE(files.has(in_user("ADL-2026-000010.adllic")));
    REQUIRE(st::save_clock(files, wide, ClockState{}));
    REQUIRE(files.has(wide.clock_file));
    REQUIRE(st::remove_file(files, wide, name));
    REQUIRE_FALSE(files.has(in_user(name)));
    REQUIRE(files.has(wide.machine + "/" + name));
    REQUIRE(st::remove_file(files, wide, name).error() == st::RemoveError::not_found);
}

TEST_CASE("store: the clock file is read and written in one form", "[licence][store][clock]")
{
    ClockState state;
    REQUIRE(st::write_clock(state) == "{\"format\": 1, \"applied_resets\": []}\n");
    const auto empty = st::parse_clock(st::write_clock(state));
    REQUIRE(empty);
    REQUIRE(*empty == state);

    state.latest_seen = at(2026, 11, 13, 2) + std::chrono::minutes{10} + std::chrono::seconds{5};
    state.applied_resets = {"reset-2026-11-13-a", "reset-2027-01-05-b"};
    const std::string text = st::write_clock(state);
    REQUIRE(text == "{\"format\": 1, \"latest_seen\": \"2026-11-13T02:10:05Z\", \"applied_resets\": "
                    "[\"reset-2026-11-13-a\", \"reset-2027-01-05-b\"]}\n");
    const auto parsed = st::parse_clock(text);
    REQUIRE(parsed);
    REQUIRE(*parsed == state);

    // Other layouts of the same data are read, and fields this version does not know are ignored.
    const auto spaced = st::parse_clock(
        "{ \"applied_resets\" : [ ], \"format\" : 1, \"latest_seen\" : \"2026-11-13T02:10:05Z\", \"other\": [1, {}] }");
    REQUIRE(spaced);
    REQUIRE(spaced->latest_seen == state.latest_seen);
    REQUIRE(st::parse_clock(R"({"format":1,"latest_seen":null})")->latest_seen == TimePoint::min());
}

TEST_CASE("store: a clock file that is not the form is refused", "[licence][store][clock]")
{
    const auto refuses = [](std::string_view text, Error why)
    {
        CAPTURE(text);
        const auto result = st::parse_clock(text);
        REQUIRE_FALSE(result);
        REQUIRE(result.error() == why);
    };
    refuses("", Error::malformed);
    refuses("[]", Error::malformed);
    refuses("{}", Error::malformed);
    refuses(R"({"format":0})", Error::malformed);
    refuses(R"({"format":"1"})", Error::malformed);
    refuses(R"({"format":1.5})", Error::malformed);
    // Made by a newer version.
    refuses(R"({"format":2})", Error::newer_version);
    refuses(R"({"format":1,"format":1})", Error::malformed);
    refuses(R"({"format":1,"latest_seen":"yesterday"})", Error::malformed);
    refuses(R"({"format":1,"latest_seen":"2026-11-13 02:10:05"})", Error::malformed);
    refuses(R"({"format":1,"latest_seen":7})", Error::malformed);
    refuses(R"({"format":1,"applied_resets":"a"})", Error::malformed);
    refuses(R"({"format":1,"applied_resets":[1]})", Error::malformed);
    refuses(R"({"format":1,"applied_resets":[""]})", Error::malformed);
    refuses("{\"format\":1,\"applied_resets\":[\"a\\nb\"]}", Error::malformed);
    refuses(R"({"format":1,} )", Error::malformed);
    refuses(R"({"format":1} extra)", Error::malformed);
    refuses(std::string(signed_file::max_file_size + 1, ' '), Error::too_large);

    // A token is at most 64 characters, and 32 are kept.
    refuses(R"({"format":1,"applied_resets":[")" + std::string(65, 'a') + R"("]})", Error::malformed);
    std::string many = R"({"format":1,"applied_resets":[)";
    for (std::size_t i = 0; i <= max_applied_resets; ++i)
    {
        many += (i == 0 ? "\"t" : ",\"t") + std::to_string(i) + "\"";
    }
    refuses(many + "]}", Error::malformed);
}

TEST_CASE("store: the clock state is loaded and saved", "[licence][store][clock]")
{
    MemoryFiles files;
    // Nothing there: nothing seen.
    REQUIRE(st::load_clock(files, folders) == ClockState{});

    ClockState state;
    state.latest_seen = at(2027, 1, 10);
    state.applied_resets = {"a"};
    REQUIRE(st::save_clock(files, folders, state));
    REQUIRE(files.has(folders.clock_file));
    REQUIRE(st::load_clock(files, folders) == state);

    // Saving again replaces the file.
    state.latest_seen = at(2027, 1, 11);
    REQUIRE(st::save_clock(files, folders, state));
    REQUIRE(st::load_clock(files, folders).latest_seen == at(2027, 1, 11));
    REQUIRE(files.replaces == 2);

    // Damaged, unreadable or too large: the default, and the program carries on.
    files.put(folders.clock_file, "{ damaged");
    REQUIRE(st::load_clock(files, folders) == ClockState{});
    files.put(folders.clock_file, std::string(signed_file::max_file_size + 1, ' '));
    REQUIRE(st::load_clock(files, folders) == ClockState{});
    files.put(folders.clock_file, st::write_clock(state));
    files.read_fails = st::FilesError::failed;
    REQUIRE(st::load_clock(files, folders) == ClockState{});
    files.read_fails = std::nullopt;

    // A failed write says so, and the old file stays.
    files.replace_fails = st::FilesError::failed;
    state.latest_seen = at(2028, 1, 1);
    const auto failed = st::save_clock(files, folders, state);
    REQUIRE_FALSE(failed);
    REQUIRE(failed.error() == st::FilesError::failed);
    files.replace_fails = std::nullopt;
    REQUIRE(st::load_clock(files, folders).latest_seen == at(2027, 1, 11));

    // No clock file configured: nothing is read and nothing can be saved.
    st::Folders without{folders.user, folders.machine, ""};
    REQUIRE(st::load_clock(files, without) == ClockState{});
    REQUIRE_FALSE(st::save_clock(files, without, state));
}

TEST_CASE("store: a clock that ran ahead is put right by a licence that carries a reset", "[licence][store][clock]")
{
    MemoryFiles files;
    // The clock once read 2030, so the program remembers that.
    ClockState stored;
    stored.latest_seen = at(2030, 1, 1);
    REQUIRE(st::save_clock(files, folders, stored));
    files.put(user_path("ADL-2026-000123.adllic"), signed_text(annual()));

    auto where = on(now());
    where.latest_seen = st::load_clock(files, folders).latest_seen;
    REQUIRE(st::scan(files, folders, where).best.state == State::clock_suspect);

    // The owner sends a licence with a reset token. Importing it and applying the token puts the time back to
    // the day it was issued, once.
    Licence reset = annual_with("ADL-2027-000001", at(2026, 12, 20), at(2027, 12, 20));
    reset.clock_reset = "reset-1";
    const auto imported = st::import_file(files, folders, signed_text(reset), where);
    REQUIRE(imported);
    REQUIRE(imported->status.value);
    const ClockState applied = apply_reset(st::load_clock(files, folders), *imported->status.value);
    REQUIRE(applied.latest_seen == at(2026, 12, 20));
    REQUIRE(applied.applied_resets == std::vector<std::string>{"reset-1"});
    REQUIRE(st::save_clock(files, folders, applied));

    where.latest_seen = st::load_clock(files, folders).latest_seen;
    REQUIRE(st::scan(files, folders, where).best.state == State::valid);

    // Importing the same file again resets nothing a second time.
    const ClockState again = apply_reset(observe(applied, at(2027, 1, 10)), *imported->status.value);
    REQUIRE(again.latest_seen == at(2027, 1, 10));
}

// SPDX-License-Identifier: GPL-3.0-or-later
// The commands of adl_licence in process, over licence files held in memory, with the published test key of
// RFC 8032 and a time of the test's own: what `show`, `import`, `remove` and `verify` print and return for every
// state a licence can be in, for every way a file can be refused, and for every way the computer can fail them.
// The states and files of the manifest are in manifest_test.cpp; the same commands over this computer's own file
// system are in real_files_test.cpp, and the built program is in test_adl_licence.py. Strings that are not ASCII
// are written as byte escapes, so no compiler setting can change them.

#include "adl/licence/licence.hpp"
#include "adl/licence/store.hpp"
#include "adl/licence/test_keys.hpp"
#include "adl/licence_cli/licence_cli.hpp"
#include "memory_files.hpp"
#include "messages.hpp"
#include "support.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lic = adl::licence;
namespace st = adl::licence::store;
namespace cli = adl::licence_cli;
using adl::test::annual;
using adl::test::at;
using adl::test::MemoryFiles;
using adl::test::signed_payload;
using adl::test::signed_text;

namespace
{

struct Ran
{
    int code = 0;
    std::string out;
    std::string err;
};

// The files of one computer and the environment a command runs in: this computer is LAB-RIG-07, it trusts the
// test key, and its clock reads 1 December 2026 unless a test sets it.
class Rig
{
public:
    MemoryFiles files;
    cli::Environment env;

    explicit Rig(lic::TimePoint now = at(2026, 12, 1))
    {
        env.files = &files;
        env.folders = {"home/licences", "machine/licences", "home/licence-clock.json"};
        env.computer_name = "LAB-RIG-07";
        const auto test_key = lic::test_keys::trusted();
        env.keys.assign(test_key.begin(), test_key.end());
        env.version = "v1.2.3-test";
        set_now(now);
    }

    void set_now(lic::TimePoint now)
    {
        env.now = [now]() -> std::optional<lic::TimePoint> { return now; };
    }

    [[nodiscard]] Ran run(std::vector<std::string> arguments) const
    {
        std::ostringstream out;
        std::ostringstream err;
        const int code = cli::run(arguments, env, {out, err});
        return {code, out.str(), err.str()};
    }

    [[nodiscard]] std::string user(std::string_view name) const
    {
        return std::string{"home/licences/"} + std::string{name};
    }
    [[nodiscard]] std::string machine(std::string_view name) const
    {
        return std::string{"machine/licences/"} + std::string{name};
    }
};

[[nodiscard]] lic::Licence numbered(std::string_view number, lic::TimePoint starts, lic::TimePoint expires,
                                    int grace_days = 28)
{
    lic::Licence value = annual();
    value.licence_id = std::string{number};
    value.issued_at = starts;
    value.starts_at = starts;
    value.expires_at = expires;
    value.grace_days = grace_days;
    return value;
}

[[nodiscard]] std::string list_text(std::uint64_t sequence, std::vector<std::string> ids)
{
    lic::RevocationList list;
    list.sequence = sequence;
    list.issued_at = at(2026, 11, 20);
    list.licence_ids = std::move(ids);
    const auto payload = lic::write_revocations(list);
    REQUIRE(payload);
    return signed_payload(*payload, lic::revocations_purpose);
}

// A licence file with its signature changed in one character.
[[nodiscard]] std::string with_bad_signature(std::string text)
{
    const std::string marker = "\"signature\":\"";
    const auto at_signature = text.find(marker);
    REQUIRE(at_signature != std::string::npos);
    char& first = text[at_signature + marker.size()];
    first = first == 'A' ? 'B' : 'A';
    return text;
}

[[nodiscard]] bool contains(const std::string& text, std::string_view part)
{
    return text.find(part) != std::string::npos;
}

// What `show` prints for the annual licence of the tests at the rig's time.
const std::string annual_lines =
    "Edition: pro\n"
    "State: valid\n"
    "Licence: ADL-2026-000123\n"
    "Kind: annual\n"
    "Licensed to: Example Lab Pty Ltd\n"
    "Term: 2026-11-13 to 2027-11-13\n"
    "Days left: 347\n"
    "Seats: 3\n"
    "Features: support\n"
    "Computers: any\n";

} // namespace

TEST_CASE("adl_licence --help prints the usage and --version the version", "[unit][licence_cli]")
{
    Rig rig;
    const Ran help = rig.run({"--help"});
    CHECK(help.code == cli::exit_ok);
    CHECK(help.out == std::string(cli::message::usage));
    CHECK(help.out == cli::usage_text());
    CHECK(help.err.empty());

    const Ran version = rig.run({"--version"});
    CHECK(version.code == cli::exit_ok);
    CHECK(version.out == "adl_licence v1.2.3-test\n");
    CHECK(version.err.empty());

    rig.env.version.clear();
    CHECK(rig.run({"--version"}).out == "adl_licence unknown\n");
}

TEST_CASE("a command line that cannot be used exits 2 with the reason and the usage, and does nothing",
          "[unit][licence_cli]")
{
    Rig rig;
    rig.files.put(rig.user("ADL-2026-000123.adllic"), signed_text(annual()));
    const auto before = rig.files.files;

    struct Case
    {
        std::vector<std::string> arguments;
        std::string sentence;
    };
    const std::vector<Case> cases{
        {{}, "give a command"},
        {{"frobnicate"}, "unknown command frobnicate"},
        {{"--frobnicate"}, "unknown option --frobnicate"},
        {{"show", "x"}, "show takes no argument"},
        {{"show", "--json"}, "unknown option --json"},
        {{"import"}, "import needs the name of one file"},
        {{"import", "a", "b"}, "import needs the name of one file"},
        {{"import", "--now"}, "unknown option --now"},
        {{"verify"}, "verify needs the name of one file"},
        {{"verify", "a", "b"}, "verify needs the name of one file"},
        {{"remove"}, "remove needs the name of one licence file, as show prints it"},
        {{"remove", "a", "b"}, "remove needs the name of one licence file, as show prints it"},
        {{"--help", "show"}, "--help cannot be given with anything else"},
        {{"--version", "show"}, "--version cannot be given with anything else"},
        // What a person typed is echoed with its control characters replaced.
        {{"fro\x1b[31mb"}, "unknown command fro?[31mb"},
    };
    for (const Case& test : cases)
    {
        CAPTURE(test.sentence);
        const Ran ran = rig.run(test.arguments);
        CHECK(ran.code == cli::exit_usage);
        CHECK(ran.out.empty());
        CHECK(ran.err == "adl_licence: " + test.sentence + "\n" + std::string(cli::message::usage));
    }
    CHECK(rig.files.files == before);
    CHECK(rig.files.creates == 0);
    CHECK(rig.files.removes == 0);
}

TEST_CASE("show: no licence file is the Community edition", "[unit][licence_cli][show]")
{
    Rig rig;
    const Ran ran = rig.run({"show"});
    CHECK(ran.code == cli::exit_ok);
    CHECK(ran.out ==
          "Edition: community\n"
          "State: none\n"
          "Reason: there is no licence\n"
          "Files: none\n");
    CHECK(ran.err.empty());
    // It reads and writes nothing: not a folder, not the clock file.
    CHECK(rig.files.files.empty());
    CHECK(rig.files.creates == 0);
    CHECK(rig.files.replaces == 0);
    CHECK(rig.files.removes == 0);
}

TEST_CASE("show: a licence in force, its dates in UTC, and the file it came from", "[unit][licence_cli][show]")
{
    Rig rig;
    rig.files.put(rig.user("ADL-2026-000123.adllic"), signed_text(annual()));
    const Ran ran = rig.run({"show"});
    CHECK(ran.code == cli::exit_ok);
    CHECK(ran.out == annual_lines + "Files:\n  ADL-2026-000123.adllic (user, valid, in use)\n");
    CHECK(ran.err.empty());
    // The email address of a licence is not printed, nor the contact, nor anything of the signature.
    CHECK_FALSE(contains(ran.out, "example-lab.invalid"));
    CHECK_FALSE(contains(ran.out, "Test team"));
    CHECK_FALSE(contains(ran.out, "test-rfc8032-1"));
    CHECK(rig.files.creates == 0);
    CHECK(rig.files.replaces == 0);

    SECTION("a licence that ends soon shows the days left")
    {
        rig.set_now(at(2027, 10, 20));
        const Ran soon = rig.run({"show"});
        CHECK(contains(soon.out, "State: expiring\n"));
        CHECK(contains(soon.out, "Days left: 24\n"));
    }
    SECTION("a licence in its grace period shows when the grace ends, and is not Pro")
    {
        rig.set_now(at(2027, 11, 20));
        const Ran grace = rig.run({"show"});
        CHECK(contains(grace.out, "State: grace\n"));
        CHECK(contains(grace.out, "Grace ends: 2027-12-11\n"));
        CHECK_FALSE(contains(grace.out, "Days left"));
    }
    SECTION("an ended licence is Community, and says so")
    {
        rig.set_now(at(2028, 6, 1));
        const Ran ended = rig.run({"show"});
        CHECK(contains(ended.out, "Edition: community\n"));
        CHECK(contains(ended.out, "State: expired\n"));
        CHECK(contains(ended.out, "Reason: the licence has ended\n"));
        CHECK(contains(ended.out, "Licence: ADL-2026-000123\n"));
    }
}

TEST_CASE("show: every field of a licence that has them", "[unit][licence_cli][show]")
{
    Rig rig;
    rig.files.put(rig.user("ADL-2026-000124.adllic"), signed_text(adl::test::full()));
    rig.env.computer_name = "LAB-RIG-07";
    const Ran ran = rig.run({"show"});
    CHECK(ran.code == cli::exit_ok);
    CHECK(ran.out ==
          "Edition: enterprise\n"
          "State: valid\n"
          "Licence: ADL-2026-000124\n"
          "Kind: enterprise\n"
          "Licensed to: Example Avionics Ltd\n"
          "Term: 2026-11-13 to 2028-11-13\n"
          "Days left: 713\n"
          "Seats: site licence\n"
          "Features: support, pro.*, enterprise.*, data_packs\n"
          "Computers: LAB-RIG-*, bench-3\n"
          "Updates until: 2028-11-13\n"
          "Files:\n"
          "  ADL-2026-000124.adllic (user, valid, in use)\n");
    CHECK_FALSE(contains(ran.out, "tests@example-lab.invalid"));
}

TEST_CASE("show: several files, the one in use marked, and the folder of this computer", "[unit][licence_cli][show]")
{
    Rig rig;
    const lic::Licence old_one = numbered("ADL-2025-000007", at(2025, 1, 1), at(2026, 1, 1), 0);
    rig.files.put(rig.user("ADL-2026-000123.adllic"), signed_text(annual()));
    rig.files.put(rig.machine("ADL-2025-000007.adllic"), signed_text(old_one));
    rig.files.put(rig.machine("damaged.adllic"), "this is not a licence");
    rig.files.put(rig.user("notes.txt"), "not a licence file at all");

    const Ran ran = rig.run({"show"});
    CHECK(ran.code == cli::exit_ok);
    CHECK(ran.out == annual_lines +
                         "Files:\n"
                         "  ADL-2026-000123.adllic (user, valid, in use)\n"
                         "  ADL-2025-000007.adllic (machine, expired)\n"
                         "  damaged.adllic (machine, invalid, the file is damaged or is not a licence)\n");
}

TEST_CASE("show: a list of withdrawn licences in a folder withdraws a licence", "[unit][licence_cli][show]")
{
    Rig rig;
    rig.files.put(rig.user("ADL-2026-000123.adllic"), signed_text(annual()));
    rig.files.put(rig.user("revocations-1.adlrev"), list_text(1, {"ADL-2026-000123"}));
    rig.files.put(rig.user("revocations-2.adlrev"), "not a list");

    const Ran ran = rig.run({"show"});
    CHECK(ran.code == cli::exit_ok);
    CHECK(contains(ran.out, "Edition: community\n"));
    CHECK(contains(ran.out, "State: invalid\n"));
    CHECK(contains(ran.out, "Reason: the licence or its key was withdrawn\n"));
    CHECK(contains(ran.out, "  ADL-2026-000123.adllic (user, invalid, the licence or its key was withdrawn"));
    CHECK(contains(ran.out, "Revocation lists: 1 accepted, 1 refused\n"));
}

TEST_CASE("show: a release that trusts no key says it accepts no licence", "[unit][licence_cli][show]")
{
    Rig rig;
    rig.env.keys.clear();
    rig.files.put(rig.user("ADL-2026-000123.adllic"), signed_text(annual()));
    const Ran ran = rig.run({"show"});
    CHECK(ran.code == cli::exit_ok);
    CHECK(contains(ran.out, "Edition: community\n"));
    CHECK(contains(ran.out, "  ADL-2026-000123.adllic (user, invalid, the licence needs a newer version"));
    CHECK(contains(ran.out, "Note: This version of ARINC Data Loader cannot check licence files yet, so it accepts none."));

    // With no file there is nothing to explain.
    Rig empty;
    empty.env.keys.clear();
    CHECK_FALSE(contains(empty.run({"show"}).out, "Note:"));
}

TEST_CASE("show: a folder that holds too much or cannot be read is reported, and nothing is hidden", "[unit][licence_cli][show]")
{
    SECTION("more than 64 files")
    {
        Rig rig;
        for (int i = 0; i < 65; ++i)
        {
            rig.files.put(rig.user("a" + std::to_string(1000 + i) + ".adllic"), "not a licence");
        }
        const Ran ran = rig.run({"show"});
        CHECK(ran.code == cli::exit_ok);
        CHECK(contains(ran.out, "Note: A licence folder holds more than 64 files; the rest were not read.\n"));
    }
    SECTION("a folder that cannot be listed")
    {
        Rig rig;
        rig.files.list_fails = st::FilesError::failed;
        const Ran ran = rig.run({"show"});
        CHECK(ran.code == cli::exit_ok);
        CHECK(contains(ran.out, "Files: none\n"));
        CHECK(contains(ran.out, "Note: A licence folder could not be read.\n"));
    }
}

TEST_CASE("show: a clock that ran ahead makes the licence wait to be judged", "[unit][licence_cli][show]")
{
    Rig rig;
    rig.files.put(rig.user("ADL-2026-000123.adllic"), signed_text(annual()));
    rig.files.put(rig.env.folders.clock_file, st::write_clock({.latest_seen = at(2027, 6, 1), .applied_resets = {}}));
    const Ran ran = rig.run({"show"});
    CHECK(ran.code == cli::exit_ok);
    CHECK(contains(ran.out, "State: clock_suspect\n"));
    CHECK(contains(ran.out, "Edition: community\n"));
    // It looked at the clock file and did not write it.
    CHECK(rig.files.replaces == 0);
}

TEST_CASE("show, import, remove and verify say when the computer cannot do what they need", "[unit][licence_cli]")
{
    Rig rig;
    rig.files.put("inbox/a.adllic", signed_text(annual()));

    SECTION("the clock cannot be read")
    {
        rig.env.now = []() -> std::optional<lic::TimePoint> { return std::nullopt; };
        const std::vector<std::vector<std::string>> commands{
            {"show"}, {"import", "inbox/a.adllic"}, {"verify", "inbox/a.adllic"}};
        for (const std::vector<std::string>& arguments : commands)
        {
            CAPTURE(arguments.front());
            const Ran ran = rig.run(arguments);
            CHECK(ran.code == cli::exit_failed);
            CHECK(ran.out.empty());
            CHECK(ran.err == arguments.front() + ": the clock of this computer cannot be read, so a licence cannot be judged\n");
        }
        CHECK(rig.files.creates == 0);
    }
    SECTION("there is no licence folder")
    {
        rig.env.folders = {};
        const std::vector<std::vector<std::string>> commands{
            {"show"}, {"import", "inbox/a.adllic"}, {"verify", "inbox/a.adllic"}, {"remove", "a.adllic"}};
        for (const std::vector<std::string>& arguments : commands)
        {
            CAPTURE(arguments.front());
            const Ran ran = rig.run(arguments);
            CHECK(ran.code == cli::exit_failed);
            CHECK(ran.err == arguments.front() + ": this computer has no folder for licence files\n");
        }
    }
    SECTION("a clock that is not set at all")
    {
        rig.env.now = nullptr;
        CHECK(rig.run({"show"}).code == cli::exit_failed);
    }
}

TEST_CASE("import: a licence is copied into the user's folder under its number, once", "[unit][licence_cli][import]")
{
    Rig rig;
    const std::string text = signed_text(annual());
    rig.files.put("inbox/from the customer.adllic", text);

    const Ran first = rig.run({"import", "inbox/from the customer.adllic"});
    CHECK(first.code == cli::exit_ok);
    CHECK(first.out ==
          "Imported licence ADL-2026-000123 as ADL-2026-000123.adllic.\n"
          "This is now the Pro edition.\n");
    CHECK(first.err.empty());
    REQUIRE(rig.files.has(rig.user("ADL-2026-000123.adllic")));
    CHECK(rig.files.files.at(rig.user("ADL-2026-000123.adllic")) == text);
    // The file it came from is not touched.
    CHECK(rig.files.files.at("inbox/from the customer.adllic") == text);
    CHECK(rig.files.removes == 0);

    const Ran again = rig.run({"import", "inbox/from the customer.adllic"});
    CHECK(again.code == cli::exit_ok);
    CHECK(again.out == "That file is already in the licence folder as ADL-2026-000123.adllic. Nothing was copied.\n");
    CHECK(rig.files.creates == 1);

    // And `show` now sees it.
    CHECK(contains(rig.run({"show"}).out, "State: valid\n"));
}

TEST_CASE("import: an Enterprise licence says so", "[unit][licence_cli][import]")
{
    Rig rig;
    rig.files.put("inbox/e.adllic", signed_text(adl::test::full()));
    CHECK(rig.run({"import", "inbox/e.adllic"}).out ==
          "Imported licence ADL-2026-000124 as ADL-2026-000124.adllic.\nThis is now the Enterprise edition.\n");
}

TEST_CASE("import: a licence that has ended, has not started or is judged by a wrong clock is taken, and says so",
          "[unit][licence_cli][import]")
{
    Rig rig;
    const auto imported = [&](const lic::Licence& value)
    {
        rig.files.put("inbox/x.adllic", signed_text(value));
        const Ran ran = rig.run({"import", "inbox/x.adllic"});
        CHECK(ran.code == cli::exit_ok);
        CHECK(ran.err.empty());
        return ran.out;
    };

    const std::string ended = imported(numbered("ADL-2025-000007", at(2025, 1, 1), at(2026, 1, 1), 0));
    CHECK(ended ==
          "Imported licence ADL-2025-000007 as ADL-2025-000007.adllic.\n"
          "This licence ended on 2026-01-01. It was imported, but it includes nothing now.\n");

    const std::string grace = imported(numbered("ADL-2026-000008", at(2025, 12, 1), at(2026, 11, 20)));
    CHECK(grace ==
          "Imported licence ADL-2026-000008 as ADL-2026-000008.adllic.\n"
          "This licence ended on 2026-11-20. It was imported, and its grace period lasts until 2026-12-18.\n");

    // Issued before this computer's time, and starting after it.
    lic::Licence not_started = numbered("ADL-2026-000009", at(2027, 1, 1), at(2028, 1, 1));
    not_started.issued_at = at(2026, 11, 1);
    const std::string later = imported(not_started);
    CHECK(later ==
          "Imported licence ADL-2026-000009 as ADL-2026-000009.adllic.\n"
          "This licence starts on 2027-01-01.\n");

    rig.files.put(rig.env.folders.clock_file, st::write_clock({.latest_seen = at(2027, 6, 1), .applied_resets = {}}));
    const std::string wrong_clock = imported(annual());
    CHECK(wrong_clock ==
          "Imported licence ADL-2026-000123 as ADL-2026-000123.adllic.\n"
          "The date on this computer looks wrong, so the licence cannot be judged yet. It was imported.\n");
}

TEST_CASE("import: a list of withdrawn licences is copied, and then the licence on it is withdrawn", "[unit][licence_cli][import]")
{
    Rig rig;
    rig.files.put(rig.user("ADL-2026-000123.adllic"), signed_text(annual()));
    rig.files.put("inbox/list.adlrev", list_text(4, {"ADL-2026-000123"}));

    const Ran ran = rig.run({"import", "inbox/list.adlrev"});
    CHECK(ran.code == cli::exit_ok);
    CHECK(ran.out == "Imported the list of withdrawn licences as revocations-4.adlrev. A licence on it is shown as withdrawn.\n");
    CHECK(rig.files.has(rig.user("revocations-4.adlrev")));
    CHECK(contains(rig.run({"show"}).out, "State: invalid\n"));
    CHECK(rig.run({"import", "inbox/list.adlrev"}).out ==
          "That file is already in the licence folder as revocations-4.adlrev. Nothing was copied.\n");
}

TEST_CASE("import: a licence that a list in the folder withdraws is refused", "[unit][licence_cli][import]")
{
    Rig rig;
    rig.files.put(rig.user("revocations-1.adlrev"), list_text(1, {"ADL-2026-000123"}));
    rig.files.put("inbox/a.adllic", signed_text(annual()));
    const Ran ran = rig.run({"import", "inbox/a.adllic"});
    CHECK(ran.code == cli::exit_failed);
    CHECK(ran.err == "import: This licence was withdrawn. Contact support.\n");
    CHECK_FALSE(rig.files.has(rig.user("ADL-2026-000123.adllic")));
}

TEST_CASE("import: a clock reset puts the stored time right, once", "[unit][licence_cli][import]")
{
    Rig rig;
    // The clock file says a time far ahead of this computer's clock, so a licence is judged `clock_suspect`.
    rig.files.put(rig.env.folders.clock_file, st::write_clock({.latest_seen = at(2027, 6, 1), .applied_resets = {}}));
    lic::Licence value = annual();
    value.clock_reset = "reset-2026-12-a";
    value.issued_at = at(2026, 11, 30);
    rig.files.put("inbox/reset.adllic", signed_text(value));

    const Ran ran = rig.run({"import", "inbox/reset.adllic"});
    CHECK(ran.code == cli::exit_ok);
    CHECK(ran.out ==
          "Imported licence ADL-2026-000123 as ADL-2026-000123.adllic.\n"
          "This is now the Pro edition.\n");
    const auto stored = st::parse_clock(rig.files.files.at(rig.env.folders.clock_file));
    REQUIRE(stored);
    CHECK(stored->latest_seen == at(2026, 11, 30));
    CHECK(stored->applied_resets == std::vector<std::string>{"reset-2026-12-a"});
    // The licence is in force now, by the corrected time.
    CHECK(contains(rig.run({"show"}).out, "State: valid\n"));
}

TEST_CASE("import: what is refused, and why, and nothing is written", "[unit][licence_cli][import]")
{
    Rig rig;
    const auto refused = [&](const std::string& text, std::string_view sentence)
    {
        rig.files.put("inbox/x.adllic", text);
        const Ran ran = rig.run({"import", "inbox/x.adllic"});
        CAPTURE(sentence);
        CHECK(ran.code == cli::exit_failed);
        CHECK(ran.out.empty());
        CHECK(ran.err == "import: " + std::string(sentence) + "\n");
        CHECK(rig.files.creates == 0);
        CHECK(rig.files.replaces == 0);
        CHECK(rig.files.removes == 0);
    };
    const std::string damaged =
        "This file is not a valid ARINC Data Loader licence. It may be damaged or incomplete. Ask for the file again.";

    refused("this is not a licence", damaged);
    refused(with_bad_signature(signed_text(annual())), damaged);
    refused(signed_text(annual(), lic::revocations_purpose), damaged);

    lic::Licence other_product = annual();
    other_product.product = "vss";
    refused(signed_text(other_product),
            "This licence is for another product. Ask for an ARINC Data Loader licence.");

    lic::Licence other_computer = annual();
    other_computer.machines = {"OTHER-*"};
    refused(signed_text(other_computer),
            "This licence is for other computers. This computer is LAB-RIG-07. Ask your administrator for a "
            "licence that names it.");

    const auto payload = lic::write_payload(annual());
    REQUIRE(payload);
    refused(signed_payload(*payload, lic::licence_purpose, "lic-2099-a"),
            "This licence needs a newer version of ARINC Data Loader. Install the latest version, then import the "
            "licence again.");

    rig.env.keys.clear();
    refused(signed_text(annual()),
            "This version of ARINC Data Loader cannot check licence files yet, so it accepts none. The Community "
            "edition works as it always did.");
}

TEST_CASE("import: a file that cannot be read, a folder that is full and a write that fails", "[unit][licence_cli][import]")
{
    Rig rig;
    SECTION("no such file")
    {
        const Ran ran = rig.run({"import", "inbox/missing.adllic"});
        CHECK(ran.code == cli::exit_failed);
        CHECK(ran.err == "import: cannot read inbox/missing.adllic: it does not exist\n");
    }
    SECTION("a file larger than a licence file can be")
    {
        rig.files.put("inbox/big.adllic", std::string(64 * 1024 + 1, 'x'));
        const Ran ran = rig.run({"import", "inbox/big.adllic"});
        CHECK(ran.code == cli::exit_failed);
        CHECK(ran.err == "import: cannot read inbox/big.adllic: it is larger than 65536 bytes, which no licence file is\n");
    }
    SECTION("a file that cannot be read")
    {
        rig.files.put("inbox/a.adllic", signed_text(annual()));
        rig.files.read_fails = st::FilesError::failed;
        const Ran ran = rig.run({"import", "inbox/a.adllic"});
        CHECK(ran.code == cli::exit_failed);
        CHECK(ran.err == "import: cannot read inbox/a.adllic: it is not a file, or it could not be read\n");
    }
    SECTION("an empty name")
    {
        const Ran ran = rig.run({"import", ""});
        CHECK(ran.code == cli::exit_failed);
        CHECK(contains(ran.err, "it is not a file, or it could not be read"));
    }
    SECTION("a full folder")
    {
        for (int i = 0; i < 64; ++i)
        {
            rig.files.put(rig.user("a" + std::to_string(1000 + i) + ".adllic"), "not a licence");
        }
        rig.files.put("inbox/a.adllic", signed_text(annual()));
        const Ran ran = rig.run({"import", "inbox/a.adllic"});
        CHECK(ran.code == cli::exit_failed);
        CHECK(ran.err == "import: The licence folder already holds 64 files. Remove one, then import again.\n");
    }
    SECTION("a disk that is full")
    {
        rig.files.put("inbox/a.adllic", signed_text(annual()));
        rig.files.create_fails = st::FilesError::failed;
        const Ran ran = rig.run({"import", "inbox/a.adllic"});
        CHECK(ran.code == cli::exit_failed);
        CHECK(ran.err == "import: The licence could not be saved in home/licences. Check that the folder can be written.\n");
    }
}

TEST_CASE("import: a name outside ASCII is passed on as the bytes it is", "[unit][licence_cli][import]")
{
    Rig rig;
    const std::string name = "inbox/\xC3\xA9\xE6\x97\xA5" ".adllic";
    rig.files.put(name, signed_text(annual()));
    const Ran ran = rig.run({"import", name});
    CHECK(ran.code == cli::exit_ok);
    CHECK(rig.files.has(rig.user("ADL-2026-000123.adllic")));

    const Ran missing = rig.run({"import", "inbox/\xC3\xA9" ".nope"});
    CHECK(missing.err == "import: cannot read inbox/\xC3\xA9" ".nope: it does not exist\n");
}

TEST_CASE("remove: one file of the user's folder, by the name show prints", "[unit][licence_cli][remove]")
{
    Rig rig;
    rig.files.put(rig.user("ADL-2026-000123.adllic"), signed_text(annual()));
    rig.files.put(rig.user("revocations-1.adlrev"), list_text(1, {}));
    rig.files.put(rig.machine("ADL-2025-000007.adllic"), signed_text(numbered("ADL-2025-000007", at(2025, 1, 1), at(2026, 1, 1))));

    const Ran ran = rig.run({"remove", "ADL-2026-000123.adllic"});
    CHECK(ran.code == cli::exit_ok);
    CHECK(ran.out == "Removed ADL-2026-000123.adllic from your licence folder. The file it was imported from was not touched.\n");
    CHECK(ran.err.empty());
    CHECK_FALSE(rig.files.has(rig.user("ADL-2026-000123.adllic")));
    CHECK(rig.files.has(rig.user("revocations-1.adlrev")));
    CHECK(rig.files.has(rig.machine("ADL-2025-000007.adllic")));

    const Ran list = rig.run({"remove", "revocations-1.adlrev"});
    CHECK(list.code == cli::exit_ok);
    CHECK_FALSE(rig.files.has(rig.user("revocations-1.adlrev")));
}

TEST_CASE("remove: what is refused, and why", "[unit][licence_cli][remove]")
{
    Rig rig;
    rig.files.put(rig.user("ADL-2026-000123.adllic"), signed_text(annual()));
    rig.files.put(rig.user("notes.txt"), "mine");
    rig.files.put(rig.machine("ADL-2025-000007.adllic"), signed_text(numbered("ADL-2025-000007", at(2025, 1, 1), at(2026, 1, 1))));
    const auto before = rig.files.files;

    const auto refused = [&](const std::string& name, std::string_view sentence)
    {
        const Ran ran = rig.run({"remove", name});
        CAPTURE(name);
        CHECK(ran.code == cli::exit_failed);
        CHECK(ran.out.empty());
        CHECK(ran.err == "remove: " + std::string(sentence) + "\n");
        CHECK(rig.files.files == before);
    };
    refused("ADL-2025-000007.adllic",
            "ADL-2025-000007.adllic is in the licence folder of this computer, which only an administrator can change.");
    refused("ADL-2024-000001.adllic",
            "there is no file called ADL-2024-000001.adllic in your licence folder; show prints the names");
    refused("notes.txt", "notes.txt is not the name of a licence file: give a name as show prints it, with no folder");
    refused("home/licences/ADL-2026-000123.adllic",
            "home/licences/ADL-2026-000123.adllic is not the name of a licence file: give a name as show prints it, "
            "with no folder");
    refused("..", ".. is not the name of a licence file: give a name as show prints it, with no folder");
}

TEST_CASE("remove: a file that cannot be removed", "[unit][licence_cli][remove]")
{
    Rig rig;
    rig.files.put(rig.user("ADL-2026-000123.adllic"), signed_text(annual()));
    rig.files.remove_fails = st::FilesError::failed;
    const Ran ran = rig.run({"remove", "ADL-2026-000123.adllic"});
    CHECK(ran.code == cli::exit_failed);
    CHECK(ran.err == "remove: the file ADL-2026-000123.adllic could not be removed\n");
    CHECK(rig.files.has(rig.user("ADL-2026-000123.adllic")));
}

TEST_CASE("verify: a file a program would take, judged and not kept", "[unit][licence_cli][verify]")
{
    Rig rig;
    rig.files.put("inbox/a.adllic", signed_text(annual()));
    const auto before = rig.files.files;

    const Ran ran = rig.run({"verify", "inbox/a.adllic"});
    CHECK(ran.code == cli::exit_ok);
    CHECK(ran.out == annual_lines + "A program would accept this licence file.\n");
    CHECK(ran.err.empty());
    CHECK(rig.files.files == before);
    CHECK(rig.files.creates == 0);
    CHECK(rig.files.replaces == 0);
    CHECK(rig.files.removes == 0);

    SECTION("a file that is in the folder already")
    {
        rig.files.put(rig.user("ADL-2026-000123.adllic"), signed_text(annual()));
        const Ran there = rig.run({"verify", "inbox/a.adllic"});
        CHECK(there.code == cli::exit_ok);
        CHECK(there.out ==
              annual_lines + "A program would accept this licence file.\nIt is in the licence folder already as ADL-2026-000123.adllic.\n");
    }
    SECTION("a licence that has ended is taken, and its state is shown")
    {
        rig.set_now(at(2029, 1, 1));
        const Ran ended = rig.run({"verify", "inbox/a.adllic"});
        CHECK(ended.code == cli::exit_ok);
        CHECK(contains(ended.out, "State: expired\n"));
        CHECK(contains(ended.out, "A program would accept this licence file.\n"));
    }
}

TEST_CASE("verify: a list of withdrawn licences", "[unit][licence_cli][verify]")
{
    Rig rig;
    rig.files.put("inbox/list.adlrev", list_text(7, {"ADL-2026-000123", "ADL-2026-000124"}));
    const Ran ran = rig.run({"verify", "inbox/list.adlrev"});
    CHECK(ran.code == cli::exit_ok);
    CHECK(ran.out ==
          "Sequence: 7\n"
          "Licences withdrawn: 2\n"
          "Keys withdrawn: 0\n"
          "A program would accept this list of withdrawn licences.\n");
    CHECK(rig.files.files.size() == 1);

    // A list that is not signed by a trusted key is refused.
    rig.env.keys.clear();
    const Ran refused = rig.run({"verify", "inbox/list.adlrev"});
    CHECK(refused.code == cli::exit_failed);
    CHECK(refused.out.empty());
}

TEST_CASE("verify: a file a program would refuse shows what it is and exits 1", "[unit][licence_cli][verify]")
{
    Rig rig;
    lic::Licence other_computer = annual();
    other_computer.machines = {"OTHER-*"};
    rig.files.put("inbox/other.adllic", signed_text(other_computer));
    rig.files.put("inbox/damaged.adllic", "this is not a licence");
    rig.files.put("inbox/signature.adllic", with_bad_signature(signed_text(annual())));
    const auto before = rig.files.files;

    const Ran other = rig.run({"verify", "inbox/other.adllic"});
    CHECK(other.code == cli::exit_failed);
    CHECK(contains(other.out, "State: invalid\n"));
    CHECK(contains(other.out, "Reason: the licence does not name this computer\n"));
    CHECK(contains(other.out, "Licence: ADL-2026-000123\n"));
    CHECK(contains(other.err, "verify: This licence is for other computers. This computer is LAB-RIG-07."));

    const Ran damaged = rig.run({"verify", "inbox/damaged.adllic"});
    CHECK(damaged.code == cli::exit_failed);
    CHECK(damaged.out.empty());
    CHECK(damaged.err.starts_with("verify: This file is not a valid ARINC Data Loader licence."));

    const Ran signature = rig.run({"verify", "inbox/signature.adllic"});
    CHECK(signature.code == cli::exit_failed);
    CHECK(contains(signature.out, "State: invalid\n"));
    CHECK(contains(signature.out, "Reason: the signature does not verify\n"));

    const Ran missing = rig.run({"verify", "inbox/missing.adllic"});
    CHECK(missing.code == cli::exit_failed);
    CHECK(missing.err == "verify: cannot read inbox/missing.adllic: it does not exist\n");
    CHECK(rig.files.files == before);
}

TEST_CASE("verify: a full folder is a reason an import would fail, so it is a reason here", "[unit][licence_cli][verify]")
{
    Rig rig;
    for (int i = 0; i < 64; ++i)
    {
        rig.files.put(rig.user("a" + std::to_string(1000 + i) + ".adllic"), "not a licence");
    }
    rig.files.put("inbox/a.adllic", signed_text(annual()));
    const Ran ran = rig.run({"verify", "inbox/a.adllic"});
    CHECK(ran.code == cli::exit_failed);
    CHECK(contains(ran.err, "verify: The licence folder already holds 64 files."));
}

TEST_CASE("nothing a command prints carries a control character from a file", "[unit][licence_cli]")
{
    Rig rig;
    // A file name with a control character in it, and a name outside ASCII that is kept.
    rig.files.put(rig.user("a\x1b[2Jb.adllic"), "not a licence");
    rig.files.put(rig.user("\xC3\xA9.adllic"), "not a licence");
    const Ran ran = rig.run({"show"});
    CHECK(ran.out.find('\x1b') == std::string::npos);
    CHECK(contains(ran.out, "a?[2Jb.adllic"));
    CHECK(contains(ran.out, "\xC3\xA9" ".adllic"));
}

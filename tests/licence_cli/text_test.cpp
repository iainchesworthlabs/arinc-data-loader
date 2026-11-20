// SPDX-License-Identifier: GPL-3.0-or-later
// The small rules of adl_licence that have none of the store in them: what is printed of text that came from a
// file, the date of a time, the list of a licence's features, the commit time of a build, and the words. Strings
// that are not ASCII are written as byte escapes, so no compiler setting can change them.

#include "adl/licence/licence.hpp"
#include "adl/licence_cli/system.hpp"
#include "messages.hpp"
#include "text.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <string>
#include <vector>

namespace cli = adl::licence_cli;
namespace lic = adl::licence;

TEST_CASE("plain keeps letters and replaces what could move the cursor or reorder the text",
          "[unit][licence_cli][text]")
{
    CHECK(cli::plain("") == "");
    CHECK(cli::plain("Example Lab Pty Ltd") == "Example Lab Pty Ltd");
    // Letters outside ASCII are kept: two bytes, three bytes, and four bytes.
    CHECK(cli::plain("\xC3\xA9") == "\xC3\xA9");
    CHECK(cli::plain("\xE6\x97\xA5\xE6\x9C\xAC") == "\xE6\x97\xA5\xE6\x9C\xAC");
    CHECK(cli::plain("\xF0\x9F\x94\x91") == "\xF0\x9F\x94\x91");

    // Control characters, DEL and the C1 controls (a CSI by its eight-bit form is U+009B).
    CHECK(cli::plain("a\x1b[31mb") == "a?[31mb");
    CHECK(cli::plain("a\tb\nc\rd") == "a?b?c?d");
    CHECK(cli::plain("a\x7f"
                     "b") == "a?b");
    CHECK(cli::plain("a\xC2\x9B"
                     "b") == "a?b");
    CHECK(cli::plain("a\xC2\xA0"
                     "b") == "a\xC2\xA0"
                             "b");
    // Line and paragraph separators, the marks that set the direction of text, and its overrides and isolates.
    CHECK(cli::plain("\xE2\x80\xA8") == "?");
    CHECK(cli::plain("\xE2\x80\xA9") == "?");
    CHECK(cli::plain("\xE2\x80\x8E\xE2\x80\x8F") == "??");
    CHECK(cli::plain("\xD8\x9C") == "?");
    CHECK(cli::plain("\xE2\x80\xAA\xE2\x80\xAE") == "??");
    CHECK(cli::plain("\xE2\x81\xA6\xE2\x81\xA9") == "??");
    // The ordinary space and the no-break space are kept, and so is a character next to the ones that are not.
    CHECK(cli::plain("\xE2\x80\xAF") == "\xE2\x80\xAF");
}

TEST_CASE("plain replaces each byte that is not part of a UTF-8 character", "[unit][licence_cli][text]")
{
    CHECK(cli::plain("\x80") == "?");                // a continuation byte alone
    CHECK(cli::plain("\xC3") == "?");                // a character cut short
    CHECK(cli::plain("\xE6\x97") == "??");           // another, two bytes of three
    CHECK(cli::plain("\xC0\xAF") == "??");           // an overlong form of '/'
    CHECK(cli::plain("\xE0\x80\xAF") == "???");      // another overlong form
    CHECK(cli::plain("\xED\xA0\x80") == "???");      // a surrogate
    CHECK(cli::plain("\xF4\x90\x80\x80") == "????"); // above U+10FFFF
    CHECK(cli::plain("\xFF\xFE") == "??");
    CHECK(cli::plain("a\xC3"
                     "b") == "a?b");
    // The same bytes in a character that is whole are kept.
    CHECK(cli::plain("\xF4\x8F\xBF\xBF") == "\xF4\x8F\xBF\xBF");
}

TEST_CASE("a date is the UTC date of a time, and a list is plain and joined", "[unit][licence_cli][text]")
{
    using namespace std::chrono;
    CHECK(cli::date_of(lic::TimePoint{sys_days{year{2027} / March / 1}}) == "2027-03-01");
    CHECK(cli::date_of(lic::TimePoint{sys_days{year{2028} / November / 13}} + hours{23} + minutes{59}) == "2028-11-13");

    CHECK(cli::joined({}, "none") == "none");
    CHECK(cli::joined({"support"}, "none") == "support");
    CHECK(cli::joined({"support", "pro.*", "data_packs"}, "none") == "support, pro.*, data_packs");
    CHECK(cli::joined({"a\x1b"
                       "b"},
                      "none") == "a?b");
}

TEST_CASE("the time of a commit is read from the text git writes, with its offset", "[unit][licence_cli][text]")
{
    using namespace std::chrono;
    const auto at = [](int y, unsigned m, unsigned d, int h, int min, int s)
    { return lic::TimePoint{sys_days{year{y} / month{m} / day{d}}} + hours{h} + minutes{min} + seconds{s}; };
    CHECK(cli::parse_commit_time("2026-11-20T10:15:30Z") == at(2026, 11, 20, 10, 15, 30));
    // An offset is taken off: 10:15 at +01:00 is 09:15 in UTC, and 10:15 at -05:30 is 15:45.
    CHECK(cli::parse_commit_time("2026-11-20T10:15:30+01:00") == at(2026, 11, 20, 9, 15, 30));
    CHECK(cli::parse_commit_time("2026-11-20T10:15:30-05:30") == at(2026, 11, 20, 15, 45, 30));
    CHECK(cli::parse_commit_time("2026-01-01T00:30:00+01:00") == at(2025, 12, 31, 23, 30, 0));
    CHECK(cli::parse_commit_time("2026-02-28T23:59:59-23:59") == at(2026, 3, 1, 23, 58, 59));

    for (const std::string text :
         {"", "unknown", "2026-11-20", "2026-11-20T10:15:30", "2026-11-20 10:15:30Z", "2026-11-20T10:15:30z",
          "2026-13-20T10:15:30Z", "2026-02-30T10:15:30Z", "2026-11-20T24:00:00Z", "2026-11-20T10:60:00Z",
          "2026-11-20T10:15:60Z", "2026-11-20T10:15:30+24:00", "2026-11-20T10:15:30+01:60", "2026-11-20T10:15:30+0100",
          "2026-11-20T10:15:30.5Z", "20x6-11-20T10:15:30Z", "1969-12-31T23:59:59Z", "2026-11-20T10:15:30+01:00 "})
    {
        CAPTURE(text);
        CHECK_FALSE(cli::parse_commit_time(text).has_value());
    }
}

TEST_CASE("a sentence has its values put in where they are named", "[unit][licence_cli][text]")
{
    CHECK(cli::format("no values", {}) == "no values");
    CHECK(cli::format("%1 and %2", {"one", "two"}) == "one and two");
    CHECK(cli::format("%2 before %1", {"one", "two"}) == "two before one");
    CHECK(cli::format("%1 %1", {"x"}) == "x x");
    // A value that is not given leaves nothing, and a percent sign that names none is kept.
    CHECK(cli::format("%1 and %2", {"one"}) == "one and ");
    CHECK(cli::format("100% sure %", {}) == "100% sure %");
    CHECK(cli::format("%0 and %a", {"x"}) == "%0 and %a");
}

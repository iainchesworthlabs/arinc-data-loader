// SPDX-License-Identifier: GPL-3.0-or-later
// A program's arguments as UTF-8 on every system. The Windows command line is split and converted by
// functions of the text the system gave (detail/command_line.hpp), so every case runs on every host; the
// system test tools.write_payloads.unicode_folder starts a program with such a name as a process.
#include "adl/platform/arguments.hpp"
#include "adl/platform/paths.hpp"
#include "detail/command_line.hpp"
#include "temp_dir.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace
{

namespace detail = adl::platform::detail;
using adl::platform::to_utf8;
using Arguments = std::vector<std::string>;

Arguments split(std::u16string_view command_line)
{
    return detail::arguments_from_command_line(command_line);
}

// A command line as it is typed, so that the backslashes and quotes of a case read as they are.
// The characters are written as escapes to keep this file ASCII: \u00e9 is e with an acute accent (in
// the Western code pages), \u65e5\u672c\u8a9e is Japanese (in none of them), and \U0001F600 is a
// character outside the Basic Multilingual Plane, two UTF-16 units.
const std::string accented = to_utf8(u8"caf\u00e9");
const std::string japanese = to_utf8(u8"\u65e5\u672c\u8a9e");
const std::string emoji = to_utf8(u8"\U0001F600");
const std::string replacement = to_utf8(u8"\uFFFD");

} // namespace

TEST_CASE("UTF-16 becomes UTF-8", "[platform][arguments]")
{
    CHECK(detail::utf16_to_utf8(u"").empty());
    CHECK(detail::utf16_to_utf8(u"plain ASCII") == "plain ASCII");
    CHECK(detail::utf16_to_utf8(u"caf\u00e9") == accented);
    CHECK(detail::utf16_to_utf8(u"\u65e5\u672c\u8a9e") == japanese);
    CHECK(detail::utf16_to_utf8(u"\U0001F600") == emoji);
    CHECK(emoji == "\xF0\x9F\x98\x80");
}

TEST_CASE("half of a surrogate pair is not text and becomes U+FFFD", "[platform][arguments]")
{
    CHECK(detail::utf16_to_utf8(u"a\xD83D"
                                u"b") == "a" + replacement + "b");
    CHECK(detail::utf16_to_utf8(u"a\xDE00"
                                u"b") == "a" + replacement + "b");
    CHECK(detail::utf16_to_utf8(u"\xD83D") == replacement);
    // The pair the wrong way round is two halves, and a high half before a letter is one.
    CHECK(detail::utf16_to_utf8(u"\xDE00\xD83D") == replacement + replacement);
    CHECK(detail::utf16_to_utf8(u"\xD83D\xD83D\xDE00") == replacement + emoji);
}

TEST_CASE("UTF-8 becomes UTF-16", "[platform][arguments]")
{
    CHECK(detail::utf8_to_utf16("").empty());
    CHECK(detail::utf8_to_utf16("plain ASCII") == u"plain ASCII");
    CHECK(detail::utf8_to_utf16(accented) == u"caf\u00e9");
    CHECK(detail::utf8_to_utf16(japanese) == u"\u65e5\u672c\u8a9e");
    CHECK(detail::utf8_to_utf16(emoji) == u"\xD83D\xDE00");
    CHECK(detail::utf8_to_utf16(to_utf8(u8"\U0010FFFF")) == u"\xDBFF\xDFFF");
}

TEST_CASE("bytes that are not UTF-8 become U+FFFD, once for each broken sequence", "[platform][arguments]")
{
    const std::u16string one = u"\uFFFD";
    CHECK(detail::utf8_to_utf16("a\xFF"
                                "b") == u"a" + one + u"b");
    CHECK(detail::utf8_to_utf16("a\x80"
                                "b") == u"a" + one + u"b"); // a continuation byte alone
    CHECK(detail::utf8_to_utf16("a\xC3") == u"a" + one);    // a sequence cut short by the end
    CHECK(detail::utf8_to_utf16("\xE6\x97"
                                "x") == one + u"x");                           // cut short by a letter: one
    CHECK(detail::utf8_to_utf16("\xC0\x80") == one + one);                     // an overlong NUL
    CHECK(detail::utf8_to_utf16("\xE0\x80\x80") == one + one + one);           // an overlong form
    CHECK(detail::utf8_to_utf16("\xED\xA0\x80") == one + one + one);           // a surrogate written as UTF-8
    CHECK(detail::utf8_to_utf16("\xF4\x90\x80\x80") == one + one + one + one); // above U+10FFFF
    CHECK(detail::utf8_to_utf16("\xF8\x88\x80\x80\x80") == one + one + one + one + one);
}

TEST_CASE("a Windows command line is split as the C run-time splits it", "[platform][arguments]")
{
    SECTION("the examples Microsoft documents for the rules")
    {
        CHECK(split(uR"(prog "abc" d e)") == Arguments{"prog", "abc", "d", "e"});
        CHECK(split(uR"(prog a\\b d"e f"g h)") == Arguments{"prog", "a\\\\b", "de fg", "h"});
        CHECK(split(uR"(prog a\\\"b c d)") == Arguments{"prog", "a\\\"b", "c", "d"});
        CHECK(split(uR"(prog a\\\\"b c" d e)") == Arguments{"prog", "a\\\\b c", "d", "e"});
        CHECK(split(uR"(prog a"b"" c d)") == Arguments{"prog", "ab\" c d"});
    }

    SECTION("a space or a tab separates, however many there are")
    {
        CHECK(split(u"prog a b") == Arguments{"prog", "a", "b"});
        CHECK(split(u"prog \t a \t\t b \t") == Arguments{"prog", "a", "b"});
        CHECK(split(u"prog") == Arguments{"prog"});
        CHECK(split(u"prog   ") == Arguments{"prog"});
    }

    SECTION("an empty argument is an argument")
    {
        CHECK(split(uR"(prog "" x)") == Arguments{"prog", "", "x"});
        CHECK(split(uR"(prog x "")") == Arguments{"prog", "x", ""});
        CHECK(split(uR"(prog "" "")") == Arguments{"prog", "", ""});
    }

    SECTION("spaces and quotes")
    {
        CHECK(split(uR"(prog "a b" c"d e"f g)") == Arguments{"prog", "a b", "cd ef", "g"});
        CHECK(split(uR"(prog "say \"hi\"" x)") == Arguments{"prog", "say \"hi\"", "x"});
        CHECK(split(u"prog \"a\tb\"") == Arguments{"prog", "a\tb"});
        // A quote that is never closed ends at the end of the line.
        CHECK(split(uR"(prog "a b c)") == Arguments{"prog", "a b c"});
    }

    SECTION("a backslash is a backslash unless a quote follows it")
    {
        CHECK(split(uR"(prog a\ b)") == Arguments{"prog", "a\\", "b"});
        CHECK(split(uR"(prog C:\dir\)") == Arguments{"prog", "C:\\dir\\"});
        CHECK(split(uR"(prog C:\dir\ x)") == Arguments{"prog", "C:\\dir\\", "x"});
        CHECK(split(uR"(prog "C:\dir with spaces\\" next)") == Arguments{"prog", "C:\\dir with spaces\\", "next"});
        // One backslash before the closing quote makes the quote text, and the argument goes on to the end.
        CHECK(split(uR"(prog "C:\dir\" next)") == Arguments{"prog", "C:\\dir\" next"});
        CHECK(split(uR"(prog \\\)") == Arguments{"prog", "\\\\\\"});
    }

    SECTION("the program name is not an argument to split")
    {
        CHECK(split(uR"("C:\Program Files\adl\prog.exe" a "b c")") ==
              Arguments{"C:\\Program Files\\adl\\prog.exe", "a", "b c"});
        CHECK(split(uR"(C:\adl\prog.exe a\b)") == Arguments{"C:\\adl\\prog.exe", "a\\b"});
        CHECK(split(uR"("C:\adl\prog.exe\" a)") == Arguments{"C:\\adl\\prog.exe\\", "a"});
        CHECK(split(u"\"prog") == Arguments{"prog"});
        // Quotes only switch quoting: what follows a closing quote is still the name, as main would see it.
        CHECK(split(uR"("prog"x y)") == Arguments{"progx", "y"});
        CHECK(split(uR"(pr"og a"b c)") == Arguments{"prog ab", "c"});
    }

    SECTION("an empty line has no arguments")
    {
        CHECK(split(u"").empty());
    }

    SECTION("the line ends at a NUL")
    {
        CHECK(split(std::u16string_view{u"prog a\0 b", 10}) == Arguments{"prog", "a"});
    }
}

TEST_CASE("an argument is converted to UTF-8 whole", "[platform][arguments]")
{
    SECTION("an accented letter, characters outside the Western code pages, and one outside the BMP")
    {
        CHECK(split(u"prog caf\u00e9 \u65e5\u672c\u8a9e \U0001F600") == Arguments{"prog", accented, japanese, emoji});
        CHECK(split(u"prog \"caf\u00e9 \u65e5\u672c\u8a9e \U0001F600\"") ==
              Arguments{"prog", accented + " " + japanese + " " + emoji});
        CHECK(split(u"\"C:\\caf\u00e9\\prog.exe\" x") == Arguments{"C:\\" + accented + "\\prog.exe", "x"});
    }

    SECTION("half of a surrogate pair is U+FFFD, and the rest of the line is not lost")
    {
        CHECK(split(u"prog a\xD83D x") == Arguments{"prog", "a" + replacement, "x"});
        CHECK(split(u"prog \xDE00 x") == Arguments{"prog", replacement, "x"});
        CHECK(split(u"\xD83D x") == Arguments{replacement, "x"});
        // The two halves are one character in the text the system holds, wherever the splitting left them.
        CHECK(split(u"prog \xD83D\"\xDE00\"") == Arguments{"prog", emoji});
        CHECK(split(u"prog \xD83D \xDE00") == Arguments{"prog", replacement, replacement});
    }
}

TEST_CASE("the arguments of main are copied as they are", "[platform][arguments]")
{
    const char* const argv[] = {"prog", "plain", "\xC3\xA9", "\xFF", "", nullptr};

    SECTION("every one of them, bytes and all")
    {
        CHECK(detail::copy_arguments(5, argv) == Arguments{"prog", "plain", "\xC3\xA9", "\xFF", ""});
    }

    SECTION("no more than argc, and nothing from a null argv or after a null entry")
    {
        CHECK(detail::copy_arguments(2, argv) == Arguments{"prog", "plain"});
        CHECK(detail::copy_arguments(0, argv).empty());
        CHECK(detail::copy_arguments(3, nullptr).empty());
        CHECK(detail::copy_arguments(9, argv) == Arguments{"prog", "plain", "\xC3\xA9", "\xFF", ""});
    }
}

TEST_CASE("the program's arguments start with the name it was started by", "[platform][arguments]")
{
    // Where the system keeps the command line (Windows), the argv given is not read; where it does not,
    // it is returned. Either way the first argument is the program.
    const char* const argv[] = {"adl_unit_tests", nullptr};

    const Arguments arguments = adl::platform::arguments(1, argv);

    REQUIRE_FALSE(arguments.empty());
    CHECK(arguments.front().find("adl_unit_tests") != std::string::npos);
}

TEST_CASE("a path from UTF-8 text holds the same characters", "[platform][arguments]")
{
    using adl::platform::path_from_utf8;

    SECTION("it is the text, whatever the system's code page is")
    {
        CHECK(path_from_utf8("").empty());
        CHECK(path_from_utf8("plain/ascii.txt") == std::filesystem::path{"plain/ascii.txt"});
        for (const std::string& name : {accented, japanese, emoji, accented + " " + japanese + " " + emoji})
        {
            CHECK(to_utf8(path_from_utf8(name).u8string()) == name);
            CHECK(to_utf8(path_from_utf8("dir/" + name).filename().u8string()) == name);
        }
    }

    SECTION("a file system holds the name the text says")
    {
        const std::string name = "payloads-" + accented + "-" + japanese + "-" + emoji + " and spaces";
        adl::test::TempDir directory;

        REQUIRE(std::filesystem::create_directory(directory.path() / path_from_utf8(name)));

        std::vector<std::string> found;
        for (const auto& entry : std::filesystem::directory_iterator(directory.path()))
        {
            found.push_back(to_utf8(entry.path().filename().u8string()));
        }
        CHECK(found == std::vector<std::string>{name});
    }

    SECTION("text that is not UTF-8 does not throw")
    {
        CHECK_NOTHROW(path_from_utf8("a\xFF"
                                     "b"));
        CHECK_NOTHROW(path_from_utf8("\xED\xA0\x80"));
        CHECK_FALSE(path_from_utf8("a\xFF"
                                   "b")
                        .empty());
    }
}

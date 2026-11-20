// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
// The owner's licence tool, in process: the parts that have rules of their own (UTF-16 to UTF-8, the form of a
// licence number and of a public key file, the options, the git work tree check) and the commands run
// against streams and a scratch folder. The program itself, started as a process, is tested by
// test_owner_tool.py. Strings that are not ASCII are written as byte escapes, so no compiler setting can
// change them.

#include "adl/licence/licence.hpp"
#include "adl/licence/sign.hpp"
#include "adl/licence/test_keys.hpp"
#include "commands.hpp"
#include "files.hpp"
#include "messages.hpp"
#include "options.hpp"
#include "owner.hpp"
#include "public_key.hpp"
#include "signing.hpp"
#include "utf8.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace lic = adl::licence;
namespace own = adl::licence_owner;
namespace fs = std::filesystem;

namespace
{

// A folder of its own under the system's temporary folder, removed when the test ends.
class Scratch
{
public:
    Scratch()
    {
        static int counter = 0;
        path_ =
            fs::temp_directory_path() / ("adl-owner-unit-" + std::to_string(++counter) + "-" +
                                         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(path_);
    }
    Scratch(const Scratch&) = delete;
    Scratch& operator=(const Scratch&) = delete;
    ~Scratch()
    {
        std::error_code error;
        fs::remove_all(path_, error);
    }
    [[nodiscard]] const fs::path& path() const noexcept
    {
        return path_;
    }

private:
    fs::path path_;
};

struct Ran
{
    int code = 0;
    std::string out;
    std::string err;
};

Ran run(std::vector<std::string> arguments)
{
    std::ostringstream out;
    std::ostringstream err;
    const own::Console console{out, err};
    const int code = own::run(arguments, console);
    return {code, out.str(), err.str()};
}

lic::Licence sample_licence()
{
    using namespace std::chrono;
    lic::Licence value;
    value.product = "adl";
    value.licence_id = "ADL-2026-000123";
    value.licensee_name = "Example Lab";
    value.issued_at = lic::TimePoint{sys_days{year{2026} / November / 13}};
    value.starts_at = value.issued_at;
    value.expires_at = lic::TimePoint{sys_days{year{2027} / November / 13}};
    value.grace_days = 28;
    value.features = {"support"};
    return value;
}

} // namespace

TEST_CASE("UTF-16 is converted to UTF-8, and a lone surrogate is refused", "[unit][licence][owner]")
{
    CHECK(own::utf16_to_utf8(u"") == std::string());
    CHECK(own::utf16_to_utf8(u"lic-2026") == std::string("lic-2026"));
    // U+00FC, U+822A U+7A7A, and U+1F511 as a surrogate pair.
    CHECK(own::utf16_to_utf8(u"\U000000fc") == std::string("\xC3\xBC"));
    CHECK(own::utf16_to_utf8(u"\U0000822a\U00007a7a") == std::string("\xE8\x88\xAA"
                                                                     "\xE7\xA9\xBA"));
    CHECK(own::utf16_to_utf8(u"\xD83D\xDD11") == std::string("\xF0\x9F\x94\x91"));
    // The highest and the lowest code points that are not surrogates.
    CHECK(own::utf16_to_utf8(u"\xD7FF") == std::string("\xED\x9F\xBF"));
    CHECK(own::utf16_to_utf8(u"\xE000") == std::string("\xEE\x80\x80"));

    CHECK_FALSE(own::utf16_to_utf8(u"\xD83D").has_value());       // a high surrogate at the end
    CHECK_FALSE(own::utf16_to_utf8(u"\xDD11").has_value());       // a low surrogate alone
    CHECK_FALSE(own::utf16_to_utf8(u"\xD83Dx").has_value());      // a high surrogate and something else
    CHECK_FALSE(own::utf16_to_utf8(u"\xD83D\xD83D").has_value()); // two high ones
}

TEST_CASE("a path made from UTF-8 gives the same UTF-8 back", "[unit][licence][owner]")
{
    for (const std::string& name : {std::string("plain.adllic"),
                                    std::string("\xC3\x9C"
                                                "berpr\xC3\xBC"
                                                "fung"),
                                    std::string("\xE3\x83\xA9\xE3\x83\x9C"), std::string("\xF0\x9F\x94\x91")})
    {
        const fs::path path = own::path_from_utf8(name);
        CHECK(own::path_to_utf8(path) == name);
        CHECK(own::path_to_utf8(own::path_from_utf8("dir/" + name).filename()) == name);
    }
}

TEST_CASE("a file with a name outside the code page is written and read", "[unit][licence][owner]")
{
    const Scratch scratch;
    const fs::path path = scratch.path() / own::path_from_utf8("\xE3\x83\xA9\xE3\x83\x9C-\xF0\x9F\x94\x91.adllic");
    REQUIRE(own::write_new_file(path, "text\n", false));
    const auto read = own::read_file(path, 100);
    REQUIRE(read);
    CHECK(*read == "text\n");
    // Never over a file, and never more than the size asked for.
    CHECK_FALSE(own::write_new_file(path, "other", false));
    CHECK(*own::read_file(path, 100) == "text\n");
    CHECK_FALSE(own::read_file(path, 4));
    CHECK_FALSE(own::read_file(scratch.path() / "missing", 4));
    CHECK_FALSE(own::read_file(scratch.path(), 4));
}

TEST_CASE("a licence number is ADL, a year and six digits", "[unit][licence][owner]")
{
    CHECK(own::is_licence_number("ADL-2026-000123"));
    CHECK(own::is_licence_number("ADL-2100-999999"));
    for (const char* bad :
         {"", "ADL-2026-00012", "ADL-2026-0001234", "ADL-26-000123", "VSS-2026-000123", "adl-2026-000123",
          "ADL-1999-000123", "ADL-2026-00012A", "ADL-2026_000123", "ADL-2026-000123 "})
    {
        CHECK_FALSE(own::is_licence_number(bad));
    }
    CHECK(own::is_test_licence_number("ADL-2026-900001"));
    CHECK(own::is_test_licence_number("ADL-2026-999999"));
    CHECK_FALSE(own::is_test_licence_number("ADL-2026-899999"));
    CHECK_FALSE(own::is_test_licence_number("ADL-2026-000123"));
    CHECK_FALSE(own::is_test_licence_number("ADL-2026-90000"));
}

TEST_CASE("a key id is lower-case letters, digits and hyphens", "[unit][licence][owner]")
{
    CHECK(own::is_key_id("lic-2026-a"));
    CHECK(own::is_key_id("a"));
    CHECK(own::is_key_id(std::string(64, 'a')));
    CHECK_FALSE(own::is_key_id(""));
    CHECK_FALSE(own::is_key_id(std::string(65, 'a')));
    for (const char* bad : {"Lic", "lic_2026", "lic.2026", "lic 2026", "lic/2026", "lic\"", "\xC3\xBC"})
    {
        CHECK_FALSE(own::is_key_id(bad));
    }
}

TEST_CASE("a public key file is written and read in one form", "[unit][licence][owner]")
{
    lic::signed_file::TrustedKey key;
    key.key_id = "lic-2026-a";
    key.purpose = "licence";
    key.key = lic::signing::public_key_of(lic::test_keys::private_key());

    const std::string text = own::format_public_key(key);
    CHECK(text == "{\"key_id\":\"lic-2026-a\",\"purpose\":\"licence\",\"public_key\":"
                  "\"11qYAYKxCrfVS_7TyWQHOg7hcvPapiMlrwIaaPcHURo\"}\n");
    const auto again = own::parse_public_key(text);
    REQUIRE(again);
    CHECK(again->key_id == key.key_id);
    CHECK(again->purpose == key.purpose);
    CHECK(again->key == key.key);
    // The line end is optional, and one only.
    CHECK(own::parse_public_key(text.substr(0, text.size() - 1)));
    CHECK_FALSE(own::parse_public_key(text + "\n"));

    key.purpose = "revocations";
    CHECK(own::parse_public_key(own::format_public_key(key)));

    const std::string good = text.substr(0, text.size() - 1);
    for (const std::string& bad : std::vector<std::string>{"", "{}", good + " ", " " + good, good + good,
                                                           "{\"key_id\":\"Lic\",\"purpose\":\"licence\",\"public_key\":"
                                                           "\"11qYAYKxCrfVS_7TyWQHOg7hcvPapiMlrwIaaPcHURo\"}",
                                                           "{\"key_id\":\"lic\",\"purpose\":\"plugin\",\"public_key\":"
                                                           "\"11qYAYKxCrfVS_7TyWQHOg7hcvPapiMlrwIaaPcHURo\"}",
                                                           "{\"key_id\":\"lic\",\"purpose\":\"licence\",\"public_key\":"
                                                           "\"11qYAYKxCrfVS_7TyWQHOg7hcvPapiMlrwIaaPcHUR\"}",
                                                           "{\"key_id\":\"lic\",\"purpose\":\"licence\",\"public_key\":"
                                                           "\"11qYAYKxCrfVS_7TyWQHOg7hcvPapiMlrwIaaPcHURp\"}",
                                                           "{\"key_id\":\"lic\",\"purpose\":\"licence\",\"public_key\":"
                                                           "\"11qYAYKxCrfVS_7TyWQHOg7hcvPapiMlrwIaaPcHURo=\"}",
                                                           "{ "
                                                           "\"key_id\":\"lic\",\"purpose\":\"licence\",\"public_key\":"
                                                           "\"11qYAYKxCrfVS_7TyWQHOg7hcvPapiMlrwIaaPcHURo\"}",
                                                           "{\"purpose\":\"licence\",\"key_id\":\"lic\",\"public_key\":"
                                                           "\"11qYAYKxCrfVS_7TyWQHOg7hcvPapiMlrwIaaPcHURo\"}"})
    {
        CHECK_FALSE(own::parse_public_key(bad));
    }
}

TEST_CASE("options are read as a command lists them", "[unit][licence][owner]")
{
    static constexpr std::array<own::OptionSpec, 3> specs{
        {{"--test", false, false}, {"--trust", true, true}, {"--now", true, false}}};
    const std::vector<std::string> arguments{"--trust", "a", "file", "--test", "--trust", "b", "--now", "t"};
    const auto options = own::parse_options("verify", arguments, specs, 1);
    REQUIRE(options);
    CHECK(options->positional() == std::vector<std::string>{"file"});
    CHECK(options->has("--test"));
    CHECK(options->has("--now"));
    CHECK_FALSE(options->has("--other"));
    CHECK(options->value("--now") == "t");
    CHECK(options->value("--other").empty());
    CHECK(options->values("--trust") == std::vector<std::string>{"a", "b"});

    const auto expect_error = [&](std::vector<std::string> args, std::size_t positional, const std::string& words)
    {
        const auto result = own::parse_options("verify", args, specs, positional);
        REQUIRE_FALSE(result);
        CHECK(result.error().find(words) != std::string::npos);
    };
    expect_error({"--bogus"}, 1, "unknown option --bogus for verify");
    expect_error({"--now"}, 1, "--now needs a value");
    expect_error({"--now", "a", "--now", "b"}, 1, "--now was given twice");
    expect_error({"a", "b"}, 1, "verify needs one file");
    expect_error({"a"}, 0, "verify takes no file name");
}

TEST_CASE("a sentence has its values filled in", "[unit][licence][owner]")
{
    CHECK(own::format("%1 needs %2", {"issue", "--out"}) == "issue needs --out");
    CHECK(own::format("%2 before %1", {"a", "b"}) == "b before a");
    CHECK(own::format("%1 and %1", {"x"}) == "x and x");
    CHECK(own::format("100% sure %9", {"x"}) == "100% sure ");
    CHECK(own::format("no values", {}) == "no values");
}

TEST_CASE("a folder is inside a git work tree when it or a folder above holds .git", "[unit][licence][owner]")
{
    const Scratch scratch;
    // The system's temporary folder may itself be in a work tree (CTest points TEMP at the build folder, which
    // is inside the checkout), so what is asked about the folders that hold no .git is "no more than the
    // folder they are in".
    const bool temporary_folder_is_in_a_work_tree = own::inside_git_work_tree(scratch.path());
    fs::create_directories(scratch.path() / "plain" / "deeper");
    CHECK(own::inside_git_work_tree(scratch.path() / "plain" / "deeper") == temporary_folder_is_in_a_work_tree);
    fs::create_directories(scratch.path() / "work" / ".git");
    fs::create_directories(scratch.path() / "work" / "sub" / "deeper");
    CHECK(own::inside_git_work_tree(scratch.path() / "work"));
    CHECK(own::inside_git_work_tree(scratch.path() / "work" / "sub" / "deeper"));
    // A work tree made by `git worktree add` has a .git file, not a folder.
    fs::create_directories(scratch.path() / "linked" / "sub");
    std::ofstream(scratch.path() / "linked" / ".git") << "gitdir: elsewhere\n";
    CHECK(own::inside_git_work_tree(scratch.path() / "linked" / "sub"));
}

TEST_CASE("a signed file is read back before it is reported, and removed if it fails", "[unit][licence][owner]")
{
    const Scratch scratch;
    const auto payload = lic::write_payload(sample_licence());
    REQUIRE(payload);
    own::SigningKey key;
    key.set("lic-2026-a", lic::test_keys::private_key());

    const fs::path good = scratch.path() / "good.adllic";
    const auto written = own::sign_and_write(lic::licence_purpose, key, *payload, good,
                                             [](const std::string&) { return std::optional<std::string>{}; });
    REQUIRE(written);
    CHECK(written->bytes == fs::file_size(good));

    // The same bytes verify with the public half of that key, and say the key id they were made with.
    const auto text = own::read_file(good, 65536);
    REQUIRE(text);
    const auto envelope = lic::signed_file::parse(*text);
    REQUIRE(envelope);
    CHECK(envelope->key_id == "lic-2026-a");

    // A check that fails removes the file and says so.
    const fs::path refused = scratch.path() / "refused.adllic";
    const auto failed = own::sign_and_write(lic::licence_purpose, key, *payload, refused, [](const std::string&)
                                            { return std::optional<std::string>{"nobody agrees"}; });
    REQUIRE_FALSE(failed);
    CHECK(failed.error().find("was removed") != std::string::npos);
    CHECK(failed.error().find("nobody agrees") != std::string::npos);
    CHECK_FALSE(fs::exists(refused));

    // A file that exists is never written over.
    const auto again = own::sign_and_write(lic::licence_purpose, key, *payload, good,
                                           [](const std::string&) { return std::optional<std::string>{}; });
    REQUIRE_FALSE(again);
    CHECK(again.error().find("it exists already") != std::string::npos);
    CHECK(own::read_file(good, 65536).value() == *text);
}

TEST_CASE("the commands run in process", "[unit][licence][owner]")
{
    const Scratch scratch;
    const fs::path request = scratch.path() / "request.json";
    const auto payload = lic::write_payload(sample_licence());
    REQUIRE(payload);
    REQUIRE(own::write_new_file(request, *payload, false));
    const std::string out = own::path_to_utf8(scratch.path() / "out.adllic");
    const std::string request_name = own::path_to_utf8(request);

    SECTION("--help and no command")
    {
        const Ran help = run({"--help"});
        CHECK(help.code == own::exit_ok);
        CHECK(help.out == own::usage_text());
        const Ran none = run({});
        CHECK(none.code == own::exit_usage);
        CHECK(none.err.find("give a command") != std::string::npos);
        const Ran unknown = run({"sign"});
        CHECK(unknown.code == own::exit_usage);
        CHECK(unknown.err.find("unknown command sign") != std::string::npos);
    }

    SECTION("issue and verify with the test key")
    {
        const Ran issued = run({"issue", "--test", "--request", request_name, "--out", out});
        REQUIRE(issued.code == own::exit_ok);
        CHECK(issued.out.starts_with(std::string(own::message::test_note)));
        CHECK(issued.out.find("Signature: verifies\n") != std::string::npos);
        const Ran judged = run({"verify", "--test", out, "--now", "2026-12-01T00:00:00Z"});
        CHECK(judged.code == own::exit_ok);
        CHECK(judged.out.find("State: valid\n") != std::string::npos);
        const Ran ended = run({"verify", "--test", out, "--now", "2028-12-01T00:00:00Z"});
        CHECK(ended.code == own::exit_failed);
        CHECK(ended.out.find("State: expired\n") != std::string::npos);
        CHECK(ended.out.find("Why: the licence has ended\n") != std::string::npos);
    }

    SECTION("a request that is not a licence is refused and nothing is written")
    {
        const fs::path bad = scratch.path() / "bad.json";
        REQUIRE(own::write_new_file(bad, "{\"licence_id\":\"ADL-2026-000123\"}", false));
        const Ran refused = run({"issue", "--test", "--request", own::path_to_utf8(bad), "--out", out});
        CHECK(refused.code == own::exit_failed);
        CHECK(refused.err.find("is refused") != std::string::npos);
        CHECK_FALSE(fs::exists(scratch.path() / "out.adllic"));
    }

    SECTION("keygen refuses a folder inside a git work tree, a missing folder and a purpose it does not know")
    {
        // A key made in a work tree could be committed by mistake. (Making a key is tried by test_owner_tool.py,
        // in a folder outside any work tree: CTest's own temporary folder is inside the checkout.)
        const fs::path work = scratch.path() / "work";
        fs::create_directories(work / ".git");
        fs::create_directories(work / "keys");
        const Ran inside = run(
            {"keygen", "--key-id", "lic-2026-a", "--purpose", "licence", "--out", own::path_to_utf8(work / "keys")});
        CHECK(inside.code == own::exit_failed);
        CHECK(inside.err.find("is inside a git repository") != std::string::npos);
        CHECK(fs::is_empty(work / "keys"));

        const Ran missing = run({"keygen", "--key-id", "lic-2026-a", "--purpose", "licence", "--out",
                                 own::path_to_utf8(scratch.path() / "nowhere")});
        CHECK(missing.code == own::exit_failed);
        CHECK(missing.err.find("does not exist") != std::string::npos);

        const Ran purpose = run({"keygen", "--key-id", "lic-2026-a", "--purpose", "plugin", "--out", out});
        CHECK(purpose.code == own::exit_usage);
        const Ran id = run({"keygen", "--key-id", "test-a", "--purpose", "licence", "--out", out});
        CHECK(id.code == own::exit_usage);
        CHECK(id.err.find("begins with test-") != std::string::npos);
    }

    SECTION("a seed file of the wrong size, of zeros or of the test key is refused before any folder is looked at")
    {
        const std::string folder = own::path_to_utf8(scratch.path());
        const auto refused = [&](const std::string& name, const std::string& bytes, const std::string& why)
        {
            REQUIRE(own::write_new_file(scratch.path() / name, bytes, true));
            const Ran ran = run({"keygen", "--key-id", "lic-2026-a", "--purpose", "licence", "--out", folder,
                                 "--seed-file", own::path_to_utf8(scratch.path() / name)});
            CHECK(ran.code == own::exit_failed);
            CHECK(ran.err.find("the seed file") != std::string::npos);
            CHECK(ran.err.find(why) != std::string::npos);
            CHECK(ran.out.empty());
        };
        refused("empty.bin", "", "not exactly 32 bytes");
        refused("short.bin", std::string(31, 'x'), "not exactly 32 bytes");
        refused("long.bin", std::string(33, 'x'), "not exactly 32 bytes");
        refused("zeros.bin", std::string(32, '\0'), "all zero or are the test key");
        const auto test_seed = lic::test_keys::private_key();
        refused("test.bin", std::string(test_seed.begin(), test_seed.end()), "all zero or are the test key");
        const Ran missing = run({"keygen", "--key-id", "lic-2026-a", "--purpose", "licence", "--out", folder,
                                 "--seed-file", own::path_to_utf8(scratch.path() / "nothing.bin")});
        CHECK(missing.code == own::exit_failed);
        CHECK(missing.err.find("it does not exist") != std::string::npos);
    }

    SECTION("a key file that is not a key is refused and never printed")
    {
        const fs::path junk = scratch.path() / "junk.adlownerkey";
        REQUIRE(own::write_new_file(junk, "not a key at all\n", true));
        const Ran refused = run({"issue", "--request", request_name, "--out", out, "--key-id", "lic-2026-a",
                                 "--key-file", own::path_to_utf8(junk)});
        CHECK(refused.code == own::exit_failed);
        CHECK(refused.err.find("does not hold a key") != std::string::npos);
        CHECK(refused.err.find("not a key at all") == std::string::npos);
        CHECK_FALSE(fs::exists(scratch.path() / "out.adllic"));
    }
}

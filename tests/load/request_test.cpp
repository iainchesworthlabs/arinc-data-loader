// SPDX-License-Identifier: GPL-3.0-or-later
// The requests of `adl_load serve`: a line of standard input read as a request, the lines cut from a stream, and the
// seeds of the fuzz harness that does both to whatever bytes it is given, replayed under the ordinary compilers.

#include "adl/load/request.hpp"
#include "serve_request_fuzz.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

using namespace adl::load;

namespace
{

constexpr const char* tab = "\t";

std::string line_of(std::initializer_list<std::string_view> fields)
{
    std::string line;
    for (const std::string_view field : fields)
    {
        line += line.empty() ? "" : tab;
        line += field;
    }
    return line;
}

// The sentence of a refusal, or the empty string if the line is a request.
std::string refusal_of(const std::string& line)
{
    const auto request = parse_request(line);
    return request ? std::string{} : request.error().detail;
}

} // namespace

TEST_CASE("an id is 1 to 64 of the characters A-Z a-z 0-9 . _ -", "[load][serve][request]")
{
    CHECK(is_valid_id("a"));
    CHECK(is_valid_id("fms-left.1"));
    CHECK(is_valid_id("Run_0.9-Z"));
    CHECK(is_valid_id(std::string(64, 'x')));
    CHECK_FALSE(is_valid_id(""));
    CHECK_FALSE(is_valid_id(std::string(65, 'x')));
    CHECK_FALSE(is_valid_id("a b"));
    CHECK_FALSE(is_valid_id("a/b"));
    CHECK_FALSE(is_valid_id("a\\b"));
    CHECK_FALSE(is_valid_id("\xc3\xa9")); // outside ASCII
    CHECK_FALSE(is_valid_id("a\tb"));
    CHECK_FALSE(is_valid_id(std::string_view{"a\0b", 3}));
}

TEST_CASE("a run request carries the options of its command", "[load][serve][request]")
{
    SECTION("information")
    {
        const auto request =
            parse_request(line_of({"run", "a1", "information", "--target", "10.0.0.2", "--position", "HNPFMS_L"}));
        REQUIRE(request.has_value());
        CHECK(request->kind == RequestKind::run);
        CHECK(request->id == "a1");
        CHECK(request->options.command == Command::information);
        CHECK(request->options.target == adl::loader_core::Ipv4Octets{10, 0, 0, 2});
        CHECK(request->options.position == "HNPFMS_L");
        CHECK(request->options.target_port == 59);
    }

    SECTION("an upload with a path that has spaces, a record and a target port")
    {
        const auto request = parse_request(line_of(
            {"run", "fms-left.1", "upload", "--target", "192.168.10.21", "--position", "HNPFMS_L", "--media-set",
             "E:\\sets\\FMS STD14", "--all-loads", "--record", "E:\\runs\\fms-left.1.json", "--target-port", "5059"}));
        REQUIRE(request.has_value());
        CHECK(request->options.command == Command::upload);
        CHECK(request->options.media_set == "E:\\sets\\FMS STD14"); // taken as it is, with no quoting
        CHECK(request->options.all_loads);
        CHECK(request->options.record == "E:\\runs\\fms-left.1.json");
        CHECK(request->options.target_port == 5059);
    }

    SECTION("a download of picked files, and one of named files")
    {
        const auto picked = parse_request(line_of({"run", "d", "download", "--target", "10.0.0.3", "--position",
                                                   "HNPFMS_R", "--into", "out", "--pick", "A.BIN", "--pick", "B.BIN"}));
        REQUIRE(picked.has_value());
        CHECK(picked->options.choice == FileChoice::picked);
        CHECK(picked->options.names == std::vector<std::string>{"A.BIN", "B.BIN"});
        const auto named = parse_request(line_of({"run", "e", "download", "--target", "10.0.0.3", "--position",
                                                  "HNPFMS_R", "--into", "out", "--file", "A.BIN"}));
        REQUIRE(named.has_value());
        CHECK(named->options.choice == FileChoice::named);
    }

    SECTION("a path in any script")
    {
        const std::string path = "wide-\xc3\xa9-\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e-\xf0\x9f\x98\x80 and spaces";
        const auto request = parse_request(line_of(
            {"run", "u", "upload", "--target", "10.0.0.2", "--position", "P", "--media-set", path, "--all-loads"}));
        REQUIRE(request.has_value());
        CHECK(request->options.media_set == path); // the bytes as they came
    }

    SECTION("an empty field is an empty argument, and is refused as one")
    {
        CHECK(refusal_of("run\ta1\tinformation\t\t10.0.0.2").find("Unexpected argument") != std::string::npos);
        CHECK(refusal_of("run\ta1\tinformation\t--target\t\t--position\tP").find("--target needs a value") !=
              std::string::npos);
    }
}

TEST_CASE("cancel and stop", "[load][serve][request]")
{
    const auto cancel = parse_request("cancel\tfms-left.1");
    REQUIRE(cancel.has_value());
    CHECK(cancel->kind == RequestKind::cancel);
    CHECK(cancel->id == "fms-left.1");

    const auto stop = parse_request("stop");
    REQUIRE(stop.has_value());
    CHECK(stop->kind == RequestKind::stop);
    CHECK(stop->id.empty());
}

TEST_CASE("a line that is not a request is refused with the reason, and the id when it had one",
          "[load][serve][request]")
{
    struct Case
    {
        std::string line;
        std::optional<std::string> id;
        std::string says;
    };
    const std::vector<Case> cases{
        {"", std::nullopt, "The line is empty."},
        {"\t", std::nullopt, "does not begin with a request"},
        {"pause\ta1", std::nullopt, "Unknown request \"pause\""},
        {"RUN\ta1\tinformation", std::nullopt, "Unknown request \"RUN\""},
        {"stop\tnow", std::nullopt, "stop takes no fields"},
        {"cancel", std::nullopt, "cancel needs an id"},
        {"cancel\ta b", std::nullopt, "needs an id of 1 to 64"},
        {"cancel\ta\tb", "a", "cancel takes an id and nothing else"},
        {"run", std::nullopt, "run needs an id"},
        {"run\t", std::nullopt, "needs an id of 1 to 64"},
        {"run\ta b\tinformation", std::nullopt, "needs an id of 1 to 64"},
        {"run\ta1", "a1", "run needs a command after the id"},
        {"run\ta1\tfetch", "a1", "Unknown command \"fetch\""},
        {"run\ta1\tfind", "a1", "is not an operation on one target"},
        {"run\ta1\tserve", "a1", "is not an operation on one target"},
        {"run\ta1\tinformation\t--target\t10.0.0.2\t--position\tP\t--json", "a1", "--json is not taken in a request"},
        {"run\ta1\tinformation\t--target\t10.0.0.2\t--position\tP\t--local-address\t127.0.0.1", "a1",
         "--local-address is not taken in a request"},
        {"run\ta1\tinformation\t--target\t10.0.0.2\t--position\tP\t--control-port\t59", "a1",
         "--control-port is not taken in a request"},
        {"run\ta1\tinformation\t--target\t10.0.0.2\t--position\tP\t--scenario\tfaults.json", "a1",
         "--scenario is not taken in a request"},
        {"run\ta1\tinformation\t--help", "a1", "--help is not taken in a request"},
        {"run\ta1\tinformation\t--version", "a1", "--version is not taken in a request"},
        {"run\ta1\tinformation\t--target\t10.0.0.2", "a1", "--position is missing"},
        {"run\ta1\tinformation\t--target\t10.0.0\t--position\tP", "a1", "--target takes an IPv4 address"},
        {"run\ta1\tupload\t--target\t10.0.0.2\t--position\tP", "a1", "--media-set is missing"},
        {"run\ta1\tinformation\t--target\t10.0.0.2\t--position\tP\t--max-operations\t4", "a1",
         "--max-operations is not an option of information"},
        {std::string("run\ta1\tinformation\t--target\t10.0.0.2\t--position\tP") + std::string(1, '\0') + "Q",
         std::nullopt, "NUL"},
        {"run\ta1\tinformation\t--target\t10.0.0.2\t--position\tP\xff", std::nullopt, "not valid UTF-8"},
        {"run\ta1\tinformation\t--target\t10.0.0.2\t--position\t\xc0\x80", std::nullopt, "not valid UTF-8"},
        {"run\ta1\tinformation\t--target\t10.0.0.2\t--position\t\xed\xa0\x80", std::nullopt, "not valid UTF-8"},
        {"run\ta1\tinformation\t--target\t10.0.0.2\t--position\t\xf4\x90\x80\x80", std::nullopt, "not valid UTF-8"},
        {"run\ta1\tinformation\t--target\t10.0.0.2\t--position\t\xe6\x97", std::nullopt, "not valid UTF-8"},
    };
    for (const Case& test : cases)
    {
        INFO("the line: " << test.line);
        const auto request = parse_request(test.line);
        REQUIRE_FALSE(request.has_value());
        CHECK(request.error().id == test.id);
        CHECK(request.error().detail.find(test.says) != std::string::npos);
        // Which request it was meant to be is told where the line can be taken apart and starts with its word.
        const std::string word = test.line.substr(0, test.line.find('	'));
        const bool unreadable = test.says == "not valid UTF-8" || test.says == "NUL";
        const bool a_request = word == "run" || word == "cancel" || word == "stop";
        CHECK(request.error().request ==
              (a_request && !unreadable ? std::optional<std::string>{word} : std::optional<std::string>{}));
    }
}

TEST_CASE("a refusal that quotes a long field does not repeat all of it", "[load][serve][request]")
{
    const std::string long_word(500, 'x');
    const auto request = parse_request(long_word + "\ta1");
    REQUIRE_FALSE(request.has_value());
    CHECK(request.error().detail.size() < 120);
    const auto id = parse_request("run\t" + long_word + "\tinformation");
    REQUIRE_FALSE(id.has_value());
    CHECK_FALSE(id.error().id.has_value());
    CHECK(id.error().detail.size() < 200);

    // A cut that falls inside a character stays valid UTF-8: it is made before the character.
    const std::string accents = "run\t" + std::string(39, 'x') + "\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\txx";
    const auto cut = parse_request(accents);
    REQUIRE_FALSE(cut.has_value());
    const std::string& detail = cut.error().detail;
    CHECK(detail.find(std::string(39, 'x') + "\"...") != std::string::npos);
}

TEST_CASE("lines are cut from a stream at the line feed, with a carriage return before it", "[load][serve][request]")
{
    std::istringstream in(std::string("first\nsecond\r\n\nthird\tfield\r\r\nlast"));
    std::string text;
    REQUIRE(read_line(in, text) == LineEnd::line);
    CHECK(text == "first");
    REQUIRE(read_line(in, text) == LineEnd::line);
    CHECK(text == "second");
    REQUIRE(read_line(in, text) == LineEnd::line);
    CHECK(text.empty());
    REQUIRE(read_line(in, text) == LineEnd::line);
    CHECK(text == "third\tfield\r"); // one carriage return is the end; another is part of the line
    REQUIRE(read_line(in, text) == LineEnd::unfinished);
    CHECK(text == "last");
    CHECK(read_line(in, text) == LineEnd::end);
    CHECK(text.empty());
    CHECK(read_line(in, text) == LineEnd::end); // and stays ended
}

TEST_CASE("a line that is too long is read to its end and dropped, and the next one is read", "[load][serve][request]")
{
    std::istringstream in(std::string("0123456789\r\nabcdefghijkl\nxy\n0123456789\nlast"));
    std::string text;
    REQUIRE(read_line(in, text, 10) == LineEnd::line); // ten bytes and the carriage return is not too long
    CHECK(text == "0123456789");
    REQUIRE(read_line(in, text, 10) == LineEnd::too_long);
    CHECK(text.empty());
    REQUIRE(read_line(in, text, 10) == LineEnd::line);
    CHECK(text == "xy");
    REQUIRE(read_line(in, text, 10) == LineEnd::line);
    CHECK(text == "0123456789");
    REQUIRE(read_line(in, text, 10) == LineEnd::unfinished);

    // The input ends inside a line that was already too long.
    std::istringstream cut(std::string(100, 'x'));
    CHECK(read_line(cut, text, 10) == LineEnd::unfinished);
    CHECK(read_line(cut, text, 10) == LineEnd::end);
}

TEST_CASE("bytes are read as they are", "[load][serve][request]")
{
    std::istringstream in(std::string("a\0b\xff\x1a\xc3\xa9\n", 8));
    std::string text;
    REQUIRE(read_line(in, text) == LineEnd::line);
    CHECK(text == std::string("a\0b\xff\x1a\xc3\xa9", 7));
}

TEST_CASE("a stream that has failed is the end of the input", "[load][serve][request]")
{
    std::istringstream in("never read\n");
    in.setstate(std::ios::failbit);
    std::string text = "left over";
    CHECK(read_line(in, text) == LineEnd::end);
    CHECK(text.empty());
}

namespace
{

// The bytes of every file in `directory`, which has to be there.
std::vector<std::vector<std::uint8_t>> files_in(const std::filesystem::path& directory)
{
    REQUIRE(std::filesystem::is_directory(directory));
    std::vector<std::vector<std::uint8_t>> contents;
    for (const auto& entry : std::filesystem::directory_iterator(directory))
    {
        if (!entry.is_regular_file() || entry.path().filename() == ".gitkeep")
        {
            continue;
        }
        std::ifstream in(entry.path(), std::ios::binary);
        contents.emplace_back(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    return contents;
}

} // namespace

TEST_CASE("the seeds of the requests fuzzer are read, and everything read is consistent with its line",
          "[load][serve][request][fuzz]")
{
    const std::filesystem::path fuzz{ADL_LOAD_FUZZ_DIR};
    std::size_t lines = 0;
    std::size_t requests = 0;
    std::size_t seeds = 0;
    for (const char* directory : {"corpus", "regressions"})
    {
        for (const std::vector<std::uint8_t>& bytes : files_in(fuzz / directory / "serve_request"))
        {
            const adl::test::RequestFuzzResult result = adl::test::fuzz_requests(bytes.data(), bytes.size());
            CHECK(result.consistent);
            lines += result.lines;
            requests += result.requests;
            ++seeds;
        }
    }
    // Requests of each kind, and a good many lines that are not.
    CHECK(seeds >= 40);
    CHECK(requests >= 14);
    CHECK(lines - requests >= 25);
}

TEST_CASE("a run request is written out and read again as the same options", "[load][serve][request][fuzz]")
{
    const std::string text =
        "run\tr\tupload\t--target\t10.0.0.2\t--position\tP\t--media-set\tm\t--load\tA\t--load\tB\t--record\tr.json\n";
    const adl::test::RequestFuzzResult result =
        adl::test::fuzz_requests(reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
    CHECK(result.consistent);
    CHECK(result.lines == 1);
    CHECK(result.requests == 1);
}

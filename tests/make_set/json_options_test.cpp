// SPDX-License-Identifier: GPL-3.0-or-later
// The JSON adl_make_set writes, and its command line.

#include "adl/licence/json.hpp"
#include "adl/make_set/json.hpp"
#include "adl/make_set/options.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <string>
#include <string_view>
#include <vector>

namespace
{

using namespace adl::make_set;
namespace json = adl::licence::json;

// Parses the arguments given as text.
std::expected<Options, UsageError> parse(std::vector<std::string_view> arguments)
{
    return parse_options(arguments);
}

std::string refusal(std::vector<std::string_view> arguments)
{
    const auto options = parse(std::move(arguments));
    REQUIRE_FALSE(options.has_value());
    return options.error().sentence;
}

} // namespace

TEST_CASE("json: a string is escaped as little as JSON allows", "[cli]")
{
    CHECK(json_string("plain") == "\"plain\"");
    CHECK(json_string("a \"quote\" and a \\ backslash") == "\"a \\\"quote\\\" and a \\\\ backslash\"");
    CHECK(json_string("\b\f\n\r\t") == "\"\\b\\f\\n\\r\\t\"");
    CHECK(json_string(std::string{"\x01\x1f", 2}) == "\"\\u0001\\u001f\"");
    CHECK(json_string("/ and <>") == "\"/ and <>\""); // nothing else is escaped
    CHECK(json_string("caf\xC3\xA9 \xE6\x97\xA5\xE6\x9C\xAC \xF0\x9F\x98\x80") ==
          "\"caf\xC3\xA9 \xE6\x97\xA5\xE6\x9C\xAC \xF0\x9F\x98\x80\"");
}

TEST_CASE("json: bytes that are not UTF-8 are written as U+FFFD, and the result is read by the strict reader", "[cli]")
{
    for (const std::string& text : {std::string{"a\xFF"
                                                "b"},
                                    std::string{"\xC3"}, std::string{"\xC0\xAF"}, std::string{"\xED\xA0\x80"},
                                    std::string{"\xF4\x90\x80\x80"}, std::string{"\xE2\x82"}, std::string{"x\x80y"}})
    {
        INFO(json_string(text));
        const auto value = json::parse_strict(json_string(text));
        REQUIRE(value.has_value());
        REQUIRE(value->as_string() != nullptr);
        CHECK(value->as_string()->find("\xEF\xBF\xBD") != std::string::npos);
    }
    // Text that is UTF-8 comes back as it was.
    const std::string good = "caf\xC3\xA9 \xE6\x97\xA5 \xF0\x9F\x98\x80 \x01 \" \\ ";
    CHECK(*json::parse_strict(json_string(good))->as_string() == good);
}

TEST_CASE("json: seconds are written with milliseconds", "[cli]")
{
    using std::chrono::milliseconds;
    CHECK(seconds_text(milliseconds{0}) == "0.000");
    CHECK(seconds_text(milliseconds{1250}) == "1.250");
    CHECK(seconds_text(milliseconds{7}) == "0.007");
    CHECK(seconds_text(milliseconds{61000}) == "61.000");
    CHECK(seconds_text(milliseconds{-1500}) == "-1.500");
}

TEST_CASE("json: the writer puts the commas where they belong", "[cli]")
{
    JsonWriter out;
    out.begin_object();
    out.member("a", "x").member_int("b", 2).member_bool("c", true).member_null("d");
    out.member("e", std::vector<std::string>{"p", "q"});
    out.key("f")
        .begin_array()
        .integer(1)
        .begin_object()
        .member("g", "h")
        .end_object()
        .begin_array()
        .end_array()
        .end_array();
    out.key("t").seconds(std::chrono::milliseconds{1250});
    out.end_object();
    CHECK(out.text() == R"({"a":"x","b":2,"c":true,"d":null,"e":["p","q"],"f":[1,{"g":"h"},[]],"t":1.250})");
    CHECK(json::parse_strict(out.text()).has_value());
}

TEST_CASE("options: each command with its options", "[cli]")
{
    auto make = parse({"make", "set.json", "--output", "out", "--source-root", "src", "--json"});
    REQUIRE(make.has_value());
    CHECK(make->command == Command::make);
    CHECK(make->subject == "set.json");
    CHECK(make->output == std::optional<std::string>{"out"});
    CHECK(make->source_root == std::optional<std::string>{"src"});
    CHECK(make->json);

    // The options may come before the subject.
    make = parse({"--json", "make", "--output", "out", "set.json"});
    REQUIRE(make.has_value());
    CHECK(make->subject == "set.json");
    CHECK_FALSE(make->source_root.has_value());

    const auto verify = parse({"verify", "folder"});
    REQUIRE(verify.has_value());
    CHECK(verify->command == Command::verify);
    CHECK(verify->subject == "folder");
    CHECK_FALSE(verify->json);

    const auto inspect = parse({"inspect", "folder", "--json"});
    REQUIRE(inspect.has_value());
    CHECK(inspect->command == Command::inspect);
    CHECK(inspect->json);

    // Names with spaces and characters of any language are one argument each.
    const auto unicode = parse({"verify", "folder \xC3\xA9 \xE6\x97\xA5\xE6\x9C\xAC \xF0\x9F\x98\x80"});
    REQUIRE(unicode.has_value());
    CHECK(unicode->subject == "folder \xC3\xA9 \xE6\x97\xA5\xE6\x9C\xAC \xF0\x9F\x98\x80");
}

TEST_CASE("options: help and version stop at once", "[cli]")
{
    CHECK(parse({"--help"})->help);
    CHECK(parse({"make", "--help"})->help);
    CHECK(parse({"--version"})->version);
    CHECK(
        parse({"verify", "--output", "x", "--version"})->version); // read in order, and the first word that stops wins
    CHECK(usage_text().starts_with("usage: adl_make_set make <description> --output <folder>"));
}

TEST_CASE("options: what is refused, and why", "[cli]")
{
    CHECK(refusal({}) == "no command given (make, verify or inspect)");
    CHECK(refusal({"--json"}) == "no command given (make, verify or inspect)");
    CHECK(refusal({"compare", "a"}) == "unknown command compare");
    CHECK(refusal({"make", "set.json"}) == "make needs --output");
    CHECK(refusal({"make", "--output", "out"}) == "make takes one description file");
    CHECK(refusal({"make", "a", "b", "--output", "out"}) == "make takes one description file");
    CHECK(refusal({"verify"}) == "verify takes one folder");
    CHECK(refusal({"inspect", "a", "b"}) == "inspect takes one folder");
    CHECK(refusal({"inspect", ""}) == "inspect takes one folder");
    CHECK(refusal({"verify", "a", "--output", "out"}) == "--output is not for verify");
    CHECK(refusal({"inspect", "a", "--source-root", "src"}) == "--source-root is not for inspect");
    CHECK(refusal({"make", "a", "--output"}) == "--output needs a value");
    CHECK(refusal({"make", "a", "--output", "--json"}) == "--output needs a value");
    CHECK(refusal({"make", "a", "--output", "x", "--output", "y"}) == "--output is given twice");
    CHECK(refusal({"make", "a", "--output", "x", "--source-root", "p", "--source-root", "q"}) ==
          "--source-root is given twice");
    CHECK(refusal({"verify", "a", "--json", "--json"}) == "--json is given twice");
    CHECK(refusal({"verify", "a", "--quiet"}) == "unknown option --quiet");
}

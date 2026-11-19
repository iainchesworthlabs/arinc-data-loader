// SPDX-License-Identifier: GPL-3.0-or-later
// The JSON writer of adl_mock, and the reader the tests use to check it. The writer is hand-written, so
// its output is read back by a reader that did not write it.

#include "adl/mock/cli/json.hpp"
#include "json_read.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <string>
#include <string_view>
#include <vector>

using adl::mock::cli::json_string;
using adl::mock::cli::JsonObject;
using adl::mock::cli::seconds_text;
using adl::test::JsonReader;
using namespace std::chrono_literals;

namespace
{

// The text a string reads back as, or nothing when what the writer wrote is not a JSON string.
[[nodiscard]] std::optional<std::string> round_trip(std::string_view text)
{
    const auto json = JsonReader::read(json_string(text));
    if (!json || !std::holds_alternative<std::string>(json->value))
    {
        return std::nullopt;
    }
    return json->string();
}

} // namespace

TEST_CASE("a plain string is quoted and nothing else", "[mock_cli][json]")
{
    CHECK(json_string("") == "\"\"");
    CHECK(json_string("HNPFMS_L") == "\"HNPFMS_L\"");
    CHECK(json_string("FMS LEFT") == "\"FMS LEFT\"");
    CHECK(json_string("a/b") == "\"a/b\""); // a solidus needs no escape
}

TEST_CASE("a quotation mark, a backslash and the control characters are escaped", "[mock_cli][json]")
{
    CHECK(json_string("say \"hi\"") == R"("say \"hi\"")");
    CHECK(json_string("C:\\data\\x") == R"("C:\\data\\x")");
    CHECK(json_string("a\nb\r\tc\b\f") == R"("a\nb\r\tc\b\f")");
    CHECK(json_string(std::string("\x01\x1f", 2)) == R"("\u0001\u001f")");
    CHECK(json_string(std::string("nul\0byte", 8)) == R"("nul\u0000byte")");
    CHECK(json_string("\x7f") == "\"\x7f\""); // DEL is allowed in a JSON string
}

TEST_CASE("every control character is escaped and reads back", "[mock_cli][json]")
{
    for (int c = 0; c < 0x20; ++c)
    {
        const std::string text(1, static_cast<char>(c));
        const std::string written = json_string(text);
        for (const char byte : written)
        {
            CHECK(static_cast<unsigned char>(byte) >= 0x20);
        }
        CHECK(round_trip(text) == text);
    }
}

TEST_CASE("well-formed UTF-8 is written as it is", "[mock_cli][json]")
{
    for (const std::string_view text : {"caf\xC3\xA9", "\xE2\x82\xAC", "\xF0\x9F\x98\x80", "\xEF\xBF\xBD",
                                        "\xED\x9F\xBF", "\xF4\x8F\xBF\xBF", "\xC2\x80"})
    {
        CHECK(json_string(text) == "\"" + std::string{text} + "\"");
        CHECK(round_trip(text) == std::string{text});
    }
}

TEST_CASE("bytes that are not well-formed UTF-8 are replaced, one by one, with U+FFFD", "[mock_cli][json]")
{
    const std::string replacement = "\xEF\xBF\xBD";
    // a lone continuation byte, a byte that is never used, a lead byte with nothing after it, a
    // truncated sequence, an overlong form, a surrogate, and a code point above U+10FFFF
    CHECK(json_string("a\x80z") == "\"a" + replacement + "z\"");
    CHECK(json_string("\xFF") == "\"" + replacement + "\"");
    CHECK(json_string("\xC3") == "\"" + replacement + "\"");
    CHECK(json_string("\xE2\x82") == "\"" + replacement + replacement + "\"");
    CHECK(json_string("\xC0\x80") == "\"" + replacement + replacement + "\"");
    CHECK(json_string("\xED\xA0\x80") == "\"" + replacement + replacement + replacement + "\"");
    CHECK(json_string("\xF4\x90\x80\x80") == "\"" + replacement + replacement + replacement + replacement + "\"");
    CHECK(json_string("\xF5\x80\x80\x80") == "\"" + replacement + replacement + replacement + replacement + "\"");
    // a good character after a bad byte is kept
    CHECK(json_string("\xFF\xC3\xA9") == "\"" + replacement + "\xC3\xA9\"");
}

TEST_CASE("whatever the bytes are, what is written is one valid JSON string", "[mock_cli][json]")
{
    std::string every_byte;
    for (int c = 0; c < 256; ++c)
    {
        every_byte.push_back(static_cast<char>(c));
    }
    CHECK(round_trip(every_byte).has_value());
    // and every pair of bytes
    for (int a = 0; a < 256; a += 5)
    {
        for (int b = 0; b < 256; ++b)
        {
            const std::string pair{static_cast<char>(a), static_cast<char>(b)};
            INFO(a << " " << b);
            CHECK(round_trip(pair).has_value());
        }
    }
}

TEST_CASE("seconds are written with exactly three decimals", "[mock_cli][json]")
{
    CHECK(seconds_text(0ms) == "0.000");
    CHECK(seconds_text(5ms) == "0.005");
    CHECK(seconds_text(50ms) == "0.050");
    CHECK(seconds_text(1250ms) == "1.250");
    CHECK(seconds_text(1000ms) == "1.000");
    CHECK(seconds_text(61000ms) == "61.000");
    CHECK(seconds_text(123456789ms) == "123456.789");
    CHECK(seconds_text(-1250ms) == "-1.250");
    CHECK(seconds_text(-5ms) == "-0.005");
}

TEST_CASE("an object keeps its members in the order they were added", "[mock_cli][json]")
{
    CHECK(JsonObject{}.text() == "{}");
    JsonObject object;
    object.string("event", "started")
        .seconds("t", 1250ms)
        .number("port", 59)
        .number("negative", -7)
        .boolean("on", true)
        .boolean("off", false)
        .null("nothing");
    CHECK(object.text() ==
          R"({"event":"started","t":1.250,"port":59,"negative":-7,"on":true,"off":false,"nothing":null})");
}

TEST_CASE("a list of strings is written as an array", "[mock_cli][json]")
{
    const std::vector<std::string> none;
    const std::vector<std::string> some = {"--silent", "a \"b\"", ""};
    JsonObject object;
    object.strings("none", none).strings("some", some);
    CHECK(object.text() == R"({"none":[],"some":["--silent","a \"b\"",""]})");
}

TEST_CASE("an object with hostile strings reads back as the same strings", "[mock_cli][json]")
{
    const std::string key = "k\"\\\n";
    const std::string value = std::string("v\x01\xFF\xC3\xA9\"", 6);
    JsonObject object;
    object.string(key, value);
    const auto json = JsonReader::read(object.text());
    REQUIRE(json);
    REQUIRE(json->is_object());
    REQUIRE(json->members().size() == 1);
    CHECK(json->members()[0].first == key);
    // The byte that is not UTF-8 came back as U+FFFD; the rest is as it was.
    CHECK(json->members()[0].second.string() == std::string("v\x01\xEF\xBF\xBD\xC3\xA9\"", 8));
}

TEST_CASE("the reader the tests use refuses what is not JSON", "[mock_cli][json]")
{
    for (const std::string_view text : {"", "{", "{\"a\":1,}", "[1,]", "{\"a\" 1}", "'a'", "\"a", "\"\n\"", "\"\\x\"",
                                        "01", "1.", "-", "{} {}", "nul", "\"\xFF\"", "{\"a\":1}x"})
    {
        INFO(text);
        CHECK_FALSE(JsonReader::read(text).has_value());
    }
    for (const std::string_view text :
         {"{}", "[]", "{\"a\":[1,2.5,-3e2,true,false,null,\"x\"]}", " {\"a\" : 1} ", "0", "-0.5E+3"})
    {
        INFO(text);
        CHECK(JsonReader::read(text).has_value());
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
// The strict JSON reader behind the signed files. It reads what a licence holds and refuses everything a
// plain parser lets through that two readers could understand differently.

#include "json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

using namespace adl::licence;
using namespace adl::licence::detail;

namespace
{

JsonValue parsed(std::string_view text)
{
    auto value = parse_strict(text);
    REQUIRE(value);
    return std::move(*value);
}

void refused(std::string_view text)
{
    const auto value = parse_strict(text);
    INFO(text);
    REQUIRE(!value);
    REQUIRE(value.error() == Error::malformed);
}

} // namespace

TEST_CASE("json: every kind of value is read", "[licence][json]")
{
    const JsonValue document =
        parsed(R"( { "text": "a", "whole": -12, "list": ["x", "y"], "nothing": null, "yes": true, "no": false,
                     "real": 1.5, "inner": { "k": "v" } } )");
    const JsonObject* object = document.as_object();
    REQUIRE(object != nullptr);
    REQUIRE(object->size() == 8);
    REQUIRE(*find(*object, "text")->as_string() == "a");
    REQUIRE(*find(*object, "whole")->as_integer() == -12);
    REQUIRE(find(*object, "list")->as_array()->size() == 2);
    REQUIRE(find(*object, "nothing")->is_null());
    REQUIRE(find(*object, "yes")->as_string() == nullptr);
    REQUIRE(find(*object, "real")->as_integer() == nullptr);
    REQUIRE(find(*object, "inner")->as_object()->size() == 1);
    REQUIRE(find(*object, "missing") == nullptr);
    REQUIRE(document.as_array() == nullptr);

    REQUIRE(parsed("[]").as_array()->empty());
    REQUIRE(parsed("{}").as_object()->empty());
    REQUIRE(parsed(" \t\r\n[ 1 , 2 ] \n").as_array()->size() == 2);
    REQUIRE(*parsed("0").as_integer() == 0);
    REQUIRE(*parsed("-0").as_integer() == 0);
    REQUIRE(*parsed("9223372036854775807").as_integer() == std::numeric_limits<std::int64_t>::max());
    REQUIRE(*parsed("-9223372036854775808").as_integer() == std::numeric_limits<std::int64_t>::min());
}

TEST_CASE("json: numbers that are not whole numbers in range are read as other numbers", "[licence][json]")
{
    for (const char* text : {"1.0", "1e3", "1E-3", "2.5e+2", "9223372036854775808", "-9223372036854775809",
                             "123456789012345678901234567890"})
    {
        INFO(text);
        const JsonValue value = parsed(text);
        REQUIRE(value.as_integer() == nullptr);
        REQUIRE(!value.is_null());
    }
}

TEST_CASE("json: text is read with its escapes", "[licence][json]")
{
    REQUIRE(*parsed(R"("a\"b\\c\/d\b\f\n\r\t")").as_string() == "a\"b\\c/d\b\f\n\r\t");
    REQUIRE(*parsed(R"("Aé€")").as_string() == "A\xc3\xa9\xe2\x82\xac");
    // A surrogate pair is one code point.
    REQUIRE(*parsed(R"("😀")").as_string() == "\xf0\x9f\x98\x80");
    // Raw UTF-8 passes as it is.
    REQUIRE(*parsed("\"\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80\"").as_string() == "\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80");
    REQUIRE(parsed("\"\"").as_string()->empty());
}

TEST_CASE("json: what a plain parser lets through is refused", "[licence][json]")
{
    // Nothing, and what is not JSON.
    refused("");
    refused("   ");
    refused("nul");
    refused("tru");
    refused("falsy");
    refused("{");
    refused("[");
    refused("]");
    refused("}");
    refused("'a'");
    refused("undefined");

    // Text after the value, a comment, a trailing comma, a byte order mark.
    refused("{} x");
    refused("{}{}");
    refused("{} // note");
    refused("/* note */ {}");
    refused("[1,]");
    refused("{\"a\":1,}");
    refused("[,1]");
    refused("\xef\xbb\xbf{}");

    // Members.
    refused("{\"a\"}");
    refused("{\"a\":}");
    refused("{\"a\" 1}");
    refused("{a:1}");
    refused("{1:1}");
    refused("{\"a\":1 \"b\":2}");
    refused("[1 2]");

    // The same key twice, at any depth, and spelled in two ways.
    refused(R"({"a":1,"a":2})");
    refused(R"({"x":{"a":1,"a":2}})");
    refused(R"({"x":[{"a":1,"b":2,"a":3}]})");
    refused(R"({"a":1,"a":2})");

    // Numbers.
    refused("01");
    refused("-");
    refused("+1");
    refused("1.");
    refused(".5");
    refused("1e");
    refused("1e+");
    refused("0x10");
    refused("NaN");
    refused("Infinity");

    // Strings.
    refused("\"abc");
    refused("\"a\nb\"");
    refused(std::string("\"a") + '\0' + "b\"");
    refused(R"("\x41")");
    refused(R"("\u12")");
    refused(R"("\u12g4")");
    refused(R"("\)");
    refused(R"("\ud800")");
    refused(R"("\ud800A")");
    refused(R"("\udc00")");
    refused(R"("\ud83d\ude0)");
}

TEST_CASE("json: bytes that are not UTF-8 are refused", "[licence][json]")
{
    for (const char* text :
         {"\"\x80\"", "\"\xbf\"", "\"\xc0\x80\"", "\"\xc1\xbf\"", "\"\xc3\"", "\"\xc3\x28\"", "\"\xe0\x80\x80\"",
          "\"\xe0\x9f\xbf\"", "\"\xed\xa0\x80\"", "\"\xe2\x82\"", "\"\xe2\x28\xa1\"", "\"\xf0\x80\x80\x80\"",
          "\"\xf0\x8f\xbf\xbf\"", "\"\xf4\x90\x80\x80\"", "\"\xf5\x80\x80\x80\"", "\"\xf0\x9f\x98\"",
          "\"\xf0\x28\x8c\xbc\"", "\"\xff\"", "\"\xfe\""})
    {
        refused(text);
    }
    // Outside a string only ASCII structure can be.
    refused("\xc3\xa9");
}

TEST_CASE("json: nesting is bounded", "[licence][json]")
{
    std::string deep;
    for (std::size_t i = 0; i < max_json_depth; ++i)
    {
        deep += '[';
    }
    for (std::size_t i = 0; i < max_json_depth; ++i)
    {
        deep += ']';
    }
    REQUIRE(parse_strict(deep));
    refused("[" + deep + "]");

    std::string objects;
    for (std::size_t i = 0; i < max_json_depth; ++i)
    {
        objects += "{\"a\":";
    }
    objects += "1";
    for (std::size_t i = 0; i < max_json_depth; ++i)
    {
        objects += '}';
    }
    REQUIRE(parse_strict(objects));
    refused("{\"a\":" + objects + "}");

    // A hostile file does not need the stack to be deep.
    refused(std::string(100000, '['));
}

TEST_CASE("json: the field readers", "[licence][json]")
{
    const JsonValue document = parsed(
        R"({"name":"abc","empty":"","long":"abcdefgh","whole":5,"big":500,"list":["a","b"],"mixed":["a",1],"none":null,"real":2.5})");
    const JsonObject& object = *document.as_object();

    REQUIRE(*required_string(object, "name", 10) == "abc");
    REQUIRE(!required_string(object, "empty", 10));
    REQUIRE(!required_string(object, "long", 5));
    REQUIRE(!required_string(object, "whole", 10));
    REQUIRE(!required_string(object, "missing", 10));
    REQUIRE(!required_string(object, "none", 10));

    REQUIRE(*optional_string(object, "name", 10) == "abc");
    REQUIRE(optional_string(object, "missing", 10)->empty());
    REQUIRE(optional_string(object, "none", 10)->empty());
    REQUIRE(!optional_string(object, "whole", 10));
    REQUIRE(!optional_string(object, "long", 5));

    REQUIRE(*required_integer(object, "whole", 0, 10) == 5);
    REQUIRE(!required_integer(object, "big", 0, 10));
    REQUIRE(!required_integer(object, "whole", 6, 10));
    REQUIRE(!required_integer(object, "real", 0, 10));
    REQUIRE(!required_integer(object, "name", 0, 10));
    REQUIRE(!required_integer(object, "missing", 0, 10));

    REQUIRE(string_list(object, "list", 4, 4)->size() == 2);
    REQUIRE(string_list(object, "missing", 4, 4)->empty());
    REQUIRE(!string_list(object, "list", 1, 4));
    REQUIRE(!string_list(object, "long", 4, 4));
    REQUIRE(!string_list(object, "mixed", 4, 4));
    REQUIRE(!string_list(object, "name", 4, 4));
    const JsonValue long_entry = parsed(R"({"l":["abcdef"]})");
    REQUIRE(!string_list(*long_entry.as_object(), "l", 4, 4));
    const JsonValue empty_entry = parsed(R"({"l":[""]})");
    REQUIRE(!string_list(*empty_entry.as_object(), "l", 4, 4));
}

TEST_CASE("json: tokens", "[licence][json]")
{
    REQUIRE(is_token("abc-1", 8, "-"));
    REQUIRE(is_token("a.b_c", 8, "._"));
    REQUIRE(!is_token("", 8, "-"));
    REQUIRE(!is_token("abcdefghi", 8, "-"));
    REQUIRE(!is_token("Abc", 8, "-"));
    REQUIRE(!is_token("a_b", 8, "-"));
    REQUIRE(!is_token("a b", 8, "-"));
}

TEST_CASE("json: quoting", "[licence][json]")
{
    REQUIRE(quote("") == "\"\"");
    REQUIRE(quote("plain") == "\"plain\"");
    REQUIRE(quote("a\"b\\c") == "\"a\\\"b\\\\c\"");
    REQUIRE(quote("\b\f\n\r\t") == "\"\\b\\f\\n\\r\\t\"");
    REQUIRE(quote(std::string_view("\x01\x1f", 2)) == "\"\\u0001\\u001f\"");
    // Text that is not ASCII passes as it is, and what is quoted reads back as what it was.
    const std::string text = "caf\xc3\xa9 \"\\ \n \x02";
    REQUIRE(*parsed(quote(text)).as_string() == text);
}

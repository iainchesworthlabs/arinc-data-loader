// SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause
// The strict JSON reader as a program outside the library reaches it: through adl/licence/json.hpp alone, in a
// test that is given no include directory of the library's src/. What the reader refuses is tested in
// json_test.cpp; this file tests that the public names are the reader's and that a document of the shape a
// scenario or a description file has (a list of objects that hold objects) is read.

#include "adl/licence/json.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace json = adl::licence::json;

namespace
{

// The public names are the reader's own types, not copies of them.
static_assert(std::is_same_v<json::Value, adl::licence::detail::JsonValue>);
static_assert(std::is_same_v<json::Member, adl::licence::detail::JsonMember>);
static_assert(std::is_same_v<json::Array, adl::licence::detail::JsonArray>);
static_assert(std::is_same_v<json::Object, adl::licence::detail::JsonObject>);
static_assert(std::is_same_v<json::OtherNumber, adl::licence::detail::JsonOtherNumber>);
static_assert(json::max_depth == 8);

json::Value parsed(std::string_view text)
{
    auto value = json::parse_strict(text);
    REQUIRE(value);
    return std::move(*value);
}

} // namespace

TEST_CASE("json public: a document of lists and objects is read through the public names", "[licence][json]")
{
    const json::Value document = parsed(R"({
        "kind": "example", "format": 1, "seed": 12345,
        "steps": [
            { "when": { "request": 2 }, "do": { "wait": { "seconds": 3 } } },
            { "when": { "after_ms": 1500 }, "do": { "refuse": { "operation": "upload", "code": "0x1002" } } }
        ] })");
    const json::Object* top = document.as_object();
    REQUIRE(top != nullptr);
    REQUIRE(*json::required_string(*top, "kind", 16) == "example");
    REQUIRE(*json::required_integer(*top, "format", 1, 1) == 1);
    REQUIRE(*json::required_integer(*top, "seed", 0, 1000000) == 12345);

    const json::Value* steps = json::find(*top, "steps");
    REQUIRE(steps != nullptr);
    REQUIRE(steps->as_array() != nullptr);
    REQUIRE(steps->as_array()->size() == 2);
    const json::Object& second = *(*steps->as_array())[1].as_object();
    const json::Object& when = *json::find(second, "when")->as_object();
    REQUIRE(*json::required_integer(when, "after_ms", 0, 100000) == 1500);
    const json::Object& action = *json::find(second, "do")->as_object();
    const json::Object& refuse = *json::find(action, "refuse")->as_object();
    REQUIRE(*json::required_string(refuse, "operation", 16) == "upload");
}

TEST_CASE("json public: a number that is not a whole number is another number", "[licence][json]")
{
    const json::Value document = parsed(R"({"a": 1.5, "b": 1e3, "c": 2})");
    const json::Object& object = *document.as_object();
    REQUIRE(std::holds_alternative<json::OtherNumber>(json::find(object, "a")->data));
    REQUIRE(!json::required_integer(object, "a", 0, 10));
    REQUIRE(!json::required_integer(object, "b", 0, 10000));
    REQUIRE(*json::required_integer(object, "c", 0, 10) == 2);
}

TEST_CASE("json public: the errors are the reader's", "[licence][json]")
{
    for (const char* text : {"", "{", R"({"a":1,"a":2})", "[1,]", "{} x", "\xef\xbb\xbf{}", "// c\n{}", "\"\xc3\x28\""})
    {
        INFO(text);
        const auto value = json::parse_strict(text);
        REQUIRE(!value);
        REQUIRE(value.error() == adl::licence::Error::malformed);
    }
}

TEST_CASE("json public: nesting stops at max_depth", "[licence][json]")
{
    std::string ok;
    std::string too_deep;
    for (std::size_t i = 0; i < json::max_depth; ++i)
    {
        ok += '[';
    }
    for (std::size_t i = 0; i < json::max_depth; ++i)
    {
        ok += ']';
    }
    too_deep = "[" + ok + "]";
    REQUIRE(json::parse_strict(ok));
    REQUIRE(!json::parse_strict(too_deep));
}

TEST_CASE("json public: the other helpers", "[licence][json]")
{
    const json::Value document = parsed(R"({"names":["a","b"],"nothing":null})");
    const json::Object& object = *document.as_object();
    REQUIRE(json::string_list(object, "names", 4, 4)->size() == 2);
    REQUIRE(json::string_list(object, "missing", 4, 4)->empty());
    REQUIRE(json::optional_string(object, "nothing", 4)->empty());
    REQUIRE(json::is_token("fault-1", 16, "-"));
    REQUIRE(!json::is_token("Fault", 16, "-"));
    const std::string text = "a \"quoted\" \\ line\n";
    REQUIRE(*parsed(json::quote(text)).as_string() == text);
}

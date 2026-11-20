// SPDX-License-Identifier: GPL-3.0-or-later
// The page of the files (docs/file-reference.md) says what the program reads and writes: the example description on
// it is read by the program, and every key that the program writes in an inspection, or that the example description
// uses, is a key that the page's tables name.

#include "adl/licence/json.hpp"
#include "adl/make_set/command.hpp"
#include "adl/make_set/description.hpp"
#include "support.hpp"
#include "temp_dir.hpp"

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{

namespace json = adl::licence::json;

std::string page_text()
{
    std::ifstream in(ADL_FILE_REFERENCE, std::ios::binary);
    std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    std::string lf;
    for (const char c : text)
    {
        if (c != '\r')
        {
            lf += c;
        }
    }
    return lf;
}

// The text between a heading (at the start of a line) and the next heading of the same level.
std::string section(const std::string& page, const std::string& heading)
{
    const std::size_t start = page.find("\n" + heading + "\n");
    REQUIRE(start != std::string::npos);
    const std::size_t level = heading.find(' ');
    const std::size_t next = page.find("\n" + heading.substr(0, level + 1), start + heading.size() + 2);
    return page.substr(start, next == std::string::npos ? std::string::npos : next - start);
}

// The first block of JSON in a stretch of the page.
std::string json_block(const std::string& text)
{
    const std::size_t start = text.find("```json\n");
    REQUIRE(start != std::string::npos);
    const std::size_t end = text.find("\n```", start + 8);
    REQUIRE(end != std::string::npos);
    return text.substr(start + 8, end - start - 8);
}

// A key is named on the page as a word in code (`key`) or as a key of an object in code ("key").
bool named(const std::string& text, const std::string& key)
{
    return text.find("`" + key + "`") != std::string::npos || text.find("\"" + key + "\"") != std::string::npos;
}

void keys_of(const json::Value& value, std::set<std::string>& keys)
{
    if (const json::Object* object = value.as_object())
    {
        for (const json::Member& member : *object)
        {
            keys.insert(member.key);
            keys_of(member.value, keys);
        }
    }
    else if (const json::Array* array = value.as_array())
    {
        for (const json::Value& item : *array)
        {
            keys_of(item, keys);
        }
    }
}

} // namespace

TEST_CASE("docs: the example description of the page is read, and its keys are on the page", "[docs]")
{
    const std::string page = page_text();
    const std::string description = section(page, "## The media set description");
    const std::string example = json_block(description);

    const auto read = adl::make_set::read_description(example, "root");
    REQUIRE(read.has_value());
    CHECK(read->plan.loads.size() == 2);

    const auto document = json::parse_strict(example);
    REQUIRE(document.has_value());
    std::set<std::string> keys;
    keys_of(*document, keys);
    CHECK(keys.size() >= 18);
    for (const std::string& key : keys)
    {
        INFO(key);
        CHECK(named(description, key));
    }
    // Keys the example does not use are on the page too.
    for (const char* key : {"targets", "id", "loads", "description"})
    {
        INFO(key);
        CHECK(named(description, key));
    }
}

TEST_CASE("docs: every key that an inspection document has is on the page", "[docs]")
{
    const std::string page = page_text();
    const std::string inspection = section(page, "## The media set inspection");

    // A set with everything a set can have in a description: two loads, two members and a batch.
    adl::test::TempDir work;
    const std::string description = section(page, "## The media set description");
    adl::test::set_maker::write_file(work.path() / "payloads", "app.bin", "0123456789ABCDEFG");
    adl::test::set_maker::write_file(work.path() / "payloads", "nav.bin", "HELLO");
    adl::test::set_maker::write_file(work.path() / "payloads", "CONFIG.LDR", "ABCD");
    adl::test::set_maker::write_file(work.path() / "payloads", "selftest.bin", "SELFTEST");
    adl::test::set_maker::write_file(work.path(), "set.json", json_block(description));

    const auto utf8 = [](const std::filesystem::path& path) { return path.string(); };
    std::vector<std::string> arguments = {"make", utf8(work.path() / "set.json"), "--output",
                                          utf8(work.path() / "out")};
    std::vector<std::string_view> views(arguments.begin(), arguments.end());
    std::ostringstream out;
    std::ostringstream err;
    REQUIRE(adl::make_set::run_command(views, out, err) == adl::make_set::exit_ok);

    arguments = {"inspect", utf8(work.path() / "out"), "--json"};
    views.assign(arguments.begin(), arguments.end());
    std::ostringstream document_out;
    REQUIRE(adl::make_set::run_command(views, document_out, err) == adl::make_set::exit_ok);
    const auto document = json::parse_strict(document_out.str());
    REQUIRE(document.has_value());
    std::set<std::string> keys;
    keys_of(*document, keys);
    CHECK(keys.size() >= 50);
    for (const std::string& key : keys)
    {
        INFO(key);
        CHECK(named(inspection, key));
    }

    // The example of the page is itself an inspection document, and has the same keys, but for the lists it cuts.
    const auto example = json::parse_strict(json_block(inspection.substr(inspection.find("### An example"))));
    REQUIRE(example.has_value());
    std::set<std::string> example_keys;
    keys_of(*example, example_keys);
    for (const std::string& key : example_keys)
    {
        INFO(key);
        CHECK(keys.contains(key));
    }
}

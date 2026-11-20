// SPDX-License-Identifier: GPL-3.0-or-later
// The description file: what it says, what it refuses and the words it refuses with.

#include "adl/make_set/description.hpp"
#include "support.hpp"
#include "temp_dir.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <string>
#include <string_view>

namespace
{

namespace fs = std::filesystem;
using namespace adl::make_set;
using adl::arinc665::CheckValueType;
using adl::set_maker::FileKind;

// The description of the guide's example: two loads, two members, a batch.
constexpr std::string_view example = R"({
  "format": 1,
  "kind": "adl-media-set-description",
  "media_set": { "part_number": "ACM-SET-0001", "members": 2, "files_check": "sha1", "files_list_check": "crc32" },
  "loads": [
    {
      "part_number": "ACM47-1234-5678",
      "header_file": "ACM47.LUH",
      "type": "OPS",
      "target_hardware_ids": ["ACME", "ACMEFMS"],
      "check": "sha1",
      "files": [
        { "source": "payloads/app.bin",    "name": "APPCODE.LUP", "kind": "data",    "check": "crc32" },
        { "source": "payloads/nav.bin",    "name": "NAVDATA.LUP", "kind": "data",    "check": "crc32" },
        { "source": "payloads/CONFIG.LDR", "kind": "support" }
      ]
    },
    {
      "part_number": "ACM4C-0100-0200",
      "type": { "description": "Test software", "id": 4 },
      "target_hardware_ids": ["ACME"],
      "files": [ { "source": "payloads/selftest.bin", "name": "SELFTEST.LUP" } ]
    }
  ],
  "batch": { "part_number": "ACM4D-0100-0300", "comment": "Both loads for the unit" }
})";

// A description with one load, built from a few pieces so that a test changes one.
std::string small(std::string_view media_set_extra = {}, std::string_view load_extra = {},
                  std::string_view file_extra = {}, std::string_view top_extra = {})
{
    std::string text = R"({"format":1,"kind":"adl-media-set-description",)";
    text += R"("media_set":{"part_number":"ACM-SET-0001")";
    text += media_set_extra;
    text += R"(},"loads":[{"part_number":"ACM47-1234-5678","files":[{"source":"a/app.bin","name":"APP.LUP")";
    text += file_extra;
    text += "}]";
    text += load_extra;
    text += "}]";
    text += top_extra;
    text += "}";
    return text;
}

// The sentences of a refusal, which there has to be.
std::vector<std::string> refused(std::string_view text)
{
    const auto description = read_description(text, "root");
    REQUIRE_FALSE(description.has_value());
    REQUIRE_FALSE(description.error().sentences.empty());
    return description.error().sentences;
}

std::string first_refusal(std::string_view text)
{
    return refused(text).front();
}

} // namespace

TEST_CASE("description: the example of the guide is read", "[description]")
{
    const auto description = read_description(example, "root");
    REQUIRE(description.has_value());
    const auto& plan = description->plan;
    CHECK(plan.part_number == "ACM-SET-0001");
    CHECK(plan.members == 2);
    CHECK(plan.files_check == CheckValueType::sha1);
    CHECK(plan.files_list_check == CheckValueType::crc32);
    REQUIRE(plan.loads.size() == 2);

    const auto& first = plan.loads[0];
    CHECK(first.part_number == "ACM47-1234-5678");
    CHECK(first.header_file_name == "ACM47.LUH");
    REQUIRE(first.type.has_value());
    CHECK(first.type->description == "Operational software");
    CHECK(first.type->id == 1);
    CHECK(first.target_hardware_ids == std::vector<std::string>{"ACME", "ACMEFMS"});
    CHECK(first.check == CheckValueType::sha1);
    REQUIRE(first.files.size() == 3);
    CHECK(first.files[0].source == fs::path{"root"} / "payloads/app.bin");
    CHECK(first.files[0].name == "APPCODE.LUP");
    CHECK(first.files[0].kind == FileKind::data);
    CHECK(first.files[0].check == CheckValueType::crc32);
    CHECK(first.files[1].name == "NAVDATA.LUP");
    // The name is the last part of the source, the kind is data and the check is none, unless said.
    CHECK(first.files[2].name == "CONFIG.LDR");
    CHECK(first.files[2].kind == FileKind::support);
    CHECK(first.files[2].check == CheckValueType::none);

    const auto& second = plan.loads[1];
    CHECK(second.header_file_name == "ACM.LUH"); // the first three characters, and .LUH
    REQUIRE(second.type.has_value());
    CHECK(second.type->description == "Test software");
    CHECK(second.type->id == 4);
    CHECK(second.check == CheckValueType::none);

    REQUIRE(plan.batch.has_value());
    CHECK(plan.batch->part_number == "ACM4D-0100-0300");
    CHECK(plan.batch->file_name == "ACM.LUB"); // the first three characters, and .LUB
    CHECK(plan.batch->comment == "Both loads for the unit");
    // Derived: each distinct id, in the order met, with the loads that name it.
    REQUIRE(plan.batch->targets.size() == 2);
    CHECK(plan.batch->targets[0].id == "ACME");
    CHECK(plan.batch->targets[0].load_part_numbers == std::vector<std::string>{"ACM47-1234-5678", "ACM4C-0100-0200"});
    CHECK(plan.batch->targets[1].id == "ACMEFMS");

    REQUIRE(description->sources.size() == 2);
    CHECK(description->sources[0][2] == "payloads/CONFIG.LDR");
}

TEST_CASE("description: only what has to be given has to be", "[description]")
{
    const auto description = read_description(small(), "root");
    REQUIRE(description.has_value());
    CHECK(description->plan.members == 1);
    CHECK(description->plan.files_check == CheckValueType::none);
    CHECK(description->plan.files_list_check == CheckValueType::none);
    CHECK_FALSE(description->plan.loads[0].type.has_value());
    CHECK(description->plan.loads[0].target_hardware_ids.empty());
    CHECK(description->plan.loads[0].header_file_name == "ACM.LUH");
    CHECK_FALSE(description->plan.batch.has_value());

    // A null is the same as leaving the key out.
    CHECK(read_description(small(R"(,"members":null,"files_check":null)",
                                 R"(,"type":null,"check":null,"header_file":null)", R"(,"kind":null,"check":null)",
                                 R"(,"batch":null)"),
                           "root")
              .has_value());
}

TEST_CASE("description: a batch names its targets, or they are derived", "[description]")
{
    const std::string load = R"(,"target_hardware_ids":["ACME"])";
    auto description = read_description(
        small(
            {}, load, {},
            R"(,"batch":{"part_number":"ACM4C-0100-0200","file_name":"BATCH.LUB","targets":[{"id":"ACME_L","loads":["ACM47-1234-5678"]},{"id":"ACME_R","loads":["ACM47-1234-5678"]}]})"),
        "root");
    REQUIRE(description.has_value());
    CHECK(description->plan.batch->file_name == "BATCH.LUB");
    REQUIRE(description->plan.batch->targets.size() == 2);
    CHECK(description->plan.batch->targets[1].id == "ACME_R");

    // No id anywhere and no targets given: there is nothing for the batch to name.
    CHECK(first_refusal(small({}, {}, {}, R"(,"batch":{"part_number":"ACM4C-0100-0200"})")) ==
          "batch.targets: the batch names no target.");
    // A target that names a load the set does not have.
    CHECK(
        first_refusal(small(
            {}, load, {},
            R"(,"batch":{"part_number":"ACM4C-0100-0200","targets":[{"id":"ACME_L","loads":["ACM99-9999-9999"]}]})")) ==
        "batch.targets[0].loads[0]: no load of the set has this part number.");
}

TEST_CASE("description: the check type words", "[description]")
{
    for (const char* word : {"none", "crc8", "crc16", "crc32", "md5", "sha1"})
    {
        INFO(word);
        CHECK(read_description(small(std::string{R"(,"files_check":")"} + word + "\""), "root").has_value());
    }
    CHECK(first_refusal(small(R"(,"files_check":"sha256")")) ==
          "media_set.files_check: expected one of none, crc8, crc16, crc32, md5, sha1.");
    CHECK(first_refusal(small(R"(,"files_list_check":5)")) ==
          "media_set.files_list_check: expected one of none, crc8, crc16, crc32, md5, sha1.");
    CHECK(first_refusal(small({}, R"(,"check":"SHA1")")) ==
          "loads[0].check: expected one of none, crc8, crc16, crc32, md5, sha1.");
    CHECK(first_refusal(small({}, {}, R"(,"check":"crc")")) ==
          "loads[0].files[0].check: expected one of none, crc8, crc16, crc32, md5, sha1.");
}

TEST_CASE("description: the load types", "[description]")
{
    for (const char* code : {"OPS", "NDB", "ADB", "TST"})
    {
        INFO(code);
        const auto description = read_description(small({}, std::string{R"(,"type":")"} + code + "\""), "root");
        REQUIRE(description.has_value());
        CHECK(description->plan.loads[0].type.has_value());
    }
    const auto own =
        read_description(small({}, R"(,"type":{"description":"EEC Operational Software","id":65535})"), "root");
    REQUIRE(own.has_value());
    CHECK(own->plan.loads[0].type->description == "EEC Operational Software");
    CHECK(own->plan.loads[0].type->id == 65535);

    const std::string expected = ": expected one of OPS, NDB, ADB, TST, or an object with description and id.";
    CHECK(first_refusal(small({}, R"(,"type":"ops")")) == "loads[0].type" + expected);
    CHECK(first_refusal(small({}, R"(,"type":7)")) == "loads[0].type" + expected);
    CHECK(first_refusal(small({}, R"(,"type":{"description":"x"})")) == "loads[0].type.id: this key is missing.");
    CHECK(first_refusal(small({}, R"(,"type":{"id":1})")) == "loads[0].type.description: this key is missing.");
    CHECK(first_refusal(small({}, R"(,"type":{"description":"x","id":65536})")) ==
          "loads[0].type.id: expected a whole number from 0 to 65535.");
    CHECK(first_refusal(small({}, R"(,"type":{"description":"x","id":1.5})")) ==
          "loads[0].type.id: expected a whole number from 0 to 65535.");
    CHECK(first_refusal(small({}, R"(,"type":{"description":"caf\u00e9","id":1})")) ==
          "loads[0].type.description: expected printable ASCII characters.");
    CHECK(first_refusal(small({}, R"(,"type":{"description":"x","id":1,"colour":"red"})")) ==
          "loads[0].type.colour: this key is not known.");
}

TEST_CASE("description: a key it does not know is refused, wherever it is", "[description]")
{
    CHECK(first_refusal(small({}, {}, {}, R"(,"colour":1)")) == "colour: this key is not known.");
    CHECK(first_refusal(small(R"(,"member":2)")) == "media_set.member: this key is not known.");
    CHECK(first_refusal(small({}, R"(,"chek":"sha1")")) == "loads[0].chek: this key is not known.");
    CHECK(first_refusal(small({}, {}, R"(,"sorce":"x")")) == "loads[0].files[0].sorce: this key is not known.");
    CHECK(first_refusal(small({}, {}, {}, R"(,"batch":{"part_number":"ACM4C-0100-0200","comments":"x"})")) ==
          "batch.comments: this key is not known.");
    CHECK(
        first_refusal(small(
            {}, R"(,"target_hardware_ids":["ACME"])", {},
            R"(,"batch":{"part_number":"ACM4C-0100-0200","targets":[{"id":"ACME","loads":["ACM47-1234-5678"],"x":1}]})")) ==
        "batch.targets[0].x: this key is not known.");
}

TEST_CASE("description: the first things that are missing or of the wrong kind", "[description]")
{
    CHECK(first_refusal(R"({"kind":"adl-media-set-description","media_set":{},"loads":[]})") ==
          "format: this key is missing.");
    CHECK(first_refusal(R"({"format":"1"})") == "format: expected a whole number.");
    CHECK(first_refusal(R"({"format":2})") == "format 2 is not one this program reads (it reads format 1).");
    CHECK(first_refusal(R"({"format":1})") == "kind: this key is missing.");
    CHECK(first_refusal(R"({"format":1,"kind":"adl-operation-record"})") ==
          "kind: expected one of adl-media-set-description.");
    CHECK(first_refusal(R"({"format":1,"kind":"adl-media-set-description"})") == "media_set: this key is missing.");
    CHECK(first_refusal(R"({"format":1,"kind":"adl-media-set-description","media_set":[]})") ==
          "media_set: expected an object.");
    CHECK(first_refusal(R"({"format":1,"kind":"adl-media-set-description","media_set":{}})") ==
          "media_set.part_number: this key is missing.");
    CHECK(first_refusal(R"({"format":1,"kind":"adl-media-set-description","media_set":{"part_number":"X"}})") ==
          "loads: this key is missing.");
    CHECK(first_refusal(
              R"({"format":1,"kind":"adl-media-set-description","media_set":{"part_number":"X"},"loads":{}})") ==
          "loads: expected a list.");
    CHECK(first_refusal(
              R"({"format":1,"kind":"adl-media-set-description","media_set":{"part_number":"X"},"loads":[]})") ==
          "loads: expected a list of 1 to 100 entries.");
    CHECK(first_refusal(
              R"({"format":1,"kind":"adl-media-set-description","media_set":{"part_number":"X"},"loads":[1]})") ==
          "loads[0]: expected an object.");
    CHECK(first_refusal(small(R"(,"part_number":"Y")")).starts_with("The text is not JSON")); // a key given twice
    CHECK(first_refusal("[]") == "The description is not an object.");
    CHECK(first_refusal(small(R"(,"members":0)")) == "media_set.members: expected a whole number from 1 to 255.");
    CHECK(first_refusal(small(R"(,"members":256)")) == "media_set.members: expected a whole number from 1 to 255.");
    CHECK(first_refusal(small(R"(,"members":2.0)")) == "media_set.members: expected a whole number from 1 to 255.");
    CHECK(first_refusal(small(R"(,"members":"2")")) == "media_set.members: expected a whole number from 1 to 255.");
    CHECK(first_refusal(small({}, R"(,"files":[])")).starts_with("The text is not JSON")); // given twice
    CHECK(
        first_refusal(
            R"({"format":1,"kind":"adl-media-set-description","media_set":{"part_number":"X"},"loads":[{"part_number":"X"}]})") ==
        "loads[0].files: this key is missing.");
    CHECK(
        first_refusal(
            R"({"format":1,"kind":"adl-media-set-description","media_set":{"part_number":"X"},"loads":[{"part_number":"X","files":[]}]})") ==
        "loads[0].files: expected a list of 1 to 2000 entries.");
    CHECK(first_refusal(small({}, {}, R"(,"kind":"binary")")) ==
          "loads[0].files[0].kind: expected one of data, support.");
    CHECK(first_refusal(small({}, R"(,"target_hardware_ids":[7])")) ==
          "loads[0].target_hardware_ids[0]: expected a string of 1 to 255 bytes.");
    CHECK(first_refusal(small({}, R"(,"target_hardware_ids":"ACME")")) ==
          "loads[0].target_hardware_ids: expected a list.");
    CHECK(first_refusal(small(R"(,"files_check":"none")", {}, {}, R"(,"batch":{"part_number":""})")) ==
          "batch.part_number: expected a string of 1 to 255 bytes.");
}

TEST_CASE("description: a source is a relative path written with a slash", "[description]")
{
    const std::string sentence = "loads[0].files[0].source: this source path is not a relative path written with /.";
    for (const std::string source :
         {"/etc/app.bin", "C:/app.bin", "C:app.bin", "a\\\\b", "a//b", "a/", "./a", "a/./b", "a:b", "\\u0001"})
    {
        INFO(source);
        const std::string text =
            R"({"format":1,"kind":"adl-media-set-description","media_set":{"part_number":"ACM-SET-0001"},"loads":[{"part_number":"ACM47-1234-5678","files":[{"source":")" +
            source + R"(","name":"APP.LUP"}]}]})";
        CHECK(first_refusal(text) == sentence);
    }
    for (const std::string source : {"app.bin", "a/b/app.bin", "../payloads/app.bin", "a/../b.bin",
                                     "dir with spaces/app.bin", "caf\xC3\xA9/\xE6\x97\xA5\xE6\x9C\xAC.bin"})
    {
        INFO(source);
        const std::string text =
            R"({"format":1,"kind":"adl-media-set-description","media_set":{"part_number":"ACM-SET-0001"},"loads":[{"part_number":"ACM47-1234-5678","files":[{"source":")" +
            source + R"(","name":"APP.LUP"}]}]})";
        const auto description = read_description(text, "root");
        REQUIRE(description.has_value());
        CHECK(description->sources[0][0] == source);
    }
    CHECK(first_refusal(small({}, {}, R"(,"source":"b")")).starts_with("The text is not JSON"));
    const std::string too_long(1025, 'a');
    CHECK(
        first_refusal(
            R"({"format":1,"kind":"adl-media-set-description","media_set":{"part_number":"X"},"loads":[{"part_number":"X","files":[{"source":")" +
            too_long + R"("}]}]})") == "loads[0].files[0].source: expected a string of 1 to 1024 bytes.");
}

TEST_CASE("description: the rules of the window are the rules of the description", "[description]")
{
    // Each of these is marked in the window's form, and is refused here with the place in the file.
    CHECK(first_refusal(small(R"(,"part_number":"x")")).starts_with("The text is not JSON")); // given twice
    CHECK(
        first_refusal(
            R"({"format":1,"kind":"adl-media-set-description","media_set":{"part_number":"ACM SET"},"loads":[{"part_number":"ACM47-1234-5678","files":[{"source":"a","name":"APP.LUP"}]}]})")
            .starts_with("media_set.part_number: "));
    CHECK(
        first_refusal(
            R"({"format":1,"kind":"adl-media-set-description","media_set":{"part_number":"ACM-SET-0001"},"loads":[{"part_number":"ACM47-1234-0000","files":[{"source":"a","name":"APP.LUP"}]}]})")
            .starts_with("loads[0].part_number: 2.1.3: "));
    CHECK(first_refusal(small({}, R"(,"header_file":"CON.LUH")")) ==
          "loads[0].header_file: this name will not work on every system.");
    CHECK(first_refusal(small({}, {}, R"(,"name":"A*B.LUP")")) ==
          "The text is not JSON that this program reads: a key given twice, a comment, a trailing comma, a byte order "
          "mark, text after the value, a control character in a string, bytes that are not UTF-8 or a value nested "
          "more than eight deep are refused.");

    // Every problem is said, not only the first.
    const std::string text =
        R"({"format":1,"kind":"adl-media-set-description","media_set":{"part_number":"ACM SET"},"loads":[{"part_number":"nope","files":[{"source":"a","name":"x y"}]}]})";
    const auto sentences = refused(text);
    CHECK(sentences.size() >= 4);
    CHECK(sentences[0].starts_with("media_set.part_number: "));
    CHECK(sentences[1].starts_with("loads[0].part_number: "));
}

TEST_CASE("description: the strict reader's refusals are refusals here", "[description]")
{
    const std::string not_json_start = "The text is not JSON that this program reads";
    for (const std::string text :
         {std::string{}, std::string{"{"}, std::string{"{} x"},
          std::string{"\xEF\xBB\xBF"
                      "{}"},
          std::string{"// comment\n{}"}, std::string{R"({"format":1,})"}, std::string{R"({"format":1,"format":1})"},
          std::string{"\"\xC3\x28\""}, std::string{"{\"a\":\"line\nfeed\"}"}, std::string{"not json"}})
    {
        INFO(text);
        CHECK(first_refusal(text).starts_with(not_json_start));
    }
    // Nested deeper than the reader allows (eight): the description needs five.
    std::string deep;
    for (int i = 0; i < 9; ++i)
    {
        deep += '[';
    }
    for (int i = 0; i < 9; ++i)
    {
        deep += ']';
    }
    CHECK(first_refusal(deep).starts_with(not_json_start));
    // A text larger than the limit is refused before it is read.
    CHECK(first_refusal(std::string(max_description_bytes + 1, ' ')) == "The text is larger than 4 MiB.");
}

TEST_CASE("description: the limits of format 1", "[description]")
{
    const auto load = [](int n, int files)
    {
        std::string text = R"({"part_number":"ACM47-1234-5678)" + std::to_string(n) + R"(","files":[)";
        for (int i = 0; i < files; ++i)
        {
            text += std::string{i == 0 ? "" : ","} + R"({"source":"a/f)" + std::to_string(i) + R"(.bin"})";
        }
        return text + "]}";
    };
    const auto document = [&](int loads, int files)
    {
        std::string text =
            R"({"format":1,"kind":"adl-media-set-description","media_set":{"part_number":"X"},"loads":[)";
        for (int i = 0; i < loads; ++i)
        {
            text += std::string{i == 0 ? "" : ","} + load(i, files);
        }
        return text + "]}";
    };

    CHECK(first_refusal(document(101, 1)) == "loads: expected a list of 1 to 100 entries.");
    CHECK(first_refusal(document(1, 2001)) == "loads[0].files: expected a list of 1 to 2000 entries.");
    // 11 loads of 2000 files are 22000 files: more than all the files that are allowed.
    CHECK(first_refusal(document(11, 2000)) == "More than 20000 files in all.");
    // At the limits, the plan is read and then refused by the rules of the window (these are not part numbers).
    CHECK(first_refusal(document(100, 1)).starts_with("loads[0].part_number: "));
    CHECK(first_refusal(small(
              {}, R"(,"target_hardware_ids":)" +
                      []
                      {
                          std::string ids = "[";
                          for (int i = 0; i < 65; ++i)
                          {
                              ids += std::string{i == 0 ? "" : ","} + "\"ACME" + std::to_string(i) + "\"";
                          }
                          return ids + "]";
                      }())) == "loads[0].target_hardware_ids: expected a list of 0 to 64 entries.");
}

TEST_CASE("description: written out and read again it is the same description", "[description]")
{
    const auto description = read_description(example, "root");
    REQUIRE(description.has_value());
    const std::string written = write_description(*description);
    CHECK(written.find('\n') == std::string::npos); // one line
    const auto again = read_description(written, "root");
    REQUIRE(again.has_value());
    CHECK(write_description(*again) == written);
    CHECK(again->plan.loads.size() == 2);
    CHECK(again->plan.batch->targets.size() == 2);
    CHECK(again->plan.loads[1].type->id == 4);
    CHECK(again->sources == description->sources);
}

TEST_CASE("description: a file is read, with the folder that holds it as the root of its sources", "[description]")
{
    adl::test::TempDir directory;
    const fs::path file = adl::test::set_maker::write_file(directory.path() / "desc", "set.json", example);

    auto description = read_description_file(file, std::nullopt);
    REQUIRE(description.has_value());
    CHECK(description->plan.loads[0].files[0].source == directory.path() / "desc" / "payloads/app.bin");

    description = read_description_file(file, directory.path() / "elsewhere");
    REQUIRE(description.has_value());
    CHECK(description->plan.loads[0].files[0].source == directory.path() / "elsewhere" / "payloads/app.bin");

    const auto missing = read_description_file(directory.path() / "missing.json", std::nullopt);
    REQUIRE_FALSE(missing.has_value());
    CHECK(missing.error().sentences.front().starts_with("Cannot read "));

    const fs::path big =
        adl::test::set_maker::write_file(directory.path(), "big.json", std::string(max_description_bytes + 1, ' '));
    const auto too_big = read_description_file(big, std::nullopt);
    REQUIRE_FALSE(too_big.has_value());
    CHECK(too_big.error().sentences.front().ends_with(" is larger than 4 MiB."));

    // A description with a folder, not a file.
    CHECK_FALSE(read_description_file(directory.path(), std::nullopt).has_value());
}

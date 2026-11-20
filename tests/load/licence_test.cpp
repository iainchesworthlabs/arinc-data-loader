// SPDX-License-Identifier: GPL-3.0-or-later
// The licence in the operation record of adl_load: what a program that gives a licence source gets in the record's
// `licence`, that a program that gives none gets null, that the source is asked once for an operation and before
// anything is written, and that the commands' events do not change (docs/file-reference.md, "The licence in a
// record"). Reading the licence files is tested in tests/licence_platform; here the source is a function.

#include "adl/load/licence.hpp"
#include "command_rig.hpp"

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <vector>

using adl::loader_core::RecordedLicence;
using adl::test::CommandRig;
using adl::test::CommandRun;
using adl::test::Json;
using adl::test::SimPlan;

namespace
{

// Where the fake world's wall clock starts: 2026-11-19T10:15:30Z, the time of the contract's own example.
constexpr std::int64_t wall_origin = 1'795'083'330;

CommandRun information_with_record(CommandRig& rig)
{
    rig.rig.world.set_wall_origin(wall_origin);
    return rig.run(
        {"information", "--target", "10.0.0.2", "--position", "HNPFMS_L", "--json", "--record", "/rec.json"});
}

[[nodiscard]] Json record_of(CommandRig& rig)
{
    const auto parsed = adl::test::parse_json(adl::test::fake_text(rig.rig.loader_host, "/rec.json"));
    REQUIRE(parsed.has_value());
    return *parsed;
}

[[nodiscard]] adl::load::LicenceSource gives(std::optional<RecordedLicence> licence, int* asked = nullptr)
{
    return adl::load::LicenceSource{[licence, asked](adl::platform::Host&) -> std::optional<RecordedLicence>
                                    {
                                        if (asked != nullptr)
                                        {
                                            ++*asked;
                                        }
                                        return licence;
                                    }};
}

} // namespace

TEST_CASE("a record says null for the licence when the program gives no licence source",
          "[load][integration][record][licence]")
{
    CommandRig rig;
    const CommandRun run = information_with_record(rig);
    CHECK(run.exit_code == 0);
    CHECK(record_of(rig).at("licence").is(Json::Kind::null));
}

TEST_CASE("a record carries the licence the source gives", "[load][integration][record][licence]")
{
    CommandRig rig;
    rig.licence = gives(RecordedLicence{"pro", "expiring", "ADL-2026-000123"});
    const CommandRun run = information_with_record(rig);
    CHECK(run.exit_code == 0);

    const Json record = record_of(rig);
    const Json& licence = record.at("licence");
    REQUIRE(licence.is(Json::Kind::object));
    CHECK(licence.keys() == std::vector<std::string>{"edition", "state", "number"});
    CHECK(licence.at("edition").text == "pro");
    CHECK(licence.at("state").text == "expiring");
    CHECK(licence.at("number").text == "ADL-2026-000123");
    // It sits where the file's order puts it, between the download folder and the log.
    const std::vector<std::string> keys = record.keys();
    CHECK(keys[keys.size() - 3] == "download_folder");
    CHECK(keys[keys.size() - 2] == "licence");
    CHECK(keys[keys.size() - 1] == "log");
}

TEST_CASE("the Community edition with no licence file is a state and no number", "[load][integration][record][licence]")
{
    CommandRig rig;
    rig.licence = gives(RecordedLicence{"community", "none", std::nullopt});
    const CommandRun run = information_with_record(rig);
    CHECK(run.exit_code == 0);
    const Json record = record_of(rig);
    CHECK(record.at("licence").at("edition").text == "community");
    CHECK(record.at("licence").at("state").text == "none");
    CHECK(record.at("licence").at("number").is(Json::Kind::null));
}

TEST_CASE("a source that cannot say leaves the licence null and the operation as it was",
          "[load][integration][record][licence]")
{
    CommandRig rig;
    rig.licence = gives(std::nullopt);
    const CommandRun run = information_with_record(rig);
    CHECK(run.exit_code == 0);
    CHECK(record_of(rig).at("licence").is(Json::Kind::null));
}

TEST_CASE("the licence source is asked once for an operation", "[load][integration][record][licence]")
{
    CommandRig rig;
    int asked = 0;
    rig.licence = gives(RecordedLicence{"community", "none", std::nullopt}, &asked);
    const CommandRun run = information_with_record(rig);
    CHECK(run.exit_code == 0);
    CHECK(asked == 1);
}

TEST_CASE("a licence changes nothing an operation does or says", "[load][integration][record][licence]")
{
    // The events and the exit code of the same operation are the same with a licence, with none, and with one that
    // is not in force: nothing is gated, and the events do not carry the licence.
    const auto events_with = [](adl::load::LicenceSource licence)
    {
        CommandRig rig;
        rig.licence = std::move(licence);
        const CommandRun run = information_with_record(rig);
        CHECK(run.exit_code == 0);
        return run;
    };
    const CommandRun none = events_with({});
    const CommandRun pro = events_with(gives(RecordedLicence{"pro", "valid", "ADL-2026-000123"}));
    const CommandRun ended = events_with(gives(RecordedLicence{"community", "expired", std::nullopt}));
    CHECK(pro.out == none.out);
    CHECK(ended.out == none.out);
    CHECK(pro.err == none.err);
    CHECK(none.out.find("licence") == std::string::npos);
    CHECK(none.out.find("ADL-2026-000123") == std::string::npos);
}

TEST_CASE("the log of the record has no line about the licence", "[load][integration][record][licence]")
{
    CommandRig rig;
    rig.licence = gives(RecordedLicence{"pro", "valid", "ADL-2026-000123"});
    const CommandRun run = information_with_record(rig);
    CHECK(run.exit_code == 0);
    // The record is kept in a variable: a range-for over a member of the temporary would read freed memory.
    const Json record = record_of(rig);
    for (const Json& line : record.at("log").elements)
    {
        CHECK(line.at("text").text.find("icence") == std::string::npos);
    }
}

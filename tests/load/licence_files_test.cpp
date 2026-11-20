// SPDX-License-Identifier: GPL-3.0-or-later
// The licence source that adl_load gives its records on a desktop (adl/load/licence_files.hpp): the licence files of
// the computer, through the host, judged against the keys it is given, and what the record of a whole command says
// of them. The licences are signed with the published test key of RFC 8032 (tests/licence/support.hpp).

#include "adl/licence/test_keys.hpp"
#include "adl/load/licence_files.hpp"
#include "command_rig.hpp"
#include "support.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using adl::test::CommandRig;
using adl::test::CommandRun;
using adl::test::Json;

namespace
{

// 2026-11-19T10:15:30Z: the time of the contract's own example. A licence for it is made below.
constexpr std::int64_t wall_origin = 1'795'083'330;

std::vector<adl::licence::signed_file::TrustedKey> test_keys()
{
    const auto trusted = adl::licence::test_keys::trusted();
    return {trusted.begin(), trusted.end()};
}

// A licence for a Pro edition that is in force on the day of wall_origin.
adl::licence::Licence in_force_on_the_day()
{
    adl::licence::Licence value = adl::test::annual();
    value.starts_at = adl::test::at(2026, 10, 1);
    value.expires_at = adl::test::at(2027, 10, 1);
    return value;
}

CommandRun information_with_record(CommandRig& rig)
{
    rig.rig.world.set_wall_origin(wall_origin);
    return rig.run(
        {"information", "--target", "10.0.0.2", "--position", "HNPFMS_L", "--json", "--record", "/rec.json"});
}

[[nodiscard]] Json licence_of_record(CommandRig& rig)
{
    const auto parsed = adl::test::parse_json(adl::test::fake_text(rig.rig.loader_host, "/rec.json"));
    REQUIRE(parsed.has_value());
    return parsed->at("licence");
}

} // namespace

TEST_CASE("a command's record says the licence of the licence files it is given, on the day it runs",
          "[load][integration][record][licence]")
{
    CommandRig rig;
    rig.rig.loader_host.fake_files().put("/settings/licences/pro.adllic",
                                         adl::test::signed_text(in_force_on_the_day()));
    rig.licence = adl::load::licence_from_files("/settings", test_keys());
    const CommandRun run = information_with_record(rig);
    CHECK(run.exit_code == 0);

    const Json licence = licence_of_record(rig);
    CHECK(licence.at("edition").text == "pro");
    CHECK(licence.at("state").text == "valid");
    CHECK(licence.at("number").text == "ADL-2026-000123");
}

TEST_CASE("a licence that has ended is the Community edition in the record, and the operation is the same",
          "[load][integration][record][licence]")
{
    adl::licence::Licence ended = in_force_on_the_day();
    ended.starts_at = adl::test::at(2025, 10, 1);
    ended.expires_at = adl::test::at(2026, 1, 1);
    ended.grace_days = 0;

    CommandRig with_licence;
    with_licence.rig.loader_host.fake_files().put("/settings/licences/old.adllic", adl::test::signed_text(ended));
    with_licence.licence = adl::load::licence_from_files("/settings", test_keys());
    const CommandRun ended_run = information_with_record(with_licence);

    CommandRig without;
    const CommandRun plain_run = information_with_record(without);

    CHECK(ended_run.exit_code == 0);
    CHECK(ended_run.out == plain_run.out);
    const Json licence = licence_of_record(with_licence);
    CHECK(licence.at("edition").text == "community");
    CHECK(licence.at("state").text == "expired");
    CHECK(licence.at("number").is(Json::Kind::null));
}

TEST_CASE("with no licence file the record says the Community edition and a state of none",
          "[load][integration][record][licence]")
{
    CommandRig rig;
    rig.licence = adl::load::licence_from_files("/settings", test_keys());
    const CommandRun run = information_with_record(rig);
    CHECK(run.exit_code == 0);
    const Json licence = licence_of_record(rig);
    CHECK(licence.at("edition").text == "community");
    CHECK(licence.at("state").text == "none");
    CHECK(licence.at("number").is(Json::Kind::null));
}

TEST_CASE("a release with no keys refuses every licence, and the record says the Community edition",
          "[load][integration][record][licence]")
{
    CommandRig rig;
    rig.rig.loader_host.fake_files().put("/settings/licences/pro.adllic",
                                         adl::test::signed_text(in_force_on_the_day()));
    rig.licence = adl::load::licence_from_files("/settings", {});
    const CommandRun run = information_with_record(rig);
    CHECK(run.exit_code == 0);
    const Json licence = licence_of_record(rig);
    CHECK(licence.at("edition").text == "community");
    CHECK(licence.at("state").text == "invalid");
    CHECK(licence.at("number").is(Json::Kind::null));
}

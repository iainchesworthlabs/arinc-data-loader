// SPDX-License-Identifier: GPL-3.0-or-later
// The stamp a program writes into a record of what it did: the edition in force, the state of the licence and its
// number (adl/licence_platform/stamp.hpp). Each state of a licence, the number of a licence that is not in force
// left out, that nothing is written, and the same read through a host's file system and clock. The licences are
// signed with the published test key of RFC 8032 (tests/licence/support.hpp) and nothing else.

#include "adl/licence/licence.hpp"
#include "adl/licence/store.hpp"
#include "adl/licence/test_keys.hpp"
#include "adl/licence_platform/files.hpp"
#include "adl/licence_platform/locations.hpp"
#include "adl/licence_platform/stamp.hpp"
#include "fake_host.hpp"
#include "fake_storage.hpp"
#include "support.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace adl::licence;
using namespace adl::test;
using adl::licence_platform::PlatformFiles;
using adl::licence_platform::Stamp;
using adl::licence_platform::stamp_for;
using adl::licence_platform::stamp_of;
namespace st = adl::licence::store;

namespace
{

std::vector<signed_file::TrustedKey> the_test_key()
{
    const auto trusted = test_keys::trusted();
    return {trusted.begin(), trusted.end()};
}

// One licence file in the user's folder of a Linux desktop, and the folders the store reads.
struct Machine
{
    adl::fake::MemoryFileSystem fs;
    PlatformFiles files{fs};
    st::Folders folders{"/user/licences", "/machine/licences", "/user/licence-clock.json"};

    void put(const Licence& value, std::string_view name = "a.adllic")
    {
        fs.put(std::string{"/user/licences/"} + std::string{name}, signed_text(value));
    }

    [[nodiscard]] Stamp stamp(TimePoint now, std::string_view computer = "LAB-RIG-07")
    {
        return stamp_for(files, folders, now, computer, the_test_key());
    }

    // The content of a file, or an empty text when there is none.
    [[nodiscard]] std::string text(std::string_view path) const
    {
        const auto bytes = fs.get(path);
        if (!bytes)
        {
            return {};
        }
        std::string out;
        for (const std::byte byte : *bytes)
        {
            out.push_back(static_cast<char>(byte));
        }
        return out;
    }
};

ClockState clock_at(TimePoint seen)
{
    ClockState state;
    state.latest_seen = seen;
    return state;
}

} // namespace

TEST_CASE("no licence file is the Community edition, with a state and no number",
          "[licence_platform][stamp][integration]")
{
    Machine machine;
    const Stamp stamp = machine.stamp(at(2027, 1, 10));
    CHECK(stamp.edition == "community");
    CHECK(stamp.state == "none");
    CHECK_FALSE(stamp.number.has_value());
}

TEST_CASE("a licence in force gives its edition and its number", "[licence_platform][stamp][integration]")
{
    Machine machine;
    machine.put(annual());
    const Stamp stamp = machine.stamp(at(2027, 1, 10));
    CHECK(stamp.edition == "pro");
    CHECK(stamp.state == "valid");
    REQUIRE(stamp.number.has_value());
    CHECK(*stamp.number == "ADL-2026-000123");
}

TEST_CASE("an Enterprise licence gives the Enterprise edition", "[licence_platform][stamp][integration]")
{
    Machine machine;
    machine.put(full());
    const Stamp stamp = machine.stamp(at(2027, 1, 10));
    CHECK(stamp.edition == "enterprise");
    CHECK(stamp.state == "valid");
    REQUIRE(stamp.number.has_value());
    CHECK(*stamp.number == full().licence_id);
}

TEST_CASE("a licence that ends soon and one in its grace period are still in force",
          "[licence_platform][stamp][integration]")
{
    Machine machine;
    machine.put(annual());

    const Stamp expiring = machine.stamp(at(2027, 11, 1));
    CHECK(expiring.edition == "pro");
    CHECK(expiring.state == "expiring");
    CHECK(expiring.number == std::optional<std::string>{"ADL-2026-000123"});

    const Stamp grace = machine.stamp(at(2027, 11, 20));
    CHECK(grace.edition == "pro");
    CHECK(grace.state == "grace");
    CHECK(grace.number == std::optional<std::string>{"ADL-2026-000123"});
}

TEST_CASE("a licence that is not in force is the Community edition, with its state and no number",
          "[licence_platform][stamp][integration]")
{
    Machine machine;
    machine.put(annual());

    const Stamp ended = machine.stamp(at(2027, 12, 20));
    CHECK(ended.edition == "community");
    CHECK(ended.state == "expired");
    CHECK_FALSE(ended.number.has_value());

    // A licence that starts later than the day it was issued, read before it starts.
    Machine later;
    Licence future = annual();
    future.issued_at = at(2027, 1, 1);
    future.starts_at = at(2027, 3, 1);
    future.expires_at = at(2028, 3, 1);
    later.put(future);
    const Stamp not_yet = later.stamp(at(2027, 1, 10));
    CHECK(not_yet.edition == "community");
    CHECK(not_yet.state == "not_yet_valid");
    CHECK_FALSE(not_yet.number.has_value());
}

TEST_CASE("a licence for other computers is the Community edition", "[licence_platform][stamp][integration]")
{
    Machine machine;
    machine.put(full()); // names LAB-RIG-* and bench-3
    const Stamp elsewhere = machine.stamp(at(2027, 1, 10), "OFFICE-PC");
    CHECK(elsewhere.edition == "community");
    CHECK(elsewhere.state == "invalid");
    CHECK_FALSE(elsewhere.number.has_value());
}

TEST_CASE("a build whose list has no keys accepts no licence, so the stamp is Community",
          "[licence_platform][stamp][integration]")
{
    Machine machine;
    machine.put(annual());
    const Stamp stamp = stamp_for(machine.files, machine.folders, at(2027, 1, 10), "LAB-RIG-07", {});
    CHECK(stamp.edition == "community");
    CHECK(stamp.state == "invalid");
    CHECK_FALSE(stamp.number.has_value());
}

TEST_CASE("a clock that reads before a time a program has seen is a suspect clock",
          "[licence_platform][stamp][integration]")
{
    Machine machine;
    machine.put(annual());
    REQUIRE(st::save_clock(machine.files, machine.folders, clock_at(at(2027, 6, 1))));
    const Stamp stamp = machine.stamp(at(2027, 1, 10));
    CHECK(stamp.edition == "community");
    CHECK(stamp.state == "clock_suspect");
    CHECK_FALSE(stamp.number.has_value());
}

TEST_CASE("reading the stamp writes nothing", "[licence_platform][stamp][integration]")
{
    Machine machine;
    machine.put(annual());
    const std::string before = machine.text("/user/licences/a.adllic");
    REQUIRE_FALSE(before.empty());
    REQUIRE_FALSE(machine.fs.get("/user/licence-clock.json").has_value());

    for (const TimePoint now : {at(2027, 1, 10), at(2027, 11, 20), at(2027, 12, 20)})
    {
        static_cast<void>(machine.stamp(now));
    }
    // Not even the latest time seen: the windows keep it, a record only reads it.
    CHECK_FALSE(machine.fs.get("/user/licence-clock.json").has_value());
    CHECK(machine.text("/user/licences/a.adllic") == before);

    // The same with a clock file that is there: it is read and left as it is.
    REQUIRE(st::save_clock(machine.files, machine.folders, clock_at(at(2027, 1, 1))));
    const std::string clock = machine.text("/user/licence-clock.json");
    REQUIRE_FALSE(clock.empty());
    static_cast<void>(machine.stamp(at(2027, 1, 10)));
    CHECK(machine.text("/user/licence-clock.json") == clock);
}

TEST_CASE("the licensee is no part of a stamp", "[licence_platform][stamp][integration]")
{
    Machine machine;
    Licence value = annual();
    value.licensee_name = "Example Lab Pty Ltd";
    value.licensee_email = "someone@example.com";
    machine.put(value);
    const Stamp stamp = machine.stamp(at(2027, 1, 10));
    CHECK(stamp.edition.find("Example") == std::string::npos);
    CHECK(stamp.state.find("Example") == std::string::npos);
    CHECK(stamp.number.value_or("").find("Example") == std::string::npos);
    CHECK(stamp.number.value_or("").find("example.com") == std::string::npos);
}

// ---- Through a host ---------------------------------------------------------------------------------

TEST_CASE("a stamp is read through the host's file system and clock, in the folders the settings directory moves",
          "[licence_platform][stamp][integration]")
{
    adl::fake::FakeSystem system;
    system.world().set_wall_origin(at(2027, 1, 10).time_since_epoch().count());
    system.host().fake_files().put("/settings/licences/a.adllic", signed_text(annual()));

    const auto stamp = stamp_of(system.host(), "/settings", the_test_key());
    REQUIRE(stamp.has_value());
    CHECK(stamp->edition == "pro");
    CHECK(stamp->state == "valid");
    CHECK(stamp->number == std::optional<std::string>{"ADL-2026-000123"});

    // Another settings directory has no licence in it, so it is the Community edition.
    const auto elsewhere = stamp_of(system.host(), "/other", the_test_key());
    REQUIRE(elsewhere.has_value());
    CHECK(elsewhere->edition == "community");
    CHECK(elsewhere->state == "none");
}

TEST_CASE("a host's clock that is past the end of a licence says it has ended",
          "[licence_platform][stamp][integration]")
{
    adl::fake::FakeSystem system;
    system.world().set_wall_origin(at(2028, 3, 1).time_since_epoch().count());
    system.host().fake_files().put("/settings/licences/a.adllic", signed_text(annual()));
    const auto stamp = stamp_of(system.host(), "/settings", the_test_key());
    REQUIRE(stamp.has_value());
    CHECK(stamp->edition == "community");
    CHECK(stamp->state == "expired");
    CHECK_FALSE(stamp->number.has_value());
}

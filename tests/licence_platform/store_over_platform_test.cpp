// SPDX-License-Identifier: GPL-3.0-or-later
// The licence store over the platform's FileSystem, on the fake file system: a licence imported, found,
// judged and removed in the folders the product keeps them in, a licence an administrator put in the machine
// folder, the clock file, and the folders that ADL_SETTINGS_DIR moves so that a test never reads a real one.

#include "adl/licence/licence.hpp"
#include "adl/licence/store.hpp"
#include "adl/licence_platform/files.hpp"
#include "adl/licence_platform/locations.hpp"
#include "fake_storage.hpp"
#include "support.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace adl::licence;
using namespace adl::test;
using adl::licence_platform::PlatformFiles;
using adl::licence_platform::Roots;
namespace st = adl::licence::store;

namespace
{

constexpr auto in_force = [] { return at(2027, 1, 10); };

// The roots of a Linux desktop, with or without ADL_SETTINGS_DIR.
Roots linux_roots(std::string settings = {})
{
    Roots roots;
    roots.settings_directory = std::move(settings);
    roots.user_data_directory = "/home/pat/.local/share";
    roots.machine_config_directory = "/etc";
    roots.host_name = "linux";
    return roots;
}

[[nodiscard]] std::size_t count_files(const adl::fake::MemoryFileSystem& fs, std::string_view path)
{
    return fs.get(path).has_value() ? 1U : 0U;
}

} // namespace

TEST_CASE("licence store over the platform: a licence is imported, found and removed",
          "[licence_platform][store][integration]")
{
    adl::fake::MemoryFileSystem fs;
    PlatformFiles files{fs};
    const auto folders = adl::licence_platform::folders_for(linux_roots());

    // Nothing there yet: the folders are not even made.
    REQUIRE(st::scan(files, folders, on(in_force())).best.state == State::none);
    REQUIRE_FALSE(fs.stat("/home/pat/.local/share/arinc-data-loader").has_value());

    const std::string text = signed_text(annual());
    const auto imported = st::import_file(files, folders, text, on(in_force()));
    REQUIRE(imported);
    REQUIRE(imported->name == "ADL-2026-000123.adllic");
    REQUIRE(count_files(fs, "/home/pat/.local/share/arinc-data-loader/licences/ADL-2026-000123.adllic") == 1);

    const auto overview = st::scan(files, folders, on(in_force()));
    REQUIRE(overview.best.state == State::valid);
    REQUIRE(overview.licences.size() == 1);
    REQUIRE(overview.licences[0].origin == st::Origin::user);

    // The same file again is the file that is there; a corrected one is a second file.
    REQUIRE(st::import_file(files, folders, text, on(in_force()))->already_there);
    Licence corrected = annual();
    corrected.seats = 5;
    REQUIRE(st::import_file(files, folders, signed_text(corrected), on(in_force()))->name ==
            "ADL-2026-000123-2.adllic");
    REQUIRE(st::scan(files, folders, on(in_force())).licences.size() == 2);

    REQUIRE(st::remove_file(files, folders, "ADL-2026-000123-2.adllic"));
    REQUIRE(st::scan(files, folders, on(in_force())).licences.size() == 1);
    REQUIRE(st::remove_file(files, folders, "ADL-2026-000123.adllic"));
    REQUIRE(st::scan(files, folders, on(in_force())).best.state == State::none);
}

TEST_CASE("licence store over the platform: a licence in the machine folder is read, and never written or removed",
          "[licence_platform][store][integration]")
{
    adl::fake::MemoryFileSystem fs;
    PlatformFiles files{fs};
    const auto folders = adl::licence_platform::folders_for(linux_roots());
    Licence site = annual();
    site.licence_id = "ADL-2026-000500";
    site.seats = 0;
    fs.put("/etc/arinc-data-loader/licences/site.adllic", signed_text(site));

    const auto overview = st::scan(files, folders, on(in_force()));
    REQUIRE(overview.best.state == State::valid);
    REQUIRE(overview.best.value->seats == 0);
    REQUIRE(overview.licences.size() == 1);
    REQUIRE(overview.licences[0].origin == st::Origin::machine);

    // Removal works on the user's folder only, whatever the name.
    const auto removed = st::remove_file(files, folders, "site.adllic");
    REQUIRE_FALSE(removed);
    REQUIRE(removed.error() == st::RemoveError::not_found);
    REQUIRE(count_files(fs, "/etc/arinc-data-loader/licences/site.adllic") == 1);

    // An import goes to the user's folder, and the better licence of the two wins.
    Licence enterprise = full();
    enterprise.machines.clear();
    REQUIRE(st::import_file(files, folders, signed_text(enterprise), on(in_force())));
    REQUIRE(st::scan(files, folders, on(in_force())).best.value->edition == Edition::enterprise);
    REQUIRE(count_files(fs, "/etc/arinc-data-loader/licences/ADL-2026-000124.adllic") == 0);
}

TEST_CASE("licence store over the platform: ADL_SETTINGS_DIR moves every folder and hides the real ones",
          "[licence_platform][store][integration]")
{
    adl::fake::MemoryFileSystem fs;
    PlatformFiles files{fs};
    // A real licence in the real folders, as a developer's computer might have.
    fs.put("/home/pat/.local/share/arinc-data-loader/licences/mine.adllic", signed_text(annual()));
    fs.put("/etc/arinc-data-loader/licences/site.adllic", signed_text(annual()));

    const auto real = adl::licence_platform::folders_for(linux_roots());
    REQUIRE(st::scan(files, real, on(in_force())).best.state == State::valid);

    const auto moved = adl::licence_platform::folders_for(linux_roots("/tmp/test-settings"));
    REQUIRE(st::scan(files, moved, on(in_force())).best.state == State::none);

    // What a test does with them goes below the settings folder, and nowhere else.
    Licence other = annual();
    other.licence_id = "ADL-2026-000777";
    const std::string text = signed_text(other);
    REQUIRE(st::import_file(files, moved, text, on(in_force())));
    REQUIRE(count_files(fs, "/tmp/test-settings/licences/ADL-2026-000777.adllic") == 1);

    ClockState clock;
    clock.latest_seen = in_force();
    REQUIRE(st::save_clock(files, moved, clock));
    REQUIRE(count_files(fs, "/tmp/test-settings/licence-clock.json") == 1);
    REQUIRE(count_files(fs, "/home/pat/.local/share/arinc-data-loader/licence-clock.json") == 0);

    fs.put("/tmp/test-settings/machine-licences/site.adllic", signed_text(annual()));
    REQUIRE(st::scan(files, moved, on(in_force())).licences.size() == 2);
    REQUIRE(st::scan(files, real, on(in_force())).licences.size() == 2);
}

TEST_CASE("licence store over the platform: the clock file is saved, replaced and loaded",
          "[licence_platform][store][integration]")
{
    adl::fake::MemoryFileSystem fs;
    PlatformFiles files{fs};
    const auto folders = adl::licence_platform::folders_for(linux_roots());
    REQUIRE(st::load_clock(files, folders) == ClockState{});

    ClockState clock = observe({}, at(2027, 1, 10));
    clock.applied_resets = {"reset-1"};
    REQUIRE(st::save_clock(files, folders, clock));
    REQUIRE(st::load_clock(files, folders) == clock);

    clock = observe(clock, at(2027, 1, 11));
    REQUIRE(st::save_clock(files, folders, clock));
    REQUIRE(st::load_clock(files, folders).latest_seen == at(2027, 1, 11));
    const auto names = files.list("/home/pat/.local/share/arinc-data-loader");
    REQUIRE(names);
    REQUIRE(*names == std::vector<std::string>{"licence-clock.json"});

    // A disk that fills leaves the old file.
    fs.fail_next_write(adl::platform::ErrorCode::no_space);
    REQUIRE_FALSE(st::save_clock(files, folders, observe(clock, at(2028, 1, 1))));
    REQUIRE(st::load_clock(files, folders).latest_seen == at(2027, 1, 11));
}

TEST_CASE("licence store over the platform: a system with no user folder reads what it can and imports nothing",
          "[licence_platform][store][integration]")
{
    adl::fake::MemoryFileSystem fs;
    PlatformFiles files{fs};
    Roots roots;
    roots.machine_config_directory = "/etc";
    roots.host_name = "linux";
    const auto folders = adl::licence_platform::folders_for(roots);
    fs.put("/etc/arinc-data-loader/licences/site.adllic", signed_text(annual()));

    REQUIRE(st::scan(files, folders, on(in_force())).best.state == State::valid);
    const auto imported = st::import_file(files, folders, signed_text(annual()), on(in_force()));
    REQUIRE_FALSE(imported);
    REQUIRE(imported.error().failure == st::ImportFailure::write_failed);
    REQUIRE(st::load_clock(files, folders) == ClockState{});
    REQUIRE_FALSE(st::save_clock(files, folders, ClockState{}));
}

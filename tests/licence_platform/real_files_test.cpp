// SPDX-License-Identifier: GPL-3.0-or-later
// The licence store over this computer's own file system, in a scratch folder: what the fake cannot show, such
// as a rename over a file that is there, a folder that has to be made, and a path with a space and letters
// outside ASCII in it.

#include "adl/licence/store.hpp"
#include "adl/licence_platform/files.hpp"
#include "adl/licence_platform/locations.hpp"
#include "adl/platform/host.hpp"
#include "adl/platform/paths.hpp"
#include "support.hpp"
#include "temp_dir.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

using namespace adl::licence;
using namespace adl::test;
namespace st = adl::licence::store;

namespace
{

// A path made from UTF-8 bytes, which is how the store and the platform write every name.
std::filesystem::path utf8_path(const std::string& utf8)
{
    return std::filesystem::path{std::u8string{reinterpret_cast<const char8_t*>(utf8.data()), utf8.size()}};
}

} // namespace

TEST_CASE("licence store on the real file system: import, scan, remove and the clock",
          "[licence_platform][store][integration][real]")
{
    auto host = adl::platform::make_host();
    REQUIRE(host);
    TempDir temp;
    adl::licence_platform::PlatformFiles files{(*host)->files()};

    // A folder name with a space and letters outside ASCII, as a user's folder may have, written as UTF-8 bytes so
    // that no setting of the compiler can change it.
    const std::string settings_name = "settings \xC3\xA9 \xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E";
    adl::licence_platform::Roots roots;
    roots.settings_directory = adl::platform::to_platform_path(temp.path()) + "/" + settings_name;
    roots.host_name = (*host)->info().name;
    const auto folders = adl::licence_platform::folders_for(roots);
    const auto now = at(2027, 1, 10);

    // Nothing there, and the folders are not made by looking.
    REQUIRE(st::scan(files, folders, on(now)).best.state == State::none);
    REQUIRE_FALSE(std::filesystem::exists(temp.path() / utf8_path(settings_name)));

    const std::string text = signed_text(annual());
    const auto imported = st::import_file(files, folders, text, on(now));
    REQUIRE(imported);
    REQUIRE(imported->name == "ADL-2026-000123.adllic");

    // The file on the disk is the file given, byte for byte.
    const auto on_disk = files.read(folders.user + "/" + imported->name, signed_file::max_file_size);
    REQUIRE(on_disk);
    REQUIRE(*on_disk == text);
    REQUIRE(st::scan(files, folders, on(now)).best.state == State::valid);

    // The same file again, and a corrected one: nothing is replaced.
    REQUIRE(st::import_file(files, folders, text, on(now))->already_there);
    Licence corrected = annual();
    corrected.seats = 9;
    const auto second = st::import_file(files, folders, signed_text(corrected), on(now));
    REQUIRE(second);
    REQUIRE(second->name == "ADL-2026-000123-2.adllic");
    REQUIRE(*files.read(folders.user + "/ADL-2026-000123.adllic", signed_file::max_file_size) == text);

    // The clock file is replaced in place twice, and no temporary file stays.
    ClockState clock = observe({}, now);
    REQUIRE(st::save_clock(files, folders, clock));
    clock = observe(clock, at(2027, 1, 11));
    REQUIRE(st::save_clock(files, folders, clock));
    REQUIRE(st::load_clock(files, folders) == clock);
    const auto settings = files.list(roots.settings_directory);
    REQUIRE(settings);
    REQUIRE(*settings == std::vector<std::string>{"licence-clock.json"});

    // Removal takes the one file, and a name that is not a licence file is refused.
    REQUIRE(st::remove_file(files, folders, "ADL-2026-000123-2.adllic"));
    REQUIRE(st::scan(files, folders, on(now)).licences.size() == 1);
    REQUIRE(st::remove_file(files, folders, "licence-clock.json").error() == st::RemoveError::not_allowed);
    REQUIRE(st::load_clock(files, folders) == clock);
    REQUIRE(st::remove_file(files, folders, "ADL-2026-000123.adllic"));
    REQUIRE(st::scan(files, folders, on(now)).best.state == State::none);
}

TEST_CASE("licence store on the real file system: files that are not the store's, and a folder that is a file",
          "[licence_platform][store][integration][real]")
{
    auto host = adl::platform::make_host();
    REQUIRE(host);
    TempDir temp;
    adl::licence_platform::PlatformFiles files{(*host)->files()};

    st::Folders folders;
    folders.user = adl::platform::to_platform_path(temp.path() / "licences");
    folders.machine = adl::platform::to_platform_path(temp.path() / "machine");
    folders.clock_file = adl::platform::to_platform_path(temp.path() / "licence-clock.json");
    const auto now = at(2027, 1, 10);

    REQUIRE(files.create(folders.user + "/notes.txt", "not a licence"));
    REQUIRE(files.create(folders.user + "/damaged.adllic", "not a licence"));
    REQUIRE(files.create(folders.machine + "/site.adllic", signed_text(annual())));
    std::filesystem::create_directory(temp.path() / "licences" / "sub.adllic");

    const auto overview = st::scan(files, folders, on(now));
    // A folder is not a file, whatever it is called.
    REQUIRE(overview.licences.size() == 2);
    REQUIRE(overview.licences[0].name == "damaged.adllic");
    REQUIRE(overview.licences[0].status.state == State::invalid);
    REQUIRE(overview.licences[1].origin == st::Origin::machine);
    REQUIRE(overview.best.state == State::valid);

    // A name in the path that is a file is not a folder.
    REQUIRE(files.list(folders.user + "/notes.txt").error() == st::FilesError::not_found);
    REQUIRE_FALSE(files.create(folders.user + "/notes.txt/a.adllic", "x"));
    REQUIRE(files.create(folders.user + "/notes.txt", "again").error() == st::FilesError::already_exists);
}

TEST_CASE("licence store on the real file system: names with letters outside the code page are not changed on the way",
          "[licence_platform][store][integration][real]")
{
    auto host = adl::platform::make_host();
    REQUIRE(host);
    TempDir temp;
    adl::licence_platform::PlatformFiles files{(*host)->files()};

    // UTF-8 bytes written out: e acute, a space, three Japanese characters (outside the Windows code pages of
    // the West), an emoji (outside the Basic Multilingual Plane) and u umlaut.
    const std::string folder_name = "\xC3\xA9 \xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E \xF0\x9F\x98\x80";
    const std::string file_name = "M\xC3\xBC"
                                  "ller \xE6\x97\xA5\xE6\x9C\xAC.adllic";
    const std::string base = adl::platform::to_platform_path(temp.path()) + "/" + folder_name;
    const auto path_of = [](const std::string& utf8) { return utf8_path(utf8); };

    st::Folders folders;
    folders.user = base + "/licences";
    folders.machine = base + "/machine";
    folders.clock_file = base + "/licence-clock.json";
    const auto now = at(2027, 1, 10);

    // A licence file with such a name, in a folder with such a name, is found, read and judged, and the name
    // that comes back is the name that was written.
    REQUIRE(files.create(folders.user + "/" + file_name, signed_text(annual())));
    REQUIRE(std::filesystem::exists(path_of(base) / "licences" / path_of(file_name)));
    const auto overview = st::scan(files, folders, on(now));
    REQUIRE(overview.licences.size() == 1);
    REQUIRE(overview.licences[0].name == file_name);
    REQUIRE(overview.best.state == State::valid);

    // An import and the clock file go into the same folder, and the file is removed by its name.
    REQUIRE(st::import_file(files, folders, signed_text(full()), on(now)));
    REQUIRE(std::filesystem::exists(path_of(base) / "licences" / "ADL-2026-000124.adllic"));
    ClockState clock = observe({}, now);
    REQUIRE(st::save_clock(files, folders, clock));
    REQUIRE(std::filesystem::exists(path_of(base) / "licence-clock.json"));
    REQUIRE(st::load_clock(files, folders) == clock);
    REQUIRE(st::remove_file(files, folders, file_name));
    REQUIRE_FALSE(std::filesystem::exists(path_of(base) / "licences" / path_of(file_name)));
    REQUIRE(st::scan(files, folders, on(now)).licences.size() == 1);
}

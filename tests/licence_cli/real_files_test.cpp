// SPDX-License-Identifier: GPL-3.0-or-later
// The commands of adl_licence over this computer's own file system, in a scratch folder: the folders of
// ADL_SETTINGS_DIR made where they are not, a licence imported from a folder and under a name with letters outside
// the code page in them (accented, Japanese), shown, checked and removed, and what is on the disk after each. The
// strings are written as UTF-8 bytes, so that no setting of the compiler can change them.

#include "adl/licence/licence.hpp"
#include "adl/licence/test_keys.hpp"
#include "adl/licence_cli/licence_cli.hpp"
#include "adl/licence_cli/system.hpp"
#include "adl/platform/arguments.hpp"
#include "adl/platform/paths.hpp"
#include "support.hpp"
#include "temp_dir.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace lic = adl::licence;
namespace cli = adl::licence_cli;
namespace fs = std::filesystem;
using adl::test::annual;
using adl::test::at;
using adl::test::signed_text;

namespace
{

// A path made from UTF-8 bytes, which is how the commands are given every name.
[[nodiscard]] fs::path utf8_path(const std::string& utf8)
{
    return adl::platform::path_from_utf8(utf8);
}

// A path as the text an argument or ADL_SETTINGS_DIR holds: UTF-8, in the system's own separators.
[[nodiscard]] std::string argument_of(const fs::path& path)
{
    return adl::platform::to_utf8(path.u8string());
}

void write_bytes(const fs::path& path, const std::string& bytes)
{
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    REQUIRE(out);
    out << bytes;
}

[[nodiscard]] std::string read_bytes(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

struct Ran
{
    int code = 0;
    std::string out;
    std::string err;
};

[[nodiscard]] bool contains(const std::string& text, const std::string& part)
{
    return text.find(part) != std::string::npos;
}

} // namespace

TEST_CASE("adl_licence over the real file system: import, show, verify and remove", "[integration][licence_cli][real]")
{
    adl::test::TempDir temp;
    // The folder of ADL_SETTINGS_DIR, with a space and letters outside the code page in its name, and not made yet.
    const std::string settings_name = "settings \xC3\xA9 \xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E";
    const fs::path settings = temp.path() / utf8_path(settings_name);
    const fs::path inbox = temp.path() / utf8_path("inbox \xC3\xA9");
    const fs::path licence_file = inbox / utf8_path("licence \xE6\x97\xA5\xE6\x9C\xAC.adllic");
    const std::string text = signed_text(annual());
    write_bytes(licence_file, text);

    const auto test_key = lic::test_keys::trusted();
    auto system = cli::SystemEnvironment::open(argument_of(settings),
                                               std::vector<lic::signed_file::TrustedKey>(test_key.begin(), test_key.end()),
                                               lic::TimePoint::min(), "test");
    REQUIRE(system);
    cli::Environment& environment = (*system)->environment();
    environment.computer_name = "LAB-RIG-07";
    environment.now = []() -> std::optional<lic::TimePoint> { return at(2026, 12, 1); };

    const auto run = [&](std::vector<std::string> arguments)
    {
        std::ostringstream out;
        std::ostringstream err;
        const int code = cli::run(arguments, environment, {out, err});
        return Ran{code, out.str(), err.str()};
    };

    // Looking makes nothing: not the settings folder, not a licence folder, not a clock file.
    const Ran first = run({"show"});
    CHECK(first.code == cli::exit_ok);
    CHECK(contains(first.out, "State: none\n"));
    CHECK_FALSE(fs::exists(settings));

    const Ran verified = run({"verify", argument_of(licence_file)});
    CHECK(verified.code == cli::exit_ok);
    CHECK(contains(verified.out, "A program would accept this licence file.\n"));
    CHECK_FALSE(fs::exists(settings));

    const Ran imported = run({"import", argument_of(licence_file)});
    CHECK(imported.code == cli::exit_ok);
    CHECK(imported.out ==
          "Imported licence ADL-2026-000123 as ADL-2026-000123.adllic.\nThis is now the Pro edition.\n");
    const fs::path copy = settings / "licences" / "ADL-2026-000123.adllic";
    REQUIRE(fs::exists(copy));
    CHECK(read_bytes(copy) == text);
    CHECK(read_bytes(licence_file) == text);

    const Ran shown = run({"show"});
    CHECK(shown.code == cli::exit_ok);
    CHECK(contains(shown.out, "Edition: pro\n"));
    CHECK(contains(shown.out, "  ADL-2026-000123.adllic (user, valid, in use)\n"));

    // The same file again changes nothing on the disk.
    const Ran again = run({"import", argument_of(licence_file)});
    CHECK(again.code == cli::exit_ok);
    CHECK(contains(again.out, "already in the licence folder"));
    std::size_t files = 0;
    for ([[maybe_unused]] const auto& entry : fs::directory_iterator(settings / "licences"))
    {
        ++files;
    }
    CHECK(files == 1);

    const Ran removed = run({"remove", "ADL-2026-000123.adllic"});
    CHECK(removed.code == cli::exit_ok);
    CHECK_FALSE(fs::exists(copy));
    CHECK(fs::exists(licence_file));
    CHECK(contains(run({"show"}).out, "State: none\n"));

    const Ran gone = run({"remove", "ADL-2026-000123.adllic"});
    CHECK(gone.code == cli::exit_failed);
    CHECK(contains(gone.err, "there is no file called ADL-2026-000123.adllic in your licence folder"));
}

TEST_CASE("adl_licence over the real file system: a name that is not a file, and a file that is too large",
          "[integration][licence_cli][real]")
{
    adl::test::TempDir temp;
    const fs::path settings = temp.path() / "settings";
    const auto test_key = lic::test_keys::trusted();
    auto system = cli::SystemEnvironment::open(argument_of(settings),
                                               std::vector<lic::signed_file::TrustedKey>(test_key.begin(), test_key.end()),
                                               lic::TimePoint::min(), "test");
    REQUIRE(system);
    cli::Environment& environment = (*system)->environment();
    environment.now = []() -> std::optional<lic::TimePoint> { return at(2026, 12, 1); };
    const auto run = [&](std::vector<std::string> arguments)
    {
        std::ostringstream out;
        std::ostringstream err;
        const int code = cli::run(arguments, environment, {out, err});
        return Ran{code, out.str(), err.str()};
    };

    // A folder is not a file.
    const Ran folder = run({"import", argument_of(temp.path())});
    CHECK(folder.code == cli::exit_failed);
    CHECK(contains(folder.err, "import: cannot read "));

    // A file that does not exist.
    const Ran missing = run({"import", argument_of(temp.path() / "nope.adllic")});
    CHECK(missing.code == cli::exit_failed);
    CHECK(contains(missing.err, "it does not exist"));

    // A file larger than a licence file can be is refused without reading it all.
    write_bytes(temp.path() / "big.adllic", std::string(64 * 1024 + 1, 'x'));
    const Ran big = run({"import", argument_of(temp.path() / "big.adllic")});
    CHECK(big.code == cli::exit_failed);
    CHECK(contains(big.err, "it is larger than 65536 bytes"));
    CHECK_FALSE(fs::exists(settings));
}

TEST_CASE("the licence folders of this computer are those of ADL_SETTINGS_DIR when it is given", "[integration][licence_cli][real]")
{
    adl::test::TempDir temp;
    auto system = cli::SystemEnvironment::open(argument_of(temp.path()), {}, lic::TimePoint::min(), "v");
    REQUIRE(system);
    const cli::Environment& environment = (*system)->environment();
    CHECK(environment.folders.user == adl::platform::to_platform_path(temp.path()) + "/licences");
    CHECK(environment.folders.machine == adl::platform::to_platform_path(temp.path()) + "/machine-licences");
    CHECK(environment.folders.clock_file == adl::platform::to_platform_path(temp.path()) + "/licence-clock.json");
    CHECK(environment.version == "v");
    REQUIRE(environment.now);
    const auto now = environment.now();
    REQUIRE(now);
    // A time after this code was written, and one that is not far ahead of it.
    CHECK(*now > at(2026, 1, 1));
    CHECK(*now < at(2200, 1, 1));
}

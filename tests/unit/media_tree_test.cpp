// Media sets on a disk: the members of a set written to a directory tree and read
// back, with the standard's backslash pathnames mapped to the platform's paths.

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "adl/arinc665/media_set.hpp"
#include "adl/media_tree/media_tree.hpp"
#include "adl/samples/scenarios.hpp"
#include "bytes.hpp"
#include "temp_dir.hpp"

using namespace adl::arinc665;
using namespace adl::media_tree;
using adl::test::bytes_of;
using adl::test::TempDir;

namespace fs = std::filesystem;

namespace {

// Members as read from a tree come ordered by pathname and name, so that is how
// the ones to compare with are put.
std::vector<MediaMember> sorted(std::vector<MediaMember> members) {
    std::sort(members.begin(), members.end(), [](const MediaMember& a, const MediaMember& b) {
        return a.sequence_number < b.sequence_number;
    });
    for (MediaMember& member : members) {
        std::sort(member.files.begin(), member.files.end(),
                  [](const MediaFile& a, const MediaFile& b) {
                      return std::tie(a.pathname, a.name) < std::tie(b.pathname, b.name);
                  });
    }
    return members;
}

bool same_members(const std::vector<MediaMember>& a, const std::vector<MediaMember>& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].sequence_number != b[i].sequence_number || a[i].files != b[i].files) {
            return false;
        }
    }
    return true;
}

MediaMember member_with(std::uint16_t sequence_number, std::vector<MediaFile> files) {
    return MediaMember{sequence_number, std::move(files)};
}

MediaFile file_at(std::string pathname, std::string name, std::string_view content = "x") {
    return MediaFile{std::move(pathname), std::move(name), bytes_of(content)};
}

std::vector<fs::path> entries(const fs::path& directory) {
    std::vector<fs::path> paths;
    for (const auto& entry : fs::directory_iterator(directory)) {
        paths.push_back(entry.path().filename());
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

void put_file(const fs::path& path, std::string_view content) {
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << content;
}

}  // namespace

TEST_CASE("every sample media set survives a trip through a directory tree", "[media-tree]") {
    for (const adl::samples::Scenario& scenario : adl::samples::scenarios()) {
        CAPTURE(scenario.name);
        TempDir dir;
        const auto spec = scenario.make();
        REQUIRE(spec.has_value());
        const auto members = build_media_set(*spec);
        REQUIRE(members.has_value());

        REQUIRE(write_media_set_tree(dir.path(), *members).has_value());
        const auto read = read_media_set_tree(dir.path());

        REQUIRE(read.has_value());
        CHECK(same_members(*read, sorted(*members)));

        // And what was read is a media set that verifies, as the one in memory does.
        const auto from_memory = read_media_set(*members);
        const auto from_disk = read_media_set(*read);
        REQUIRE(from_memory.has_value());
        REQUIRE(from_disk.has_value());
        CHECK(from_disk->loads.size() == from_memory->loads.size());
        CHECK(from_disk->batches.size() == from_memory->batches.size());
        CHECK(from_disk->findings == from_memory->findings);
    }
}

TEST_CASE("each member is a directory and each pathname a path below it", "[media-tree]") {
    TempDir dir;
    const auto members = build_media_set(adl::samples::sample_media_set_spec().value()).value();
    REQUIRE(write_media_set_tree(dir.path(), members).has_value());

    CHECK(entries(dir.path()) == std::vector<fs::path>{"member_001", "member_002"});
    // The list files are in the root of every member.
    for (const char* member : {"member_001", "member_002"}) {
        CHECK(fs::is_regular_file(dir.path() / member / "LOADS.LUM"));
        CHECK(fs::is_regular_file(dir.path() / member / "FILES.LUM"));
    }
    // The sample puts a data file on the second member, in disk001 below its part root.
    bool found_in_subdirectory = false;
    for (const MediaFile& file : members[1].files) {
        const auto directory = native_directory(dir.path() / "member_002", file.pathname);
        REQUIRE(directory.has_value());
        const fs::path on_disk = *directory / file.name;
        CHECK(fs::is_regular_file(on_disk));
        CHECK(static_cast<std::uintmax_t>(fs::file_size(on_disk)) == file.bytes.size());
        found_in_subdirectory =
            found_in_subdirectory || file.pathname.find("disk001") != std::string::npos;
    }
    CHECK(found_in_subdirectory);
}

TEST_CASE("a pathname of the standard becomes a path of this platform", "[media-tree]") {
    const fs::path base = fs::path{"some"} / "base";

    CHECK(native_directory(base, "\\").value() == base);
    CHECK(native_directory(base, "\\disk001\\").value() == base / "disk001");
    CHECK(native_directory(base, "\\a\\b\\c\\").value() == base / "a" / "b" / "c");
    CHECK(native_directory(base, "\\ACM47123\\disk 1\\").value() == base / "ACM47123" / "disk 1");

    for (const char* bad : {"", "a\\", "\\a", "\\\\", "\\a\\\\b\\", "\\..\\", "\\.\\", "\\a\\..\\",
                            "\\a/b\\", "/a/", "\\C:\\", "\\a\\b", "\\NUL\\", "\\a.\\"}) {
        CAPTURE(bad);
        const auto result = native_directory(base, bad);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error().error == TreeError::invalid_pathname);
    }
}

TEST_CASE("member directories are named by their sequence number", "[media-tree]") {
    CHECK(member_directory_name(1) == "member_001");
    CHECK(member_directory_name(42) == "member_042");
    CHECK(member_directory_name(255) == "member_255");
}

TEST_CASE("empty files and nested directories round trip", "[media-tree]") {
    TempDir dir;
    const std::vector<MediaMember> members = {
        member_with(1,
                    {file_at("\\", "EMPTY.BIN", ""), file_at("\\a\\b\\c\\d\\", "DEEP.BIN", "deep"),
                     file_at("\\a\\", "A.BIN"), file_at("\\", "Z.BIN")}),
        member_with(3, {file_at("\\", "THIRD.BIN")}),
    };

    REQUIRE(write_media_set_tree(dir.path(), members).has_value());
    const auto read = read_media_set_tree(dir.path());

    REQUIRE(read.has_value());
    CHECK(same_members(*read, sorted(members)));
    CHECK(read->at(1).sequence_number == 3);  // the member numbers are kept, gaps and all
}

TEST_CASE("binary content is kept byte for byte", "[media-tree]") {
    TempDir dir;
    const auto content = adl::test::ascending();  // includes 0x0A, 0x0D and 0x1A
    std::vector<MediaMember> members = {member_with(1, {MediaFile{"\\", "ALL.BIN", content}})};

    REQUIRE(write_media_set_tree(dir.path(), members).has_value());

    CHECK(read_media_set_tree(dir.path()).value()[0].files[0].bytes == content);
}

TEST_CASE("a file that cannot be written leaves nothing behind", "[media-tree]") {
    TempDir dir;
    const fs::path root = dir.path() / "tree";
    const std::vector<MediaMember> members = {
        member_with(1, {file_at("\\", "GOOD.BIN")}),
        member_with(2, {file_at("\\", "BAD:NAME")}),
    };

    const auto written = write_media_set_tree(root, members);

    REQUIRE_FALSE(written.has_value());
    CHECK(written.error().error == TreeError::invalid_file_name);
    CHECK_FALSE(fs::exists(root));
}

TEST_CASE("names that are not usable on every platform are refused", "[media-tree]") {
    const auto error_for = [](const MediaFile& file) {
        TempDir dir;
        const auto written =
            write_media_set_tree(dir.path(), std::vector<MediaMember>{member_with(1, {file})});
        return written ? std::optional<TreeError>{}
                       : std::optional<TreeError>{written.error().error};
    };

    for (const char* bad : {"",
                            ".",
                            "..",
                            "A/B",
                            "A\\B",
                            "A:B",
                            "A*B",
                            "A?B",
                            "A\"B",
                            "A<B",
                            "A>B",
                            "A|B",
                            "TRAILING.",
                            "TRAILING ",
                            "CON",
                            "con",
                            "NUL.TXT",
                            "Aux.bin",
                            "PRN",
                            "COM1",
                            "lpt9.dat",
                            "TAB\tNAME",
                            "NON\xC3\xA9"
                            "ASCII"}) {
        CAPTURE(bad);
        CHECK(error_for(file_at("\\", bad)) == TreeError::invalid_file_name);
    }
    for (const char* good : {"CONFIG.LDR", "NON_LOAD.CRC", "ACM12345.001", "A B.BIN", "COM10",
                             "CONSOLE.TXT", "LPT0", "COM.TXT", ".hidden", "a.b.c", "x~y"}) {
        CAPTURE(good);
        CHECK_FALSE(error_for(file_at("\\", good)).has_value());
    }
    CHECK(error_for(file_at("no-backslashes", "A.BIN")) == TreeError::invalid_pathname);
    CHECK(error_for(file_at("\\CON\\", "A.BIN")) == TreeError::invalid_pathname);
}

TEST_CASE("sequence numbers have to be 1 to 255 and different", "[media-tree]") {
    const auto error_for = [](std::vector<MediaMember> members) {
        TempDir dir;
        const auto written = write_media_set_tree(dir.path(), members);
        return written ? std::optional<TreeError>{}
                       : std::optional<TreeError>{written.error().error};
    };

    CHECK(error_for({}) == TreeError::no_members);
    CHECK(error_for({member_with(0, {})}) == TreeError::invalid_member);
    CHECK(error_for({member_with(256, {})}) == TreeError::invalid_member);
    CHECK(error_for({member_with(2, {}), member_with(2, {})}) == TreeError::invalid_member);
    CHECK_FALSE(error_for({member_with(255, {})}).has_value());
}

TEST_CASE("two files of one name in one directory are refused, however they are cased",
          "[media-tree]") {
    TempDir dir;
    const std::vector<MediaMember> members = {
        member_with(1, {file_at("\\", "DATA.BIN"), file_at("\\", "data.bin")})};

    const auto written = write_media_set_tree(dir.path() / "tree", members);

    REQUIRE_FALSE(written.has_value());
    CHECK(written.error().error == TreeError::duplicate_file);
    CHECK_FALSE(fs::exists(dir.path() / "tree"));
}

TEST_CASE("the same name in different directories is two files", "[media-tree]") {
    TempDir dir;
    const std::vector<MediaMember> members = {member_with(
        1, {file_at("\\disk001\\", "LOAD.BIN", "one"), file_at("\\disk002\\", "LOAD.BIN", "two")})};

    REQUIRE(write_media_set_tree(dir.path(), members).has_value());

    CHECK(same_members(read_media_set_tree(dir.path()).value(), sorted(members)));
}

TEST_CASE("a tree is never written over", "[media-tree]") {
    TempDir dir;
    const std::vector<MediaMember> members = {member_with(1, {file_at("\\", "NEW.BIN", "new")})};

    SECTION("a member directory with files in it") {
        put_file(dir.path() / "member_001" / "OLD.BIN", "old");

        const auto written = write_media_set_tree(dir.path(), members);

        REQUIRE_FALSE(written.has_value());
        CHECK(written.error().error == TreeError::already_exists);
        CHECK(written.error().path == dir.path() / "member_001");
        CHECK(entries(dir.path() / "member_001") == std::vector<fs::path>{"OLD.BIN"});
    }
    SECTION("the same tree twice") {
        REQUIRE(write_media_set_tree(dir.path(), members).has_value());

        const auto again = write_media_set_tree(dir.path(), members);

        REQUIRE_FALSE(again.has_value());
        CHECK(again.error().error == TreeError::already_exists);
    }
    SECTION("a file where a member directory goes") {
        put_file(dir.path() / "member_001", "a file");

        const auto written = write_media_set_tree(dir.path(), members);

        REQUIRE_FALSE(written.has_value());
        CHECK(written.error().error == TreeError::already_exists);
    }
    SECTION("an empty member directory is fine, and so are other members' directories") {
        fs::create_directories(dir.path() / "member_001");
        put_file(dir.path() / "member_009" / "OTHER.BIN", "other");

        REQUIRE(write_media_set_tree(dir.path(), members).has_value());

        CHECK(fs::is_regular_file(dir.path() / "member_001" / "NEW.BIN"));
        CHECK(fs::is_regular_file(dir.path() / "member_009" / "OTHER.BIN"));
    }
}

TEST_CASE("the root of a tree to write to is created, however deep", "[media-tree]") {
    TempDir dir;
    const fs::path root = dir.path() / "a" / "b" / "tree";

    REQUIRE(write_media_set_tree(root,
                                 std::vector<MediaMember>{member_with(1, {file_at("\\", "A.BIN")})})
                .has_value());

    CHECK(fs::is_regular_file(root / "member_001" / "A.BIN"));
}

TEST_CASE("a root that is a file cannot be written to", "[media-tree]") {
    TempDir dir;
    put_file(dir.path() / "file", "not a directory");

    const auto written = write_media_set_tree(
        dir.path() / "file", std::vector<MediaMember>{member_with(1, {file_at("\\", "A.BIN")})});

    REQUIRE_FALSE(written.has_value());
    CHECK(written.error().error == TreeError::cannot_create_directory);
}

TEST_CASE("reading a tree that is not a tree of members", "[media-tree]") {
    TempDir dir;

    SECTION("a root that is not there") {
        const auto read = read_media_set_tree(dir.path() / "missing");
        REQUIRE_FALSE(read.has_value());
        CHECK(read.error().error == TreeError::not_a_directory);
    }
    SECTION("a root that is a file") {
        put_file(dir.path() / "file", "x");
        const auto read = read_media_set_tree(dir.path() / "file");
        REQUIRE_FALSE(read.has_value());
        CHECK(read.error().error == TreeError::not_a_directory);
    }
    SECTION("an empty root") {
        const auto read = read_media_set_tree(dir.path());
        REQUIRE_FALSE(read.has_value());
        CHECK(read.error().error == TreeError::no_members);
    }
    SECTION("a file beside the members") {
        put_file(dir.path() / "member_001" / "A.BIN", "a");
        put_file(dir.path() / "README.TXT", "stray");
        const auto read = read_media_set_tree(dir.path());
        REQUIRE_FALSE(read.has_value());
        CHECK(read.error().error == TreeError::unexpected_entry);
        CHECK(read.error().path.filename() == "README.TXT");
    }
    SECTION("directories that are not named for a member") {
        for (const char* name : {"member_000", "member_256", "member_1", "member_0001",
                                 "Member_001", "other", "member_00a"}) {
            CAPTURE(name);
            TempDir each;
            put_file(each.path() / name / "A.BIN", "a");
            const auto read = read_media_set_tree(each.path());
            REQUIRE_FALSE(read.has_value());
            CHECK(read.error().error == TreeError::unexpected_entry);
        }
    }
    SECTION("a name that cannot be a file on every platform, below a member") {
        put_file(dir.path() / "member_001" / "BAD*NAME", "a");
        const auto read = read_media_set_tree(dir.path());
        // Where the platform lets such a file exist at all (Windows does not).
        if (fs::exists(dir.path() / "member_001" / "BAD*NAME")) {
            REQUIRE_FALSE(read.has_value());
            CHECK(read.error().error == TreeError::invalid_file_name);
        }
    }
}

TEST_CASE("directories without files leave nothing in a member", "[media-tree]") {
    TempDir dir;
    put_file(dir.path() / "member_001" / "A.BIN", "a");
    fs::create_directories(dir.path() / "member_001" / "empty" / "deeper");
    fs::create_directories(dir.path() / "member_002");

    const auto read = read_media_set_tree(dir.path());

    REQUIRE(read.has_value());
    REQUIRE(read->size() == 2);
    CHECK((*read)[0].files == std::vector<MediaFile>{file_at("\\", "A.BIN", "a")});
    CHECK((*read)[1].files.empty());
}

TEST_CASE("a link in a tree is refused, not followed", "[media-tree]") {
    TempDir dir;
    TempDir elsewhere;
    put_file(elsewhere.path() / "SECRET.BIN", "outside the tree");
    fs::create_directories(dir.path() / "member_001");
    std::error_code ec;
    fs::create_directory_symlink(elsewhere.path(), dir.path() / "member_001" / "link", ec);
    if (ec) {
        SKIP("this account cannot create symbolic links: " << ec.message());
    }

    const auto read = read_media_set_tree(dir.path());

    REQUIRE_FALSE(read.has_value());
    CHECK(read.error().error == TreeError::unexpected_entry);
}

TEST_CASE("damage done to a file on the disk is found when the set is read", "[media-tree]") {
    TempDir dir;
    const auto members = build_media_set(adl::samples::sample_media_set_spec().value()).value();
    REQUIRE(write_media_set_tree(dir.path(), members).has_value());
    REQUIRE(read_media_set(read_media_set_tree(dir.path()).value()).has_value());

    // One byte of LOADS.LUM changed on the first member.
    const fs::path loads_list = dir.path() / "member_001" / "LOADS.LUM";
    {
        std::fstream file(loads_list, std::ios::binary | std::ios::in | std::ios::out);
        REQUIRE(file.is_open());
        file.seekp(10);
        file.put('\x5A');
    }

    const auto read = read_media_set_tree(dir.path());
    REQUIRE(read.has_value());
    CHECK_FALSE(read_media_set(*read).has_value());
}

TEST_CASE("every error has a name", "[media-tree]") {
    for (const TreeError error :
         {TreeError::no_members, TreeError::invalid_member, TreeError::invalid_pathname,
          TreeError::invalid_file_name, TreeError::duplicate_file, TreeError::already_exists,
          TreeError::cannot_create_directory, TreeError::cannot_write_file,
          TreeError::cannot_read_file, TreeError::not_a_directory, TreeError::unexpected_entry}) {
        CHECK_FALSE(to_string(error).empty());
        CHECK(to_string(error) != "unknown error");
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
// What the mock target does with folders: what it keeps of an upload, and what it holds for a
// download. Plain C++ over a temporary directory, no network.

#include "adl/mock/storage.hpp"
#include "temp_dir.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

using namespace adl::mock;

namespace
{

[[nodiscard]] std::vector<std::byte> bytes_of(std::string_view text)
{
    std::vector<std::byte> bytes;
    for (const char c : text)
    {
        bytes.push_back(std::byte{static_cast<unsigned char>(c)});
    }
    return bytes;
}

void write_text(const std::filesystem::path& path, std::string_view text)
{
    std::ofstream out(path, std::ios::binary);
    out << text;
}

[[nodiscard]] std::string read_text(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

[[nodiscard]] UploadRecord record(std::string part_number, std::vector<StoredFile> files)
{
    UploadRecord upload;
    upload.load_part_number = std::move(part_number);
    upload.files = std::move(files);
    return upload;
}

} // namespace

TEST_CASE("an upload is stored under its part number, each file byte for byte", "[mock][storage]")
{
    adl::test::TempDir directory;
    const auto outcome = store_upload(directory.path() / "kept", // made on the way
                                      record("ACM47-1234-5678", {StoredFile{"LOAD.LUH", bytes_of("header")},
                                                                 StoredFile{"DATA1.BIN", bytes_of("0123456789")}}));
    CHECK(outcome.issues.empty());
    REQUIRE(outcome.stored.size() == 2);
    CHECK(outcome.stored[0].name == "LOAD.LUH");
    CHECK(outcome.stored[0].size == 6);
    CHECK(outcome.stored[1].name == "DATA1.BIN");
    CHECK(outcome.stored[1].size == 10);
    const std::filesystem::path load_folder = directory.path() / "kept" / "ACM47-1234-5678";
    CHECK(outcome.stored[0].path == load_folder / "LOAD.LUH");
    CHECK(read_text(outcome.stored[0].path) == "header");
    CHECK(read_text(outcome.stored[1].path) == "0123456789");
}

TEST_CASE("a second upload of a file replaces the first", "[mock][storage]")
{
    adl::test::TempDir directory;
    static_cast<void>(
        store_upload(directory.path(), record("P1", {StoredFile{"A.BIN", bytes_of("long first content")}})));
    const auto outcome = store_upload(directory.path(), record("P1", {StoredFile{"A.BIN", bytes_of("second")}}));
    REQUIRE(outcome.stored.size() == 1);
    CHECK(read_text(outcome.stored[0].path) == "second");
}

TEST_CASE("with no storage folder nothing is stored and the operator is told", "[mock][storage]")
{
    const auto outcome = store_upload({}, record("P1", {StoredFile{"A.BIN", bytes_of("x")}}));
    CHECK(outcome.stored.empty());
    REQUIRE(outcome.issues.size() == 1);
    CHECK(outcome.issues[0].problem == StoreProblem::no_storage_folder);
}

TEST_CASE("a file with an unsafe name is skipped and the others are still stored", "[mock][storage]")
{
    adl::test::TempDir directory;
    const auto outcome =
        store_upload(directory.path(), record("P1", {StoredFile{"..", bytes_of("x")}, StoredFile{"a/b", bytes_of("x")},
                                                     StoredFile{"GOOD.BIN", bytes_of("ok")}}));
    REQUIRE(outcome.stored.size() == 1);
    CHECK(outcome.stored[0].name == "GOOD.BIN");
    REQUIRE(outcome.issues.size() == 2);
    CHECK(outcome.issues[0].problem == StoreProblem::unsafe_file_name);
    CHECK(outcome.issues[0].subject == "..");
    CHECK(outcome.issues[1].subject == "a/b");
    // Nothing was written outside the load's folder.
    CHECK(std::filesystem::exists(directory.path() / "P1" / "GOOD.BIN"));
    CHECK_FALSE(std::filesystem::exists(directory.path() / "a"));
}

TEST_CASE("a part number that is not a safe name is stored under one that is", "[mock][storage]")
{
    adl::test::TempDir directory;
    const auto outcome = store_upload(directory.path(), record("a b/c:d", {StoredFile{"A.BIN", bytes_of("x")}}));
    REQUIRE(outcome.stored.size() == 1);
    CHECK(outcome.stored[0].path == directory.path() / "a_b_c_d" / "A.BIN");
}

TEST_CASE("a storage folder that cannot be made is reported with the system's words", "[mock][storage]")
{
    adl::test::TempDir directory;
    write_text(directory.path() / "occupied", "a file where the folder should be");
    const auto outcome =
        store_upload(directory.path() / "occupied", record("P1", {StoredFile{"A.BIN", bytes_of("x")}}));
    CHECK(outcome.stored.empty());
    REQUIRE(outcome.issues.size() == 1);
    CHECK(outcome.issues[0].problem == StoreProblem::storage_folder_failed);
    CHECK_FALSE(outcome.issues[0].detail.empty());
}

TEST_CASE("a load folder that cannot be made is reported", "[mock][storage]")
{
    adl::test::TempDir directory;
    write_text(directory.path() / "P1", "a file where the load's folder should be");
    const auto outcome = store_upload(directory.path(), record("P1", {StoredFile{"A.BIN", bytes_of("x")}}));
    CHECK(outcome.stored.empty());
    REQUIRE(outcome.issues.size() == 1);
    CHECK(outcome.issues[0].problem == StoreProblem::load_folder_failed);
    CHECK_FALSE(outcome.issues[0].detail.empty());
}

TEST_CASE("a file that cannot be written is reported and the others are stored", "[mock][storage]")
{
    adl::test::TempDir directory;
    std::filesystem::create_directories(directory.path() / "P1" / "BLOCKED.BIN"); // a folder of that name
    const auto outcome = store_upload(
        directory.path(), record("P1", {StoredFile{"BLOCKED.BIN", bytes_of("x")}, StoredFile{"B.BIN", bytes_of("y")}}));
    REQUIRE(outcome.stored.size() == 1);
    CHECK(outcome.stored[0].name == "B.BIN");
    REQUIRE(outcome.issues.size() == 1);
    CHECK(outcome.issues[0].problem == StoreProblem::file_write_failed);
    CHECK(outcome.issues[0].subject == "BLOCKED.BIN");
}

TEST_CASE("the files a target holds are the regular files of its folder, by name", "[mock][storage]")
{
    adl::test::TempDir directory;
    write_text(directory.path() / "B.BIN", "second");
    write_text(directory.path() / "A.BIN", "first");
    write_text(directory.path() / "empty.dat", "");
    std::filesystem::create_directories(directory.path() / "a folder"); // not a file, not held
    const auto held = read_held_files(directory.path());
    REQUIRE(held);
    REQUIRE(held->size() == 3);
    CHECK((*held)[0].name == "A.BIN");
    CHECK((*held)[0].content == bytes_of("first"));
    CHECK((*held)[1].name == "B.BIN");
    CHECK((*held)[1].content == bytes_of("second"));
    CHECK((*held)[2].name == "empty.dat");
    CHECK((*held)[2].content.empty());
}

TEST_CASE("a folder with nothing in it holds nothing", "[mock][storage]")
{
    adl::test::TempDir directory;
    const auto held = read_held_files(directory.path());
    REQUIRE(held);
    CHECK(held->empty());
}

TEST_CASE("a folder that is not there, or is a file, cannot be read for held files", "[mock][storage]")
{
    adl::test::TempDir directory;
    const auto missing = read_held_files(directory.path() / "missing");
    REQUIRE_FALSE(missing);
    CHECK_FALSE(missing.error().empty());

    write_text(directory.path() / "plain", "x");
    const auto file = read_held_files(directory.path() / "plain");
    REQUIRE_FALSE(file);
    CHECK_FALSE(file.error().empty());
}

TEST_CASE("the demonstration file says where the held files are kept", "[mock][storage]")
{
    const HeldFile file = demonstration_file();
    CHECK(file.name == "DEMO.TXT");
    CHECK_FALSE(file.content.empty());
    CHECK_FALSE(file.description.empty());
    const std::string text(reinterpret_cast<const char*>(file.content.data()), file.content.size());
    CHECK(text.contains("<storage>/holds"));
}

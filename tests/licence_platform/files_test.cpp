// SPDX-License-Identifier: GPL-3.0-or-later
// The store's file interface over the platform's FileSystem, on the fake file system: what each call makes,
// what it refuses, and what it leaves behind when the disk fails.

#include "adl/licence_platform/files.hpp"
#include "fake_storage.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>

using adl::licence::store::FilesError;
using adl::licence_platform::PlatformFiles;

namespace
{

[[nodiscard]] std::string text_of(const std::optional<std::vector<std::byte>>& bytes)
{
    REQUIRE(bytes.has_value());
    std::string text;
    for (const std::byte b : *bytes)
    {
        text.push_back(static_cast<char>(std::to_integer<unsigned char>(b)));
    }
    return text;
}

} // namespace

TEST_CASE("platform files: a file is created with the folders above it, and read back",
          "[licence_platform][files][integration]")
{
    adl::fake::MemoryFileSystem fs;
    PlatformFiles files{fs};

    REQUIRE(files.create("/home/pat/.local/share/adl/licences/a.adllic", "hello"));
    REQUIRE(text_of(fs.get("/home/pat/.local/share/adl/licences/a.adllic")) == "hello");
    const auto folder = fs.stat("/home/pat/.local/share/adl/licences");
    REQUIRE(folder);
    REQUIRE(folder->kind == adl::platform::FileKind::directory);

    const auto read = files.read("/home/pat/.local/share/adl/licences/a.adllic", 100);
    REQUIRE(read);
    REQUIRE(*read == "hello");

    // A second file in the same folder needs no new folder.
    REQUIRE(files.create("/home/pat/.local/share/adl/licences/b.adllic", ""));
    REQUIRE(files.read("/home/pat/.local/share/adl/licences/b.adllic", 100).value().empty());
}

TEST_CASE("platform files: create never replaces a file", "[licence_platform][files][integration]")
{
    adl::fake::MemoryFileSystem fs;
    PlatformFiles files{fs};
    REQUIRE(files.create("/d/a.adllic", "first"));

    const auto again = files.create("/d/a.adllic", "second");
    REQUIRE_FALSE(again);
    REQUIRE(again.error() == FilesError::already_exists);
    REQUIRE(text_of(fs.get("/d/a.adllic")) == "first");
}

TEST_CASE("platform files: a file that was not written whole is not left behind",
          "[licence_platform][files][integration]")
{
    adl::fake::MemoryFileSystem fs;
    PlatformFiles files{fs};

    fs.fail_next_write(adl::platform::ErrorCode::no_space);
    const auto result = files.create("/d/a.adllic", "content");
    REQUIRE_FALSE(result);
    REQUIRE(result.error() == FilesError::failed);
    REQUIRE_FALSE(fs.get("/d/a.adllic").has_value());

    // The next try works, and the name is free.
    REQUIRE(files.create("/d/a.adllic", "content"));

    // A disk with room for less than the file.
    adl::fake::MemoryFileSystem small;
    small.set_capacity(4);
    PlatformFiles in_small{small};
    REQUIRE_FALSE(in_small.create("/d/a.adllic", "content"));
    REQUIRE_FALSE(small.get("/d/a.adllic").has_value());
}

TEST_CASE("platform files: a path below a file is refused", "[licence_platform][files][integration]")
{
    adl::fake::MemoryFileSystem fs;
    PlatformFiles files{fs};
    fs.put("/d/file", "x");
    REQUIRE_FALSE(files.create("/d/file/a.adllic", "x"));
    REQUIRE_FALSE(files.replace("/d/file/a.adllic", "x"));
    REQUIRE(files.list("/d/file").error() == FilesError::not_found);
}

TEST_CASE("platform files: read gives the whole file, and refuses one that is too large or not there",
          "[licence_platform][files][integration]")
{
    adl::fake::MemoryFileSystem fs;
    PlatformFiles files{fs};
    fs.put("/d/a", std::string(1000, 'x'));

    REQUIRE(files.read("/d/a", 1000).value() == std::string(1000, 'x'));
    const auto large = files.read("/d/a", 999);
    REQUIRE_FALSE(large);
    REQUIRE(large.error() == FilesError::too_large);

    const auto missing = files.read("/d/b", 100);
    REQUIRE_FALSE(missing);
    REQUIRE(missing.error() == FilesError::not_found);
    // A folder is not a file.
    REQUIRE_FALSE(files.read("/d", 100));
}

TEST_CASE("platform files: list gives the regular files of a folder", "[licence_platform][files][integration]")
{
    adl::fake::MemoryFileSystem fs;
    PlatformFiles files{fs};
    fs.put("/d/a.adllic", "x");
    fs.put("/d/b.txt", "x");
    fs.put("/d/sub/c.adllic", "x");

    auto names = files.list("/d");
    REQUIRE(names);
    std::ranges::sort(*names);
    // The sub-folder is not a file, and what is in it is not in this folder.
    REQUIRE(*names == std::vector<std::string>{"a.adllic", "b.txt"});

    const auto missing = files.list("/nowhere");
    REQUIRE_FALSE(missing);
    REQUIRE(missing.error() == FilesError::not_found);
}

TEST_CASE("platform files: replace puts the whole new file where the old one was",
          "[licence_platform][files][integration]")
{
    adl::fake::MemoryFileSystem fs;
    PlatformFiles files{fs};

    // No file yet, and no folder: both are made.
    REQUIRE(files.replace("/home/pat/clock.json", "one"));
    REQUIRE(text_of(fs.get("/home/pat/clock.json")) == "one");
    REQUIRE(files.replace("/home/pat/clock.json", "two"));
    REQUIRE(text_of(fs.get("/home/pat/clock.json")) == "two");
    // No temporary file is left.
    REQUIRE_FALSE(fs.get("/home/pat/clock.json.tmp").has_value());
    const auto names = files.list("/home/pat");
    REQUIRE(names);
    REQUIRE(names->size() == 1);
}

TEST_CASE("platform files: a replace that fails leaves the old file as it was",
          "[licence_platform][files][integration]")
{
    adl::fake::MemoryFileSystem fs;
    PlatformFiles files{fs};
    REQUIRE(files.replace("/d/clock.json", "old"));

    fs.fail_next_write(adl::platform::ErrorCode::no_space);
    const auto result = files.replace("/d/clock.json", "new");
    REQUIRE_FALSE(result);
    REQUIRE(text_of(fs.get("/d/clock.json")) == "old");
    REQUIRE_FALSE(fs.get("/d/clock.json.tmp").has_value());
}

TEST_CASE("platform files: remove takes one file away", "[licence_platform][files][integration]")
{
    adl::fake::MemoryFileSystem fs;
    PlatformFiles files{fs};
    fs.put("/d/a.adllic", "x");
    fs.put("/d/b.adllic", "x");

    REQUIRE(files.remove("/d/a.adllic"));
    REQUIRE_FALSE(fs.get("/d/a.adllic").has_value());
    REQUIRE(fs.get("/d/b.adllic").has_value());

    const auto again = files.remove("/d/a.adllic");
    REQUIRE_FALSE(again);
    REQUIRE(again.error() == FilesError::not_found);
}

// SPDX-License-Identifier: GPL-3.0-or-later
// The directory a download is written into, on files in the fake platform's memory, with a file system
// that misbehaves where the test asks it to.

#include "adl/loader_core/download_directory.hpp"
#include "fake_storage.hpp"
#include "faulty_files.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

using namespace adl::loader_core;
using adl::platform::ErrorCode;
using adl::platform::FileKind;
using Kind = DirectoryError::Kind;

namespace
{

bool is_directory(adl::platform::FileSystem& files, std::string_view path)
{
    const auto info = files.stat(path);
    return info.has_value() && info->kind == FileKind::directory;
}

} // namespace

TEST_CASE("the directory is named after the target and a number", "[loader_core][download_directory]")
{
    CHECK(download_directory_name("HNPFMS_L", 1) == "DNLD_DATA_HNPFMS_L_1");
    CHECK(download_directory_name("A", 12) == "DNLD_DATA_A_12");
}

TEST_CASE("only an id that can name a directory can be downloaded from", "[loader_core][download_directory]")
{
    for (const char* id : {"HNPFMS_L", "A", "pos 2"})
    {
        CHECK(can_download_from(id));
    }
    CHECK(can_download_from(std::string(240, 'a')));
    // What the loader refuses.
    CHECK_FALSE(can_download_from(""));
    CHECK_FALSE(can_download_from(std::string(241, 'a')));
    CHECK_FALSE(can_download_from("a/b"));
    CHECK_FALSE(can_download_from("..\\x"));
    CHECK_FALSE(can_download_from("a\tb"));
    // What the loader takes, and a directory cannot be named after on every platform.
    CHECK_FALSE(can_download_from("a:b"));
    CHECK_FALSE(can_download_from("a*"));
    CHECK_FALSE(can_download_from("a|b"));
    CHECK_FALSE(can_download_from("caf\xC3\xA9"));
}

TEST_CASE("a chain of directories is made and a directory is left alone", "[loader_core][download_directory]")
{
    adl::fake::MemoryFileSystem files;
    REQUIRE(make_directory_chain(files, "/a/b/c").has_value());
    CHECK(is_directory(files, "/a"));
    CHECK(is_directory(files, "/a/b/c"));
    files.put("/a/b/c/keep.txt", "kept");
    CHECK(make_directory_chain(files, "/a/b/c").has_value()); // there already
    CHECK(files.get("/a/b/c/keep.txt").has_value());
}

TEST_CASE("a chain says what it could not do", "[loader_core][download_directory]")
{
    adl::fake::MemoryFileSystem memory;
    adl::test::FaultyFileSystem files(memory);

    memory.put("/data/file", "x");
    auto chain = make_directory_chain(files, "/data/file");
    REQUIRE_FALSE(chain.has_value());
    CHECK(chain.error().kind == Kind::not_a_directory);
    CHECK(chain.error().describe() == "/data/file is not a folder");
    chain = make_directory_chain(files, "/data/file/below");
    REQUIRE_FALSE(chain.has_value());
    CHECK(chain.error().path == "/data/file");

    files.stat_fails["/locked"] = ErrorCode::permission_denied;
    chain = make_directory_chain(files, "/locked");
    REQUIRE_FALSE(chain.has_value());
    CHECK(chain.error().kind == Kind::cannot_look_at);
    CHECK(chain.error().code == ErrorCode::permission_denied);
    CHECK(chain.error().describe() == "cannot look at /locked (permission denied)");

    files.make_directory_fails["/blocked"] = ErrorCode::no_space;
    chain = make_directory_chain(files, "/blocked/below/more");
    REQUIRE_FALSE(chain.has_value());
    CHECK(chain.error().kind == Kind::cannot_create);
    CHECK(chain.error().describe() == "cannot create /blocked (no space)");
    CHECK_FALSE(is_directory(memory, "/blocked/below"));
}

TEST_CASE("a download directory takes the lowest free number", "[loader_core][download_directory]")
{
    adl::fake::MemoryFileSystem files;
    auto made = make_download_directory(files, "/dl/here", "HNPFMS_L");
    REQUIRE(made.has_value());
    CHECK(*made == "/dl/here/DNLD_DATA_HNPFMS_L_1");
    CHECK(is_directory(files, *made));
    CHECK(*make_download_directory(files, "/dl/here", "HNPFMS_L") == "/dl/here/DNLD_DATA_HNPFMS_L_2");

    // A number is taken by whatever is there, and the number of a directory that was removed is free
    // again. Another target's directories do not count.
    files.put("/dl/here/DNLD_DATA_HNPFMS_L_3", "not even a directory");
    REQUIRE(files.make_directory("/dl/here/DNLD_DATA_OTHER_1").has_value());
    REQUIRE(files.remove("/dl/here/DNLD_DATA_HNPFMS_L_1").has_value());
    CHECK(*make_download_directory(files, "/dl/here", "HNPFMS_L") == "/dl/here/DNLD_DATA_HNPFMS_L_1");
    CHECK(*make_download_directory(files, "/dl/here", "HNPFMS_L") == "/dl/here/DNLD_DATA_HNPFMS_L_4");
}

TEST_CASE("a number taken between looking and making is not an error", "[loader_core][download_directory]")
{
    adl::fake::MemoryFileSystem memory;
    adl::test::FaultyFileSystem files(memory);
    int races = 0;
    files.before_make_directory = [&](std::string_view path)
    {
        if (path.ends_with("/DNLD_DATA_HNPFMS_L_1") || path.ends_with("/DNLD_DATA_HNPFMS_L_2"))
        {
            ++races;
            static_cast<void>(memory.make_directory(path)); // the other loader's
        }
    };
    const auto made = make_download_directory(files, "/dl", "HNPFMS_L");
    REQUIRE(made.has_value());
    CHECK(races == 2);
    CHECK(*made == "/dl/DNLD_DATA_HNPFMS_L_3");
}

TEST_CASE("a download directory says what it could not do", "[loader_core][download_directory]")
{
    adl::fake::MemoryFileSystem memory;
    adl::test::FaultyFileSystem files(memory);

    memory.put("/file", "x");
    auto made = make_download_directory(files, "/file", "T");
    REQUIRE_FALSE(made.has_value());
    CHECK(made.error().kind == Kind::not_a_directory);

    files.stat_fails["/dl/DNLD_DATA_T_1"] = ErrorCode::io_error;
    made = make_download_directory(files, "/dl", "T");
    REQUIRE_FALSE(made.has_value());
    CHECK(made.error().kind == Kind::cannot_look_at);
    CHECK(made.error().path == "/dl/DNLD_DATA_T_1");
    files.stat_fails.clear();

    files.make_directory_fails["/dl/DNLD_DATA_T_1"] = ErrorCode::no_space;
    made = make_download_directory(files, "/dl", "T");
    REQUIRE_FALSE(made.has_value());
    CHECK(made.error().kind == Kind::cannot_create);
    CHECK_FALSE(is_directory(memory, "/dl/DNLD_DATA_T_1"));
    files.make_directory_fails.clear();

    // A file system that says every number is taken does not keep the loader looking forever.
    files.make_directory_always_exists = true;
    made = make_download_directory(files, "/dl", "T");
    REQUIRE_FALSE(made.has_value());
    CHECK(made.error().kind == Kind::too_many);
    CHECK(made.error().describe() == "there are already 10000 download folders for T in /dl");
}

TEST_CASE("only an empty directory is removed", "[loader_core][download_directory]")
{
    adl::fake::MemoryFileSystem memory;
    adl::test::FaultyFileSystem files(memory);

    REQUIRE(memory.make_directory("/empty").has_value());
    CHECK(remove_if_empty(files, "/empty"));
    CHECK_FALSE(memory.stat("/empty").has_value());

    // A directory with a file, a part file or a directory in it is left alone.
    memory.put("/full/a.bin", "data");
    CHECK_FALSE(remove_if_empty(files, "/full"));
    memory.put("/partial/a.bin.part", "half");
    CHECK_FALSE(remove_if_empty(files, "/partial"));
    REQUIRE(memory.make_directory("/nested").has_value());
    REQUIRE(memory.make_directory("/nested/inner").has_value());
    CHECK_FALSE(remove_if_empty(files, "/nested"));
    CHECK(is_directory(memory, "/nested/inner"));

    // Not a directory, not there, or not readable: nothing is done.
    memory.put("/plain", "x");
    CHECK_FALSE(remove_if_empty(files, "/plain"));
    CHECK_FALSE(remove_if_empty(files, "/missing"));
    REQUIRE(memory.make_directory("/unreadable").has_value());
    files.list_fails["/unreadable"] = ErrorCode::permission_denied;
    CHECK_FALSE(remove_if_empty(files, "/unreadable"));
    CHECK(is_directory(memory, "/unreadable"));
}

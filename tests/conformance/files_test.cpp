// SPDX-License-Identifier: GPL-3.0-or-later
// Conformance: files and storage.

#include "host_under_test.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using namespace adl::platform;
using namespace adl::test;

namespace
{

[[nodiscard]] std::vector<std::byte> pattern(std::size_t size, unsigned seed = 1)
{
    std::vector<std::byte> bytes(size);
    unsigned state = seed;
    for (auto& byte : bytes)
    {
        state = state * 1664525U + 1013904223U;
        byte = static_cast<std::byte>(state >> 24);
    }
    return bytes;
}

[[nodiscard]] std::span<const std::byte> view(const std::vector<std::byte>& bytes)
{
    return {bytes.data(), bytes.size()};
}

void write_file(FileSystem& files, const std::string& path, const std::vector<std::byte>& bytes)
{
    auto file = files.create(path, CreateMode::replace);
    REQUIRE(file.has_value());
    REQUIRE((*file)->write(view(bytes)).has_value());
    REQUIRE((*file)->close().has_value());
}

[[nodiscard]] std::map<std::string, FileKind> entries_of(FileSystem& files, const std::string& dir)
{
    std::map<std::string, FileKind> found;
    const auto listed = for_each_entry(files, dir,
                                       [&](std::string_view name, FileKind kind)
                                       {
                                           found[std::string(name)] = kind;
                                           return true;
                                       });
    REQUIRE(listed.has_value());
    return found;
}

} // namespace

TEST_CASE("a file written is read back at any offset", "[platform][conformance][files]")
{
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    FileSystem& files = under.host().files();
    const std::string path = under.in_scratch("data.bin");
    const auto content = pattern(10'000);

    auto out = files.create(path, CreateMode::exclusive);
    REQUIRE(out.has_value());
    // In pieces, as TFTP blocks arrive.
    std::size_t at = 0;
    for (const std::size_t piece : {std::size_t{0}, std::size_t{1}, std::size_t{511}, std::size_t{4096}})
    {
        REQUIRE((*out)->write(view(content).subspan(at, piece)).has_value());
        at += piece;
    }
    REQUIRE((*out)->write(view(content).subspan(at)).has_value());
    CHECK((*out)->size().value() == content.size());
    REQUIRE((*out)->sync().has_value());
    REQUIRE((*out)->close().has_value());

    auto in = files.open_read(path);
    REQUIRE(in.has_value());
    CHECK((*in)->size().value() == content.size());

    SECTION("a block in the middle")
    {
        std::vector<std::byte> block(512);
        const auto got = (*in)->read_at(1000, block);
        REQUIRE(got.has_value());
        CHECK(*got == 512);
        CHECK(std::equal(block.begin(), block.end(), content.begin() + 1000));
    }
    SECTION("the whole file in one call")
    {
        std::vector<std::byte> all(content.size());
        const auto got = (*in)->read_at(0, all);
        REQUIRE(got.has_value());
        CHECK(*got == content.size());
        CHECK(all == content);
    }
    SECTION("a short count at the end, and nothing beyond")
    {
        std::vector<std::byte> block(512);
        const auto tail = (*in)->read_at(content.size() - 100, block);
        REQUIRE(tail.has_value());
        CHECK(*tail == 100);
        CHECK(std::equal(block.begin(), block.begin() + 100, content.end() - 100));
        CHECK((*in)->read_at(content.size(), block).value() == 0);
        CHECK((*in)->read_at(content.size() + 1'000'000, block).value() == 0);
    }
    SECTION("reads can go back")
    {
        std::vector<std::byte> a(100);
        std::vector<std::byte> b(100);
        REQUIRE((*in)->read_at(5000, a).has_value());
        REQUIRE((*in)->read_at(10, b).has_value());
        std::vector<std::byte> again(100);
        REQUIRE((*in)->read_at(5000, again).has_value());
        CHECK(again == a);
        CHECK(std::equal(b.begin(), b.end(), content.begin() + 10));
    }
    SECTION("an empty read")
    {
        CHECK((*in)->read_at(0, std::span<std::byte>{}).value() == 0);
    }
}

TEST_CASE("an empty file is a file", "[platform][conformance][files]")
{
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    FileSystem& files = under.host().files();
    const std::string path = under.in_scratch("empty");

    write_file(files, path, {});
    const auto info = files.stat(path);
    REQUIRE(info.has_value());
    CHECK(info->kind == FileKind::regular);
    CHECK(info->size == 0);
    auto in = files.open_read(path);
    REQUIRE(in.has_value());
    std::byte one{};
    CHECK((*in)->read_at(0, {&one, 1}).value() == 0);
}

TEST_CASE("a file crosses the largest TFTP block size and back", "[platform][conformance][files]")
{
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    FileSystem& files = under.host().files();
    const std::string path = under.in_scratch("large.bin");
    constexpr std::size_t block = 65464; // RFC 2348's largest block size
    const auto content = pattern(block * 5 + 17, 99);

    auto out = files.create(path, CreateMode::exclusive);
    REQUIRE(out.has_value());
    for (std::size_t at = 0; at < content.size(); at += block)
    {
        const std::size_t n = std::min(block, content.size() - at);
        REQUIRE((*out)->write(view(content).subspan(at, n)).has_value());
    }
    REQUIRE((*out)->close().has_value());
    CHECK(files.stat(path)->size == content.size());

    auto in = files.open_read(path);
    REQUIRE(in.has_value());
    std::vector<std::byte> buffer(block);
    std::vector<std::byte> joined;
    for (std::uint64_t at = 0;; at += block)
    {
        const auto got = (*in)->read_at(at, buffer);
        REQUIRE(got.has_value());
        joined.insert(joined.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(*got));
        if (*got < block)
        {
            break;
        }
    }
    CHECK(joined == content);
}

TEST_CASE("creating a file does not overwrite unless asked", "[platform][conformance][files]")
{
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    FileSystem& files = under.host().files();
    const std::string path = under.in_scratch("once");
    write_file(files, path, pattern(100));

    SECTION("exclusive fails and leaves the file alone")
    {
        auto again = files.create(path, CreateMode::exclusive);
        REQUIRE_FALSE(again.has_value());
        CHECK(again.error().code == ErrorCode::already_exists);
        CHECK(files.stat(path)->size == 100);
    }
    SECTION("replace empties it")
    {
        auto again = files.create(path, CreateMode::replace);
        REQUIRE(again.has_value());
        CHECK(files.stat(path)->size == 0);
        const auto bytes = pattern(7, 5);
        REQUIRE((*again)->write(view(bytes)).has_value());
        REQUIRE((*again)->close().has_value());
        CHECK(files.stat(path)->size == 7);
    }
}

TEST_CASE("a file that is not there is not found", "[platform][conformance][files]")
{
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    FileSystem& files = under.host().files();
    const std::string missing = under.in_scratch("missing");

    CHECK(files.open_read(missing).error().code == ErrorCode::not_found);
    CHECK(files.stat(missing).error().code == ErrorCode::not_found);
    CHECK(files.remove(missing).error().code == ErrorCode::not_found);
    CHECK(files.rename(missing, under.in_scratch("elsewhere")).error().code == ErrorCode::not_found);
    CHECK(files.create(under.in_scratch("no/such/directory/file"), CreateMode::replace).error().code ==
          ErrorCode::not_found);
    CHECK(files.list(
                   missing, [](void*, std::string_view, FileKind) noexcept { return true; }, nullptr)
              .error()
              .code == ErrorCode::not_found);
}

TEST_CASE("directories are made, listed and removed", "[platform][conformance][files]")
{
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    FileSystem& files = under.host().files();

    const std::string dir = under.in_scratch("DNLD_DATA_HNPFMS_L_1");
    REQUIRE(files.make_directory(dir).has_value());
    CHECK(files.make_directory(dir).error().code == ErrorCode::already_exists);
    CHECK(files.stat(dir)->kind == FileKind::directory);
    CHECK(files.make_directory(dir + "/a/b").error().code == ErrorCode::not_found);

    write_file(files, dir + "/one.bin", pattern(10));
    write_file(files, dir + "/two.bin", pattern(20));
    REQUIRE(files.make_directory(dir + "/sub").has_value());

    SECTION("listing names every entry once, with its kind")
    {
        const auto found = entries_of(files, dir);
        CHECK(found.size() == 3);
        CHECK(found.at("one.bin") == FileKind::regular);
        CHECK(found.at("two.bin") == FileKind::regular);
        CHECK(found.at("sub") == FileKind::directory);
    }
    SECTION("the visitor can stop the listing")
    {
        int visited = 0;
        const auto listed = for_each_entry(files, dir,
                                           [&](std::string_view, FileKind)
                                           {
                                               ++visited;
                                               return false;
                                           });
        REQUIRE(listed.has_value());
        CHECK(visited == 1);
    }
    SECTION("the visitor may change the directory it is listing")
    {
        const auto listed = for_each_entry(files, dir,
                                           [&](std::string_view name, FileKind entry_kind)
                                           {
                                               if (entry_kind == FileKind::regular)
                                               {
                                                   CHECK(files.remove(dir + "/" + std::string(name)).has_value());
                                               }
                                               return true;
                                           });
        REQUIRE(listed.has_value());
        CHECK(entries_of(files, dir).size() == 1);
    }
    SECTION("a directory with something in it cannot be removed")
    {
        CHECK(files.remove(dir).error().code == ErrorCode::directory_not_empty);
    }
    SECTION("an empty directory can, and a file can")
    {
        CHECK(files.remove(dir + "/sub").has_value());
        CHECK(files.remove(dir + "/one.bin").has_value());
        CHECK(files.remove(dir + "/two.bin").has_value());
        CHECK(files.remove(dir).has_value());
        CHECK(files.stat(dir).error().code == ErrorCode::not_found);
    }
    SECTION("a file is not a directory")
    {
        CHECK(files.list(
                       dir + "/one.bin", [](void*, std::string_view, FileKind) noexcept { return true; }, nullptr)
                  .error()
                  .code == ErrorCode::not_a_directory);
        CHECK(files.open_read(dir).error().code == ErrorCode::is_a_directory);
    }
}

TEST_CASE("stat tells files from directories and gives sizes", "[platform][conformance][files]")
{
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    FileSystem& files = under.host().files();

    write_file(files, under.in_scratch("f"), pattern(1234));
    const auto file = files.stat(under.in_scratch("f"));
    REQUIRE(file.has_value());
    CHECK(file->kind == FileKind::regular);
    CHECK(file->size == 1234);

    const auto dir = files.stat(under.scratch());
    REQUIRE(dir.has_value());
    CHECK(dir->kind == FileKind::directory);
}

TEST_CASE("rename moves a file and replaces one that is there", "[platform][conformance][files]")
{
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    FileSystem& files = under.host().files();
    const auto first = pattern(300, 1);
    const auto second = pattern(40, 2);
    write_file(files, under.in_scratch("download.part"), first);

    SECTION("to a new name")
    {
        REQUIRE(files.rename(under.in_scratch("download.part"), under.in_scratch("download")).has_value());
        CHECK(files.stat(under.in_scratch("download.part")).error().code == ErrorCode::not_found);
        CHECK(files.stat(under.in_scratch("download"))->size == 300);
    }
    SECTION("over a file that exists")
    {
        write_file(files, under.in_scratch("download"), second);
        REQUIRE(files.rename(under.in_scratch("download.part"), under.in_scratch("download")).has_value());
        auto in = files.open_read(under.in_scratch("download"));
        REQUIRE(in.has_value());
        std::vector<std::byte> bytes(first.size());
        CHECK((*in)->read_at(0, bytes).value() == first.size());
        CHECK(bytes == first);
    }
}

TEST_CASE("a file open for reading survives being removed", "[platform][conformance][files]")
{
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    FileSystem& files = under.host().files();
    const auto content = pattern(2000, 3);
    write_file(files, under.in_scratch("served"), content);

    auto in = files.open_read(under.in_scratch("served"));
    REQUIRE(in.has_value());
    REQUIRE(files.remove(under.in_scratch("served")).has_value());

    std::vector<std::byte> bytes(content.size());
    CHECK((*in)->read_at(0, bytes).value() == content.size());
    CHECK(bytes == content);
}

TEST_CASE("a reader sees what has been written by the time the write returned", "[platform][conformance][files]")
{
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    FileSystem& files = under.host().files();
    const std::string path = under.in_scratch("growing");

    auto out = files.create(path, CreateMode::exclusive);
    REQUIRE(out.has_value());
    auto in = files.open_read(path);
    REQUIRE(in.has_value());
    const auto content = pattern(64, 4);
    REQUIRE((*out)->write(view(content)).has_value());

    std::vector<std::byte> bytes(64);
    CHECK((*in)->read_at(0, bytes).value() == 64);
    CHECK(bytes == content);
}

TEST_CASE("a path the interface does not allow is refused by every call", "[platform][conformance][files]")
{
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    FileSystem& files = under.host().files();

    // A backslash is a separator on Windows and a letter on Linux; the interface
    // refuses it everywhere, so a name from the network means one thing.
    const std::string with_nul("a\0b", 3);
    for (const std::string& bad : {std::string(), under.in_scratch("a\\b"), std::string("..\\x"), with_nul})
    {
        CAPTURE(bad.size());
        CHECK(files.open_read(bad).error().code == ErrorCode::invalid_argument);
        CHECK(files.create(bad, CreateMode::replace).error().code == ErrorCode::invalid_argument);
        CHECK(files.stat(bad).error().code == ErrorCode::invalid_argument);
        CHECK(files.make_directory(bad).error().code == ErrorCode::invalid_argument);
        CHECK(files.remove(bad).error().code == ErrorCode::invalid_argument);
        CHECK(files.rename(bad, under.in_scratch("x")).error().code == ErrorCode::invalid_argument);
        CHECK(files.rename(under.in_scratch("x"), bad).error().code == ErrorCode::invalid_argument);
    }
}

TEST_CASE("names with spaces and non-ASCII characters work", "[platform][conformance][files]")
{
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    FileSystem& files = under.host().files();

    const std::string name = "load \xC3\xA9t\xC3\xA9 1.LUH"; // "load été 1.LUH" in UTF-8
    write_file(files, under.in_scratch(name), pattern(10));
    CHECK(files.stat(under.in_scratch(name))->size == 10);
    const auto found = entries_of(files, under.scratch());
    CHECK(found.contains(name));
}

TEST_CASE("a failed write reports an error and not a partial success", "[platform][conformance][files]")
{
    // Only the fake can fill its storage; what a real disk does when full is not
    // something a test should provoke. The contract it is held to is all or an error.
    HostUnderTest under(HostKind::fake);
    auto& storage = under.fake_system().host().fake_files();
    FileSystem& files = under.host().files();

    storage.set_capacity(100);
    auto out = files.create(under.in_scratch("full"), CreateMode::exclusive);
    REQUIRE(out.has_value());
    const auto first = pattern(80);
    REQUIRE((*out)->write(view(first)).has_value());
    const auto second = pattern(40);
    const auto refused = (*out)->write(view(second));
    REQUIRE_FALSE(refused.has_value());
    CHECK(refused.error().code == ErrorCode::no_space);
    // Nothing of the refused write is in the file.
    CHECK((*out)->size().value() == 80);
    CHECK(storage.used_bytes() == 80);
}

TEST_CASE("a closed file accepts no more calls", "[platform][conformance][files]")
{
    const auto kind = GENERATE(HostKind::fake, HostKind::real);
    CAPTURE(kind);
    HostUnderTest under(kind);
    FileSystem& files = under.host().files();

    auto out = files.create(under.in_scratch("closed"), CreateMode::exclusive);
    REQUIRE(out.has_value());
    REQUIRE((*out)->close().has_value());
    const auto bytes = pattern(4);
    CHECK((*out)->write(view(bytes)).error().code == ErrorCode::closed);
    CHECK((*out)->sync().error().code == ErrorCode::closed);
    CHECK((*out)->close().error().code == ErrorCode::closed);
}

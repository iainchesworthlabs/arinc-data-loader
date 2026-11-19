// SPDX-License-Identifier: GPL-3.0-or-later
// The index of a media set's files and the FileSource an upload reads them through, on files in the
// fake platform's memory. No Qt: this is what a program with no window runs.

#include "adl/loader_core/media_source.hpp"
#include "fake_storage.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using namespace adl::loader_core;
using adl::arinc615a::ChecksumOption;
using adl::arinc615a::IoError;

namespace
{

adl::arinc665::FilesListEntry entry(std::string name, std::string pathname, std::uint16_t member, std::uint16_t crc)
{
    adl::arinc665::FilesListEntry file;
    file.file_name = std::move(name);
    file.pathname = std::move(pathname);
    file.member_sequence_number = member;
    file.crc = crc;
    return file;
}

ChecksumOption crc16_option(std::uint16_t crc)
{
    ChecksumOption option;
    option.type = adl::arinc665::CheckValueType::crc16;
    option.value[0] = static_cast<std::byte>(crc >> 8);
    option.value[1] = static_cast<std::byte>(crc & 0xFF);
    option.length = 2;
    return option;
}

// Two files called DATA.BIN in two directories of member 1, one on member 2, and a header.
adl::arinc665::FilesList sample_list()
{
    adl::arinc665::FilesList list;
    list.files = {entry("LOAD.LUH", "\\LOAD\\", 1, 0x0000), entry("DATA.BIN", "\\LOAD\\A\\", 1, 0x1111),
                  entry("DATA.BIN", "\\LOAD\\B\\", 1, 0x2222), entry("DATA.BIN", "\\", 2, 0x3333)};
    return list;
}

adl::arinc665::MediaFile media_file(std::string pathname, std::string name, std::string content)
{
    adl::arinc665::MediaFile file;
    file.pathname = std::move(pathname);
    file.name = std::move(name);
    for (const char c : content)
    {
        file.bytes.push_back(static_cast<std::byte>(c));
    }
    return file;
}

// The files sample_list() lists, as the media set read from the disk holds them.
std::vector<adl::arinc665::MediaMember> sample_members()
{
    adl::arinc665::MediaMember first;
    first.sequence_number = 1;
    first.files = {media_file("\\LOAD\\", "LOAD.LUH", "header"), media_file("\\LOAD\\A\\", "DATA.BIN", "from A"),
                   media_file("\\LOAD\\B\\", "DATA.BIN", "from B")};
    adl::arinc665::MediaMember second;
    second.sequence_number = 2;
    second.files = {media_file("\\", "DATA.BIN", "from member 2")};
    return {first, second};
}

void put_sample_files(adl::fake::MemoryFileSystem& files)
{
    files.put("/set/member_001/LOAD/LOAD.LUH", "header");
    files.put("/set/member_001/LOAD/A/DATA.BIN", "from A");
    files.put("/set/member_001/LOAD/B/DATA.BIN", "from B");
    files.put("/set/member_002/DATA.BIN", "from member 2");
}

std::string read_all(MediaTreeSource& source, adl::arinc615a::SourceFile file)
{
    std::vector<std::byte> buffer(static_cast<std::size_t>(file.size));
    const auto count = source.read(file.handle, 0, buffer);
    if (!count)
    {
        return "<read failed>";
    }
    return std::string(reinterpret_cast<const char*>(buffer.data()), *count);
}

} // namespace

TEST_CASE("the index maps each file of FILES.LUM to a path below its member directory", "[loader_core][media_source]")
{
    const auto index = MediaSetIndex::make("/set", sample_list());
    REQUIRE(index.has_value());
    REQUIRE(index->entries().size() == 4);
    CHECK(index->entries()[0].path == "/set/member_001/LOAD/LOAD.LUH");
    CHECK(index->entries()[1].path == "/set/member_001/LOAD/A/DATA.BIN");
    CHECK(index->entries()[3].path == "/set/member_002/DATA.BIN");
    CHECK(index->entries()[2].crc16 == 0x2222);
    CHECK(index->entries()[3].describe() == "DATA.BIN (member 2, \\)");
    CHECK_FALSE(index->entries()[0].fingerprint.has_value());

    adl::arinc665::FilesList bad;
    bad.files = {entry("X.BIN", "\\..\\", 1, 0)};
    const auto refused = MediaSetIndex::make("/set", bad);
    REQUIRE_FALSE(refused.has_value());
    CHECK(refused.error().find("X.BIN") != std::string::npos);
}

TEST_CASE("files of one name are told apart by the CRC in the request", "[loader_core][media_source]")
{
    const auto index = MediaSetIndex::make("/set", sample_list());
    REQUIRE(index.has_value());
    adl::fake::MemoryFileSystem files;
    put_sample_files(files);
    MediaTreeSource source(files, *index);

    const auto content_of = [&](std::optional<ChecksumOption> checksum)
    {
        auto file = source.open("DATA.BIN", checksum);
        REQUIRE(file.has_value());
        const std::string text = read_all(source, *file);
        source.close(file->handle);
        return text;
    };
    CHECK(content_of(crc16_option(0x2222)) == "from B");
    CHECK(content_of(crc16_option(0x1111)) == "from A");
    CHECK(content_of(crc16_option(0x3333)) == "from member 2");
    // No checksum and several files of the name: the first one listed. A checksum that fits none of
    // them: no such file.
    CHECK(content_of(std::nullopt) == "from A");
    const auto none = source.open("DATA.BIN", crc16_option(0x9999));
    REQUIRE_FALSE(none.has_value());
    CHECK(none.error().error == IoError::not_found);
    CHECK(source.open("MISSING.BIN", std::nullopt).error().error == IoError::not_found);
}

TEST_CASE("a file changed after the set was checked is found and refused", "[loader_core][media_source]")
{
    const auto members = sample_members();
    const auto index = MediaSetIndex::make("/set", sample_list(), members);
    REQUIRE(index.has_value());
    CHECK(index->entries()[0].fingerprint.has_value());

    adl::fake::MemoryFileSystem files;
    put_sample_files(files);
    CHECK_FALSE(index->changed_file(files).has_value()); // as it was checked

    MediaTreeSource source(files, *index);
    files.put("/set/member_001/LOAD/A/DATA.BIN", "edited");
    const auto changed = index->changed_file(files);
    REQUIRE(changed.has_value());
    CHECK(*changed == "DATA.BIN (member 1, \\LOAD\\A\\)");

    // The target is not given a byte of it, and the reason is kept for the operator.
    const auto refused = source.open("DATA.BIN", crc16_option(0x1111));
    REQUIRE_FALSE(refused.has_value());
    CHECK(refused.error().error == IoError::access_denied);
    REQUIRE(source.changed_file().has_value());
    CHECK(*source.changed_file() == *changed);

    // A file taken away is a change too.
    put_sample_files(files);
    CHECK(files.remove("/set/member_002/DATA.BIN").has_value());
    CHECK(index->changed_file(files) == std::optional<std::string>{"DATA.BIN (member 2, \\)"});
}

TEST_CASE("an index made from files that are not in the set is refused", "[loader_core][media_source]")
{
    auto members = sample_members();
    members[1].files.clear();
    const auto index = MediaSetIndex::make("/set", sample_list(), members);
    REQUIRE_FALSE(index.has_value());
    CHECK(index.error().find("not in the media set on the disk") != std::string::npos);
}

#include <string>
#include <vector>

#include <QTest>

#include "adl/app/media_source.hpp"
#include "fake_storage.hpp"
#include "test_classes.hpp"

using namespace adl::app;
using adl::arinc615a::ChecksumOption;
using adl::arinc615a::IoError;

namespace {

adl::arinc665::FilesListEntry entry(std::string name, std::string pathname, std::uint16_t member,
                                    std::uint16_t crc) {
    adl::arinc665::FilesListEntry file;
    file.file_name = std::move(name);
    file.pathname = std::move(pathname);
    file.member_sequence_number = member;
    file.crc = crc;
    return file;
}

ChecksumOption crc16_option(std::uint16_t crc) {
    ChecksumOption option;
    option.type = adl::arinc665::CheckValueType::crc16;
    option.value[0] = static_cast<std::byte>(crc >> 8);
    option.value[1] = static_cast<std::byte>(crc & 0xFF);
    option.length = 2;
    return option;
}

// Two files called DATA.BIN in two directories of member 1, one on member 2, and a header.
adl::arinc665::FilesList sample_list() {
    adl::arinc665::FilesList list;
    list.files = {entry("LOAD.LUH", "\\LOAD\\", 1, 0x0000),
                  entry("DATA.BIN", "\\LOAD\\A\\", 1, 0x1111),
                  entry("DATA.BIN", "\\LOAD\\B\\", 1, 0x2222), entry("DATA.BIN", "\\", 2, 0x3333)};
    return list;
}

std::string read_all(MediaTreeSource& source, adl::arinc615a::SourceFile file) {
    std::vector<std::byte> buffer(static_cast<std::size_t>(file.size));
    const auto count = source.read(file.handle, 0, buffer);
    if (!count) {
        return "<read failed>";
    }
    return std::string(reinterpret_cast<const char*>(buffer.data()), *count);
}

}  // namespace

void MediaSourceTest::index_maps_the_files_to_their_directories() {
    const auto index = MediaSetIndex::make("/set", sample_list());
    QVERIFY(index.has_value());
    QCOMPARE(index->entries().size(), std::size_t{4});
    QCOMPARE(index->entries()[0].path, std::string("/set/member_001/LOAD/LOAD.LUH"));
    QCOMPARE(index->entries()[1].path, std::string("/set/member_001/LOAD/A/DATA.BIN"));
    QCOMPARE(index->entries()[3].path, std::string("/set/member_002/DATA.BIN"));
    QCOMPARE(index->entries()[2].crc16, std::uint16_t{0x2222});
}

void MediaSourceTest::index_refuses_a_pathname_a_tree_cannot_hold() {
    adl::arinc665::FilesList list;
    list.files = {entry("X.BIN", "\\..\\", 1, 0)};
    const auto index = MediaSetIndex::make("/set", list);
    QVERIFY(!index.has_value());
    QVERIFY(index.error().find("X.BIN") != std::string::npos);
}

void MediaSourceTest::two_files_of_one_name_are_told_apart_by_their_crc() {
    const auto index = MediaSetIndex::make("/set", sample_list());
    QVERIFY(index.has_value());
    adl::fake::MemoryFileSystem files;
    files.put("/set/member_001/LOAD/A/DATA.BIN", "from A");
    files.put("/set/member_001/LOAD/B/DATA.BIN", "from B");
    files.put("/set/member_002/DATA.BIN", "from member 2");
    MediaTreeSource source(files, *index);

    const auto check = [&](std::uint16_t crc, const char* expected) {
        auto file = source.open("DATA.BIN", crc16_option(crc));
        QVERIFY2(file.has_value(), "the file was not found");
        QCOMPARE(read_all(source, *file), std::string(expected));
        QCOMPARE(file->size, std::uint64_t{std::string_view(expected).size()});
        source.close(file->handle);
    };
    check(0x2222, "from B");
    check(0x1111, "from A");
    check(0x3333, "from member 2");

    // With no checksum in the request and several files of the name, the first one listed.
    auto file = source.open("DATA.BIN", std::nullopt);
    QVERIFY(file.has_value());
    QCOMPARE(read_all(source, *file), std::string("from A"));
    source.close(file->handle);

    // Several files of the name and a CRC none of them has: there is no such file.
    const auto none = source.open("DATA.BIN", crc16_option(0x9999));
    QVERIFY(!none.has_value());
    QCOMPARE(static_cast<int>(none.error().error), static_cast<int>(IoError::not_found));
}

void MediaSourceTest::one_file_of_a_name_is_served_whatever_the_crc() {
    const auto index = MediaSetIndex::make("/set", sample_list());
    QVERIFY(index.has_value());
    adl::fake::MemoryFileSystem files;
    files.put("/set/member_001/LOAD/LOAD.LUH", "header");
    MediaTreeSource source(files, *index);

    // The CRC of a header file as the target knows it need not be the one of FILES.LUM.
    auto file = source.open("LOAD.LUH", crc16_option(0xABCD));
    QVERIFY(file.has_value());
    QCOMPARE(read_all(source, *file), std::string("header"));
    source.close(file->handle);

    // A checksum of another kind says nothing about which file is meant.
    ChecksumOption other = crc16_option(0);
    other.type = adl::arinc665::CheckValueType::crc32;
    other.length = 4;
    QVERIFY(source.open("LOAD.LUH", other).has_value());
}

void MediaSourceTest::no_such_file_is_not_found() {
    const auto index = MediaSetIndex::make("/set", sample_list());
    QVERIFY(index.has_value());
    adl::fake::MemoryFileSystem files;
    MediaTreeSource source(files, *index);

    // Not in the media set at all.
    const auto unknown = source.open("OTHER.BIN", std::nullopt);
    QVERIFY(!unknown.has_value());
    QCOMPARE(static_cast<int>(unknown.error().error), static_cast<int>(IoError::not_found));

    // In the media set's lists, and not on the disk (a file taken away since it was read).
    const auto gone = source.open("LOAD.LUH", std::nullopt);
    QVERIFY(!gone.has_value());
    QCOMPARE(static_cast<int>(gone.error().error), static_cast<int>(IoError::not_found));

    // A handle that was never given.
    std::vector<std::byte> buffer(4);
    QVERIFY(!source.read(3, 0, buffer).has_value());
    source.close(3);  // harmless
}

void MediaSourceTest::reads_at_offsets_and_gives_back_its_slots() {
    const auto index = MediaSetIndex::make("/set", sample_list());
    QVERIFY(index.has_value());
    adl::fake::MemoryFileSystem files;
    files.put("/set/member_001/LOAD/LOAD.LUH", "0123456789");
    MediaTreeSource source(files, *index);

    auto file = source.open("LOAD.LUH", std::nullopt);
    QVERIFY(file.has_value());
    std::vector<std::byte> buffer(4);
    auto count = source.read(file->handle, 6, buffer);
    QVERIFY(count.has_value());
    QCOMPARE(*count, std::size_t{4});
    QCOMPARE(std::string(reinterpret_cast<const char*>(buffer.data()), 4), std::string("6789"));
    count = source.read(file->handle, 8, buffer);  // a short count at the end
    QVERIFY(count.has_value());
    QCOMPARE(*count, std::size_t{2});
    count = source.read(file->handle, 10, buffer);
    QVERIFY(count.has_value());
    QCOMPARE(*count, std::size_t{0});
    source.close(file->handle);
    QVERIFY(!source.read(file->handle, 0, buffer).has_value());  // closed

    // At most max_open files at once; closing one makes room.
    std::vector<adl::arinc615a::SourceHandle> handles;
    for (std::size_t i = 0; i < MediaTreeSource::max_open; ++i) {
        auto opened = source.open("LOAD.LUH", std::nullopt);
        QVERIFY(opened.has_value());
        handles.push_back(opened->handle);
    }
    const auto too_many = source.open("LOAD.LUH", std::nullopt);
    QVERIFY(!too_many.has_value());
    QCOMPARE(static_cast<int>(too_many.error().error), static_cast<int>(IoError::io_error));
    source.close(handles.front());
    QVERIFY(source.open("LOAD.LUH", std::nullopt).has_value());
}

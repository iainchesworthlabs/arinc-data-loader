// The FileSource and FileSink over a directory of the platform's files, on the fake file system.

#include <catch2/catch_test_macros.hpp>

#include "adl/arinc615a/directory_files.hpp"
#include "adl/arinc615a_mini/mini_tftp.hpp"
#include "fake_storage.hpp"
#include "transfer_pair.hpp"

using namespace adl::arinc615a;
using namespace adl::arinc615a_mini;
using namespace adl::test;

namespace {

std::vector<std::byte> bytes_of(std::string_view text) {
    std::vector<std::byte> bytes;
    for (const char c : text) {
        bytes.push_back(static_cast<std::byte>(c));
    }
    return bytes;
}

}  // namespace

TEST_CASE("only plain names are safe", "[engine][files]") {
    CHECK(is_safe_file_name("APP.LUP"));
    CHECK(is_safe_file_name("HNPFMS_L.LCS"));
    CHECK(is_safe_file_name(std::string(254, 'a')));
    CHECK_FALSE(is_safe_file_name(std::string(255, 'a')));
    for (const std::string_view bad :
         {"", ".", "..", "../x", "a/b", "/abs", "a\\b", "C:\\x", "a\tb", "a\nb"}) {
        CAPTURE(bad);
        CHECK_FALSE(is_safe_file_name(bad));
    }
    const std::string with_nul("a\0b", 3);
    CHECK_FALSE(is_safe_file_name(with_nul));
}

TEST_CASE("a source reads the files of a directory in blocks", "[engine][files]") {
    adl::fake::MemoryFileSystem files;
    const Bytes content = pattern(2500, 1);
    files.put("/load/APP.LUP", content);
    DirectoryFileSource source(files, "/load");

    const auto file = source.open("APP.LUP", std::nullopt);
    REQUIRE(file);
    CHECK(file->size == 2500);
    Bytes out(1024);
    auto count = source.read(file->handle, 0, out);
    REQUIRE(count);
    CHECK(*count == 1024);
    CHECK(std::equal(out.begin(), out.end(), content.begin()));
    count = source.read(file->handle, 2048, out);
    REQUIRE(count);
    CHECK(*count == 452);  // a short read only at the end
    count = source.read(file->handle, 2500, out);
    REQUIRE(count);
    CHECK(*count == 0);
    source.close(file->handle);
    CHECK_FALSE(source.read(file->handle, 0, out));
}

TEST_CASE("a source refuses what is not a name, and reports what is missing", "[engine][files]") {
    adl::fake::MemoryFileSystem files;
    files.put("/secret/PASSWD", "x");
    files.put("/load/APP.LUP", "x");
    DirectoryFileSource source(files, "/load");
    for (const std::string_view name : {"../secret/PASSWD", "/secret/PASSWD", "..", "a/b"}) {
        CAPTURE(name);
        const auto file = source.open(name, std::nullopt);
        REQUIRE_FALSE(file);
        CHECK(file.error().error == IoError::access_denied);
    }
    const auto missing = source.open("NONE.BIN", std::nullopt);
    REQUIRE_FALSE(missing);
    CHECK(missing.error().error == IoError::not_found);
}

TEST_CASE("a source holds a limited number of files open", "[engine][files]") {
    adl::fake::MemoryFileSystem files;
    files.put("/load/A.BIN", "abc");
    DirectoryFileSource source(files, "/load");
    std::vector<SourceHandle> handles;
    for (std::size_t i = 0; i < DirectoryFileSource::max_open; ++i) {
        const auto file = source.open("A.BIN", std::nullopt);
        REQUIRE(file);
        handles.push_back(file->handle);
    }
    CHECK_FALSE(source.open("A.BIN", std::nullopt));
    source.close(handles[3]);
    const auto again = source.open("A.BIN", std::nullopt);
    REQUIRE(again);
    CHECK(again->handle == handles[3]);
}

TEST_CASE("a sink makes a file when it is complete and not before", "[engine][files]") {
    adl::fake::MemoryFileSystem files;
    REQUIRE(files.make_directory("/down"));
    DirectoryFileSink sink(files, "/down");
    const auto handle = sink.open("A.BIN", 6);
    REQUIRE(handle);
    REQUIRE(sink.write(*handle, bytes_of("abc")));
    CHECK_FALSE(files.get("/down/A.BIN"));  // not under its name yet
    REQUIRE(sink.write(*handle, bytes_of("def")));
    REQUIRE(sink.finish(*handle, ReceivedFile{6, ChecksumVerdict::matched}));
    const auto stored = files.get("/down/A.BIN");
    REQUIRE(stored);
    CHECK(*stored == bytes_of("abcdef"));
    CHECK_FALSE(files.get("/down/A.BIN.part"));
}

TEST_CASE("a sink leaves nothing behind for a transfer that failed", "[engine][files]") {
    adl::fake::MemoryFileSystem files;
    REQUIRE(files.make_directory("/down"));
    DirectoryFileSink sink(files, "/down");
    const auto handle = sink.open("A.BIN", std::nullopt);
    REQUIRE(handle);
    REQUIRE(sink.write(*handle, bytes_of("abc")));
    sink.discard(*handle);
    CHECK_FALSE(files.get("/down/A.BIN"));
    CHECK_FALSE(files.get("/down/A.BIN.part"));
    // The handle is free again.
    CHECK(sink.open("A.BIN", std::nullopt));
}

TEST_CASE("a sink never overwrites and refuses what is not a name", "[engine][files]") {
    adl::fake::MemoryFileSystem files;
    REQUIRE(files.make_directory("/down"));
    files.put("/down/KEEP.BIN", "mine");
    DirectoryFileSink sink(files, "/down");
    const auto taken = sink.open("KEEP.BIN", std::nullopt);
    REQUIRE_FALSE(taken);
    CHECK(taken.error().error == IoError::access_denied);
    CHECK(*files.get("/down/KEEP.BIN") == bytes_of("mine"));
    for (const std::string_view name : {"../KEEP.BIN", "a/b", "", "..", "x\\y"}) {
        CAPTURE(name);
        CHECK_FALSE(sink.open(name, std::nullopt));
    }
}

TEST_CASE("a sink reports a full disk", "[engine][files]") {
    adl::fake::MemoryFileSystem files;
    REQUIRE(files.make_directory("/down"));
    files.set_capacity(10);
    DirectoryFileSink sink(files, "/down");
    const auto handle = sink.open("A.BIN", std::nullopt);
    REQUIRE(handle);
    const auto written = sink.write(*handle, Bytes(11, std::byte{1}));
    REQUIRE_FALSE(written);
    CHECK(written.error().error == IoError::no_space);
    sink.discard(*handle);
}

TEST_CASE("a sink holds a limited number of files open", "[engine][files]") {
    adl::fake::MemoryFileSystem files;
    REQUIRE(files.make_directory("/down"));
    DirectoryFileSink sink(files, "/down");
    for (std::size_t i = 0; i < DirectoryFileSink::max_open; ++i) {
        CHECK(sink.open("F" + std::to_string(i), std::nullopt));
    }
    CHECK_FALSE(sink.open("ONE_MORE", std::nullopt));
}

TEST_CASE("a handle that is not open is refused", "[engine][files]") {
    adl::fake::MemoryFileSystem files;
    REQUIRE(files.make_directory("/down"));
    DirectoryFileSink sink(files, "/down");
    CHECK_FALSE(sink.write(3, bytes_of("x")));
    CHECK_FALSE(sink.write(99, bytes_of("x")));
    CHECK_FALSE(sink.finish(3, ReceivedFile{}));
    sink.discard(3);  // does nothing
    sink.discard(99);
}

// The target chooses the names of an operator defined download, so none of the names a host
// treats specially may reach the file system, whichever host this is: the directory is portable.
TEST_CASE("names a host treats specially are not safe", "[engine][files]") {
    for (const std::string_view bad :
         {"a:b", "a:b:$DATA", "file.txt::$DATA", "C:x", "NUL", "nul.txt", "CON", "con.log", "PRN",
          "AUX", "COM1", "com9.bin", "LPT1", "lpt9.x", "CONIN$", "CONOUT$", "trailing.",
          "trailing ", "a*b", "a?b", "a\"b", "a<b", "a>b", "a|b", "caf\xC3\xA9", "del\x7f",
          // the suffix a file has while it is received
          "x.part", "NOTES.part"}) {
        CAPTURE(bad);
        CHECK_FALSE(is_safe_file_name(bad));
    }
    for (const std::string_view fine : {"COMPORT.BIN", "NULL", "A B.TXT", "a.b.c", "x~1", ".hidden"}) {
        CAPTURE(fine);
        CHECK(is_safe_file_name(fine));
    }
}

TEST_CASE("a sink writes nothing under a name a host treats specially", "[engine][files]") {
    adl::fake::MemoryFileSystem files;
    REQUIRE(files.make_directory("/dl"));
    DirectoryFileSink sink(files, "/dl");
    for (const std::string_view bad : {"a:b", "NUL", "con.txt", "COM1", "dot.", "pending.part"}) {
        CAPTURE(bad);
        const auto opened = sink.open(bad, std::nullopt);
        REQUIRE_FALSE(opened);
        CHECK(opened.error().error == IoError::access_denied);
    }
    std::size_t entries = 0;
    REQUIRE(adl::platform::for_each_entry(files, "/dl", [&](std::string_view, auto) {
        ++entries;
        return true;
    }));
    CHECK(entries == 0);
}

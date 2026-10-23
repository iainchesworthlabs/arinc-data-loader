#include <string>

#include <QTest>

#include "adl/app/download_directory.hpp"
#include "fake_storage.hpp"
#include "faulty_files.hpp"
#include "test_classes.hpp"

using namespace adl::app;
using adl::platform::ErrorCode;
using adl::platform::FileKind;

namespace {

bool is_directory(adl::platform::FileSystem& files, std::string_view path) {
    const auto info = files.stat(path);
    return info.has_value() && info->kind == FileKind::directory;
}

}  // namespace

void DownloadDirectoryTest::the_directory_is_named_after_the_target_and_a_number() {
    QCOMPARE(download_directory_name("HNPFMS_L", 1), std::string("DNLD_DATA_HNPFMS_L_1"));
    QCOMPARE(download_directory_name("A", 12), std::string("DNLD_DATA_A_12"));
}

void DownloadDirectoryTest::only_an_id_that_can_name_a_directory_can_be_downloaded_from() {
    QVERIFY(can_download_from("HNPFMS_L"));
    QVERIFY(can_download_from("A"));
    QVERIFY(can_download_from("pos 2"));
    QVERIFY(can_download_from(std::string(240, 'a')));
    // What the loader refuses.
    QVERIFY(!can_download_from(""));
    QVERIFY(!can_download_from(std::string(241, 'a')));
    QVERIFY(!can_download_from("a/b"));
    QVERIFY(!can_download_from("..\\x"));
    QVERIFY(!can_download_from("a\tb"));
    QVERIFY(!can_download_from(std::string("a\0b", 3)));
    // What the loader takes, and a directory cannot be named after on every platform.
    QVERIFY(!can_download_from("a:b"));
    QVERIFY(!can_download_from("a*"));
    QVERIFY(!can_download_from("a|b"));
    QVERIFY(!can_download_from("caf\xC3\xA9"));
}

void DownloadDirectoryTest::a_chain_of_directories_is_made_and_a_directory_is_left_alone() {
    adl::fake::MemoryFileSystem files;
    QVERIFY(make_directory_chain(files, "/a/b/c").has_value());
    QVERIFY(is_directory(files, "/a"));
    QVERIFY(is_directory(files, "/a/b"));
    QVERIFY(is_directory(files, "/a/b/c"));
    files.put("/a/b/c/keep.txt", "kept");
    QVERIFY(make_directory_chain(files, "/a/b/c").has_value());  // there already
    QVERIFY(files.get("/a/b/c/keep.txt").has_value());
}

void DownloadDirectoryTest::a_chain_says_why_it_could_not_be_made() {
    adl::fake::MemoryFileSystem memory;
    adl::test::FaultyFileSystem files(memory);

    // A file where a directory is wanted.
    memory.put("/data/file", "x");
    auto chain = make_directory_chain(files, "/data/file");
    QVERIFY(!chain.has_value());
    QVERIFY(chain.error().text().contains(QStringLiteral("is not a folder")));
    chain = make_directory_chain(files, "/data/file/below");
    QVERIFY(!chain.has_value());
    QVERIFY(chain.error().text().contains(QStringLiteral("is not a folder")));

    // A path that cannot be looked at.
    files.stat_fails["/locked"] = ErrorCode::permission_denied;
    chain = make_directory_chain(files, "/locked");
    QVERIFY(!chain.has_value());
    QVERIFY(chain.error().text().contains(QStringLiteral("cannot look at /locked")));

    // A directory that cannot be made, the last one and one on the way.
    files.make_directory_fails["/full/sub"] = ErrorCode::no_space;
    chain = make_directory_chain(files, "/full/sub");
    QVERIFY(!chain.has_value());
    QVERIFY(chain.error().text().contains(QStringLiteral("cannot create /full/sub")));
    files.make_directory_fails.clear();
    files.make_directory_fails["/blocked"] = ErrorCode::permission_denied;
    chain = make_directory_chain(files, "/blocked/below/more");
    QVERIFY(!chain.has_value());
    QVERIFY(chain.error().text().contains(QStringLiteral("cannot create /blocked")));
    QVERIFY(!is_directory(memory, "/blocked/below"));

    // Another loader made the directory first: it is there, which is what was wanted. If what
    // took the name is not a directory, it is not.
    files.make_directory_fails.clear();
    files.before_make_directory = [&](std::string_view path) {
        if (path == "/race") {
            static_cast<void>(memory.make_directory("/race"));
        }
        if (path == "/race-file") {
            memory.put("/race-file", "a file took the name");
        }
    };
    files.make_directory_always_exists = true;
    QVERIFY(make_directory_chain(files, "/race").has_value());
    chain = make_directory_chain(files, "/race-file");
    QVERIFY(!chain.has_value());
    QVERIFY(chain.error().text().contains(QStringLiteral("cannot create /race-file")));
}

void DownloadDirectoryTest::a_download_directory_takes_the_lowest_free_number() {
    adl::fake::MemoryFileSystem files;
    auto made = make_download_directory(files, "/dl/here", "HNPFMS_L");
    QVERIFY(made.has_value());
    QCOMPARE(*made, std::string("/dl/here/DNLD_DATA_HNPFMS_L_1"));
    QVERIFY(is_directory(files, *made));

    made = make_download_directory(files, "/dl/here", "HNPFMS_L");
    QVERIFY(made.has_value());
    QCOMPARE(*made, std::string("/dl/here/DNLD_DATA_HNPFMS_L_2"));

    // A number is taken by whatever is there, and the number of a directory that was removed
    // is free again. Another target's directories do not count.
    files.put("/dl/here/DNLD_DATA_HNPFMS_L_3", "not even a directory");
    QVERIFY(files.make_directory("/dl/here/DNLD_DATA_OTHER_1").has_value());
    QVERIFY(files.remove("/dl/here/DNLD_DATA_HNPFMS_L_1").has_value());
    made = make_download_directory(files, "/dl/here", "HNPFMS_L");
    QVERIFY(made.has_value());
    QCOMPARE(*made, std::string("/dl/here/DNLD_DATA_HNPFMS_L_1"));
    made = make_download_directory(files, "/dl/here", "HNPFMS_L");
    QVERIFY(made.has_value());
    QCOMPARE(*made, std::string("/dl/here/DNLD_DATA_HNPFMS_L_4"));
}

void DownloadDirectoryTest::a_number_taken_between_looking_and_making_is_not_an_error() {
    adl::fake::MemoryFileSystem memory;
    adl::test::FaultyFileSystem files(memory);
    // Two loaders look at the same time: by the time this one makes number 1, the other has.
    int races = 0;
    files.before_make_directory = [&](std::string_view path) {
        if (path.ends_with("/DNLD_DATA_HNPFMS_L_1") || path.ends_with("/DNLD_DATA_HNPFMS_L_2")) {
            ++races;
            static_cast<void>(memory.make_directory(path));
        }
    };
    const auto made = make_download_directory(files, "/dl", "HNPFMS_L");
    QVERIFY(made.has_value());
    QCOMPARE(races, 2);
    QCOMPARE(*made, std::string("/dl/DNLD_DATA_HNPFMS_L_3"));
    QVERIFY(is_directory(memory, "/dl/DNLD_DATA_HNPFMS_L_1"));  // the other loader's, untouched
    QVERIFY(is_directory(memory, "/dl/DNLD_DATA_HNPFMS_L_2"));
}

void DownloadDirectoryTest::a_download_directory_says_why_it_could_not_be_made() {
    adl::fake::MemoryFileSystem memory;
    adl::test::FaultyFileSystem files(memory);

    // The download directory cannot be used.
    memory.put("/file", "x");
    auto made = make_download_directory(files, "/file", "T");
    QVERIFY(!made.has_value());
    QVERIFY(made.error().text().contains(QStringLiteral("is not a folder")));

    // A number that cannot be looked at, or made.
    files.stat_fails["/dl/DNLD_DATA_T_1"] = ErrorCode::io_error;
    made = make_download_directory(files, "/dl", "T");
    QVERIFY(!made.has_value());
    QVERIFY(made.error().text().contains(QStringLiteral("cannot look at /dl/DNLD_DATA_T_1")));
    files.stat_fails.clear();
    files.make_directory_fails["/dl/DNLD_DATA_T_1"] = ErrorCode::no_space;
    made = make_download_directory(files, "/dl", "T");
    QVERIFY(!made.has_value());
    QVERIFY(made.error().text().contains(QStringLiteral("cannot create /dl/DNLD_DATA_T_1")));
    QVERIFY(!is_directory(memory, "/dl/DNLD_DATA_T_1"));

    // A file system that says every number is taken does not keep the loader looking forever.
    files.make_directory_fails.clear();
    files.make_directory_always_exists = true;
    made = make_download_directory(files, "/dl", "T");
    QVERIFY(!made.has_value());
    QVERIFY(made.error().text().contains(QStringLiteral("download folders")));
}

void DownloadDirectoryTest::only_an_empty_directory_is_removed() {
    adl::fake::MemoryFileSystem memory;
    adl::test::FaultyFileSystem files(memory);

    QVERIFY(memory.make_directory("/empty").has_value());
    QVERIFY(remove_if_empty(files, "/empty"));
    QVERIFY(!memory.stat("/empty").has_value());

    // A directory with a file in it, a part file or a directory in it: left alone.
    memory.put("/full/a.bin", "data");
    QVERIFY(!remove_if_empty(files, "/full"));
    QVERIFY(memory.get("/full/a.bin").has_value());
    memory.put("/partial/a.bin.part", "half");
    QVERIFY(!remove_if_empty(files, "/partial"));
    QVERIFY(memory.make_directory("/nested").has_value());
    QVERIFY(memory.make_directory("/nested/inner").has_value());
    QVERIFY(!remove_if_empty(files, "/nested"));
    QVERIFY(is_directory(memory, "/nested/inner"));

    // Not a directory, not there, or not readable: nothing is done.
    memory.put("/plain", "x");
    QVERIFY(!remove_if_empty(files, "/plain"));
    QVERIFY(memory.get("/plain").has_value());
    QVERIFY(!remove_if_empty(files, "/missing"));
    QVERIFY(memory.make_directory("/unreadable").has_value());
    files.list_fails["/unreadable"] = ErrorCode::permission_denied;
    QVERIFY(!remove_if_empty(files, "/unreadable"));
    QVERIFY(is_directory(memory, "/unreadable"));
}

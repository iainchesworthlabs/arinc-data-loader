#include <QTest>

#include "adl/app/notice_text.hpp"
#include "test_classes.hpp"

using adl::app::notice_text;
using adl::arinc615a::Notice;

void NoticeTextTest::names_the_file_and_gives_the_number_its_unit() {
    QCOMPARE(notice_text(Notice::waiting, "HNPFMS_L.LUI", 5),
             QStringLiteral("target asked to wait: HNPFMS_L.LUI (5 s)"));
    QCOMPARE(notice_text(Notice::file_not_available, "A.BIN", 3),
             QStringLiteral("file not available: A.BIN (3 s)"));
    QCOMPARE(notice_text(Notice::port_option_ignored, "HNPFMS_L.LCI", 5900),
             QStringLiteral("port option not taken by the target: HNPFMS_L.LCI (port 5900)"));
    QCOMPARE(notice_text(Notice::status_counter_gap, {}, 2),
             QStringLiteral("status files lost (2)"));
    QCOMPARE(notice_text(Notice::files_missing, {}, 0), QStringLiteral("requested files missing"));
}

void NoticeTextTest::says_how_a_failed_transfer_ended() {
    // The value of these two is a TransferOutcome: 2 is a time-out.
    QCOMPARE(notice_text(Notice::status_file_failed, "HNPFMS_L.LCS", 2),
             QStringLiteral("status file transfer failed: HNPFMS_L.LCS (timed out)"));
    QCOMPARE(notice_text(Notice::transfer_failed, "A.BIN", 3),
             QStringLiteral("transfer failed: A.BIN (the peer sent an error)"));
    // A number that is no outcome is left out, not shown as nonsense.
    QCOMPARE(notice_text(Notice::transfer_failed, "A.BIN", 200),
             QStringLiteral("transfer failed: A.BIN"));
}

void NoticeTextTest::leaves_the_rest_as_they_are() {
    QCOMPARE(notice_text(Notice::checksum_mismatch, "A.BIN", 0),
             QStringLiteral("checksum did not match: A.BIN"));
    QCOMPARE(notice_text(Notice::status_duplicate, {}, 0), QStringLiteral("status file repeated"));
    QCOMPARE(notice_text(Notice::request_refused, "X", 0), QStringLiteral("request refused: X"));
}

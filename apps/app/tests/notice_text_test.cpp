// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/app/notice_text.hpp"
#include "test_classes.hpp"

#include <QTest>

using adl::app::notice_text;
using adl::arinc615a::Notice;

void NoticeTextTest::names_the_file_and_gives_the_number_its_unit()
{
    QCOMPARE(notice_text(Notice::waiting, "HNPFMS_L.LUI", 5),
             QStringLiteral("target asked to wait: HNPFMS_L.LUI (5 s)"));
    QCOMPARE(notice_text(Notice::file_not_available, "A.BIN", 3), QStringLiteral("file not available: A.BIN (3 s)"));
    QCOMPARE(notice_text(Notice::port_option_ignored, "HNPFMS_L.LCI", 5900),
             QStringLiteral("port option not taken by the target: HNPFMS_L.LCI (port 5900)"));
    QCOMPARE(notice_text(Notice::status_counter_gap, {}, 2), QStringLiteral("status files lost (2)"));
    QCOMPARE(notice_text(Notice::files_missing, {}, 0), QStringLiteral("requested files missing"));
}

void NoticeTextTest::says_how_a_failed_transfer_ended()
{
    // The value of these two is a TransferOutcome: 2 is a time-out.
    QCOMPARE(notice_text(Notice::status_file_failed, "HNPFMS_L.LCS", 2),
             QStringLiteral("status file transfer failed: HNPFMS_L.LCS (timed out)"));
    QCOMPARE(notice_text(Notice::transfer_failed, "A.BIN", 3),
             QStringLiteral("transfer failed: A.BIN (the peer sent an error)"));
    // A number that is no outcome is left out, not shown as nonsense.
    QCOMPARE(notice_text(Notice::transfer_failed, "A.BIN", 200), QStringLiteral("transfer failed: A.BIN"));
}

void NoticeTextTest::leaves_the_rest_as_they_are()
{
    QCOMPARE(notice_text(Notice::checksum_mismatch, "A.BIN", 0), QStringLiteral("checksum did not match: A.BIN"));
    QCOMPARE(notice_text(Notice::status_duplicate, {}, 0), QStringLiteral("status file repeated"));
    QCOMPARE(notice_text(Notice::request_refused, "X", 0), QStringLiteral("request refused: X"));
    // The three that no other check named: a status file out of order, a checksum algorithm the
    // loader cannot calculate (the number is the algorithm's, and is not shown), and the options
    // the target refused.
    QCOMPARE(notice_text(Notice::status_stale, {}, 7), QStringLiteral("status file out of order"));
    QCOMPARE(notice_text(Notice::checksum_unsupported, "A.BIN", 9),
             QStringLiteral("checksum algorithm not supported: A.BIN"));
    QCOMPARE(notice_text(Notice::option_fallback, {}, 0), QStringLiteral("options refused, asked again without them"));
}

void NoticeTextTest::every_notice_has_a_sentence_of_its_own()
{
    // Walks the whole enumeration: a notice added to the engine without a sentence would show
    // as "unknown", and one that shares another's text could not be told from it.
    QStringList sentences;
    for (int value = 0; value <= static_cast<int>(Notice::files_missing); ++value)
    {
        const QString text = notice_text(static_cast<Notice>(value), {}, 0);
        QVERIFY2(!text.isEmpty() && text != QStringLiteral("unknown"), qPrintable(text));
        QVERIFY2(!sentences.contains(text), qPrintable(text));
        sentences.append(text);
    }
    QCOMPARE(sentences.size(), static_cast<int>(Notice::files_missing) + 1);
}

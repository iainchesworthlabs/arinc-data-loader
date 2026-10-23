#include <QList>
#include <QRect>
#include <QTest>

#include "adl/ui/window_state.hpp"
#include "test_classes.hpp"

using adl::ui::stored_coordinate;
using adl::ui::title_bar_reachable;

void WindowStateTest::reads_a_stored_coordinate_as_a_signed_value() {
    QCOMPARE(stored_coordinate(QVariant(120), 7), 120);
    QCOMPARE(stored_coordinate(QVariant(-2274), 7), -2274);
    QCOMPARE(stored_coordinate(QVariant(QStringLiteral("-1500")), 7), -1500);
    QCOMPARE(stored_coordinate(QVariant(QStringLiteral("640")), 7), 640);
}

void WindowStateTest::wraps_a_coordinate_the_registry_kept_unsigned() {
    // The Windows registry holds an int as a DWORD: x = -2274 on a monitor left of the primary one
    // reads back as 4294965022, which as an int is 0 (or nonsense) and lost the window.
    QCOMPARE(stored_coordinate(QVariant(4294965022U), 7), -2274);
    QCOMPARE(stored_coordinate(QVariant(QStringLiteral("4294965022")), 7), -2274);
    QCOMPARE(stored_coordinate(QVariant(qlonglong{4294967295LL}), 7), -1);
    QCOMPARE(stored_coordinate(QVariant(qlonglong{2147483648LL}), 7), -2147483647 - 1);
}

void WindowStateTest::falls_back_for_a_value_that_is_no_coordinate() {
    QCOMPARE(stored_coordinate(QVariant(), 7), 7);
    QCOMPARE(stored_coordinate(QVariant(QStringLiteral("left")), 7), 7);
    QCOMPARE(stored_coordinate(QVariant(qlonglong{4294967296LL}), 7), 7);
    QCOMPARE(stored_coordinate(QVariant(qlonglong{-4294967296LL}), 7), 7);
}

void WindowStateTest::accepts_a_window_whose_title_bar_is_on_a_screen() {
    const QList<QRect> screens{QRect(0, 0, 1920, 1040)};
    QVERIFY(title_bar_reachable(QRect(100, 100, 1280, 900), screens));
    // Hanging off the right and the bottom is fine while the title bar can be grabbed.
    QVERIFY(title_bar_reachable(QRect(1000, 900, 1280, 900), screens));
    // 96 px of the title bar showing is the least that counts.
    QVERIFY(title_bar_reachable(QRect(1920 - 96, 100, 1280, 900), screens));
    QVERIFY(title_bar_reachable(QRect(-1280 + 96, 100, 1280, 900), screens));
}

void WindowStateTest::refuses_a_window_that_barely_touches_a_screen() {
    const QList<QRect> screens{QRect(0, 0, 1920, 1040)};
    // One pixel of overlap passed the old rule; it leaves nothing to drag.
    QVERIFY(!title_bar_reachable(QRect(1919, 100, 1280, 900), screens));
    QVERIFY(!title_bar_reachable(QRect(-1279, 100, 1280, 900), screens));
    QVERIFY(!title_bar_reachable(QRect(1920 - 95, 100, 1280, 900), screens));
    // The title bar above the top edge, or below the bottom of the working area, cannot be
    // reached even though the body of the window overlaps the screen.
    QVERIFY(!title_bar_reachable(QRect(100, -40, 1280, 900), screens));
    QVERIFY(!title_bar_reachable(QRect(100, 1040 - 10, 1280, 900), screens));
    QVERIFY(!title_bar_reachable(QRect(100, 1500, 1280, 900), screens));
}

void WindowStateTest::finds_a_window_on_a_monitor_left_of_the_primary_one() {
    const QList<QRect> screens{QRect(-1920, 0, 1920, 1040), QRect(0, 0, 2560, 1400)};
    QVERIFY(title_bar_reachable(QRect(-1500, 120, 1280, 900), screens));
    // The same window when that monitor has been unplugged.
    QVERIFY(!title_bar_reachable(QRect(-1500, 120, 1280, 900), {QRect(0, 0, 2560, 1400)}));
    // No screens at all: nothing is reachable.
    QVERIFY(!title_bar_reachable(QRect(100, 100, 1280, 900), {}));
}

void WindowStateTest::a_title_bar_narrower_than_the_minimum_needs_all_of_itself() {
    const QList<QRect> screens{QRect(0, 0, 1920, 1040)};
    QVERIFY(title_bar_reachable(QRect(100, 100, 80, 40), screens));
    QVERIFY(!title_bar_reachable(QRect(1900, 100, 80, 40), screens));
}

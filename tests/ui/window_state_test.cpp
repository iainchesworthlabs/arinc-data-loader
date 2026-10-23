// SPDX-License-Identifier: GPL-3.0-or-later
// What install_window_state promises (libs/ui/include/adl/ui/window_state.hpp and section 8 of
// docs/design-overhaul.md), checked on a bare window on the offscreen platform: where a window
// opens when nothing was stored, that a stored size and position come back, that a size below the
// window's minimum is raised and one larger than the screen allows is brought inside it, that a
// position on no screen is ignored, that a maximised window opens maximised, and what is written
// when the window closes. The settings are the program's QSettings, moved into a scratch
// directory.

#include "adl/ui/window_state.hpp"
#include "scratch_settings.hpp"

#include <QGuiApplication>
#include <QPoint>
#include <QQuickWindow>
#include <QRect>
#include <QScreen>
#include <QSettings>
#include <QSize>
#include <QTest>
#include <QWindow>
#include <algorithm>

namespace
{

// The default size the tests give the window: small enough for the offscreen platform's screen.
constexpr int default_width = 600;
constexpr int default_height = 400;
constexpr QSize minimum{400, 300};

QRect available()
{
    return QGuiApplication::primaryScreen()->availableGeometry();
}

// Where a window with no usable stored position is put: centred across, a third of the free height
// down.
QPoint default_position(const QSize& size)
{
    const QRect area = available();
    return {area.x() + std::max(0, (area.width() - size.width()) / 2),
            area.y() + std::max(0, (area.height() - size.height()) / 3)};
}

void store(int x, int y, int width, int height, bool placed = true, bool maximized = false)
{
    QSettings settings;
    settings.setValue(QStringLiteral("window/x"), x);
    settings.setValue(QStringLiteral("window/y"), y);
    settings.setValue(QStringLiteral("window/width"), width);
    settings.setValue(QStringLiteral("window/height"), height);
    settings.setValue(QStringLiteral("window/placed"), placed);
    settings.setValue(QStringLiteral("window/maximized"), maximized);
    settings.sync();
}

void configure(QQuickWindow& window)
{
    window.setMinimumSize(minimum);
    adl::ui::install_window_state(&window, default_width, default_height);
}

} // namespace

class WindowStateTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        // An empty settings directory for every test.
        adl::test::scratch_settings().next();
        QVERIFY(QGuiApplication::primaryScreen() != nullptr);
    }

    void opens_at_the_default_size_and_place_when_nothing_was_stored()
    {
        QQuickWindow window;
        configure(window);

        QCOMPARE(window.size(), QSize(default_width, default_height));
        QCOMPARE(window.position(), default_position(window.size()));
    }

    void a_stored_size_and_position_come_back()
    {
        const QRect area = available();
        const QPoint where{area.x() + 30, area.y() + 20};
        store(where.x(), where.y(), 500, 350);

        QQuickWindow window;
        configure(window);

        QCOMPARE(window.size(), QSize(500, 350));
        QCOMPARE(window.position(), where);
    }

    void a_size_below_the_minimum_is_raised()
    {
        store(available().x() + 10, available().y() + 10, 120, 90);

        QQuickWindow window;
        configure(window);

        QCOMPARE(window.size(), minimum);
    }

    void a_size_larger_than_the_screen_allows_is_brought_inside_it()
    {
        store(0, 0, 100000, 100000);

        QQuickWindow window;
        configure(window);

        // Less 48 pixels across and 80 down for the frame and the task bar, never below the
        // minimum.
        const QRect area = available();
        QCOMPARE(window.width(), std::max(minimum.width(), area.width() - 48));
        QCOMPARE(window.height(), std::max(minimum.height(), area.height() - 80));
    }

    void a_position_on_no_screen_is_ignored()
    {
        store(available().right() + 20000, available().bottom() + 20000, 500, 350);

        QQuickWindow window;
        configure(window);

        // The size is still the stored one; only the place is the default's.
        QCOMPARE(window.size(), QSize(500, 350));
        QCOMPARE(window.position(), default_position(window.size()));
        QVERIFY(available().intersects(QRect(window.position(), window.size())));
    }

    void a_position_that_was_never_saved_is_not_used()
    {
        // A position without the "placed" mark is a leftover, not the operator's choice.
        store(available().x() + 77, available().y() + 55, 500, 350, false);

        QQuickWindow window;
        configure(window);

        QCOMPARE(window.position(), default_position(window.size()));
    }

    void a_maximised_window_opens_maximised()
    {
        store(available().x() + 10, available().y() + 10, 500, 350, true, true);

        // Installing the state shows the window maximised by itself (a show() after it would
        // put it back to its normal state).
        QQuickWindow window;
        configure(window);

        QCOMPARE(window.visibility(), QWindow::Maximized);
    }

    void closing_stores_the_size_and_position()
    {
        QQuickWindow window;
        configure(window);
        window.show();
        const QRect area = available();
        window.setGeometry(area.x() + 40, area.y() + 30, 520, 360);
        QCOMPARE(window.geometry(), QRect(area.x() + 40, area.y() + 30, 520, 360));

        window.close();

        const QSettings settings;
        QCOMPARE(settings.value(QStringLiteral("window/x")).toInt(), area.x() + 40);
        QCOMPARE(settings.value(QStringLiteral("window/y")).toInt(), area.y() + 30);
        QCOMPARE(settings.value(QStringLiteral("window/width")).toInt(), 520);
        QCOMPARE(settings.value(QStringLiteral("window/height")).toInt(), 360);
        QCOMPARE(settings.value(QStringLiteral("window/placed")).toBool(), true);
        QCOMPARE(settings.value(QStringLiteral("window/maximized")).toBool(), false);

        // And the next window opens where this one was.
        QQuickWindow next;
        configure(next);
        QCOMPARE(next.geometry(), QRect(area.x() + 40, area.y() + 30, 520, 360));
    }

    void closing_a_maximised_window_keeps_the_size_it_would_restore_to()
    {
        const QRect area = available();
        store(area.x() + 12, area.y() + 14, 500, 350, true, true);

        QQuickWindow window;
        configure(window);
        QCOMPARE(window.visibility(), QWindow::Maximized);

        window.close();

        const QSettings settings;
        QCOMPARE(settings.value(QStringLiteral("window/maximized")).toBool(), true);
        // The maximised rectangle is not remembered as the window's size.
        QCOMPARE(settings.value(QStringLiteral("window/width")).toInt(), 500);
        QCOMPARE(settings.value(QStringLiteral("window/height")).toInt(), 350);
        QCOMPARE(settings.value(QStringLiteral("window/x")).toInt(), area.x() + 12);
        QCOMPARE(settings.value(QStringLiteral("window/y")).toInt(), area.y() + 14);
    }
};

int main(int argc, char* argv[])
{
    QGuiApplication application(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("adl-tests"));
    QGuiApplication::setApplicationName(QStringLiteral("adl_window_state_tests"));
    adl::test::scratch_settings();
    WindowStateTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "window_state_test.moc"

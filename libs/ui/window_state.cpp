#include "adl/ui/window_state.hpp"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QRect>
#include <QScreen>
#include <QSettings>
#include <QWindow>
#include <algorithm>
#include <bit>
#include <cstdint>
#include <limits>

namespace adl::ui
{

namespace
{

constexpr QLatin1StringView key_x{"window/x"};
constexpr QLatin1StringView key_y{"window/y"};
constexpr QLatin1StringView key_width{"window/width"};
constexpr QLatin1StringView key_height{"window/height"};
constexpr QLatin1StringView key_maximized{"window/maximized"};
constexpr QLatin1StringView key_placed{"window/placed"};

QList<QRect> available_areas()
{
    QList<QRect> areas;
    for (const QScreen* screen : QGuiApplication::screens())
    {
        areas.append(screen->availableGeometry());
    }
    return areas;
}

void save(QQuickWindow* window)
{
    if (window == nullptr)
    {
        return;
    }
    QSettings settings;
    const bool maximized = window->visibility() == QWindow::Maximized;
    settings.setValue(key_maximized, maximized);
    if (!maximized)
    {
        const QRect geometry = window->geometry();
        if (geometry.isValid())
        {
            // Signed ints: a window on a monitor left of the primary one has a negative x. The
            // registry stores it as an unsigned DWORD; stored_coordinate() reads it back signed.
            settings.setValue(key_x, geometry.x());
            settings.setValue(key_y, geometry.y());
            settings.setValue(key_width, geometry.width());
            settings.setValue(key_height, geometry.height());
            settings.setValue(key_placed, true);
        }
    }
    settings.sync();
}

} // namespace

int stored_coordinate(const QVariant& value, int fallback)
{
    bool ok = false;
    const qlonglong wide = value.toLongLong(&ok);
    if (!ok)
    {
        return fallback;
    }
    constexpr qlonglong int_min = std::numeric_limits<std::int32_t>::min();
    constexpr qlonglong int_max = std::numeric_limits<std::int32_t>::max();
    if (wide >= int_min && wide <= int_max)
    {
        return static_cast<int>(wide);
    }
    if (wide >= 0 && wide <= std::numeric_limits<std::uint32_t>::max())
    {
        // An int the registry kept as an unsigned DWORD.
        return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(wide));
    }
    return fallback;
}

bool title_bar_reachable(const QRect& window, const QList<QRect>& screens)
{
    const QRect bar(window.x(), window.y(), window.width(), title_bar_height);
    const int least_width = std::min(title_bar_min_visible_width, bar.width());
    return std::ranges::any_of(screens,
                               [&](const QRect& screen)
                               {
                                   const QRect shown = bar.intersected(screen);
                                   return shown.width() >= least_width &&
                                          shown.height() >= title_bar_min_visible_height;
                               });
}

void install_window_state(QQuickWindow* window, int default_width, int default_height)
{
    if (window == nullptr)
    {
        return;
    }

    QSettings settings;
    const int min_width = std::max(window->minimumWidth(), 1);
    const int min_height = std::max(window->minimumHeight(), 1);

    QScreen* screen = window->screen();
    if (screen == nullptr)
    {
        screen = QGuiApplication::primaryScreen();
    }
    const QRect available =
        screen != nullptr ? screen->availableGeometry() : QRect(0, 0, default_width, default_height);

    int width = stored_coordinate(settings.value(key_width), default_width);
    int height = stored_coordinate(settings.value(key_height), default_height);
    width = std::clamp(width, min_width, std::max(min_width, available.width() - 48));
    height = std::clamp(height, min_height, std::max(min_height, available.height() - 80));

    const int x = stored_coordinate(settings.value(key_x), available.x() + (available.width() - width) / 2);
    const int y = stored_coordinate(settings.value(key_y), available.y() + (available.height() - height) / 3);
    const bool placed = settings.value(key_placed, false).toBool();

    window->resize(width, height);
    if (placed && title_bar_reachable(QRect(x, y, width, height), available_areas()))
    {
        window->setPosition(x, y);
    }
    else
    {
        window->setPosition(available.x() + std::max(0, (available.width() - width) / 2),
                            available.y() + std::max(0, (available.height() - height) / 3));
    }

    if (settings.value(key_maximized, false).toBool())
    {
        window->showMaximized();
    }

    QObject::connect(window, &QQuickWindow::closing, window, [window] { save(window); });
    QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, window, [window] { save(window); });
}

} // namespace adl::ui

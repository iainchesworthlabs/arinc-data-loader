#include "adl/ui/window_state.hpp"

#include <algorithm>

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QRect>
#include <QScreen>
#include <QSettings>
#include <QWindow>

namespace adl::ui {

namespace {

constexpr QLatin1StringView key_x{"window/x"};
constexpr QLatin1StringView key_y{"window/y"};
constexpr QLatin1StringView key_width{"window/width"};
constexpr QLatin1StringView key_height{"window/height"};
constexpr QLatin1StringView key_maximized{"window/maximized"};
constexpr QLatin1StringView key_placed{"window/placed"};

bool intersects_any_screen(const QRect& rect) {
    for (QScreen* screen : QGuiApplication::screens()) {
        if (screen->geometry().intersects(rect)) {
            return true;
        }
    }
    return false;
}

void save(QQuickWindow* window) {
    if (window == nullptr) {
        return;
    }
    QSettings settings;
    const bool maximized = window->visibility() == QWindow::Maximized;
    settings.setValue(key_maximized, maximized);
    if (!maximized) {
        const QRect geometry = window->geometry();
        if (geometry.isValid()) {
            settings.setValue(key_x, geometry.x());
            settings.setValue(key_y, geometry.y());
            settings.setValue(key_width, geometry.width());
            settings.setValue(key_height, geometry.height());
            settings.setValue(key_placed, true);
        }
    }
    settings.sync();
}

}  // namespace

void install_window_state(QQuickWindow* window, int default_width, int default_height) {
    if (window == nullptr) {
        return;
    }

    QSettings settings;
    const int min_width = std::max(window->minimumWidth(), 1);
    const int min_height = std::max(window->minimumHeight(), 1);

    QScreen* screen = window->screen();
    if (screen == nullptr) {
        screen = QGuiApplication::primaryScreen();
    }
    const QRect available = screen != nullptr ? screen->availableGeometry()
                                              : QRect(0, 0, default_width, default_height);

    int width = settings.value(key_width, default_width).toInt();
    int height = settings.value(key_height, default_height).toInt();
    width = std::clamp(width, min_width, std::max(min_width, available.width() - 48));
    height = std::clamp(height, min_height, std::max(min_height, available.height() - 80));

    const int x = settings.value(key_x, available.x() + (available.width() - width) / 2).toInt();
    const int y = settings.value(key_y, available.y() + (available.height() - height) / 3).toInt();
    const bool placed = settings.value(key_placed, false).toBool();

    window->resize(width, height);
    if (placed && intersects_any_screen(QRect(x, y, width, height))) {
        window->setPosition(x, y);
    } else {
        window->setPosition(available.x() + std::max(0, (available.width() - width) / 2),
                            available.y() + std::max(0, (available.height() - height) / 3));
    }

    if (settings.value(key_maximized, false).toBool()) {
        window->showMaximized();
    }

    QObject::connect(window, &QQuickWindow::closing, window, [window] { save(window); });
    QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, window,
                     [window] { save(window); });
}

}  // namespace adl::ui

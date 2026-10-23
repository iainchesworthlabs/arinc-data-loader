// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QRect>
#include <QVariant>

class QQuickWindow;

namespace adl::ui
{

// Restores a window's last size and position from QSettings and saves them when it closes. The
// category is "window" under the application's organisation/name; a remembered position whose
// title bar cannot be reached on any connected screen is ignored, and a size below the window's
// minimum is raised.
void install_window_state(QQuickWindow* window, int default_width = 1280, int default_height = 900);

// The height of the title bar the placement rule asks to be reachable, and the least width of it
// that must lie on a screen. A window with less than this showing cannot be dragged back.
inline constexpr int title_bar_height = 32;
inline constexpr int title_bar_min_visible_width = 96;
inline constexpr int title_bar_min_visible_height = 24;

// A stored coordinate as a signed value. A negative coordinate (a window on a monitor to the left
// of the primary one) is written as an int, but the Windows registry keeps it as an unsigned
// DWORD, so -2 reads back as 4294967294; this wraps such a value back to the int it was. A value
// that is no integer, or that fits neither range, gives `fallback`.
[[nodiscard]] int stored_coordinate(const QVariant& value, int fallback);

// Whether a usable part of the window's title bar lies on one of `screens` (their available
// areas): at least title_bar_min_visible_width wide and title_bar_min_visible_height tall. A window
// that merely overlaps a screen by a pixel, or whose title bar is under the task bar or above the
// top edge, fails.
[[nodiscard]] bool title_bar_reachable(const QRect& window, const QList<QRect>& screens);

} // namespace adl::ui

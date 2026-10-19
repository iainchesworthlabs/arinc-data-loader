#pragma once

class QQuickWindow;

namespace adl::ui {

// Restores a window's last size and position from QSettings and saves them when it closes. The
// category is "window" under the application's organisation/name; a remembered position that is
// on no connected screen is ignored, and a size below the window's minimum is raised.
void install_window_state(QQuickWindow* window, int default_width = 1280, int default_height = 900);

}  // namespace adl::ui

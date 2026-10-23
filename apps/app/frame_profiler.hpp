// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QElapsedTimer>
#include <QObject>

class QQuickWindow;

namespace adl::app
{

// Turns the window's frame loop into trace data: one frame mark per swap, and plots of the frame
// interval and of the time the render thread spent drawing the frame. Tracy's GPU zones would need
// the graphics API's own instrumentation (which Qt's RHI does not expose), so the 2D scene graph
// is timed from the window's own signals; this window draws no 3D scene.
//
// Must be destroyed after the window: the connections belong to the window, and a queued render
// signal must never find a half-destroyed profiler. main.cpp holds it in an optional and resets
// the engine first.
class FrameProfiler : public QObject
{
    Q_OBJECT

public:
    explicit FrameProfiler(QQuickWindow* window);

private:
    void on_frame_swapped();
    void on_before_rendering();
    void on_after_rendering();

    QElapsedTimer frame_clock_;
    QElapsedTimer render_clock_;
};

} // namespace adl::app

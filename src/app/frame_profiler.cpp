#include "frame_profiler.hpp"

#include <QQuickWindow>

#include <adl/profiling.hpp>

namespace adl::app {

FrameProfiler::FrameProfiler(QQuickWindow* window) {
    if (window == nullptr) {
        return;
    }
    frame_clock_.start();
    render_clock_.start();

    // The frame mark and the frame interval are the GUI thread's: Qt emits frameSwapped there.
    connect(window, &QQuickWindow::frameSwapped, this, [this] { on_frame_swapped(); });

    // Rendering is the render thread's, and a queued delivery would measure the wait in the GUI
    // thread's event loop instead of the drawing: these two are direct on purpose.
    connect(
        window, &QQuickWindow::beforeRendering, this, [this] { on_before_rendering(); },
        Qt::DirectConnection);
    connect(
        window, &QQuickWindow::afterRendering, this, [this] { on_after_rendering(); },
        Qt::DirectConnection);
}

void FrameProfiler::on_frame_swapped() {
    adl::profiling::frame_mark();
    // [[maybe_unused]]: with profiling off, the plots below expand to nothing and only the clock
    // restarts remain.
    const double milliseconds [[maybe_unused]] =
        static_cast<double>(frame_clock_.nsecsElapsed()) / 1'000'000.0;
    frame_clock_.restart();
    adl::profiling::plot("ui.frame_ms", milliseconds);
    if (milliseconds > 0.0) {
        adl::profiling::plot("ui.fps", 1000.0 / milliseconds);
    }
}

void FrameProfiler::on_before_rendering() {
    render_clock_.restart();
}

void FrameProfiler::on_after_rendering() {
    const double milliseconds [[maybe_unused]] =
        static_cast<double>(render_clock_.nsecsElapsed()) / 1'000'000.0;
    adl::profiling::plot("ui.render_ms", milliseconds);
}

}  // namespace adl::app

// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/ui/smoke_test.hpp"

#include <QCoreApplication>
#include <QObject>
#include <QQuickWindow>
#include <QString>
#include <cstdio>

namespace adl::ui
{

bool smoke_test_requested()
{
    return QCoreApplication::arguments().contains(QString::fromLatin1(smoke_test_flag));
}

void install_smoke_test(QQuickWindow* window)
{
    if (window == nullptr || !smoke_test_requested())
    {
        return;
    }
    // Queued, so the frame that was just swapped is finished before the event loop is left.
    QObject::connect(
        window, &QQuickWindow::frameSwapped, window,
        []
        {
            std::puts("smoke-test: first frame drawn");
            std::fflush(stdout);
            QCoreApplication::exit(0);
        },
        Qt::QueuedConnection);
}

} // namespace adl::ui

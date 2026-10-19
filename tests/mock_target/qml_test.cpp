// Loads the mock target's window (argv[1], the path of TargetWindow.qml) with the real model
// offscreen, starts and stops the target through the window's own buttons, and fails if the
// window does not load, the buttons do not work or the QML engine has a single complaint. With
// a second argument it saves what the window looks like there. The platform's graphics are not
// used: the test's environment asks for the offscreen platform and the software renderer.

#include <cstdio>
#include <expected>
#include <functional>
#include <memory>

#include <QEventLoop>
#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QUrl>
#include <QVariant>

#include "adl/mock/model.hpp"
#include "adl/ui/fonts.hpp"
#include "fake_link.hpp"

namespace {

// The item called `name` in the visual tree below `root` (a delegate's items are not always
// children of anything a QObject search would find).
QQuickItem* find_item(QQuickItem* root, const QString& name) {
    if (root->objectName() == name) {
        return root;
    }
    for (QQuickItem* child : root->childItems()) {
        if (QQuickItem* found = find_item(child, name)) {
            return found;
        }
    }
    return nullptr;
}

// Lets the window and the model's signals run until the job is done (or 20 seconds pass).
bool wait_until(const std::function<bool()>& done) {
    QEventLoop loop;
    QTimer poll;
    QTimer limit;
    limit.setSingleShot(true);
    bool timed_out = false;
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
        if (done()) {
            loop.quit();
        }
    });
    QObject::connect(&limit, &QTimer::timeout, &loop, [&] {
        timed_out = true;
        loop.quit();
    });
    poll.start(10);
    limit.start(20000);
    loop.exec();
    return !timed_out;
}

}  // namespace

int main(int argc, char* argv[]) {
    QGuiApplication application(argc, argv);
    if (argc > 2) {
        std::fprintf(stderr, "usage: adl_mock_qml_test [picture.png]\n");
        return 2;
    }
    adl::ui::register_fonts();
    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    // The model over the fake link, so nothing is sent to a real network.
    adl::fake::FakeWorld world(2);
    adl::fake::FakeNetwork network(world);
    adl::mock::MockTargetModel model(
        [&world, &network]() -> std::expected<std::unique_ptr<adl::mock::TargetLink>, QString> {
            return std::unique_ptr<adl::mock::TargetLink>(
                std::make_unique<adl::test::FakeTargetLink>(
                    world, network, adl::platform::Ipv4Address{{10, 0, 0, 2}}));
        });

    QQmlApplicationEngine engine;
    int complaints = 0;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings,
                     [&complaints](const QList<QQmlError>& warnings) {
                         for (const QQmlError& warning : warnings) {
                             std::fprintf(stderr, "QML: %s\n", qPrintable(warning.toString()));
                             ++complaints;
                         }
                     });
    engine.setInitialProperties(
        {{QStringLiteral("app"), QVariant::fromValue(&model)},
         // ADL_DARK=1 renders in the dark scheme (the window sets a dark palette, which is what
         // a dark desktop theme supplies), so both schemes can be checked and pictured.
         {QStringLiteral("renderDark"),
          QVariant::fromValue(qEnvironmentVariable("ADL_DARK") == QStringLiteral("1"))}});
    engine.loadFromModule("ArincMockTarget", "TargetWindow");

    if (engine.rootObjects().size() != 1) {
        std::fprintf(stderr, "the window did not load\n");
        return 1;
    }
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
    if (window == nullptr || window->title() != QStringLiteral("ARINC Mock Target")) {
        std::fprintf(stderr, "the root object is not the mock target window\n");
        return 1;
    }

    QQuickItem* content = window->contentItem();
    QQuickItem* start = find_item(content, QStringLiteral("startButton"));
    QQuickItem* stop = find_item(content, QStringLiteral("stopButton"));
    QQuickItem* hardware = find_item(content, QStringLiteral("hardwareIdField"));
    if (start == nullptr || stop == nullptr || hardware == nullptr) {
        std::fprintf(stderr,
                     "the Start, Stop or hardware identifier controls are not in the window\n");
        return 1;
    }

    // What the operator does: start the target with the window's own button, watch the log
    // fill, and stop it again with the window's own button.
    QMetaObject::invokeMethod(start, "click");
    if (!model.running()) {
        std::fprintf(stderr, "the window's Start button did not start the target\n");
        return 1;
    }
    auto* activity = qobject_cast<adl::mock::ActivityModel*>(model.activity());
    if (!wait_until([&] { return activity->rowCount() > 0; })) {
        std::fprintf(stderr, "the activity log stayed empty\n");
        return 1;
    }
    QCoreApplication::processEvents();
    QMetaObject::invokeMethod(stop, "click");
    if (model.running()) {
        std::fprintf(stderr, "the window's Stop button did not stop the target\n");
        return 1;
    }
    QCoreApplication::processEvents();

    const QImage picture = window->grabWindow();
    if (picture.isNull()) {
        std::fprintf(stderr, "the window could not be drawn\n");
        return 1;
    }
    if (argc > 1 && !picture.save(QString::fromLocal8Bit(argv[1]))) {
        std::fprintf(stderr, "the picture could not be saved\n");
        return 1;
    }

    if (complaints != 0) {
        std::fprintf(stderr, "%d QML complaint(s)\n", complaints);
        return 1;
    }
    return 0;
}

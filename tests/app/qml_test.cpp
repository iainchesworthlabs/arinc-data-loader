// Loads the application's window (argv[1], the path of Main.qml) with the real model, offscreen,
// runs a FIND and an Information operation through the model against a simulated target on the
// fake platform while the window is showing, then reads a media set, ticks its load and starts the
// upload through the window's own check box and button, and fails if the window does not load,
// the operations do not end as they should or the QML engine has a single complaint. With a second
// argument it saves what the window looks like there. The platform's graphics are not used: the
// test's environment asks for the offscreen platform and the software renderer.

#include <cstdio>
#include <expected>

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

#include "adl/app/loader_model.hpp"
#include "rig.hpp"
#include "upload_fixture.hpp"

namespace {

// Lets the window and the model's signals run until the job is done (or 20 seconds pass).
bool wait_until_idle(adl::app::LoaderModel& model) {
    QEventLoop loop;
    QTimer poll;
    QTimer limit;
    limit.setSingleShot(true);
    bool timed_out = false;
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] {
        if (!model.busy()) {
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

}  // namespace

int main(int argc, char* argv[]) {
    QGuiApplication application(argc, argv);
    if (argc < 2) {
        std::fprintf(stderr, "usage: adl_app_qml_test <path to Main.qml> [picture.png]\n");
        return 2;
    }
    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    adl::test::UploadFixture fixture;
    adl::test::SimPlan plan = fixture.plan();
    plan.find_answer = adl::arinc615a::FindAnswer{"HNPFMS", "FMS", "L", "FMS LEFT", "HNP"};
    adl::test::Rig rig(plan);
    if (!fixture.write(rig)) {
        std::fprintf(stderr, "the media set could not be written\n");
        return 1;
    }
    adl::app::LoaderModel model([&rig]() -> std::expected<adl::app::HostLease, QString> {
        adl::app::HostLease lease;
        lease.host = &rig.loader_host;
        return lease;
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
    engine.setInitialProperties({{QStringLiteral("app"), QVariant::fromValue(&model)}});
    engine.load(QUrl::fromLocalFile(QString::fromLocal8Bit(argv[1])));

    if (engine.rootObjects().size() != 1) {
        std::fprintf(stderr, "the window did not load\n");
        return 1;
    }
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst());
    if (window == nullptr || window->title() != QStringLiteral("ARINC Data Loader")) {
        std::fprintf(stderr, "the root object is not the application window\n");
        return 1;
    }

    // What the operator does: find the target, take it from the list, read its configuration.
    model.findTargets();
    if (!wait_until_idle(model)) {
        std::fprintf(stderr, "FIND did not end\n");
        return 1;
    }
    model.selectTarget(0);
    model.readInformation();
    if (!wait_until_idle(model)) {
        std::fprintf(stderr, "the Information operation did not end\n");
        return 1;
    }
    QCoreApplication::processEvents();

    if (model.resultState() != 1 || model.thwIdPos() != QStringLiteral("HNPFMS_L")) {
        std::fprintf(stderr, "the operation did not complete: %s\n",
                     qPrintable(model.resultText()));
        return 1;
    }

    // Then a media set, its load ticked in the window, and the upload started from its button.
    // (The simulated target does one operation at a time: it starts again from nothing.)
    rig.target.reset_operation();
    model.chooseMediaSet(fixture.url());
    if (!wait_until_idle(model) || !model.mediaSetError().isEmpty()) {
        std::fprintf(stderr, "the media set was not read: %s\n", qPrintable(model.mediaSetError()));
        return 1;
    }
    QCoreApplication::processEvents();
    QQuickItem* content = window->contentItem();
    QQuickItem* check = find_item(content, QStringLiteral("loadCheck"));
    QQuickItem* upload = find_item(content, QStringLiteral("uploadButton"));
    if (check == nullptr || upload == nullptr) {
        std::fprintf(stderr, "the load's check box or the Upload button is not in the window\n");
        return 1;
    }
    if (upload->isEnabled() || model.canUpload()) {
        std::fprintf(stderr, "Upload is enabled with no load ticked\n");
        return 1;
    }
    QMetaObject::invokeMethod(check, "click");
    QCoreApplication::processEvents();
    if (model.selectedLoadCount() != 1 || !upload->isEnabled()) {
        std::fprintf(stderr, "ticking the load in the window did not enable Upload\n");
        return 1;
    }
    QMetaObject::invokeMethod(upload, "click");
    if (!model.busy() || !wait_until_idle(model)) {
        std::fprintf(stderr, "the upload did not start or did not end\n");
        return 1;
    }
    QCoreApplication::processEvents();
    if (model.resultState() != 1 || model.progress() != 100 ||
        rig.target.log().files_read.size() != fixture.load.item.reads.size()) {
        std::fprintf(stderr, "the upload did not complete: %s\n", qPrintable(model.resultText()));
        return 1;
    }

    const QImage picture = window->grabWindow();
    if (picture.isNull()) {
        std::fprintf(stderr, "the window could not be drawn\n");
        return 1;
    }
    if (argc > 2 && !picture.save(QString::fromLocal8Bit(argv[2]))) {
        std::fprintf(stderr, "the picture could not be saved\n");
        return 1;
    }

    if (complaints != 0) {
        std::fprintf(stderr, "%d QML complaint(s)\n", complaints);
        return 1;
    }
    return 0;
}

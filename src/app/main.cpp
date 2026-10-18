#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QVariant>

#include "adl/app/loader_model.hpp"

int main(int argc, char* argv[]) {
    QGuiApplication application(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("ARINC Data Loader"));
    QGuiApplication::setOrganizationName(QStringLiteral("iainchesworthlabs"));

    // One look on every desktop, drawn by Qt Quick itself (the style can still be overridden with
    // QT_QUICK_CONTROLS_STYLE).
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(QStringLiteral("Fusion"));
    }

    // Declared before the engine, so the engine's thread is stopped after the window has gone.
    adl::app::LoaderModel model;

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &application,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.setInitialProperties({{QStringLiteral("app"), QVariant::fromValue(&model)}});
    engine.loadFromModule("ArincDataLoader", "Main");

    return QGuiApplication::exec();
}

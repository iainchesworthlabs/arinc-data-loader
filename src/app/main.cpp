#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QVariant>

#include <optional>

#include "adl/app/loader_model.hpp"
#include "language_manager.hpp"

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

    // Held in an optional so it can be destroyed explicitly, before the language manager: the
    // window's bindings read that manager, and a binding still evaluated while the QML tree is
    // being torn down would see it as null. The tree goes first, the manager after it.
    std::optional<QQmlApplicationEngine> engine{std::in_place};

    // Declared after the engine (it holds a reference to it) and destroyed after it has been
    // reset at the end of main.
    adl::app::LanguageManager language_manager(application, *engine);
    language_manager.applyInitialLanguage();
    engine->rootContext()->setContextProperty(QStringLiteral("languageManager"), &language_manager);

    QObject::connect(
        &*engine, &QQmlApplicationEngine::objectCreationFailed, &application,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine->setInitialProperties({{QStringLiteral("app"), QVariant::fromValue(&model)}});
    engine->loadFromModule("ArincDataLoader", "Main");

    const int exit_code = QGuiApplication::exec();

    // Explicitly torn down here, while the language manager (destroyed below, by the ordinary
    // end-of-scope unwind) is still there for any binding evaluated during the teardown.
    engine.reset();

    return exit_code;
}

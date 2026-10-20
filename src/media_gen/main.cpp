#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QVariant>

#include "adl/gen/generator_model.hpp"
#include "adl/ui/fonts.hpp"
#include "adl/ui/settings_location.hpp"
#include "adl/ui/smoke_test.hpp"
#include "adl/ui/version.hpp"
#include "adl/ui/window_state.hpp"

int main(int argc, char* argv[]) {
    QGuiApplication application(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("ARINC Media Set Generator"));
    QGuiApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QGuiApplication::setOrganizationName(QStringLiteral("iainchesworthlabs"));
    // ADL_SETTINGS_DIR moves the preferences into an ini file there (the tests set it).
    adl::ui::redirect_settings_from_environment();

    adl::ui::register_fonts();

    // One look on every desktop, drawn by Qt Quick itself (the style can still be overridden with
    // QT_QUICK_CONTROLS_STYLE).
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(QStringLiteral("Fusion"));
    }

    // Declared before the engine, so the engine's thread is stopped after the window has gone.
    adl::gen::GeneratorModel model;

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &application,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.setInitialProperties({{QStringLiteral("app"), QVariant::fromValue(&model)}});
    adl::ui::set_about_properties(*engine.rootContext(),
                                  QStringLiteral("ARINC Media Set Generator"));
    engine.loadFromModule("ArincMediaGen", "GeneratorWindow");
    if (!engine.rootObjects().isEmpty()) {
        if (auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst())) {
            adl::ui::install_window_state(window);
            adl::ui::install_smoke_test(window);
        }
    }

    return QGuiApplication::exec();
}

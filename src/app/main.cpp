#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QVariant>

#include <optional>

#include <adl/profiling.hpp>

#include "adl/app/loader_model.hpp"
#include "adl/ui/fonts.hpp"
#include "adl/ui/settings_location.hpp"
#include "adl/ui/smoke_test.hpp"
#include "adl/ui/version.hpp"
#include "adl/ui/window_state.hpp"
#include "frame_profiler.hpp"
#include "language_manager.hpp"

int main(int argc, char* argv[]) {
    QGuiApplication application(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("ARINC Data Loader"));
    QGuiApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QGuiApplication::setOrganizationName(QStringLiteral("iainchesworthlabs"));
    // ADL_SETTINGS_DIR moves the preferences into an ini file there (the tests set it).
    adl::ui::redirect_settings_from_environment();

    adl::ui::register_fonts();

    adl::profiling::thread_name("gui");
    adl::profiling::app_info("ARINC Data Loader");

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
    adl::ui::set_about_properties(*engine->rootContext(), QStringLiteral("ARINC Data Loader"));

    QObject::connect(
        &*engine, &QQmlApplicationEngine::objectCreationFailed, &application,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine->setInitialProperties({{QStringLiteral("app"), QVariant::fromValue(&model)}});
    {
        // The QML is compiled and instantiated inside this zone: the window's start-up cost.
        const adl::profiling::Zone zone{"app: load QML"};
        engine->loadFromModule("ArincDataLoader", "Main");
    }

    // The window's frame loop, for the trace: frame marks, the frame interval and the render
    // thread's time. Destroyed after the engine below, once no signal can arrive.
    std::optional<adl::app::FrameProfiler> frame_profiler;
    if (!engine->rootObjects().isEmpty()) {
        if (auto* window = qobject_cast<QQuickWindow*>(engine->rootObjects().constFirst())) {
            frame_profiler.emplace(window);
            adl::ui::install_window_state(window);
            adl::ui::install_smoke_test(window);
        }
    }

    const int exit_code = QGuiApplication::exec();

    // Explicitly torn down here, while the language manager (destroyed below, by the ordinary
    // end-of-scope unwind) is still there for any binding evaluated during the teardown. The
    // window goes first, the profiler that listened to it after.
    engine.reset();
    frame_profiler.reset();

    return exit_code;
}

#include "adl/mock/model.hpp"
#include "adl/ui/build_info.hpp"
#include "adl/ui/fonts.hpp"
#include "adl/ui/language_manager.hpp"
#include "adl/ui/settings_location.hpp"
#include "adl/ui/smoke_test.hpp"
#include "adl/ui/version.hpp"
#include "adl/ui/window_state.hpp"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QVariant>
#include <optional>

int main(int argc, char* argv[])
{
    QGuiApplication application(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("ARINC Mock Target"));
    QGuiApplication::setApplicationVersion(QString::fromLatin1(adl::ui::kVersion));
    QGuiApplication::setOrganizationName(QStringLiteral("iainchesworthlabs"));
    // ADL_SETTINGS_DIR moves the preferences into an ini file there (the tests set it).
    adl::ui::redirect_settings_from_environment();

    adl::ui::register_fonts();

    // One look on every desktop, drawn by Qt Quick itself (the style can still be overridden
    // with QT_QUICK_CONTROLS_STYLE).
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE"))
    {
        QQuickStyle::setStyle(QStringLiteral("Fusion"));
    }

    // Declared before the engine, so the engine's thread is stopped after the window has gone.
    adl::mock::MockTargetModel model;

    // Held in an optional so it can be destroyed explicitly, before the language manager: the
    // window's bindings read that manager, and a binding still evaluated while the QML tree is
    // being torn down would see it as null.
    std::optional<QQmlApplicationEngine> engine{std::in_place};

    // Declared after the engine (it holds a reference to it) and destroyed after it has been
    // reset below. The language is installed before the QML is loaded, so the first frame is
    // already in the right language and direction.
    adl::ui::LanguageManager language_manager(application, *engine, QStringLiteral("adl_mock_target"));
    language_manager.applyInitialLanguage();
    // What the model holds as phrases (results, failures, log lines) follows a language switch.
    QObject::connect(&language_manager, &adl::ui::LanguageManager::currentLanguageChanged, &model,
                     &adl::mock::MockTargetModel::retranslate);
    engine->rootContext()->setContextProperty(QStringLiteral("languageManager"), &language_manager);

    QObject::connect(
        &*engine, &QQmlApplicationEngine::objectCreationFailed, &application, [] { QCoreApplication::exit(1); },
        Qt::QueuedConnection);
    engine->setInitialProperties({{QStringLiteral("app"), QVariant::fromValue(&model)}});
    adl::ui::set_about_properties(*engine->rootContext(), QStringLiteral("ARINC Mock Target"));
    engine->loadFromModule("ArincMockTarget", "TargetWindow");
    if (!engine->rootObjects().isEmpty())
    {
        if (auto* window = qobject_cast<QQuickWindow*>(engine->rootObjects().constFirst()))
        {
            adl::ui::install_window_state(window);
            adl::ui::install_smoke_test(window);
        }
    }

    const int exit_code = QGuiApplication::exec();
    engine.reset();
    return exit_code;
}

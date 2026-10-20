#pragma once

// The setup of the Qt Quick Test run: one Harness (the application's real model, the simulated
// target and a media set) for every test file, handed to QML as the context properties the tests
// drive. Qt Quick Test creates the QGuiApplication, then calls applicationAvailable(), and then
// qmlEngineAvailable() for the engine the test files are loaded into.

#include <memory>

#include <QGuiApplication>
#include <QObject>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QUrl>

#include "adl/ui/fonts.hpp"
#include "adl/ui/version.hpp"
#include "ui_harness.hpp"

namespace adl::ui_test {

class UiSetup : public QObject {
    Q_OBJECT

public:
    UiSetup() = default;
    ~UiSetup() override = default;

public Q_SLOTS:
    void applicationAvailable() {
        QQuickStyle::setStyle(QStringLiteral("Fusion"));
        adl::ui::register_fonts();
        // The window's error MessageDialog is a native one on Windows, and a native modal dialog
        // spins its own event loop, which no Qt Quick Test can drive. The offscreen platform (and
        // this attribute) keep it a Qt Quick dialog, so the suite can click its button.
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
        harness_ = std::make_unique<Harness>();
    }

    void qmlEngineAvailable(QQmlEngine* engine) {
        engine->rootContext()->setContextProperty(QStringLiteral("appModel"), &harness_->model());
        engine->rootContext()->setContextProperty(QStringLiteral("harness"), harness_.get());
        adl::ui::set_about_properties(*engine->rootContext(), QStringLiteral("ARINC Data Loader"));
        engine->rootContext()->setContextProperty(
            QStringLiteral("mainQmlUrl"), QUrl::fromLocalFile(QString::fromUtf8(ADL_MAIN_QML)));
    }

    // After every test file has run and its window has gone: stop the engine's thread before the
    // application is torn down.
    void cleanupTestCase() { harness_.reset(); }

private:
    std::unique_ptr<Harness> harness_;
};

}  // namespace adl::ui_test

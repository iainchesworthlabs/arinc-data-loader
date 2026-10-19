#pragma once

#include <QGuiApplication>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QTranslator>
#include <QVariantList>

namespace adl::app {

// Owns the application's translators, the persisted language preference, and what a language
// switch touches: the installed translators and the layout direction (right to left for the
// scripts that read that way), then QQmlEngine::retranslate() so the running window follows.
//
// `app` and `engine` must outlive this object. Declare it after the engine in main.cpp, so the
// QML tree is gone before the manager is (a binding still reading it while the engine tears down
// would see a null).
class LanguageManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentLanguage READ currentLanguage NOTIFY currentLanguageChanged)

public:
    LanguageManager(QGuiApplication& app, QQmlEngine& engine, QObject* parent = nullptr);

    // Installs the translators for the initial language - the persisted preference, falling back
    // to the system locale and then to "en" - and sets the layout direction. Called once from
    // main(), before the QML is loaded, so the first frame is already in the right language.
    void applyInitialLanguage();

    [[nodiscard]] QString currentLanguage() const { return current_language_; }

    // One { "code": QString, "name": QString } entry per language a catalogue exists for, each
    // language's own name so it can be found whatever language the window currently shows.
    Q_INVOKABLE QVariantList availableLanguages() const;

    // Persists `code`, swaps the translators, sets the layout direction and retranslates the
    // running QML. False (and no change) for a code that is not in availableLanguages().
    Q_INVOKABLE bool setLanguage(const QString& code);

Q_SIGNALS:
    void currentLanguageChanged();

private:
    void installTranslators(const QString& code);

    QGuiApplication& app_;
    QQmlEngine& engine_;
    QTranslator qt_translator_;
    QTranslator app_translator_;
    QString current_language_ = QStringLiteral("en");
};

}  // namespace adl::app

#pragma once

#include <QGuiApplication>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QTranslator>
#include <QVariantList>

namespace adl::ui {

// Owns an application's translators, the persisted language preference, and what a language
// switch touches: the installed translators, the layout direction (right to left for the
// scripts that read that way) and the typeface (the Hebrew script needs its own), then
// QQmlEngine::retranslate() so the running window follows. The
// three applications share it, so one picker in one place behaves the same in each.
//
// Three catalogues are installed for a language: Qt's own (the standard dialogs), the shared
// interface's (`adl_ui`, the components every window draws) and the application's own
// (`catalogue`, for example "adl_app"). English has no ordinary catalogue - the source strings
// are English - but its plural-only catalogues are installed like any other, because Qt takes the
// form a count asks for from a catalogue and the source holds only one.
//
// `app` and `engine` must outlive this object. Declare it after the engine in main.cpp, so the
// QML tree is gone before the manager is (a binding still reading it while the engine tears down
// would see a null).
class LanguageManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentLanguage READ currentLanguage NOTIFY currentLanguageChanged)
    Q_PROPERTY(bool rightToLeft READ rightToLeft NOTIFY currentLanguageChanged)

public:
    LanguageManager(QGuiApplication& app, QQmlEngine& engine, QString catalogue,
                    QObject* parent = nullptr);

    // Installs the translators for the initial language - the persisted preference, falling back
    // to the system locale and then to "en" - and sets the layout direction. Called once from
    // main(), before the QML is loaded, so the first frame is already in the right language.
    void applyInitialLanguage();

    [[nodiscard]] QString currentLanguage() const { return current_language_; }

    // Whether the current language reads right to left: Hebrew and Yiddish.
    [[nodiscard]] bool rightToLeft() const;

    // One { "code": QString, "name": QString } entry per language a catalogue exists for, each
    // language's own name so it can be found whatever language the window currently shows.
    Q_INVOKABLE QVariantList availableLanguages() const;

    // Persists `code`, swaps the translators, sets the layout direction and retranslates the
    // running QML. False (and no change) for a code that is not in availableLanguages().
    Q_INVOKABLE bool setLanguage(const QString& code);

Q_SIGNALS:
    // After the translators and the layout direction have changed and the QML has been asked to
    // retranslate: C++ that holds text it translated earlier (a model's result, a log line)
    // connects here to translate it again.
    void currentLanguageChanged();

private:
    void installTranslators(const QString& code);
    // The application's typeface for the language: Noto Sans Hebrew for Hebrew and Yiddish, which
    // Archivo cannot draw.
    void applyFont(const QString& code);

    QGuiApplication& app_;
    QQmlEngine& engine_;
    QString catalogue_;
    QTranslator qt_translator_;
    QTranslator ui_translator_;
    QTranslator app_translator_;
    QString current_language_ = QStringLiteral("en");
};

}  // namespace adl::ui

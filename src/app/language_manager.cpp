#include "language_manager.hpp"

#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QVariantMap>

#include <array>

namespace adl::app {
namespace {

struct LanguageInfo {
    const char* code;
    const char* nativeName;
};

// "en" first (no .qm - the source strings are already English), then every language this
// application has a catalogue for. A language is added by putting its .ts beside the others,
// adding it to the TS_FILES list in src/app/CMakeLists.txt and adding its line here. The
// translation text itself is a first, mechanical draft, to be reviewed by a speaker.
constexpr std::array<LanguageInfo, 8> kLanguages{{
    {"en", "English"},
    {"fr", "Français"},
    {"es", "Español"},
    {"de", "Deutsch"},
    {"yi", "ייִדיש"},
    {"he", "עברית"},
    {"zh", "中文"},
    {"ja", "日本語"},
}};

constexpr auto kSettingsKey = "language/code";

[[nodiscard]] bool is_supported(const QString& code) {
    for (const LanguageInfo& info : kLanguages) {
        if (code == QLatin1String(info.code)) {
            return true;
        }
    }
    return false;
}

// Maps a system locale to one of kLanguages, or "en" when there is no catalogue for it.
[[nodiscard]] QString language_for_locale(const QLocale& locale) {
    const QString name = QLocale::languageToCode(locale.language());
    return is_supported(name) ? name : QStringLiteral("en");
}

}  // namespace

LanguageManager::LanguageManager(QGuiApplication& app, QQmlEngine& engine, QObject* parent)
    : QObject(parent), app_(app), engine_(engine) {}

void LanguageManager::applyInitialLanguage() {
    const QSettings settings;
    const QString saved = settings.value(QLatin1String(kSettingsKey)).toString();
    const QString initial = is_supported(saved) ? saved : language_for_locale(QLocale::system());

    installTranslators(initial);
    app_.setLayoutDirection(QLocale(initial).textDirection());
    current_language_ = initial;
}

QVariantList LanguageManager::availableLanguages() const {
    QVariantList result;
    result.reserve(static_cast<qsizetype>(kLanguages.size()));
    for (const LanguageInfo& info : kLanguages) {
        QVariantMap entry;
        entry["code"] = QString::fromLatin1(info.code);
        entry["name"] = QString::fromUtf8(info.nativeName);
        result << entry;
    }
    return result;
}

bool LanguageManager::setLanguage(const QString& code) {
    if (!is_supported(code)) {
        return false;
    }
    if (code == current_language_) {
        return true;
    }

    installTranslators(code);
    app_.setLayoutDirection(QLocale(code).textDirection());
    // Before retranslate(): the bindings it re-evaluates may read currentLanguage() and must
    // see the language being switched to.
    current_language_ = code;
    engine_.retranslate();

    QSettings settings;
    settings.setValue(QLatin1String(kSettingsKey), code);

    Q_EMIT currentLanguageChanged();
    return true;
}

void LanguageManager::installTranslators(const QString& code) {
    // Each translator's previous language is replaced in place, so a failed load below leaves
    // the old translator removed rather than half installed.
    app_.removeTranslator(&qt_translator_);
    app_.removeTranslator(&app_translator_);

    if (code == QLatin1String("en")) {
        return;  // no translator: the source strings are English
    }

    // Qt 6 keeps its own catalogue in one qt_<code>.qm per language.
    if (qt_translator_.load(QLocale(code), QStringLiteral("qt"), QStringLiteral("_"),
                            QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        app_.installTranslator(&qt_translator_);
    }

    if (app_translator_.load(QLocale(code), QStringLiteral("adl_app"), QStringLiteral("_"),
                             QStringLiteral(":/i18n"))) {
        app_.installTranslator(&app_translator_);
    }
}

}  // namespace adl::app

#include "adl/ui/language_manager.hpp"

#include <QFont>
#include <QFontDatabase>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QVariantMap>

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>

using namespace Qt::StringLiterals;

namespace adl::ui {
namespace {

struct LanguageInfo {
    std::string_view code;
    std::string_view native_name;
    // The typeface the window is set in. Archivo, the design's face, has no Hebrew letters; the
    // Chinese and Japanese windows keep it for the Latin letters and take their characters from
    // the system's CJK fonts.
    std::string_view family;
};

// "en" first (only its plural catalogues: the source strings are already English), then every
// language the applications have catalogues for. A language is added by putting its .ts beside the
// others in each application's translations directory (and src/ui/translations), adding it to
// their TS_FILES lists in CMake and adding its line here. The translation text itself is a first,
// machine-drafted version, to be reviewed by a speaker of the language.
constexpr std::string_view kLatinFamily = "Archivo";
constexpr std::string_view kHebrewFamily = "Noto Sans Hebrew";

constexpr std::array<LanguageInfo, 8> kLanguages{{
    {"en", "English", kLatinFamily},
    {"fr", "Français", kLatinFamily},
    {"es", "Español", kLatinFamily},
    {"de", "Deutsch", kLatinFamily},
    {"yi", "ייִדיש", kHebrewFamily},
    {"he", "עברית", kHebrewFamily},
    {"zh", "中文", kLatinFamily},
    {"ja", "日本語", kLatinFamily},
}};

constexpr QLatin1StringView kSettingsKey{"language/code"};

[[nodiscard]] QLatin1StringView latin1(std::string_view text) {
    return QLatin1StringView{text.data(), static_cast<qsizetype>(text.size())};
}

[[nodiscard]] bool is_supported(const QString& code) {
    return std::ranges::any_of(
        kLanguages, [&code](const LanguageInfo& info) { return code == latin1(info.code); });
}

// Maps a system locale to one of kLanguages, or "en" when there is no catalogue for it.
[[nodiscard]] QString language_for_locale(const QLocale& locale) {
    const QString name = QLocale::languageToCode(locale.language());
    return is_supported(name) ? name : QStringLiteral("en");
}

}  // namespace

LanguageManager::LanguageManager(QGuiApplication& app, QQmlEngine& engine, QString catalogue,
                                 QObject* parent)
    : QObject(parent), app_(app), engine_(engine), catalogue_(std::move(catalogue)) {}

void LanguageManager::applyInitialLanguage() {
    const QSettings settings;
    const QString saved = settings.value(kSettingsKey).toString();
    const QString initial = is_supported(saved) ? saved : language_for_locale(QLocale::system());

    installTranslators(initial);
    applyFont(initial);
    app_.setLayoutDirection(QLocale(initial).textDirection());
    current_language_ = initial;
}

bool LanguageManager::rightToLeft() const {
    return QLocale(current_language_).textDirection() == Qt::RightToLeft;
}

QVariantList LanguageManager::availableLanguages() const {
    QVariantList result;
    result.reserve(static_cast<qsizetype>(kLanguages.size()));
    for (const LanguageInfo& info : kLanguages) {
        QVariantMap entry;
        entry["code"] = QString{latin1(info.code)};
        entry["name"] = QString::fromUtf8(info.native_name.data(),
                                          static_cast<qsizetype>(info.native_name.size()));
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
    applyFont(code);
    app_.setLayoutDirection(QLocale(code).textDirection());
    // Before retranslate(): the bindings it re-evaluates may read currentLanguage() and must
    // see the language being switched to.
    current_language_ = code;
    engine_.retranslate();

    QSettings settings;
    settings.setValue(kSettingsKey, code);

    Q_EMIT currentLanguageChanged();
    return true;
}

void LanguageManager::applyFont(const QString& code) {
    const auto language = std::ranges::find_if(
        kLanguages, [&code](const LanguageInfo& info) { return code == latin1(info.code); });
    if (language == kLanguages.end()) {
        return;
    }
    // Only a face that is registered: a build or a test that has not registered the bundled fonts
    // keeps the font it has.
    const QString family =
        QString::fromUtf8(language->family.data(), static_cast<qsizetype>(language->family.size()));
    if (!QFontDatabase::families().contains(family)) {
        return;
    }
    QFont font = QGuiApplication::font();
    if (font.family() != family) {
        font.setFamily(family);
        QGuiApplication::setFont(font);
    }
}

void LanguageManager::installTranslators(const QString& code) {
    // Each translator's previous language is replaced in place, so a failed load below leaves
    // the old translator removed rather than half installed.
    app_.removeTranslator(&qt_translator_);
    app_.removeTranslator(&ui_translator_);
    app_.removeTranslator(&app_translator_);

    const QLocale locale(code);
    const QString catalogue_directory = QStringLiteral(":/i18n");

    // Qt keeps its own catalogue in one qt_<code>.qm per language; English has none.
    if (code != "en"_L1 &&
        qt_translator_.load(locale, QStringLiteral("qt"), QStringLiteral("_"),
                            QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        app_.installTranslator(&qt_translator_);
    }

    // The application's own are in the resources, English included: its catalogue holds the
    // plural forms of the source strings and nothing else.
    if (ui_translator_.load(locale, QStringLiteral("adl_ui"), QStringLiteral("_"),
                            catalogue_directory)) {
        app_.installTranslator(&ui_translator_);
    }
    if (app_translator_.load(locale, catalogue_, QStringLiteral("_"), catalogue_directory)) {
        app_.installTranslator(&app_translator_);
    }
}

}  // namespace adl::ui

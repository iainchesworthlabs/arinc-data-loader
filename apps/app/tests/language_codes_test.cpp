// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/ui/language_codes.hpp"
#include "test_classes.hpp"

#include <QLocale>
#include <QTest>

using adl::ui::canonical_language_code;
using adl::ui::language_code_for_locale;

void LanguageCodesTest::a_preference_saved_by_0_1_0_for_chinese_selects_simplified_chinese()
{
    QCOMPARE(canonical_language_code(QStringLiteral("zh")), QStringLiteral("zh_CN"));
}

void LanguageCodesTest::other_codes_are_left_as_they_are()
{
    for (const QString& code : {QStringLiteral("en"), QStringLiteral("fr"), QStringLiteral("he"),
                                QStringLiteral("zh_CN"), QStringLiteral(""), QStringLiteral("xx")})
    {
        QCOMPARE(canonical_language_code(code), code);
    }
}

void LanguageCodesTest::a_locale_selects_its_language()
{
    QCOMPARE(language_code_for_locale(QLocale(QStringLiteral("fr_FR"))), QStringLiteral("fr"));
    QCOMPARE(language_code_for_locale(QLocale(QStringLiteral("de_AT"))), QStringLiteral("de"));
    QCOMPARE(language_code_for_locale(QLocale(QStringLiteral("ja_JP"))), QStringLiteral("ja"));
    QCOMPARE(language_code_for_locale(QLocale(QStringLiteral("he_IL"))), QStringLiteral("he"));
}

void LanguageCodesTest::chinese_is_simplified_chinese_only_in_the_simplified_script()
{
    QCOMPARE(language_code_for_locale(QLocale(QStringLiteral("zh_CN"))), QStringLiteral("zh_CN"));
    QCOMPARE(language_code_for_locale(QLocale(QStringLiteral("zh_SG"))), QStringLiteral("zh_CN"));
    QCOMPARE(language_code_for_locale(QLocale(QStringLiteral("zh_Hans_CN"))), QStringLiteral("zh_CN"));
    QCOMPARE(language_code_for_locale(QLocale(QStringLiteral("zh_TW"))), QString{});
    QCOMPARE(language_code_for_locale(QLocale(QStringLiteral("zh_HK"))), QString{});
    QCOMPARE(language_code_for_locale(QLocale(QStringLiteral("zh_Hant_TW"))), QString{});
}

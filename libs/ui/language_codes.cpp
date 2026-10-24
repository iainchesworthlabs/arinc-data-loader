// SPDX-License-Identifier: GPL-3.0-or-later
#include "adl/ui/language_codes.hpp"

namespace adl::ui
{

QString canonical_language_code(const QString& saved)
{
    return saved == QStringLiteral("zh") ? QStringLiteral("zh_CN") : saved;
}

QString language_code_for_locale(const QLocale& locale)
{
    if (locale.language() == QLocale::Chinese)
    {
        return locale.script() == QLocale::SimplifiedChineseScript ? QStringLiteral("zh_CN") : QString{};
    }
    return QLocale::languageToCode(locale.language());
}

} // namespace adl::ui

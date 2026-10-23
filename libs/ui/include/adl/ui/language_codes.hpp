// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QLocale>
#include <QString>

namespace adl::ui
{

// The code a catalogue is named by for a language a person has chosen or saved. Release 0.1.0 wrote
// "zh" for Simplified Chinese; the catalogues are now named by the full locale, zh_CN, because "zh"
// alone does not say which Chinese. Every other code is returned as it is.
[[nodiscard]] QString canonical_language_code(const QString& saved);

// The code of the language a system locale asks for, or an empty string when the locale is one the
// applications have no catalogue for in that language. Chinese is Simplified Chinese (zh_CN) only
// for a locale written in the simplified script; Traditional Chinese gets no catalogue, and so English.
[[nodiscard]] QString language_code_for_locale(const QLocale& locale);

} // namespace adl::ui

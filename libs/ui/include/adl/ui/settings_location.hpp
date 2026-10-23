#pragma once

#include <QString>

namespace adl::ui
{

// The name of the environment variable that moves the programs' settings (see below).
inline constexpr const char* settings_directory_variable = "ADL_SETTINGS_DIR";

// Lets the environment decide where QSettings keeps the preferences. Without ADL_SETTINGS_DIR the
// programs use their platform's usual place (the registry key, the plist or the configuration
// file of the organisation and application name). With it set to a directory, every QSettings
// made without arguments reads and writes an ini file in that directory instead, whatever the
// platform, and nothing is written anywhere else. That is what the tests use, so that starting a
// program under test never touches an operator's real settings; it is also a way to run a program
// from a stick or to keep two configurations apart.
//
// Call it once, after the organisation and application names are set and before anything reads a
// setting. Returns the directory in use, or an empty string when the variable is not set (or
// empty) and the default place stays.
QString redirect_settings_from_environment();

// The same, for a directory chosen by the caller (a test).
void redirect_settings_to(const QString& directory);

} // namespace adl::ui

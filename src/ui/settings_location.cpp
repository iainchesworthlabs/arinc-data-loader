#include "adl/ui/settings_location.hpp"

#include <QDir>
#include <QSettings>
#include <QtGlobal>

namespace adl::ui {

void redirect_settings_to(const QString& directory) {
    QDir().mkpath(directory);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory);
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, directory);
}

QString redirect_settings_from_environment() {
    const QString directory = qEnvironmentVariable(settings_directory_variable);
    if (directory.isEmpty()) {
        return {};
    }
    redirect_settings_to(directory);
    return directory;
}

}  // namespace adl::ui

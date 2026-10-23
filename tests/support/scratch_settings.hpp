// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Keeps a test program's QSettings out of the operator's real settings (the registry key on
// Windows, the plist on macOS, the configuration file on Linux) and out of every other test's.
// A QSettings made without arguments - which is what the application's own code makes - lands in
// a directory that exists only for as long as the test that asked for it: next() moves it to a
// fresh, empty one and removes the one before, so what one test stored cannot be read by the next.

#include <QDir>
#include <QSettings>
#include <QString>
#include <QTemporaryDir>
#include <memory>

namespace adl::test
{

class ScratchSettings
{
public:
    ScratchSettings()
    {
        next();
    }
    ScratchSettings(const ScratchSettings&) = delete;
    ScratchSettings& operator=(const ScratchSettings&) = delete;

    // A new empty directory becomes the default place (the one before is deleted with its files).
    void next()
    {
        directory_ = std::make_unique<QTemporaryDir>();
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory_->path());
        QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, directory_->path());
    }

    [[nodiscard]] bool valid() const
    {
        return directory_ != nullptr && directory_->isValid();
    }
    [[nodiscard]] QString path() const
    {
        return directory_->path();
    }
    // A file of its own inside the current directory (not the one a QSettings() without
    // arguments uses), for a test that hands a QSettings to the code under test.
    [[nodiscard]] QString file(const QString& name = QStringLiteral("settings.ini")) const
    {
        return directory_->filePath(name);
    }

private:
    std::unique_ptr<QTemporaryDir> directory_;
};

// The one the test program shares: its main() makes it, each test's init() moves it on.
inline ScratchSettings& scratch_settings()
{
    static ScratchSettings instance;
    return instance;
}

} // namespace adl::test

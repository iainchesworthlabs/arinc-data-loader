#pragma once

#include <QSettings>
#include <QString>
#include <memory>

namespace adl::app
{

// The operator's preferences, held in QSettings. The application sets its name and
// organisation (main.cpp), so a QSettings made without arguments finds the right place;
// a test hands over a QSettings bound to a file of its own. Loaded once, when LoaderModel is
// made, and every change is written through at once (QSettings is cheap, and the value has
// to survive a crash).
class AppSettings
{
public:
    // The defaults, used where nothing is stored or what is stored is not usable.
    static constexpr int default_control_port = 59;
    static constexpr int default_log_capacity = 2000;
    static constexpr int min_log_capacity = 100;
    static constexpr int max_log_capacity = 1'000'000;

    // `storage` is used as it is (it must outlive nothing: it is owned here). Null makes the
    // default QSettings of the application.
    explicit AppSettings(std::unique_ptr<QSettings> storage = {});

    // The first launch opens on the first-run screen; passing it once is what is stored, so a
    // window that has been used before goes straight to the workbench.
    [[nodiscard]] bool firstRunComplete() const;
    void setFirstRunComplete(bool complete);

    [[nodiscard]] QString lastMediaSetDirectory() const;
    void setLastMediaSetDirectory(const QString& path);

    // Prefilled into the target fields at start-up, and again when the operator clears them.
    [[nodiscard]] QString defaultTargetAddress() const;
    void setDefaultTargetAddress(const QString& address);
    [[nodiscard]] QString defaultThwIdPos() const;
    void setDefaultThwIdPos(const QString& thw_id_pos);

    // The target of the last operation that was started, put back into the window at start-up so
    // the operator does not retype it every session. Empty until an operation has run.
    [[nodiscard]] QString lastTargetAddress() const;
    void setLastTargetAddress(const QString& address);
    [[nodiscard]] QString lastThwIdPos() const;
    void setLastThwIdPos(const QString& thw_id_pos);

    // The port the loader listens on (1 to 65535; 59 unless it has to move, on a machine
    // where another program, or the target itself, holds 59).
    [[nodiscard]] int controlPort() const;
    void setControlPort(int port);

    // Where a download is to be written (reserved for the Download operation).
    [[nodiscard]] QString downloadDirectory() const;
    void setDownloadDirectory(const QString& directory);

    // Before Information or Upload, with no target chosen: run FIND and, if exactly one
    // target answers, take it and go on.
    [[nodiscard]] bool autoFindBeforeOperation() const;
    void setAutoFindBeforeOperation(bool enabled);

    // Check a media set as soon as it is chosen.
    [[nodiscard]] bool validateOnOpen() const;
    void setValidateOnOpen(bool enabled);

    // The most lines the activity log keeps.
    [[nodiscard]] int logCapacity() const;
    void setLogCapacity(int capacity);

private:
    [[nodiscard]] QString text(const QString& key, const QString& fallback) const;
    [[nodiscard]] bool flag(const QString& key, bool fallback) const;
    [[nodiscard]] int number(const QString& key, int fallback, int minimum, int maximum) const;
    void save(const QString& key, const QString& value);
    void save(const QString& key, bool value);
    void save(const QString& key, int value);

    std::unique_ptr<QSettings> storage_;
    QString default_download_directory_;
};

} // namespace adl::app

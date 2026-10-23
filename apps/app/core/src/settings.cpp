#include "adl/app/settings.hpp"

#include <QStandardPaths>

namespace adl::app {

namespace {

const auto key_first_run_complete = QStringLiteral("app/firstRunComplete");
const auto key_last_media_set = QStringLiteral("mediaSet/lastDirectory");
const auto key_default_address = QStringLiteral("target/defaultAddress");
const auto key_default_thw_id_pos = QStringLiteral("target/defaultThwIdPos");
const auto key_last_target_address = QStringLiteral("target/lastAddress");
const auto key_last_target_thw_id_pos = QStringLiteral("target/lastThwIdPos");
const auto key_control_port = QStringLiteral("loader/controlPort");
const auto key_download_directory = QStringLiteral("download/directory");
const auto key_auto_find = QStringLiteral("target/autoFindBeforeOperation");
const auto key_validate_on_open = QStringLiteral("mediaSet/validateOnOpen");
const auto key_log_capacity = QStringLiteral("log/capacity");

}  // namespace

AppSettings::AppSettings(std::unique_ptr<QSettings> storage)
    : storage_(storage != nullptr ? std::move(storage) : std::make_unique<QSettings>()) {
    const QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    default_download_directory_ =
        (documents.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
                             : documents) +
        QStringLiteral("/ArincDownloads");
}

bool AppSettings::firstRunComplete() const {
    return flag(key_first_run_complete, false);
}

void AppSettings::setFirstRunComplete(bool complete) {
    save(key_first_run_complete, complete);
}

QString AppSettings::lastMediaSetDirectory() const {
    return text(key_last_media_set, {});
}

void AppSettings::setLastMediaSetDirectory(const QString& path) {
    save(key_last_media_set, path);
}

QString AppSettings::lastTargetAddress() const {
    return text(key_last_target_address, {});
}

void AppSettings::setLastTargetAddress(const QString& address) {
    save(key_last_target_address, address.trimmed());
}

QString AppSettings::lastThwIdPos() const {
    return text(key_last_target_thw_id_pos, {});
}

void AppSettings::setLastThwIdPos(const QString& thw_id_pos) {
    save(key_last_target_thw_id_pos, thw_id_pos.trimmed());
}

QString AppSettings::defaultTargetAddress() const {
    return text(key_default_address, {});
}

void AppSettings::setDefaultTargetAddress(const QString& address) {
    save(key_default_address, address.trimmed());
}

QString AppSettings::defaultThwIdPos() const {
    return text(key_default_thw_id_pos, {});
}

void AppSettings::setDefaultThwIdPos(const QString& thw_id_pos) {
    save(key_default_thw_id_pos, thw_id_pos.trimmed());
}

int AppSettings::controlPort() const {
    return number(key_control_port, default_control_port, 1, 65535);
}

void AppSettings::setControlPort(int port) {
    // Anything else would not open; the value is kept as it was.
    if (port >= 1 && port <= 65535) {
        save(key_control_port, port);
    }
}

QString AppSettings::downloadDirectory() const {
    return text(key_download_directory, default_download_directory_);
}

void AppSettings::setDownloadDirectory(const QString& directory) {
    save(key_download_directory, directory);
}

bool AppSettings::autoFindBeforeOperation() const {
    return flag(key_auto_find, false);
}

void AppSettings::setAutoFindBeforeOperation(bool enabled) {
    save(key_auto_find, enabled);
}

bool AppSettings::validateOnOpen() const {
    return flag(key_validate_on_open, true);
}

void AppSettings::setValidateOnOpen(bool enabled) {
    save(key_validate_on_open, enabled);
}

int AppSettings::logCapacity() const {
    return number(key_log_capacity, default_log_capacity, min_log_capacity, max_log_capacity);
}

void AppSettings::setLogCapacity(int capacity) {
    // The log is not a place for extremes; anything else is kept as it was.
    if (capacity >= min_log_capacity && capacity <= max_log_capacity) {
        save(key_log_capacity, capacity);
    }
}

QString AppSettings::text(const QString& key, const QString& fallback) const {
    const QVariant stored = storage_->value(key);
    return stored.isValid() && stored.canConvert<QString>() ? stored.toString() : fallback;
}

bool AppSettings::flag(const QString& key, bool fallback) const {
    const QVariant stored = storage_->value(key);
    return stored.isValid() ? stored.toBool() : fallback;
}

int AppSettings::number(const QString& key, int fallback, int minimum, int maximum) const {
    const QVariant stored = storage_->value(key);
    bool ok = false;
    const int value = stored.isValid() ? stored.toInt(&ok) : 0;
    return ok && value >= minimum && value <= maximum ? value : fallback;
}

void AppSettings::save(const QString& key, const QString& value) {
    storage_->setValue(key, value);
    storage_->sync();
}

void AppSettings::save(const QString& key, bool value) {
    storage_->setValue(key, value);
    storage_->sync();
}

void AppSettings::save(const QString& key, int value) {
    storage_->setValue(key, value);
    storage_->sync();
}

}  // namespace adl::app

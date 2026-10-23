#include "adl/app/settings.hpp"
#include "test_classes.hpp"

#include <QSettings>
#include <QTemporaryDir>
#include <QTest>
#include <memory>

using namespace adl::app;

namespace
{

std::unique_ptr<AppSettings> settings_at(const QString& path)
{
    return std::make_unique<AppSettings>(std::make_unique<QSettings>(path, QSettings::IniFormat));
}

} // namespace

void SettingsTest::the_defaults_hold_without_a_stored_value()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("settings.ini"));
    auto settings = settings_at(path);

    QCOMPARE(settings->controlPort(), 59);
    QCOMPARE(settings->logCapacity(), 2000);
    QCOMPARE(settings->validateOnOpen(), true);
    QCOMPARE(settings->autoFindBeforeOperation(), false);
    QVERIFY(settings->lastMediaSetDirectory().isEmpty());
    QVERIFY(settings->defaultTargetAddress().isEmpty());
    QVERIFY(settings->defaultThwIdPos().isEmpty());
    QVERIFY(settings->downloadDirectory().endsWith(QStringLiteral("ArincDownloads")));
    QCOMPARE(settings->firstRunComplete(), false);
    QVERIFY(settings->lastTargetAddress().isEmpty());
    QVERIFY(settings->lastThwIdPos().isEmpty());
}

void SettingsTest::what_is_written_is_read_back()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("settings.ini"));
    {
        auto settings = settings_at(path);
        settings->setLastMediaSetDirectory(QStringLiteral("C:/media/ACM-SET-0001"));
        settings->setDefaultTargetAddress(QStringLiteral("10.0.0.2"));
        settings->setDefaultThwIdPos(QStringLiteral("HNPFMS_L"));
        settings->setControlPort(1615);
        settings->setDownloadDirectory(QStringLiteral("C:/downloads"));
        settings->setAutoFindBeforeOperation(true);
        settings->setValidateOnOpen(false);
        settings->setLogCapacity(500);
    }

    // A new instance over the same file reads what was written.
    auto read_back = settings_at(path);
    QCOMPARE(read_back->lastMediaSetDirectory(), QStringLiteral("C:/media/ACM-SET-0001"));
    QCOMPARE(read_back->defaultTargetAddress(), QStringLiteral("10.0.0.2"));
    QCOMPARE(read_back->defaultThwIdPos(), QStringLiteral("HNPFMS_L"));
    QCOMPARE(read_back->controlPort(), 1615);
    QCOMPARE(read_back->downloadDirectory(), QStringLiteral("C:/downloads"));
    QCOMPARE(read_back->autoFindBeforeOperation(), true);
    QCOMPARE(read_back->validateOnOpen(), false);
    QCOMPARE(read_back->logCapacity(), 500);
}

void SettingsTest::numbers_out_of_range_are_kept_as_they_were()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("settings.ini"));
    auto settings = settings_at(path);

    // A port of 0 or 65536 would not open; neither is taken.
    settings->setControlPort(0);
    QCOMPARE(settings->controlPort(), 59);
    settings->setControlPort(65536);
    QCOMPARE(settings->controlPort(), 59);
    settings->setControlPort(1615);
    settings->setControlPort(-1);
    QCOMPARE(settings->controlPort(), 1615);

    // The log's length is not a place for extremes either.
    settings->setLogCapacity(5);
    QCOMPARE(settings->logCapacity(), 2000);
    settings->setLogCapacity(500);
    settings->setLogCapacity(2'000'000);
    QCOMPARE(settings->logCapacity(), 500);
}

void SettingsTest::the_first_run_is_complete_only_once()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("settings.ini"));

    {
        auto settings = settings_at(path);
        QCOMPARE(settings->firstRunComplete(), false);
        settings->setFirstRunComplete(true);
    }

    // The next launch is not a first run.
    auto read_back = settings_at(path);
    QCOMPARE(read_back->firstRunComplete(), true);
}

void SettingsTest::the_target_of_the_last_operation_is_remembered()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("settings.ini"));

    {
        auto settings = settings_at(path);
        settings->setLastTargetAddress(QStringLiteral("10.0.0.7"));
        settings->setLastThwIdPos(QStringLiteral("HNPFMS_R"));
    }

    auto read_back = settings_at(path);
    QCOMPARE(read_back->lastTargetAddress(), QStringLiteral("10.0.0.7"));
    QCOMPARE(read_back->lastThwIdPos(), QStringLiteral("HNPFMS_R"));
    // The defaults the operator set are a separate thing: the last target does not overwrite them.
    QVERIFY(read_back->defaultTargetAddress().isEmpty());
}

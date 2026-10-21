// QtTest has one main per executable; this one runs every test class and fails if any does.

#include <QCoreApplication>
#include <QTest>

#include "scratch_settings.hpp"
#include "test_classes.hpp"

int main(int argc, char* argv[]) {
    QCoreApplication application(argc, argv);
    // The application reads QSettings without arguments (its name and organisation), which on the
    // desktop is the real registry or configuration file. Tests have to be hermetic, so the
    // default format and place are moved into a scratch directory, and each test that stores
    // anything moves on to an empty one of its own (see scratch_settings.hpp).
    QCoreApplication::setOrganizationName(QStringLiteral("adl-tests"));
    QCoreApplication::setApplicationName(QStringLiteral("adl_app_tests"));
    adl::test::scratch_settings();
    int failed = 0;
    {
        AddressTest test;
        failed += QTest::qExec(&test, argc, argv);
    }
    {
        NoticeTextTest test;
        failed += QTest::qExec(&test, argc, argv);
    }
    {
        ModelsTest test;
        failed += QTest::qExec(&test, argc, argv);
    }
    {
        ControlsTest test;
        failed += QTest::qExec(&test, argc, argv);
    }
    {
        DownloadDirectoryTest test;
        failed += QTest::qExec(&test, argc, argv);
    }
    {
        EngineWorkerTest test;
        failed += QTest::qExec(&test, argc, argv);
    }
    {
        MediaSourceTest test;
        failed += QTest::qExec(&test, argc, argv);
    }
    {
        SettingsTest test;
        failed += QTest::qExec(&test, argc, argv);
    }
    {
        LoaderModelTest test;
        failed += QTest::qExec(&test, argc, argv);
    }
    return failed;
}

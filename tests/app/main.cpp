// QtTest has one main per executable; this one runs every test class and fails if any does.

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QTest>

#include "test_classes.hpp"

namespace {

// The application reads QSettings without arguments (its name and organisation), which on the
// desktop is the real registry or configuration file. Tests have to be hermetic, so the default
// format and place are moved into a scratch directory: every QSettings made without arguments
// now lands there.
class ScratchSettings {
public:
    ScratchSettings() {
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                           QDir::tempPath() + QStringLiteral("/adl_test_qsettings"));
    }
};

}  // namespace

int main(int argc, char* argv[]) {
    QCoreApplication application(argc, argv);
    ScratchSettings scratch;
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

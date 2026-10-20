// QtTest has one main per executable; this one runs every test class and fails if any does.

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QTest>
#include <QTranslator>

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
    // English has a catalogue of its own for the plural forms ("%n load(s)" is "1 load" or "2
    // loads" only through it); the application installs it, and so does the logic under test.
    QTranslator english;
    if (english.load(QStringLiteral("adl_app_en"), QStringLiteral(ADL_I18N_DIR))) {
        QCoreApplication::installTranslator(&english);
    }
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
    {
        MessageTest test;
        failed += QTest::qExec(&test, argc, argv);
    }
    return failed;
}

// QtTest has one main per executable; this one runs every test class and fails if any does.

#include <QCoreApplication>
#include <QTest>
#include <QTranslator>

#include "test_classes.hpp"

int main(int argc, char* argv[]) {
    QCoreApplication application(argc, argv);
    // English has a catalogue of its own for the plural forms ("%n data file(s)" is "1 data file"
    // or "2 data files" only through it); the application installs it, and so does the logic under
    // test.
    QTranslator english;
    if (english.load(QStringLiteral("adl_media_gen_en"), QStringLiteral(ADL_I18N_DIR))) {
        QCoreApplication::installTranslator(&english);
    }
    int failed = 0;
    {
        GeneratorTest test;
        failed += QTest::qExec(&test, argc, argv);
    }
    return failed;
}

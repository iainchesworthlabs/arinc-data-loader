// QtTest has one main per executable; this one runs every test class and fails if any does.

#include <QCoreApplication>
#include <QTest>

#include "test_classes.hpp"

int main(int argc, char* argv[]) {
    QCoreApplication application(argc, argv);
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
        MediaSourceTest test;
        failed += QTest::qExec(&test, argc, argv);
    }
    {
        LoaderModelTest test;
        failed += QTest::qExec(&test, argc, argv);
    }
    return failed;
}

// SPDX-License-Identifier: GPL-3.0-or-later
// QtTest has one main per executable; this one runs every test class and fails if any does.

#include "test_classes.hpp"

#include <QCoreApplication>
#include <QTest>

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    int failed = 0;
    {
        MockModelTest test;
        failed += QTest::qExec(&test, argc, argv);
    }
    return failed;
}

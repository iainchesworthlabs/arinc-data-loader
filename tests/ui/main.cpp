// The Qt Quick Test entry point: every tst_*.qml file under tests/ui/qml is one test case, run
// offscreen against the application's real window and model. UiSetup builds the harness and hands
// it to the test files as context properties (appModel, harness, mainQmlUrl).

#include "ui_setup.hpp"

#include <QtQuickTest/quicktest.h>

QUICK_TEST_MAIN_WITH_SETUP(adl_ui_tests, adl::ui_test::UiSetup)

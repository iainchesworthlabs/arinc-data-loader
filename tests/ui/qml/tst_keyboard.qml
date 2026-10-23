// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtTest

// Principle 9 for the dialogs: the real window, driven with keys. Where the focus starts in Preferences,
// About, the stop-and-close question and the error dialog; what Tab, Return and Esc do there; the
// stop-and-close question never being the default; Esc acknowledging a failure as OK does; and the
// focus going back to the control that opened the dialog.
TestCase {
    id: testCase

    name: "Keyboard"
    when: windowShown
    width: 1280
    height: 900

    property var mainWindow: null

    function find(name) {
        const item = harness.findItem(mainWindow, name)
        verify(item !== null, "there is no item called " + name)
        return item
    }

    function findIn(parent, name) {
        const item = harness.findItem(parent, name)
        verify(item !== null, "there is no item called " + name)
        return item
    }

    function click(name) {
        const item = find(name)
        verify(item.visible, name + " is not visible")
        verify(item.enabled, name + " is disabled")
        mouseClick(item)
        wait(50)
    }

    function key(code, modifiers) {
        mainWindow.requestActivate()
        keyClick(code, modifiers === undefined ? Qt.NoModifier : modifiers)   // sent to the active window
        wait(20)
    }

    function typeText(item, text) {
        item.forceActiveFocus()
        for (let i = 0; i < text.length; ++i) {
            const code = text.charCodeAt(i)
            const upper = code >= 0x61 && code <= 0x7A ? code - 0x20 : code
            const shift = code >= 0x41 && code <= 0x5A ? Qt.ShiftModifier : Qt.NoModifier
            keyClick(upper, shift)
        }
        wait(10)
    }

    // The 2 px ring a control draws while the keyboard is on it: its FocusRing child.
    function ringOf(item) {
        for (const child of item.children) {
            if (child.active !== undefined && child.border !== undefined && child.border.width === 2)
                return child
        }
        return null
    }

    function openPreferences() {
        click("preferencesButton")
        const dialog = find("preferencesDialog")
        tryVerify(function() { return dialog.opened })
        return dialog
    }

    function initTestCase() {
        verify(harness.ready(), "the media set could not be written")
        appModel.completeFirstRun()
        const component = Qt.createComponent(mainQmlUrl)
        verify(component.status === Component.Ready, component.errorString())
        mainWindow = component.createObject(null, { "app": appModel })
        verify(mainWindow !== null, "Main.qml did not make a window")
        mainWindow.width = 1400
        mainWindow.height = 1000
        waitForRendering(mainWindow.contentItem)
        mainWindow.requestActivate()
        wait(50)
    }

    function cleanupTestCase() {
        harness.releaseJob()
        if (mainWindow) {
            mainWindow.close()
            mainWindow.destroy()
            mainWindow = null
        }
    }

    function init() {
        harness.releaseJob()
        harness.resetTarget()
        appModel.dismissError()
        appModel.clearLog()
        appModel.autoFindBeforeOperation = false
        appModel.validateOnOpen = true
        appModel.defaultTargetAddress = ""
        appModel.defaultThwIdPos = ""
        appModel.targetAddress = ""
        appModel.thwIdPos = ""
        for (const name of ["preferencesDialog", "aboutDialog", "closeDialog", "errorDialog"]) {
            const dialog = harness.findItem(mainWindow, name)
            if (dialog !== null && dialog.visible)
                dialog.close()
        }
        wait(10)
    }

    // ---- dialogs -----------------------------------------------------------------

    function test_01_preferences_opens_on_its_first_field_and_tabs_through_the_fields() {
        const dialog = openPreferences()
        const address = findIn(dialog, "defaultTargetAddressField")
        tryVerify(function() { return address.activeFocus }, 2000,
                  "the focus did not start on the first field")

        key(Qt.Key_Tab)
        verify(findIn(dialog, "defaultThwIdPosField").activeFocus,
               "the first Tab did not reach the next field")
        key(Qt.Key_Tab)
        verify(findIn(dialog, "autoFindSwitch").activeFocus)
        key(Qt.Key_Tab)
        verify(findIn(dialog, "validateOnOpenSwitch").activeFocus)

        key(Qt.Key_Escape)
        tryVerify(function() { return !dialog.visible })
    }

    function test_02_enter_in_a_field_saves_and_escape_leaves_the_stored_values() {
        let dialog = openPreferences()
        let address = findIn(dialog, "defaultTargetAddressField")
        address.text = ""
        typeText(address, "10.4.4.4")
        key(Qt.Key_Return)
        tryVerify(function() { return !dialog.visible })
        tryCompare(appModel, "defaultTargetAddress", "10.4.4.4")

        dialog = openPreferences()
        address = findIn(dialog, "defaultTargetAddressField")
        address.text = ""
        typeText(address, "10.5.5.5")
        key(Qt.Key_Escape)
        tryVerify(function() { return !dialog.visible })
        compare(appModel.defaultTargetAddress, "10.4.4.4")
    }

    function test_03_the_focus_goes_back_to_the_control_that_opened_the_dialog() {
        const button = find("preferencesButton")
        // A click does not focus a button on macOS (only text controls take mouse focus there), so
        // the control that opens the dialog is given the focus the way a Tab would.
        button.forceActiveFocus(Qt.TabFocusReason)
        verify(button.activeFocus)
        const dialog = openPreferences()
        verify(!button.activeFocus)
        key(Qt.Key_Escape)
        tryVerify(function() { return !dialog.visible })
        tryVerify(function() { return button.activeFocus }, 2000, "the focus was not returned")
    }

    function test_04_about_starts_on_close_and_enter_closes_it() {
        click("aboutButton")
        const dialog = find("aboutDialog")
        tryVerify(function() { return dialog.opened })
        const close = dialog.standardButton(Dialog.Close)
        tryVerify(function() { return close.activeFocus }, 2000, "Close does not hold the focus")
        key(Qt.Key_Return)
        tryVerify(function() { return !dialog.visible })
    }

    function test_05_stop_and_close_is_never_the_default() {
        const dialog = find("closeDialog")
        dialog.open()
        tryVerify(function() { return dialog.opened })
        verify(dialog.secondaryItem.activeFocus, "the focus is not on Keep working")
        verify(!dialog.enterAccepts)
        key(Qt.Key_Return)
        tryVerify(function() { return !dialog.visible })
        verify(mainWindow.visible, "Enter closed the window")
        verify(!appModel.busy)
    }

    function test_06_escape_acknowledges_a_failure_as_ok_does() {
        pointAtTheTarget()
        harness.makeTargetRefuse()
        click("readInformationButton")
        tryCompare(appModel, "resultState", 2, 20000)
        tryVerify(function() { return !appModel.busy }, 20000)

        const dialog = find("errorDialog")
        tryVerify(function() { return dialog.opened }, 10000)
        verify(dialog.primaryItem.activeFocus, "OK does not hold the focus")
        verify(appModel.lastErrorText.length > 0)
        key(Qt.Key_Escape)
        tryVerify(function() { return !dialog.visible })
        compare(appModel.lastErrorText, "", "Esc left the error standing")
    }

    function pointAtTheTarget() {
        appModel.targetAddress = "10.0.0.2"
        appModel.thwIdPos = "HNPFMS_L"
    }

}

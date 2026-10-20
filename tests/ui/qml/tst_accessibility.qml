import QtQuick
import QtQuick.Controls
import QtTest

// What a person using a keyboard, a screen reader or voice control relies on: the 2 px focus ring, 24 px
// targets, accessible names that contain the text on the screen (and a spin box editor that has one),
// and a field set from outside (UI Automation, voice control) reaching the model.
TestCase {
    id: testCase

    name: "Accessibility"
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

    // ---- focus ring, targets, names -----------------------------------------------

    function test_20_a_field_in_focus_shows_the_two_pixel_ring() {
        const field = find("targetAddressField")
        field.forceActiveFocus(Qt.TabFocusReason)
        const ring = ringOf(field)
        verify(ring !== null, "the field has no focus ring")
        verify(ring.visible)
        compare(ring.border.width, 2)
        find("findTargetsButton").forceActiveFocus()
        verify(!ring.visible, "the ring stays after the focus leaves")

        const dialog = openPreferences()
        const spin = findIn(dialog, "controlPortSpin")
        spin.forceActiveFocus(Qt.TabFocusReason)
        verify(ringOf(spin) !== null && ringOf(spin).visible, "the spin box shows no ring")
        key(Qt.Key_Escape)
        tryVerify(function() { return !dialog.visible })
    }

    function test_21_targets_are_at_least_24_pixels() {
        const dialog = openPreferences()
        const spin = findIn(dialog, "controlPortSpin")
        verify(spin.up.indicator.width >= 24 && spin.up.indicator.height >= 24,
               "plus is " + spin.up.indicator.width + "x" + spin.up.indicator.height)
        verify(spin.down.indicator.width >= 24 && spin.down.indicator.height >= 24,
               "minus is " + spin.down.indicator.width + "x" + spin.down.indicator.height)
        const check = findIn(dialog, "autoFindSwitch")
        verify(check.height >= 24, "a check box row is " + check.height + " px high")
        key(Qt.Key_Escape)
        tryVerify(function() { return !dialog.visible })
    }

    function test_22_names_carry_the_text_the_screen_shows() {
        for (const name of ["preferencesButton", "aboutButton", "findTargetsButton",
                            "targetPropertiesButton", "mediaSetChooseButton", "mediaSetCheckButton",
                            "tickAllLoadsButton", "readInformationButton", "uploadButton"]) {
            const button = find(name)
            const accessible = button.Accessible.name.toLowerCase()
            verify(button.text.length > 0 && accessible.indexOf(button.text.toLowerCase()) >= 0,
                   name + " is shown as '" + button.text + "' but named '" + button.Accessible.name + "'")
        }
        verify(find("thwIdPosField").Accessible.name.indexOf("THW_ID_POS") >= 0,
               "the THW_ID_POS field is not called that: " + find("thwIdPosField").Accessible.name)

        const dialog = openPreferences()
        const spin = findIn(dialog, "controlPortSpin")
        verify(spin.contentItem.Accessible.name.length > 0, "the spin box editor has no name")
        compare(spin.contentItem.Accessible.name, spin.Accessible.name)
        verify(findIn(dialog, "defaultThwIdPosField").Accessible.name.indexOf("THW_ID_POS") >= 0)
        key(Qt.Key_Escape)
        tryVerify(function() { return !dialog.visible })
    }

    function test_23_a_field_set_from_outside_reaches_the_model() {
        // UI Automation's value pattern and voice control set the text without a key press.
        find("findTargetsButton").forceActiveFocus()
        const address = find("targetAddressField")
        address.text = "10.7.7.7"
        tryCompare(appModel, "targetAddress", "10.7.7.7")
        const thw = find("thwIdPosField")
        thw.text = "ABC_R"
        tryCompare(appModel, "thwIdPos", "ABC_R")

        const dialog = openPreferences()
        const spin = findIn(dialog, "controlPortSpin")
        spin.value = 77
        findIn(dialog, "defaultThwIdPosField").text = "SET_X"
        mouseClick(dialog.standardButton(Dialog.Save))
        tryVerify(function() { return !dialog.visible })
        tryCompare(appModel, "controlPort", 77)
        tryCompare(appModel, "defaultThwIdPos", "SET_X")
        appModel.controlPort = 59
    }

}

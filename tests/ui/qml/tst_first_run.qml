// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtTest

// The first launch: the window opens on the first-run screen (a settings file with nothing in it),
// its optional defaults are typed into the screen's own fields, and Start loading hands over to
// the workbench with those defaults in place, stored for the next launch.
TestCase {
    id: testCase

    name: "FirstRun"
    when: windowShown
    width: 1280
    height: 900

    property var mainWindow: null

    function find(name) {
        const item = harness.findItem(mainWindow, name)
        verify(item !== null, "there is no item called " + name)
        return item
    }

    function typeText(item, text) {
        if (item.window)
            item.window.requestActivate()
        item.forceActiveFocus()
        for (let i = 0; i < text.length; ++i) {
            const character = text.charAt(i)
            const code = character.charCodeAt(0)
            const key = (code >= 0x61 && code <= 0x7A) ? code - 0x20 : code
            const shift = (code >= 0x41 && code <= 0x5A) ? Qt.ShiftModifier : Qt.NoModifier
            if ((code >= 0x30 && code <= 0x39) || (code >= 0x41 && code <= 0x5A)
                    || (code >= 0x61 && code <= 0x7A) || code === 0x2E || code === 0x5F)
                keyClick(key, shift)   // Qt Quick Test sends keys to the active window
            else
                fail("no key for '" + character + "'")
        }
        wait(10)
    }

    // The defaults are folded away until "Set up defaults" is pressed (a property of the first-run
    // screen); a screen that shows them already needs no press.
    function showTheDefaults() {
        const start = find("firstRunStart")
        let screen = start.parent
        while (screen !== null && !("showDefaults" in screen))
            screen = screen.parent
        if (screen !== null && !screen.showDefaults)
            screen.showDefaults = true
        tryVerify(function() { return find("firstRunAddress").visible })
    }

    function initTestCase() {
        verify(harness.ready(), "the media set could not be written")
        verify(appModel.firstRun, "a fresh settings file did not start on the first run")
        const component = Qt.createComponent(mainQmlUrl)
        verify(component.status === Component.Ready, component.errorString())
        mainWindow = component.createObject(null, { "app": appModel })
        verify(mainWindow !== null, "Main.qml did not make a window")
        mainWindow.width = 1400
        mainWindow.height = 1600
        waitForRendering(mainWindow.contentItem)
        wait(50)
    }

    function cleanupTestCase() {
        if (mainWindow) {
            mainWindow.close()
            mainWindow.destroy()
            mainWindow = null
        }
    }

    function test_01_the_first_run_screen_stands_in_front_of_the_workbench() {
        verify(find("firstRunStart").visible)
        verify(find("firstRunStart").enabled)
        // The workbench is behind it: its controls are not on screen.
        const find_button = harness.findItem(mainWindow, "findTargetsButton")
        verify(find_button === null || !find_button.visible,
               "the workbench is showing during the first run")
    }

    function test_02_the_defaults_typed_there_are_the_workbenchs_starting_point() {
        showTheDefaults()
        const address = find("firstRunAddress")
        address.text = ""
        typeText(address, "10.1.2.3")
        const thw = find("firstRunThwIdPos")
        thw.text = ""
        typeText(thw, "abc_1")
        const directory = find("firstRunDownloadDirectory")
        directory.text = harness.downloadDirectory()
        directory.textEdited()

        // The fields write through as they are edited ...
        tryCompare(appModel, "defaultTargetAddress", "10.1.2.3")
        tryCompare(appModel, "defaultThwIdPos", "abc_1")
        tryCompare(appModel, "downloadDirectory", harness.downloadDirectory())
        // ... but the first run is not over until it is passed.
        verify(appModel.firstRun)
        compare(harness.storedPreferences().firstRun, true)

        mouseClick(find("firstRunStart"))
        tryCompare(appModel, "firstRun", false)

        // The workbench is up, and starts from the defaults.
        tryVerify(function() {
            const button = harness.findItem(mainWindow, "findTargetsButton")
            return button !== null && button.visible
        })
        compare(appModel.targetAddress, "10.1.2.3")
        compare(appModel.thwIdPos, "abc_1")
        verify(appModel.targetValid)
        const start = harness.findItem(mainWindow, "firstRunStart")
        verify(start === null || !start.visible)

        // And the next launch finds it as it was left: no first run, the same defaults.
        const stored = harness.storedPreferences()
        compare(stored.firstRun, false)
        compare(stored.defaultTargetAddress, "10.1.2.3")
        compare(stored.defaultThwIdPos, "abc_1")
        compare(stored.downloadDirectory, harness.downloadDirectory())
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtTest

// Principle 9 for the loader's lists and groups: the FIND answers and the dock tabs as one-tab-stop
// groups with arrow keys, the log and the configuration list scrolling by keyboard (and the log
// copying), and the state the header and the status bar show agreeing with the next-step
// banner.
TestCase {
    id: testCase

    name: "Navigation"
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

    function pointAtTheTarget() {
        appModel.targetAddress = "10.0.0.2"
        appModel.thwIdPos = "HNPFMS_L"
    }

    // ---- the FIND answers --------------------------------------------------------

    function test_10_find_answers_are_a_keyboard_list() {
        harness.showTargets(3)
        tryVerify(function() { return harness.findItems(mainWindow, "targetDelegate").length === 3 }, 5000)
        const rows = harness.findItems(mainWindow, "targetDelegate")
        compare(appModel.selectedTargetIndex, -1)

        // One tab stop for the group: the first answer while none is chosen.
        verify(rows[0].activeFocusOnTab)
        verify(!rows[1].activeFocusOnTab)
        compare(rows[0].Accessible.role, Accessible.RadioButton)
        verify(rows[0].Accessible.name.length > 0)

        rows[0].forceActiveFocus(Qt.TabFocusReason)
        key(Qt.Key_Return)
        compare(appModel.selectedTargetIndex, 0)
        compare(appModel.targetAddress, "10.0.0.2")

        key(Qt.Key_Down)
        compare(appModel.selectedTargetIndex, 1)
        verify(rows[1].activeFocus, "the focus did not follow the choice")
        verify(rows[1].activeFocusOnTab && !rows[0].activeFocusOnTab, "the tab stop did not move")
        key(Qt.Key_Down)
        compare(appModel.selectedTargetIndex, 2)
        key(Qt.Key_Down)
        compare(appModel.selectedTargetIndex, 0, "Down did not wrap")
        key(Qt.Key_Up)
        compare(appModel.selectedTargetIndex, 2, "Up did not wrap")
        key(Qt.Key_Home)
        compare(appModel.selectedTargetIndex, 0)
        key(Qt.Key_End)
        compare(appModel.selectedTargetIndex, 2)
        compare(appModel.thwIdPos, "HW3_L")
        verify(rows[2].activeFocus)

        const ring = ringOf(rows[2])
        verify(ring !== null && ring.visible, "the chosen answer shows no ring")
        compare(rows[2].border.width, 2, "the chosen answer is not marked by a 2 px border")
    }

    // ---- the dock ----------------------------------------------------------------

    function test_11_dock_tabs_are_one_control_with_arrow_keys_and_no_unchecking() {
        const activity = find("dockTabActivity")
        const configuration = find("dockTabConfiguration")
        verify(activity.checked && !configuration.checked)
        verify(activity.activeFocusOnTab && !configuration.activeFocusOnTab, "two tab stops")
        compare(activity.Accessible.role, Accessible.PageTab)

        activity.forceActiveFocus(Qt.TabFocusReason)
        key(Qt.Key_Right)
        verify(configuration.checked && !activity.checked)
        verify(configuration.activeFocus, "the focus did not follow the arrow")
        verify(configuration.activeFocusOnTab && !activity.activeFocusOnTab)
        const ring = ringOf(configuration)
        verify(ring !== null && ring.visible, "the focused tab shows no ring")

        mouseClick(configuration)
        verify(configuration.checked, "a click on the chosen tab unchecked it")
        key(Qt.Key_Left)
        verify(activity.checked && activity.activeFocus)
        key(Qt.Key_End)
        verify(configuration.checked)
        key(Qt.Key_Home)
        verify(activity.checked)
        key(Qt.Key_Left)
        verify(configuration.checked, "Left did not wrap")
        mouseClick(activity)
        verify(activity.checked)
    }

    function test_12_the_log_scrolls_and_copies_from_the_keyboard() {
        for (let i = 0; i < 80; ++i)
            harness.addEvent(i === 79 ? 3 : (i % 5 === 0 ? 2 : 0), "line number " + i)
        harness.addEvent(3, "boom")
        const dock = find("activityDock")
        verify(dock.expanded, "the dock is folded in a tall window")
        const list = findIn(find("activityLog"), "logList")
        verify(list.activeFocusOnTab, "the log is not a tab stop")
        tryVerify(function() { return list.contentHeight > list.height })

        list.forceActiveFocus(Qt.TabFocusReason)
        key(Qt.Key_Home)
        compare(list.contentY, list.originY)
        key(Qt.Key_PageDown)
        verify(list.contentY > list.originY, "Page Down did not scroll")
        key(Qt.Key_Down)
        const afterDown = list.contentY
        key(Qt.Key_Up)
        verify(list.contentY < afterDown, "Up did not scroll")
        key(Qt.Key_End)
        verify(list.atYEnd, "End did not reach the end")

        // Ctrl+C puts every line, with its severity word, on the clipboard.
        const probe = Qt.createQmlObject('import QtQuick\nTextEdit { visible: false }', testCase)
        probe.text = ""
        key(Qt.Key_C, Qt.ControlModifier)
        probe.paste()
        verify(probe.text.indexOf("ERROR  boom") >= 0, "the clipboard holds: " + probe.text.slice(0, 120))
        verify(probe.text.indexOf("WARN  line number 0") >= 0)
        verify(probe.text.indexOf("INFO  line number 1") >= 0)
        probe.destroy()
    }

    function test_14_the_log_names_the_platforms_copy_key_in_its_description() {
        // "Ctrl+C" on Windows and Linux, "Cmd+C" on a Mac: the text comes from the platform's own
        // Copy key, not from the source.
        const list = findIn(find("activityLog"), "logList")
        const copyKey = Qt.createQmlObject('import QtQuick\nShortcut { sequences: [StandardKey.Copy] }',
                                           testCase)
        verify(copyKey.nativeText.length > 0, "the platform has no Copy key text")
        compare(list.Accessible.description,
                "Up and Down scroll; " + copyKey.nativeText + " copies every line")
        copyKey.destroy()
    }

    function test_13_the_configuration_list_is_a_tab_stop() {
        const list = find("configurationList")
        verify(list.activeFocusOnTab)
    }

    // ---- the status the window shows ----------------------------------------------

    function test_30_the_header_and_the_status_bar_say_the_same_thing() {
        const header = find("headerStatusChip")

        pointAtTheTarget()
        harness.makeTargetRefuse()
        click("readInformationButton")
        tryCompare(appModel, "resultState", 2, 20000)
        tryVerify(function() { return !appModel.busy }, 20000)
        compare(header.text, "FAILED")
        compare(header.glyph, "cross")
        compare(find("statusState").text, "Failed")
        find("errorDialog").close()
        appModel.dismissError()
    }

    function test_31_the_next_step_title_and_button_agree() {
        pointAtTheTarget()
        appModel.chooseMediaSet(harness.mediaSetUrl())
        tryVerify(function() { return !appModel.busy && appModel.validationState === 2 }, 20000)
        appModel.selectAllLoads(false)
        tryCompare(appModel, "selectedLoadCount", 0)

        const panel = find("uploadPanel")
        compare(panel.nextTitle, "Tick at least one load")
        const button = find("nextActionButton")
        verify(button.visible)
        compare(button.text, "Tick all loads")
        mouseClick(button)
        tryVerify(function() { return appModel.selectedLoadCount > 0 })
        verify(panel.nextTitle !== "Tick at least one load")
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtTest

// The window at its smallest. The loader's minimum is 1000 x 620 so that the header, the action bar
// and the status bar stay on a 1366 x 768 display at 125% scaling; at that size the controls an
// operator needs are inside the window, the plan above the dock keeps room, the pages that overflow
// show a scroll bar, and a check tile's detail wraps instead of being cut.
TestCase {
    id: testCase

    name: "Layout"
    when: windowShown
    width: 1280
    height: 900

    property var smallWindow: null

    function find(root, name) {
        const item = harness.findItem(root, name)
        verify(item !== null, "there is no item called " + name)
        return item
    }

    // The item's rectangle in window coordinates.
    function rectOf(item) {
        const origin = item.mapToItem(null, 0, 0)
        return { left: origin.x, top: origin.y, right: origin.x + item.width, bottom: origin.y + item.height }
    }

    function verifyInside(item, window, what) {
        verify(item.visible, what + " is not visible")
        const r = rectOf(item)
        verify(r.left >= 0 && r.top >= 0 && r.right <= window.width && r.bottom <= window.height,
               what + " is outside the window: " + JSON.stringify(r) + " in "
               + window.width + "x" + window.height)
    }

    function initTestCase() {
        verify(harness.ready(), "the media set could not be written")
        appModel.completeFirstRun()
        const component = Qt.createComponent(mainQmlUrl)
        verify(component.status === Component.Ready, component.errorString())
        // The size is given at creation, so the window comes up the way a short screen opens it.
        smallWindow = component.createObject(null, { "app": appModel, "width": 1000, "height": 620 })
        verify(smallWindow !== null, "Main.qml did not make a window")
        waitForRendering(smallWindow.contentItem)
        smallWindow.requestActivate()
        wait(100)
    }

    function cleanupTestCase() {
        if (smallWindow) {
            smallWindow.close()
            smallWindow.destroy()
            smallWindow = null
        }
    }

    function init() {
        appModel.dismissError()
        appModel.targetAddress = ""
        appModel.thwIdPos = ""
    }

    function test_01_the_minimum_is_1000_by_620() {
        compare(smallWindow.minimumWidth, 1000)
        compare(smallWindow.minimumHeight, 620)
        compare(smallWindow.width, 1000)
        compare(smallWindow.height, 620)
    }

    function test_02_the_key_controls_are_inside_the_window() {
        for (const name of ["preferencesButton", "aboutButton", "findTargetsButton", "transferUpload",
                            "transferDownload", "readInformationButton", "uploadButton",
                            "statusState", "dockTabActivity", "clearLogButton"])
            verifyInside(find(smallWindow, name), smallWindow, name)
        // The action bar is the last band above the status bar and never scrolls off.
        const upload = rectOf(find(smallWindow, "uploadButton"))
        const status = rectOf(find(smallWindow, "statusState"))
        verify(upload.bottom <= status.top, "the action bar overlaps the status bar")
    }

    function test_03_a_short_window_folds_the_dock_and_the_plan_keeps_its_room() {
        const dock = find(smallWindow, "activityDock")
        verify(!dock.expanded, "the dock is open in a 620 px window")
        compare(dock.height, 44)
        const plan = find(smallWindow, "uploadPanel")
        verify(plan.height >= 340, "the plan is only " + plan.height + " px high")
        // The dock can be opened, and it is a share of the window, not 260 px.
        dock.expanded = true
        tryVerify(function() { return dock.height > 44 }, 2000)
        verify(dock.height >= 150 && dock.height <= 260, "an open dock is " + dock.height + " px")
        verify(plan.height >= 230, "the plan is only " + plan.height + " px high with the dock open")
        dock.expanded = false
        tryVerify(function() { return dock.height === 44 }, 2000)
    }

    function test_04_the_plan_scrolls_and_says_so() {
        const scroll = find(smallWindow, "uploadScroll")
        const flick = scroll.contentItem
        verify(flick.contentHeight > flick.height, "the upload plan fits; the test needs it to overflow")
        const bar = scroll.ScrollBar.vertical
        verify(bar.visible, "there is no visible scroll bar on an overflowing page")
        verify(bar.size < 1.0)
        verify(bar.width >= 10, "the scroll bar is " + bar.width + " px wide")
        // On the trailing edge and as tall as the view: a bar the application supplies to a
        // ScrollView is not laid out by Qt, and used to sit at the top left, 100 px high.
        verify(Math.abs(bar.x + bar.width - scroll.width) <= 1,
               "the scroll bar is at x=" + bar.x + " in a view " + scroll.width + " wide")
        verify(bar.height >= scroll.availableHeight - 1,
               "the scroll bar is " + bar.height + " px high in a view " + scroll.availableHeight)

        // The Payload section is below the fold and reachable by scrolling.
        flick.contentY = flick.contentHeight - flick.height
        wait(30)
        verify(flick.atYEnd)
        flick.contentY = 0
    }

    function test_05_check_tile_details_wrap_instead_of_being_cut() {
        const details = harness.findItems(smallWindow, "checkTileDetail")
        verify(details.length >= 3, "found " + details.length + " tile details")
        for (const detail of details) {
            if (!detail.visible)
                continue
            verify(!detail.truncated, "'" + detail.text + "' is cut to fit")
            verify(detail.lineCount <= 2)
        }
    }

    function test_06_download_destination_is_reachable_by_scrolling() {
        mouseClick(find(smallWindow, "transferDownload"))
        tryVerify(function() { return find(smallWindow, "downloadScroll").visible }, 2000)
        const scroll = find(smallWindow, "downloadScroll")
        const flick = scroll.contentItem
        verify(flick.contentHeight > flick.height, "the download page fits; the test needs it to overflow")
        verify(scroll.ScrollBar.vertical.visible, "no scroll bar on the download page")
        flick.contentY = flick.contentHeight - flick.height
        wait(30)
        verify(flick.atYEnd)
        flick.contentY = 0
        mouseClick(find(smallWindow, "transferUpload"))
        tryVerify(function() { return find(smallWindow, "uploadScroll").visible }, 2000)
    }

    function test_07_the_action_bar_and_status_bar_stay_visible_when_the_dock_is_open() {
        const dock = find(smallWindow, "activityDock")
        dock.expanded = true
        tryVerify(function() { return dock.height > 44 }, 2000)
        verifyInside(find(smallWindow, "uploadButton"), smallWindow, "uploadButton")
        verifyInside(find(smallWindow, "statusState"), smallWindow, "statusState")
        dock.expanded = false
    }

    function test_08_a_tall_window_keeps_a_proportional_dock() {
        const tall = Qt.createComponent(mainQmlUrl).createObject(null, { "app": appModel, "width": 1400, "height": 1000 })
        verify(tall !== null)
        wait(100)
        const dock = find(tall, "activityDock")
        verify(dock.expanded, "the dock is folded in a tall window")
        verify(dock.height <= 260 && dock.height >= 150, "the dock is " + dock.height + " px")
        tall.close()
        tall.destroy()
    }
}

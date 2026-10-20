import QtQuick
import QtQuick.Controls
import QtTest

// The right-to-left languages, Hebrew and Yiddish, switched through the language manager the way
// Preferences switches them, and what the real window then does: the rail moves to the right, the
// header's actions and a segmented control swap their order, a checkbox keeps its box on the
// reading side of its label, a spin box puts its steppers on the end edge and a progress bar fills
// from the right. English is put back after every test, and the window must come back as it was.
TestCase {
    id: testCase

    name: "RightToLeft"
    when: windowShown
    width: 1280
    height: 900

    property var mainWindow: null

    function find(name) {
        const item = harness.findItem(mainWindow, name)
        verify(item !== null, "there is no item called " + name)
        return item
    }

    // Window coordinates of an item's edges.
    function xLeft(item) { return item.mapToItem(null, 0, 0).x }
    function xRight(item) { return item.mapToItem(null, item.width, 0).x }
    function xMiddle(item) { return (xLeft(item) + xRight(item)) / 2 }

    function switchTo(code) {
        verify(languageManager.setLanguage(code), code + " is not a language")
        compare(languageManager.currentLanguage, code)
        wait(100)
    }

    function initTestCase() {
        verify(harness.ready(), "the media set could not be written")
        appModel.completeFirstRun()
        const component = Qt.createComponent(mainQmlUrl)
        verify(component.status === Component.Ready, component.errorString())
        mainWindow = component.createObject(null, { "app": appModel })
        verify(mainWindow !== null, "Main.qml did not make a window")
        mainWindow.width = 1400
        mainWindow.height = 1600
        waitForRendering(mainWindow.contentItem)
        // A media set, so the rail has a load with a checkbox, and a detail line under it.
        appModel.chooseMediaSet(harness.mediaSetUrl())
        tryVerify(function() { return !appModel.busy }, 20000)
        verify(appModel.loadCount > 0, "the media set has no load")
        wait(100)
    }

    function cleanupTestCase() {
        languageManager.setLanguage("en")
        if (mainWindow) {
            mainWindow.close()
            mainWindow.destroy()
            mainWindow = null
        }
    }

    function init() {
        languageManager.setLanguage("en")
        wait(50)
    }

    function test_01_english_reads_left_to_right() {
        verify(!languageManager.rightToLeft)
        compare(Qt.application.layoutDirection, Qt.LeftToRight)
        verify(!mainWindow.contentItem.LayoutMirroring.enabled)
        // The rail is the left column; the header's About is the last action, at the right.
        verify(xMiddle(find("rail")) < mainWindow.width / 2)
        verify(xLeft(find("preferencesButton")) < xLeft(find("aboutButton")))
        verify(xLeft(find("transferUpload")) < xLeft(find("transferDownload")))
        // A load's detail line sits under its label, 26 px in from the left edge.
        const check = find("loadCheck")
        compare(Math.round(xLeft(find("loadDetail"))),
                Math.round(xLeft(harness.findItem(check, "checkLabel"))))
    }

    function test_02_the_directions_of_the_languages_are_the_scripts() {
        const languages = languageManager.availableLanguages()
        const right_to_left = []
        for (let i = 0; i < languages.length; ++i) {
            switchTo(languages[i].code)
            if (languageManager.rightToLeft)
                right_to_left.push(languages[i].code)
            compare(Qt.application.layoutDirection === Qt.RightToLeft,
                    languageManager.rightToLeft, languages[i].code)
        }
        compare(right_to_left.sort(), ["he", "yi"])
    }

    function test_mirrored_window_data() {
        return [ { tag: "Hebrew", code: "he" }, { tag: "Yiddish", code: "yi" } ]
    }

    function test_mirrored_window(data) {
        switchTo(data.code)
        verify(languageManager.rightToLeft)
        compare(Qt.application.layoutDirection, Qt.RightToLeft)
        verify(mainWindow.contentItem.LayoutMirroring.enabled)

        // The rail moves to the right edge, and the work area takes the left.
        const rail = find("rail")
        verify(xMiddle(rail) > mainWindow.width / 2, "the rail is not on the right")
        verify(xRight(rail) <= mainWindow.width + 1)

        // The header's actions swap: Preferences, which came first, is now right of About.
        verify(xLeft(find("aboutButton")) < xLeft(find("preferencesButton")),
               "the header actions did not swap")

        // A mirrored control's pieces swap: the Upload segment is right of the Download one.
        verify(xLeft(find("transferUpload")) > xLeft(find("transferDownload")),
               "the segments did not swap")

        // A checkbox keeps its box on the reading side of its label, and the detail under it
        // starts where the label does (the box and its gap are the indent).
        const check = find("loadCheck")
        const box = harness.findItem(check, "checkBox")
        const label = harness.findItem(check, "checkLabel")
        verify(box !== null && label !== null)
        verify(xLeft(box) > xLeft(label), "the box is not right of its label")
        compare(Math.round(xRight(label)), Math.round(xLeft(box) - 10),
                "the gap between the box and its label")
        const detail = find("loadDetail")
        compare(Math.round(xRight(detail)), Math.round(xRight(label)),
                "the detail is not indented from the right")
    }

    function test_a_spin_box_puts_its_steppers_on_the_end_edge() {
        const preferences = find("preferencesButton")
        mouseClick(preferences)
        const dialog = find("preferencesDialog")
        tryVerify(function() { return dialog.opened })
        const spin = harness.findItem(dialog, "controlPortSpin")
        verify(spin !== null)
        // English: the steppers at the right; Hebrew: at the left. The dialog is a popup, which
        // follows the window's direction.
        verify(spin.up.indicator.x > spin.width / 2)
        switchTo("he")
        verify(spin.mirrored)
        verify(spin.up.indicator.x < spin.width / 2, "the steppers did not move to the left")
        verify(spin.contentItem.x > spin.up.indicator.width - 1,
               "the number overlaps the steppers")
        dialog.close()
        tryVerify(function() { return !dialog.opened })
    }

    function test_a_progress_bar_fills_from_the_start_edge() {
        const url = mainQmlUrl.toString().replace(/Main\.qml$/, "TaskProgress.qml")
        const component = Qt.createComponent(url)
        verify(component.status === Component.Ready, component.errorString())
        const bar = component.createObject(mainWindow.contentItem,
                                           { "x": 100, "y": 100, "width": 400, "value": 10 })
        verify(bar !== null)
        const fill = bar.contentItem.children[0]
        wait(50)
        // 10% of 400 px, flush with the left edge.
        compare(Math.round(fill.width), 40)
        compare(Math.round(fill.x), 0)

        switchTo("he")
        wait(50)
        // The same 10%, flush with the right edge - not 90% of the bar.
        compare(Math.round(fill.width), 40)
        compare(Math.round(fill.x + fill.width), Math.round(bar.contentItem.width))

        // The indeterminate bar travels from the start edge too: at whatever point of its run it
        // is (the animation may or may not be running, by the desktop's reduced-motion setting),
        // it is that far along the free space from the right edge.
        bar.indeterminate = true
        wait(20)
        const track = bar.contentItem
        const free = track.width - fill.width
        compare(Math.round(fill.x), Math.round(free * (1 - track.phase)))
        bar.destroy()
    }

    function test_english_puts_the_window_back() {
        switchTo("he")
        switchTo("en")
        verify(!languageManager.rightToLeft)
        verify(xMiddle(find("rail")) < mainWindow.width / 2)
        verify(xLeft(find("preferencesButton")) < xLeft(find("aboutButton")))
        verify(xLeft(find("transferUpload")) < xLeft(find("transferDownload")))
    }
}

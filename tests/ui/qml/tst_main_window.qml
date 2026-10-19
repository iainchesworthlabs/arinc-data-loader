import QtQuick
import QtQuick.Controls
import QtTest

// The loader's window driven the way an operator drives it: the real Main.qml, the real LoaderModel
// on its engine thread, and the simulated target on the fake platform. Every operation is started
// by clicking the window's own button, and the assertions read what the window shows and what the
// target or the fake file system received.
TestCase {
    id: testCase

    name: "MainWindow"
    when: windowShown
    width: 1280
    height: 900

    property var mainWindow: null

    function find(name) {
        const item = harness.findItem(mainWindow, name)
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

    // Qt Quick Test has keyClick but no keyClicks: one event per character, so the text field's
    // own handler and the model's setter are what receive it.
    function typeText(item, text) {
        if (item.window)
            item.window.requestActivate()
        item.forceActiveFocus()
        for (let i = 0; i < text.length; ++i) {
            const character = text.charAt(i)
            const code = character.charCodeAt(0)
            // Qt key codes for letters are the uppercase code with the shift state, whatever the
            // case of the character; lowercase text is the same key without Shift.
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

    // The DNLD_DATA_<THW_ID_POS>_<number> directory the last download was written into: each
    // download takes the next number, so the test reads it back from the model instead of
    // assuming which one this run got.
    function lastDownloadNumber() {
        const directory = appModel.lastDownloadDirectory
        return parseInt(directory.substring(directory.lastIndexOf("_") + 1))
    }

    function pointAtTheTarget() {
        appModel.targetAddress = "10.0.0.2"
        appModel.thwIdPos = "HNPFMS_L"
    }

    function readTheMediaSet() {
        appModel.chooseMediaSet(harness.mediaSetUrl())
        tryVerify(function() { return !appModel.busy }, 20000)
        verify(appModel.validationState === 2,
               "the media set did not validate: " + appModel.mediaSetError + " / "
               + appModel.validationError)
    }

    function initTestCase() {
        verify(harness.ready(), "the media set could not be written")
        const component = Qt.createComponent(mainQmlUrl)
        verify(component.status === Component.Ready, component.errorString())
        mainWindow = component.createObject(null, { "app": appModel })
        verify(mainWindow !== null, "Main.qml did not make a window")
        // Tall enough that the whole left column (the operation buttons included) is on screen:
        // a mouse click on an item scrolled out of view is not a click the window receives.
        mainWindow.width = 1400
        mainWindow.height = 1600
        waitForRendering(mainWindow.contentItem)
        wait(50)
        compare(mainWindow.title, "ARINC Data Loader")
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
        harness.resetDownloads()
        appModel.dismissError()
        appModel.clearLog()
        appModel.downloadMode = 0
        appModel.autoFindBeforeOperation = false
        appModel.validateOnOpen = true
        appModel.downloadDirectory = harness.downloadDirectory()
        appModel.targetAddress = ""
        appModel.thwIdPos = ""
        // A dialog left open by a failed test would sit over the window and eat the next test's
        // clicks; close it here rather than letting one failure cascade.
        const dialog = harness.findItem(mainWindow, "errorDialog")
        if (dialog !== null && dialog.visible)
            dialog.close()
        wait(10)
    }

    function test_01_the_window_starts_idle() {
        compare(find("statusState").text, "Idle")
        compare(find("statusTarget").text, "no target")
        verify(find("findTargetsButton").enabled)
        verify(!find("uploadButton").enabled)
        verify(!find("abortButton").enabled)
        verify(!find("mediaSetCheckButton").enabled)
    }

    function test_02_typing_a_target_updates_the_model_and_the_status_bar() {
        const address = find("targetAddressField")
        address.text = ""
        typeText(address, "10.0.0.2")
        tryCompare(appModel, "targetAddress", "10.0.0.2")
        compare(find("statusTarget").text, "10.0.0.2")

        const thw = find("thwIdPosField")
        thw.text = ""
        typeText(thw, "hnpfms_l")
        tryCompare(appModel, "thwIdPos", "hnpfms_l")
        verify(appModel.targetValid)
        verify(find("readInformationButton").enabled)
    }

    function test_03_find_lists_the_target_and_clicking_it_selects_it() {
        harness.resetTarget()
        click("findTargetsButton")
        tryVerify(function() { return harness.targetCount() === 1 }, 20000)
        const list = find("targetList")
        verify(list.visible)
        compare(list.count, 1)
        tryVerify(function() { return list.itemAtIndex(0) !== null }, 5000)

        const delegate = list.itemAtIndex(0)
        verify(delegate.visible)
        mouseClick(delegate)
        tryCompare(appModel, "selectedTargetIndex", 0)
        compare(appModel.targetAddress, "10.0.0.2")
        compare(appModel.thwIdPos, "HNPFMS_L")
        verify(find("targetPropertiesButton").enabled)
    }

    function test_04_read_information_fills_the_configuration_panel() {
        pointAtTheTarget()
        harness.resetTarget()
        click("readInformationButton")
        tryCompare(appModel, "resultState", 1, 20000)
        tryVerify(function() { return !appModel.busy }, 20000)
        verify(harness.configurationCount() > 0)
        compare(find("configurationList").count, harness.configurationCount())
        const result = find("resultLabel")
        verify(result.visible)
        verify(result.text.length > 0)
        verify(harness.eventCount() > 0)
    }

    function test_05_the_media_set_is_checked_and_its_loads_can_be_ticked() {
        pointAtTheTarget()
        harness.resetTarget()
        readTheMediaSet()
        verify(harness.loadCount() > 0)
        verify(find("mediaSetCheckButton").enabled)
        verify(find("mediaSetPropertiesButton").enabled)

        click("mediaSetCheckButton")
        tryVerify(function() { return !appModel.busy && appModel.validationState === 2 }, 20000)

        click("tickAllLoadsButton")
        tryCompare(appModel, "selectedLoadCount", harness.loadCount())
        click("tickNoLoadsButton")
        tryCompare(appModel, "selectedLoadCount", 0)

        const check = find("loadCheck")
        mouseClick(check)
        tryCompare(appModel, "selectedLoadCount", 1)
        verify(find("uploadButton").enabled)
    }

    function test_06_upload_is_started_from_the_windows_button() {
        pointAtTheTarget()
        harness.resetTarget()
        readTheMediaSet()
        click("tickAllLoadsButton")
        verify(find("uploadButton").enabled)

        click("uploadButton")
        tryCompare(appModel, "resultState", 1, 20000)
        tryVerify(function() { return !appModel.busy }, 20000)
        compare(harness.targetFilesReadCount(), harness.expectedUploadFiles().length)
        compare(harness.targetFilesRead().join(","), harness.expectedUploadFiles().join(","))
        compare(appModel.progress, 100)
    }

    function test_10_media_defined_download_is_named_and_started_from_the_window() {
        pointAtTheTarget()
        harness.resetTarget()
        harness.resetDownloads()
        click("downloadModeMedia")
        compare(appModel.downloadMode, 0)

        const name = find("downloadFileName")
        typeText(name, "a.bin")
        compare(name.text, "a.bin")
        tryVerify(function() { return find("addDownloadButton").enabled })
        click("addDownloadButton")
        tryCompare(appModel, "canDownload", true)
        compare(harness.downloadCount(), 1)

        click("downloadButton")
        tryVerify(function() { return !appModel.busy }, 20000)
        verify(appModel.resultState === 1,
               "the download failed: " + appModel.resultText + " / " + appModel.resultDetail
               + "\n" + harness.eventText())
        verify(appModel.lastDownloadDirectory.length > 0)
        const number = lastDownloadNumber()
        verify(harness.downloadedFileExists("a.bin", number))
        compare(harness.downloadedFileText("a.bin", number), "downloaded")
    }

    function test_11_operator_defined_download_takes_the_choice_from_the_window() {
        pointAtTheTarget()
        harness.resetTarget()
        harness.resetDownloads()
        click("downloadModeOperator")
        tryCompare(appModel, "downloadMode", 1)

        click("downloadButton")
        tryVerify(function() { return appModel.downloadChoicePending }, 20000)
        compare(harness.downloadCount(), 2)
        verify(find("confirmDownloadButton").enabled)

        click("confirmDownloadButton")
        tryCompare(appModel, "resultState", 1, 20000)
        tryVerify(function() { return !appModel.busy }, 20000)
        const number = lastDownloadNumber()
        verify(harness.downloadedFileExists("a.bin", number))
        verify(harness.downloadedFileExists("b.bin", number))
    }

    function test_07_a_failure_opens_the_error_dialog() {
        pointAtTheTarget()
        harness.resetTarget()
        harness.makeTargetRefuse()
        click("readInformationButton")
        tryCompare(appModel, "resultState", 2, 20000)
        tryVerify(function() { return !appModel.busy }, 20000)

        const dialog = find("errorDialog")
        tryVerify(function() { return dialog.visible }, 10000)
        verify(dialog.text.length > 0, dialog.text)
        verify(dialog.text.indexOf(appModel.lastErrorText) === 0, dialog.text)
        verify(appModel.lastErrorText.length > 0)

        const ok = dialog.standardButton(Dialog.Ok)
        verify(ok !== null)
        mouseClick(ok)
        tryVerify(function() { return !dialog.visible })
        verify(appModel.lastErrorText.length === 0)
    }

    function test_08_abort_stops_a_running_upload() {
        pointAtTheTarget()
        harness.resetTarget()
        readTheMediaSet()
        click("tickAllLoadsButton")

        // Hold the engine just before it takes the host, so the job is certainly still running
        // when the Abort button is clicked, however fast the fake platform would be.
        harness.holdNextJob()
        click("uploadButton")
        tryVerify(function() { return appModel.busy }, 10000)
        verify(find("abortButton").enabled)
        click("abortButton")
        harness.releaseJob()
        tryCompare(appModel, "resultState", 2, 20000)
        tryVerify(function() { return !appModel.busy }, 20000)

        // The aborted operation is a failure, so the window's error dialog opened; acknowledge
        // it so the next test starts with no dialog in the way.
        const dialog = find("errorDialog")
        if (dialog.visible) {
            const ok = dialog.standardButton(Dialog.Ok)
            if (ok !== null) {
                mouseClick(ok)
                tryVerify(function() { return !dialog.visible })
            }
        }
    }

    function test_09_clear_empties_the_activity_log() {
        pointAtTheTarget()
        harness.resetTarget()
        click("readInformationButton")
        tryCompare(appModel, "resultState", 1, 20000)
        tryVerify(function() { return !appModel.busy }, 20000)
        verify(harness.eventCount() > 0)

        click("clearLogButton")
        tryVerify(function() { return harness.eventCount() === 0 })
    }
}

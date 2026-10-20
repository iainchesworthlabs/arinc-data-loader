import QtQuick
import QtQuick.Controls
import QtTest

// The window's dialogs, opened the way the window opens them and driven through their own fields
// and standard buttons: preferences (which write through to the model and QSettings), the media
// set and target properties, and About.
TestCase {
    id: testCase

    name: "Dialogs"
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

    function pointAtTheTarget() {
        appModel.targetAddress = "10.0.0.2"
        appModel.thwIdPos = "HNPFMS_L"
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
        harness.resetTarget()
        harness.resetDownloads()
        appModel.dismissError()
        appModel.autoFindBeforeOperation = false
        appModel.validateOnOpen = true
        appModel.controlPort = 59
        appModel.logCapacity = 2000
        appModel.defaultTargetAddress = ""
        appModel.defaultThwIdPos = ""
        pointAtTheTarget()
        wait(10)
    }

    function test_01_preferences_write_through_to_the_model() {
        const dialog = find("preferencesDialog")
        dialog.open()
        tryVerify(function() { return dialog.visible })

        const address = findIn(dialog, "defaultTargetAddressField")
        address.text = ""
        typeText(address, "10.9.8.7")

        const thw = findIn(dialog, "defaultThwIdPosField")
        thw.text = ""
        typeText(thw, "test_1")

        const validate = findIn(dialog, "validateOnOpenSwitch")
        mouseClick(validate)

        const autoFind = findIn(dialog, "autoFindSwitch")
        mouseClick(autoFind)

        const directory = findIn(dialog, "downloadDirectoryField")
        directory.text = harness.downloadDirectory()
        directory.textEdited()

        const port = findIn(dialog, "controlPortSpin")
        port.forceActiveFocus()
        keyClick(Qt.Key_Up)   // Qt Quick Test sends keys to the active window

        // Preferences is transactional: the fields edit a working copy and only Save writes it
        // through, which is what Cancel relies on.
        const ok = dialog.standardButton(Dialog.Ok)
        verify(ok !== null)
        mouseClick(ok)
        tryVerify(function() { return !dialog.visible })

        tryCompare(appModel, "defaultTargetAddress", "10.9.8.7")
        tryCompare(appModel, "defaultThwIdPos", "test_1")
        tryCompare(appModel, "validateOnOpen", false)
        tryCompare(appModel, "autoFindBeforeOperation", true)
        tryCompare(appModel, "downloadDirectory", harness.downloadDirectory())
        tryCompare(appModel, "controlPort", 60)
    }

    function test_02_media_set_properties_show_what_was_read() {
        appModel.chooseMediaSet(harness.mediaSetUrl())
        tryVerify(function() { return !appModel.busy && appModel.validationState === 2 }, 20000)

        const dialog = find("mediaSetPropertiesDialog")
        dialog.open()
        tryVerify(function() { return dialog.visible })
        const partNumber = findIn(dialog, "mediaSetPartNumberLabel")
        verify(partNumber.text.length > 0)
        // The tiles show the figures of the set that was read, not what they held when the
        // window was built (before any set was chosen).
        verify(harness.loadCount() > 0)
        verify(harness.fileCount() > 0)
        compare(findIn(dialog, "mediaSetLoadCountLabel").text, String(harness.loadCount()))
        compare(findIn(dialog, "mediaSetFileCountLabel").text, String(harness.fileCount()))

        const close = dialog.standardButton(Dialog.Close)
        verify(close !== null)
        mouseClick(close)
        tryVerify(function() { return !dialog.visible })
    }

    function test_03_target_properties_show_the_find_answer() {
        harness.resetTarget()
        appModel.findTargets()
        tryVerify(function() { return !appModel.busy && harness.targetCount() === 1 }, 20000)
        appModel.selectTarget(0)
        tryCompare(appModel, "selectedTargetIndex", 0)

        const dialog = find("targetPropertiesDialog")
        dialog.open()
        tryVerify(function() { return dialog.visible })
        compare(appModel.selectedTargetAddress, "10.0.0.2")

        const close = dialog.standardButton(Dialog.Close)
        verify(close !== null)
        mouseClick(close)
        tryVerify(function() { return !dialog.visible })
    }

    function test_04_about_opens_and_closes() {
        const dialog = find("aboutDialog")
        dialog.open()
        tryVerify(function() { return dialog.visible })
        verify(dialog.title.indexOf("ARINC Data Loader") >= 0)

        const close = dialog.standardButton(Dialog.Close)
        verify(close !== null)
        mouseClick(close)
        tryVerify(function() { return !dialog.visible })
    }

    // About > Licences shows the notices the build embedded: Qt's LGPL, the typeface's OFL, and
    // the programs' own terms, not a placeholder.
    function test_05_about_shows_the_third_party_licences() {
        const about = find("aboutDialog")
        about.open()
        tryVerify(function() { return about.visible })

        const licencesButton = about.standardButton(Dialog.Cancel)
        verify(licencesButton !== null)
        compare(licencesButton.text, "Licences")
        mouseClick(licencesButton)

        const dialog = find("licencesDialog")
        tryVerify(function() { return dialog.visible })
        const text = findIn(dialog, "licenceNoticesText")
        tryVerify(function() { return text.text.length > 0 })
        verify(text.text.indexOf("GNU Lesser General Public License".toUpperCase()) >= 0,
               "the LGPL text is missing")
        verify(text.text.indexOf("SIL OPEN FONT LICENSE") >= 0, "the OFL text is missing")
        verify(text.text.indexOf("Archivo") >= 0)
        verify(text.text.indexOf("Noto Sans Hebrew") >= 0, "the Hebrew typeface is not credited")
        verify(text.text.indexOf("Qt 6.") >= 0, "the Qt version is missing")
        verify(text.text.indexOf("{{") < 0, "a placeholder was left in the notices")
        verify(text.text.indexOf("not compiled in") < 0, "the build embedded no notices")

        mouseClick(dialog.standardButton(Dialog.Close))
        tryVerify(function() { return !dialog.visible })
        // The About dialog stays open under it, as it was.
        verify(about.visible)
        mouseClick(about.standardButton(Dialog.Close))
        tryVerify(function() { return !about.visible })
    }
}

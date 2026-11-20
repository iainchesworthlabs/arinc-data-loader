// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtTest

// What the loader's window shows about the licence, in every state a licence can be in: the edition chip in the
// status bar, the notice under the header, the Licence dialog (what the licence says, what it includes, the
// files found, importing and removing), the Support dialog and the edition line of About. The licence is the
// suite's `licence` (files in memory, the published test key, a time of its own), and `licenceRig` puts it into a
// state. Nothing is gated: the same window works in every state, and these tests only read what it says.
TestCase {
    id: testCase

    name: "Licence"
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

    function open(dialogName) {
        const dialog = find(dialogName)
        dialog.open()
        tryVerify(function() { return dialog.opened })
        return dialog
    }

    function closeDialog(dialog) {
        dialog.close()
        tryVerify(function() { return !dialog.visible })
    }

    function initTestCase() {
        verify(harness.ready(), "the media set could not be written")
        appModel.completeFirstRun()
        const component = Qt.createComponent(mainQmlUrl)
        verify(component.status === Component.Ready, component.errorString())
        mainWindow = component.createObject(null, { "app": appModel })
        verify(mainWindow !== null, "Main.qml did not make a window")
        mainWindow.width = 1400
        mainWindow.height = 1200
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

    function init() {
        licenceRig.use("none")
    }

    // The words in the status bar for each state, and whether a notice shows under the header.
    function test_the_chip_says_the_edition_in_every_state() {
        const chip = find("editionChip")
        licenceRig.use("none")
        compare(chip.text, "Community")
        compare(chip.tone, "neutral")

        licenceRig.use("valid")
        compare(chip.text, "Pro")
        compare(chip.tone, "ok")

        licenceRig.use("expiring")
        verify(chip.text.indexOf("Pro, ") === 0 && chip.text.indexOf("day") > 0, chip.text)
        compare(chip.tone, "warn")

        licenceRig.use("grace")
        compare(chip.text, "Pro, licence ended")
        compare(chip.tone, "warn")

        licenceRig.use("expired")
        compare(chip.text, "Community")
        compare(chip.tone, "warn")

        licenceRig.use("not_yet_valid")
        compare(chip.text, "Community")

        licenceRig.use("clock_suspect")
        compare(chip.text, "Community")
        compare(chip.tone, "warn")

        licenceRig.use("wrong_computer")
        compare(chip.text, "Community")
        verify(chip.Accessible.name.indexOf("Edition: Community") === 0, chip.Accessible.name)
    }

    function test_the_chip_opens_the_licence_dialog_with_the_mouse_and_the_keyboard() {
        const chip = find("editionChip")
        const dialog = find("licenceDialog")
        licenceRig.use("valid")
        mouseClick(chip)
        tryVerify(function() { return dialog.opened })
        compare(findIn(dialog, "licenceEdition").text, "Pro")
        compare(findIn(dialog, "licenceState").text, "Valid")
        verify(findIn(dialog, "licenceSentence").text.indexOf("Valid until") === 0)
        closeDialog(dialog)
        // The chip is reached by the keyboard, and Enter opens the dialog again.
        mainWindow.requestActivate()
        chip.forceActiveFocus()
        verify(chip.activeFocus)
        keyClick(Qt.Key_Return)
        tryVerify(function() { return dialog.opened })
        closeDialog(dialog)
    }

    function test_every_state_can_be_shown_in_the_dialog() {
        const dialog = find("licenceDialog")
        const states = ["none", "valid", "expiring", "grace", "expired", "not_yet_valid", "clock_suspect",
                        "wrong_computer", "damaged", "two_files"]
        for (let i = 0; i < states.length; ++i) {
            licenceRig.use(states[i])
            dialog.open()
            tryVerify(function() { return dialog.opened })
            verify(findIn(dialog, "licenceState").text.length > 0, states[i])
            verify(findIn(dialog, "licenceSentence").text.length > 0, states[i])
            closeDialog(dialog)
        }
    }

    function test_the_dialog_says_what_a_licence_includes_and_why_not() {
        const dialog = find("licenceDialog")
        licenceRig.use("none")
        dialog.open()
        tryVerify(function() { return dialog.opened })
        verify(harness.findItemByText(dialog, "Needs a Pro or Enterprise licence.") !== null)
        verify(harness.findItemByText(dialog, "Needs an Enterprise licence.") !== null)
        closeDialog(dialog)

        licenceRig.use("valid")
        dialog.open()
        tryVerify(function() { return dialog.opened })
        verify(harness.findItemByText(dialog, "Your licence does not include this.") !== null)
        verify(harness.findItemByText(dialog, "Example Lab Pty Ltd") !== null)
        verify(harness.findItemByText(dialog, "ADL-2026-000123") !== null)
        closeDialog(dialog)

        licenceRig.use("wrong_computer")
        dialog.open()
        tryVerify(function() { return dialog.opened })
        compare(findIn(dialog, "licenceState").text, "Not valid")
        verify(harness.findItemByText(dialog, "This licence is for other computers. This computer is LAB-RIG-07. Ask your administrator for a licence that names it.") !== null)
        closeDialog(dialog)
    }

    function test_two_files_are_listed_and_the_better_one_is_used() {
        licenceRig.use("two_files")
        const dialog = open("licenceDialog")
        const files = findIn(dialog, "licenceFiles")
        verify(files.visible)
        verify(harness.findItemByText(dialog, "Installed by an administrator") !== null)
        compare(findIn(dialog, "licenceState").text, "Valid")
        closeDialog(dialog)
        licenceRig.use("valid")
        const again = open("licenceDialog")
        verify(!findIn(again, "licenceFiles").visible)
        closeDialog(again)
    }

    function test_a_notice_shows_under_the_header_and_can_be_put_away() {
        const banner = find("licenceBanner")
        licenceRig.use("valid")
        verify(!banner.visible)

        licenceRig.use("expiring")
        verify(banner.visible)
        verify(banner.text.indexOf("Your licence ends on") === 0, banner.text)
        mouseClick(findIn(banner, "licenceNoticeDismiss"))
        verify(!banner.visible)

        // A licence in its grace period warns, and "Remind me tomorrow" puts it away for the day.
        licenceRig.use("grace")
        verify(banner.visible)
        compare(licence.noticeDismissText, "Remind me tomorrow")
        mouseClick(findIn(banner, "licenceNoticeDismiss"))
        verify(!banner.visible)

        // The notice has a button for the Licence dialog.
        licenceRig.use("clock_suspect")
        verify(banner.visible)
        const dialog = find("licenceDialog")
        mouseClick(findIn(banner, "licenceNoticeOpen"))
        tryVerify(function() { return dialog.opened })
        closeDialog(dialog)
    }

    function test_about_says_the_edition() {
        licenceRig.use("valid")
        const about = open("aboutDialog")
        compare(findIn(about, "aboutEdition").text, "Edition: Pro.")
        const licenceDialog = find("licenceDialog")
        mouseClick(findIn(about, "aboutLicence"))
        tryVerify(function() { return licenceDialog.opened })
        verify(!about.visible)
        closeDialog(licenceDialog)
    }

    function test_the_support_dialog_copies_and_opens_nothing() {
        licenceRig.use("valid")
        const licenceDialog = open("licenceDialog")
        const support = find("supportDialog")
        findIn(licenceDialog, "licenceSupport").click()
        tryVerify(function() { return support.opened })
        verify(!licenceDialog.visible)
        compare(findIn(support, "supportLabel").text, "Support included")
        // The buttons are in a scrolling page, so they are pressed as a keyboard user presses them.
        findIn(support, "copyLicenceNumber").click()
        compare(licenceRig.clipboardText(), "ADL-2026-000123")
        verify(findIn(support, "copiedMessage").visible)
        findIn(support, "copyDetails").click()
        verify(licenceRig.clipboardText().indexOf("Licence number: ADL-2026-000123") >= 0)
        closeDialog(support)

        licenceRig.use("none")
        const again = open("supportDialog")
        compare(findIn(again, "supportLabel").text, "Community support")
        findIn(again, "copyIssuesAddress").click()
        compare(licenceRig.clipboardText(), licence.issuesAddress)
        verify(!findIn(again, "copyLicenceNumber").visible)
        closeDialog(again)
    }

    function test_importing_and_removing_a_licence_through_the_dialog() {
        const dialog = open("licenceDialog")
        compare(findIn(dialog, "licenceState").text, "Community")
        verify(!findIn(dialog, "licenceRemove").visible)

        licence.importFile(licenceRig.licenceFile("valid"))
        compare(findIn(dialog, "licenceEdition").text, "Pro")
        const result = findIn(dialog, "licenceImportResult")
        verify(result.visible)
        compare(result.text, "Licence imported")
        compare(result.detail, "This is now the Pro edition.")
        compare(licenceRig.filesInUserFolder(), 1)
        verify(findIn(dialog, "licenceRemove").visible)

        // A file that is not a licence changes nothing and says why.
        licence.importFile(licenceRig.licenceFile("damaged"))
        compare(result.text, "Could not import the licence")
        verify(result.detail.indexOf("This file is not a valid ARINC Data Loader licence") === 0)
        compare(findIn(dialog, "licenceEdition").text, "Pro")
        compare(licenceRig.filesInUserFolder(), 1)

        // Removing asks first, and the safe answer is the default.
        findIn(dialog, "licenceRemove").click()
        const confirm = find("removeLicenceDialog")
        tryVerify(function() { return confirm.opened })
        verify(confirm.secondaryItem.activeFocus)
        confirm.secondaryItem.click()
        tryVerify(function() { return !confirm.visible })
        compare(licenceRig.filesInUserFolder(), 1)

        findIn(dialog, "licenceRemove").click()
        tryVerify(function() { return confirm.opened })
        confirm.primaryItem.click()
        tryVerify(function() { return !confirm.visible })
        compare(licenceRig.filesInUserFolder(), 0)
        compare(findIn(dialog, "licenceEdition").text, "Community")
        closeDialog(dialog)
    }

    function test_nothing_is_switched_off_by_a_licence() {
        // The same controls work in every state: the preferences button is enabled whatever the licence is.
        const states = ["none", "valid", "expired", "wrong_computer", "damaged"]
        for (let i = 0; i < states.length; ++i) {
            licenceRig.use(states[i])
            verify(find("preferencesButton").enabled, states[i])
            verify(find("aboutButton").enabled, states[i])
        }
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The third-party notices this build carries, as the text of the NOTICES.txt the build places
// beside the executables: the copy embedded at build time is what is shown, so this view and the
// file cannot say different things (cmake/AdlUi.cmake writes both from one source). Reached from
// About > Licences. The same dialog in all three windows.
AppDialog {
    id: dialog

    readonly property string missing: qsTr("This build carries no embedded notices file "
                                           + "(:/adl/notices/NOTICES.txt was not compiled in); "
                                           + "the NOTICES.txt beside the application and the "
                                           + "repository's LICENSE say what it ships.")

    // set by main.cpp from adl::ui::licence_notices() (appLicenceNotices); a harness that does
    // not register it gets the sentence that says so rather than an empty view.
    readonly property string notices: typeof appLicenceNotices === "string"
                                      && appLicenceNotices.length > 0
                                      ? appLicenceNotices : dialog.missing

    objectName: "licencesDialog"
    heading: qsTr("Third-party licences")
    primaryText: qsTr("Close")
    width: Math.min(780, Overlay.overlay ? Overlay.overlay.width - 60 : 780)
    height: Math.min(660, Overlay.overlay ? Overlay.overlay.height - 60 : 660)

    // Focus on the text, so the arrow and page keys scroll it and Escape reaches the dialog.
    initialFocusItem: noticesText

    content: ScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        ScrollBar.vertical: AppScrollBar { inScrollView: true }

        TextArea {
            id: noticesText

            objectName: "licenceNoticesText"
            readOnly: true
            selectByMouse: true
            wrapMode: TextEdit.Wrap
            textFormat: TextEdit.PlainText
            text: dialog.notices
            color: Theme.text
            font.family: Theme.monoFamily
            font.pixelSize: Theme.fontSmall
            Accessible.name: qsTr("Third-party licence text")
            background: Rectangle {
                color: Theme.neutral100
                border.color: noticesText.activeFocus ? Theme.focusRing : Theme.divider
                border.width: noticesText.activeFocus ? 2 : 1
                radius: Theme.radiusControl
            }
        }
    }

    onPrimaryClicked: dialog.close()
}

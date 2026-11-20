// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Where to get help, and what to send so that a helper can understand the setup. Three cards: what the
// licence says about support, what to write, and the details of this copy that a helper asks for first.
//
// The dialog copies to the clipboard and does nothing else: it opens no browser and no mail program, and
// it sends nothing. The words are the LicenceModel's (supportLabel, supportText, supportDetails).
AppDialog {
    id: dialog

    // The LicenceModel, a context property of every program.
    property var model: null

    // What was last copied, said once in words (a screen reader hears it) and put away after a moment.
    property string copied: ""

    function copy(text, what) {
        dialog.model.copyText(text);
        dialog.copied = what;
        copiedTimer.restart();
    }

    objectName: "supportDialog"
    heading: qsTr("Support")
    primaryText: qsTr("Close")
    width: Math.min(760, Overlay.overlay ? Overlay.overlay.width - 60 : 760)
    height: Math.min(660, Overlay.overlay ? Overlay.overlay.height - 60 : 660)
    onPrimaryClicked: dialog.close()

    Timer {
        id: copiedTimer

        interval: 4000
        onTriggered: dialog.copied = ""
    }

    content: ScrollView {
        id: scroll

        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        contentWidth: availableWidth
        ScrollBar.vertical: AppScrollBar { inScrollView: true }

        ColumnLayout {
            width: scroll.availableWidth
            spacing: Theme.space3

            Text {
                Layout.fillWidth: true
                text: qsTr("Where to get help, and what to send so that a helper can understand your setup.")
                color: Theme.textMuted
                font.pixelSize: Theme.fontBody
                wrapMode: Text.WordWrap
            }

            Card {
                title: qsTr("Your support")

                StatusChip {
                    objectName: "supportLabel"
                    text: dialog.model !== null ? dialog.model.supportLabel : ""
                    tone: dialog.model !== null ? dialog.model.supportTone : "neutral"
                }

                Text {
                    objectName: "supportText"
                    Layout.fillWidth: true
                    text: dialog.model !== null ? dialog.model.supportText : ""
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space2

                    AppButton {
                        objectName: "copyLicenceNumber"
                        visible: dialog.model !== null && dialog.model.licenceNumber.length > 0
                        text: qsTr("Copy licence number")
                        onClicked: dialog.copy(dialog.model.licenceNumber, qsTr("Licence number copied."))
                    }

                    AppButton {
                        objectName: "copyIssuesAddress"
                        text: qsTr("Copy issues address")
                        onClicked: dialog.copy(dialog.model.issuesAddress, qsTr("Issues address copied."))
                    }

                    AppButton {
                        objectName: "copyProjectAddress"
                        text: qsTr("Copy project address")
                        onClicked: dialog.copy(dialog.model.projectAddress, qsTr("Project address copied."))
                    }

                    Item {
                        Layout.fillWidth: true
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("ARINC Data Loader never sends anything itself. You choose what to share.")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                    wrapMode: Text.WordWrap
                }
            }

            Card {
                title: qsTr("Before you write")

                Text {
                    Layout.fillWidth: true
                    text: qsTr("1. Say what you expected and what happened instead.")
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                    wrapMode: Text.WordWrap
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("2. Say which program and which operation it happened with.")
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                    wrapMode: Text.WordWrap
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("3. Add the details of this copy, below, and the text of the activity log.")
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                    wrapMode: Text.WordWrap
                }
            }

            Card {
                title: qsTr("This copy of ARINC Data Loader")

                Text {
                    objectName: "supportDetails"
                    Layout.fillWidth: true
                    text: dialog.model !== null ? dialog.model.supportDetails : ""
                    color: Theme.text
                    font.pixelSize: Theme.fontSmall
                    font.family: Theme.monoFamily
                    wrapMode: Text.WrapAnywhere
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space2

                    AppButton {
                        objectName: "copyDetails"
                        text: qsTr("Copy details")
                        onClicked: dialog.copy(dialog.model.supportDetails, qsTr("Details copied."))
                    }

                    Item {
                        Layout.fillWidth: true
                    }
                }
            }

            Text {
                objectName: "copiedMessage"
                Layout.fillWidth: true
                visible: dialog.copied.length > 0
                text: dialog.copied
                color: Theme.text
                font.pixelSize: Theme.fontBody
                font.weight: Font.DemiBold
                Accessible.role: Accessible.StaticText
                Accessible.name: text
            }
        }
    }
}

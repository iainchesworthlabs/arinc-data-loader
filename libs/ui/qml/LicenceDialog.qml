// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// Which edition this is, whose licence it is, for how long, what the licence includes and why, every licence
// file found, and the two things a person does: import a licence file and remove one. The same dialog in all
// three programs, reached from the edition chip, from About and from the licence notice.
//
// The dialog shows what the LicenceModel says and decides nothing: it never reads a file and never judges a
// state. Nothing in a program is switched off by a licence, so "what this edition includes" lists what a
// licence adds (support, data packs) and says why a line is not available, in the model's words.
AppDialog {
    id: dialog

    // The LicenceModel, a context property of every program.
    property var model: null

    // The Support dialog is the window's: the dialog asks, the window opens it.
    signal supportRequested()

    objectName: "licenceDialog"
    heading: qsTr("Licence")
    primaryText: qsTr("Close")
    secondaryText: qsTr("Import licence...")
    width: Math.min(760, Overlay.overlay ? Overlay.overlay.width - 60 : 760)
    height: Math.min(700, Overlay.overlay ? Overlay.overlay.height - 60 : 700)

    // The rows of the summary: a row with no value is left out, not shown empty.
    readonly property var summaryRows: dialog.model === null ? [] : [
        { "label": qsTr("Licensed to"), "value": dialog.model.licensee, "mono": false },
        { "label": qsTr("Licence number"), "value": dialog.model.licenceNumber, "mono": true },
        { "label": qsTr("Term"), "value": dialog.model.term, "mono": false },
        { "label": qsTr("Seats"), "value": dialog.model.seats, "mono": false },
        { "label": qsTr("Computers"), "value": dialog.model.computers, "mono": true }
    ]

    function importToneOf(kind) {
        return kind === "ok" ? "success" : kind === "warn" ? "warning" : kind === "error" ? "error" : "info";
    }

    // What the person sees is the model's, read again each time the dialog opens.
    Connections {
        target: dialog

        function onAboutToShow() {
            if (dialog.model !== null)
                dialog.model.refresh();
        }
    }

    FileDialog {
        id: importDialog

        title: qsTr("Import a licence")
        nameFilters: [qsTr("Licence files (*.adllic)"), qsTr("All files (*)")]
        onAccepted: dialog.model.importFile(selectedFile)
    }

    // Removing asks first. The safe answer is the default.
    AppDialog {
        id: removeDialog

        objectName: "removeLicenceDialog"
        heading: qsTr("Remove this licence?")
        primaryText: qsTr("Remove licence")
        secondaryText: qsTr("Cancel")
        initialFocus: "secondary"
        enterAccepts: false

        content: Text {
            Layout.fillWidth: true
            text: qsTr("This is the Community edition again. Your media sets, receipts and logs are unchanged. Keep the licence file so that you can import it again.")
            color: Theme.textMuted
            font.pixelSize: Theme.fontBody
            wrapMode: Text.WordWrap
        }

        onPrimaryClicked: {
            dialog.model.removeFile(dialog.model.fileInUse);
            close();
        }
        onSecondaryClicked: close()
    }

    onSecondaryClicked: importDialog.open()
    onPrimaryClicked: dialog.close()

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
                text: qsTr("Shows which edition this is and what your licence includes. ARINC Data Loader checks a licence on this computer and never contacts a server.")
                color: Theme.textMuted
                font.pixelSize: Theme.fontBody
                wrapMode: Text.WordWrap
            }

            // ---- the summary ----
            Card {
                title: qsTr("Edition")

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space3

                    Text {
                        objectName: "licenceEdition"
                        text: dialog.model !== null ? dialog.model.editionText : ""
                        color: Theme.text
                        font.pixelSize: Theme.fontDisplay
                        font.family: Theme.headingFamily
                        font.weight: Font.ExtraBold
                    }

                    StatusChip {
                        objectName: "licenceState"
                        text: dialog.model !== null ? dialog.model.stateLabel : ""
                        tone: dialog.model !== null ? dialog.model.stateTone : "neutral"
                    }

                    Item {
                        Layout.fillWidth: true
                    }
                }

                Text {
                    objectName: "licenceSentence"
                    Layout.fillWidth: true
                    text: dialog.model !== null ? dialog.model.sentence : ""
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                    wrapMode: Text.WordWrap
                    Accessible.role: Accessible.StaticText
                    Accessible.name: text
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: Theme.space4
                    rowSpacing: Theme.space2

                    Repeater {
                        model: dialog.summaryRows

                        delegate: Item {
                            id: summaryRow

                            required property var modelData

                            Layout.columnSpan: 2
                            Layout.fillWidth: true
                            visible: summaryRow.modelData.value.length > 0
                            implicitHeight: visible ? Math.max(labelText.implicitHeight, valueText.implicitHeight) : 0

                            Text {
                                id: labelText

                                width: 150
                                text: summaryRow.modelData.label
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontSmall
                                font.family: Theme.monoFamily
                                font.letterSpacing: 0.6
                                wrapMode: Text.WordWrap
                            }

                            Text {
                                id: valueText

                                anchors.left: labelText.right
                                anchors.leftMargin: Theme.space4
                                anchors.right: parent.right
                                text: summaryRow.modelData.value
                                color: Theme.text
                                font.pixelSize: Theme.fontBody
                                font.family: summaryRow.modelData.mono ? Theme.monoFamily : Theme.headingFamily
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }

            AppBanner {
                objectName: "licenceImportResult"
                Layout.fillWidth: true
                visible: dialog.model !== null && dialog.model.importResult.kind !== undefined
                         && dialog.model.importResult.kind.length > 0
                tone: dialog.model !== null && dialog.model.importResult.kind !== undefined
                      ? dialog.importToneOf(dialog.model.importResult.kind) : "info"
                text: dialog.model !== null && dialog.model.importResult.title !== undefined
                      ? dialog.model.importResult.title : ""
                detail: dialog.model !== null && dialog.model.importResult.text !== undefined
                        ? dialog.model.importResult.text : ""
            }

            // ---- what the licence includes ----
            Card {
                title: qsTr("What this edition includes")

                Repeater {
                    model: dialog.model !== null ? dialog.model.features : []

                    delegate: RowLayout {
                        id: featureRow

                        required property var modelData

                        Layout.fillWidth: true
                        spacing: Theme.space3

                        Text {
                            Layout.preferredWidth: 130
                            text: featureRow.modelData.name
                            color: Theme.text
                            font.pixelSize: Theme.fontBody
                            font.family: Theme.headingFamily
                            font.weight: Font.DemiBold
                        }

                        StatusChip {
                            Layout.preferredWidth: 130
                            text: featureRow.modelData.available ? qsTr("Available") : qsTr("Not available")
                            tone: featureRow.modelData.available ? "ok" : "neutral"
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: text.length > 0
                            text: featureRow.modelData.why
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSmall
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }

            // ---- every file found, when there is more than one ----
            Card {
                objectName: "licenceFiles"
                visible: dialog.model !== null && dialog.model.files.length > 1
                title: qsTr("Licence files on this computer")

                Text {
                    Layout.fillWidth: true
                    text: qsTr("ARINC Data Loader uses the licence that gives you the most.")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                    wrapMode: Text.WordWrap
                }

                Repeater {
                    model: dialog.model !== null ? dialog.model.files : []

                    delegate: RowLayout {
                        id: fileRow

                        required property var modelData

                        Layout.fillWidth: true
                        spacing: Theme.space3

                        Text {
                            Layout.fillWidth: true
                            text: fileRow.modelData.licenceNumber.length > 0
                                  ? [fileRow.modelData.licenceNumber, fileRow.modelData.edition,
                                     fileRow.modelData.term].join(", ")
                                  : fileRow.modelData.name
                            color: Theme.text
                            font.pixelSize: Theme.fontSmall
                            font.family: Theme.monoFamily
                            elide: Text.ElideRight
                        }

                        StatusChip {
                            text: fileRow.modelData.stateLabel
                            tone: fileRow.modelData.tone
                        }

                        StatusChip {
                            visible: fileRow.modelData.byAdministrator
                            text: qsTr("Installed by an administrator")
                            tone: "neutral"
                        }

                        AppButton {
                            visible: !fileRow.modelData.byAdministrator
                            text: qsTr("Remove")
                            onClicked: dialog.model.removeFile(fileRow.modelData.name)
                            Accessible.name: qsTr("Remove the licence file %1").arg(fileRow.modelData.name)
                        }
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: dialog.model !== null && dialog.model.machineFolder.length > 0
                      ? qsTr("Licence files go in %1. An administrator can put a licence for every user of this computer in %2.")
                            .arg(dialog.model.userFolder).arg(dialog.model.machineFolder)
                      : dialog.model !== null ? qsTr("Licence files go in %1.").arg(dialog.model.userFolder) : ""
                color: Theme.textMuted
                font.pixelSize: Theme.fontSmall
                font.family: Theme.monoFamily
                wrapMode: Text.WrapAnywhere
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space2

                AppButton {
                    objectName: "licenceRemove"
                    visible: dialog.model !== null && dialog.model.canRemove
                    text: qsTr("Remove licence")
                    onClicked: removeDialog.open()
                    Accessible.description: qsTr("Remove the licence file in use from your licence folder")
                }

                AppButton {
                    objectName: "licenceSupport"
                    text: qsTr("Support")
                    onClicked: dialog.supportRequested()
                    Accessible.description: qsTr("Open the support window")
                }

                Item {
                    Layout.fillWidth: true
                }
            }
        }
    }
}

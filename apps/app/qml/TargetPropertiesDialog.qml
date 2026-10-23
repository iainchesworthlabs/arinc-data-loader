// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// What the application knows about the target: the address and THW_ID_POS it will use, and the
// FIND answer those came from (when they came from one).
AppDialog {
    id: dialog

    required property var app

    heading: qsTr("Target properties")
    primaryText: qsTr("Close")
    width: 560

    content: ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.space3

        SectionHeader {
            label: qsTr("Addressing")
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: Theme.space4
            rowSpacing: Theme.space2

            Text {
                text: qsTr("Address")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSmall
            }

            Text {
                Layout.fillWidth: true
                text: dialog.app.targetAddress.length > 0
                      ? dialog.app.targetAddress : qsTr("not set")
                color: Theme.text
                font.pixelSize: Theme.fontBody
                font.family: Theme.monoFamily
            }

            Text {
                text: qsTr("THW_ID_POS")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSmall
            }

            Text {
                Layout.fillWidth: true
                text: dialog.app.thwIdPos.length > 0
                      ? dialog.app.thwIdPos : qsTr("not set")
                color: Theme.text
                font.pixelSize: Theme.fontBody
                font.family: Theme.monoFamily
            }
        }

        SectionHeader {
            label: qsTr("FIND answer")
            visible: dialog.app.selectedTargetIndex >= 0
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: dialog.app.selectedTargetIndex >= 0
            spacing: Theme.space2

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: Theme.space4
                rowSpacing: Theme.space2

                Text { text: qsTr("Address"); color: Theme.textMuted; font.pixelSize: Theme.fontSmall }
                Text {
                    Layout.fillWidth: true
                    text: dialog.app.selectedTargetAddress
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                    font.family: Theme.monoFamily
                }
                Text { text: qsTr("Hardware ID"); color: Theme.textMuted; font.pixelSize: Theme.fontSmall }
                Text {
                    Layout.fillWidth: true
                    text: dialog.app.selectedTargetHardwareId
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                    font.family: Theme.monoFamily
                }
                Text { text: qsTr("Type"); color: Theme.textMuted; font.pixelSize: Theme.fontSmall }
                Text {
                    Layout.fillWidth: true
                    text: dialog.app.selectedTargetTypeName
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                    font.family: Theme.monoFamily
                }
                Text { text: qsTr("Position"); color: Theme.textMuted; font.pixelSize: Theme.fontSmall }
                Text {
                    Layout.fillWidth: true
                    text: dialog.app.selectedTargetPosition
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                    font.family: Theme.monoFamily
                }
                Text { text: qsTr("Name"); color: Theme.textMuted; font.pixelSize: Theme.fontSmall }
                Text {
                    Layout.fillWidth: true
                    text: dialog.app.selectedTargetLiteralName
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                    font.family: Theme.monoFamily
                }
                Text { text: qsTr("Manufacturer code"); color: Theme.textMuted; font.pixelSize: Theme.fontSmall }
                Text {
                    Layout.fillWidth: true
                    text: dialog.app.selectedTargetManufacturer
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                    font.family: Theme.monoFamily
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: dialog.app.selectedTargetIndex < 0
            text: qsTr("No target was chosen from a FIND answer. The address and THW_ID_POS are the values typed in the rail.")
            color: Theme.textMuted
            font.pixelSize: Theme.fontBody
            wrapMode: Text.WordWrap
        }
    }

    onPrimaryClicked: dialog.close()
}

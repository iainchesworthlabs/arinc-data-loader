import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// What the application knows about the target: the fields of its FIND answer (when it was
// chosen from the list), and the address and THW_ID_POS as typed.
Dialog {
    id: dialog

    required property var app

    title: qsTr("Target properties")
    modal: true
    standardButtons: Dialog.Close
    width: 460

    ColumnLayout {
        spacing: 10

        GroupBox {
            title: qsTr("Target (typed)")
            Layout.fillWidth: true

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 12
                rowSpacing: 4

                Label {
                    text: qsTr("Address")
                    opacity: 0.7
                }
                Label {
                    text: dialog.app.targetAddress.length > 0
                          ? dialog.app.targetAddress : qsTr("not set")
                }
                Label {
                    text: qsTr("THW_ID_POS")
                    opacity: 0.7
                }
                Label {
                    text: dialog.app.thwIdPos.length > 0
                          ? dialog.app.thwIdPos : qsTr("not set")
                }
            }
        }

        GroupBox {
            title: qsTr("FIND answer")
            Layout.fillWidth: true
            visible: dialog.app.selectedTargetIndex >= 0

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 12
                rowSpacing: 4

                Label {
                    text: qsTr("Address")
                    opacity: 0.7
                }
                Label { text: dialog.app.selectedTargetAddress }
                Label {
                    text: qsTr("Hardware identifier")
                    opacity: 0.7
                }
                Label { text: dialog.app.selectedTargetHardwareId }
                Label {
                    text: qsTr("Type")
                    opacity: 0.7
                }
                Label { text: dialog.app.selectedTargetTypeName }
                Label {
                    text: qsTr("Position")
                    opacity: 0.7
                }
                Label { text: dialog.app.selectedTargetPosition }
                Label {
                    text: qsTr("Name")
                    opacity: 0.7
                }
                Label { text: dialog.app.selectedTargetLiteralName }
                Label {
                    text: qsTr("Manufacturer code")
                    opacity: 0.7
                }
                Label { text: dialog.app.selectedTargetManufacturer }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: dialog.app.selectedTargetIndex < 0
            text: qsTr("No target has been chosen from the FIND list.")
            wrapMode: Text.Wrap
            opacity: 0.7
        }
    }
}

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
    width: 480

    ColumnLayout {
        spacing: Theme.space2

        Panel {
            title: qsTr("Target (typed)")
            Layout.fillWidth: true

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: Theme.space3
                rowSpacing: Theme.space1

                Label {
                    text: qsTr("Address")
                    color: Theme.muted
                }
                Label {
                    text: dialog.app.targetAddress.length > 0
                          ? dialog.app.targetAddress : qsTr("not set")
                }
                Label {
                    text: qsTr("THW_ID_POS")
                    color: Theme.muted
                }
                Label {
                    text: dialog.app.thwIdPos.length > 0
                          ? dialog.app.thwIdPos : qsTr("not set")
                }
            }
        }

        Panel {
            title: qsTr("FIND answer")
            Layout.fillWidth: true
            visible: dialog.app.selectedTargetIndex >= 0

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: Theme.space3
                rowSpacing: Theme.space1

                Label {
                    text: qsTr("Address")
                    color: Theme.muted
                }
                Label { text: dialog.app.selectedTargetAddress }
                Label {
                    text: qsTr("Hardware identifier")
                    color: Theme.muted
                }
                Label { text: dialog.app.selectedTargetHardwareId }
                Label {
                    text: qsTr("Type")
                    color: Theme.muted
                }
                Label { text: dialog.app.selectedTargetTypeName }
                Label {
                    text: qsTr("Position")
                    color: Theme.muted
                }
                Label { text: dialog.app.selectedTargetPosition }
                Label {
                    text: qsTr("Name")
                    color: Theme.muted
                }
                Label { text: dialog.app.selectedTargetLiteralName }
                Label {
                    text: qsTr("Manufacturer code")
                    color: Theme.muted
                }
                Label { text: dialog.app.selectedTargetManufacturer }
            }
        }

        Label {
            Layout.fillWidth: true
            visible: dialog.app.selectedTargetIndex < 0
            text: qsTr("No target has been chosen from the FIND list.")
            wrapMode: Text.Wrap
            color: Theme.muted
        }
    }
}

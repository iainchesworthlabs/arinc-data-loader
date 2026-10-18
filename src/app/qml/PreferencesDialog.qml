import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The operator's preferences. Every field writes through the model (and so to QSettings) as it
// changes; OK closes the dialog, Cancel closes it without changing anything back.
Dialog {
    id: dialog

    required property var app

    title: qsTr("Preferences")
    modal: true
    standardButtons: Dialog.Ok | Dialog.Cancel
    width: 560

    ColumnLayout {
        spacing: 10

        GroupBox {
            title: qsTr("Target")
            Layout.fillWidth: true

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                GridLayout {
                    columns: 2
                    columnSpacing: 8
                    rowSpacing: 6

                    Label { text: qsTr("Default address") }
                    TextField {
                        Layout.preferredWidth: 260
                        text: dialog.app.defaultTargetAddress
                        placeholderText: qsTr("10.0.0.2")
                        onTextEdited: dialog.app.defaultTargetAddress = text
                        Accessible.name: qsTr("Default target address")
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Prefilled into the address field at start-up, and again when the operator clears the field")
                    }
                    Label { text: qsTr("Default THW_ID_POS") }
                    TextField {
                        Layout.preferredWidth: 260
                        text: dialog.app.defaultThwIdPos
                        placeholderText: qsTr("HNPFMS_L")
                        onTextEdited: dialog.app.defaultThwIdPos = text
                        Accessible.name: qsTr("Default target hardware identifier and position")
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Prefilled into the THW_ID_POS field at start-up, and again when the operator clears the field")
                    }
                }
                Switch {
                    text: qsTr("Find the target automatically")
                    checked: dialog.app.autoFindBeforeOperation
                    onToggled: dialog.app.autoFindBeforeOperation = checked
                    Accessible.name: text
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Before an operation with no target chosen, run FIND and, if exactly one target answers, take it and go on; otherwise show the FIND result and wait")
                }
            }
        }

        GroupBox {
            title: qsTr("Media set")
            Layout.fillWidth: true

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Switch {
                    text: qsTr("Check the media set as soon as it is chosen")
                    checked: dialog.app.validateOnOpen
                    onToggled: dialog.app.validateOnOpen = checked
                    Accessible.name: text
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("A set has to pass the check before an upload is allowed; with this off, choose 'Check media set' yourself")
                }
                RowLayout {
                    Label { text: qsTr("Last media set") }
                    Label {
                        Layout.fillWidth: true
                        text: dialog.app.lastMediaSetDirectory.length > 0
                              ? dialog.app.lastMediaSetDirectory : qsTr("none")
                        elide: Text.ElideMiddle
                        opacity: 0.7
                    }
                }
            }
        }

        GroupBox {
            title: qsTr("Loader")
            Layout.fillWidth: true

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                GridLayout {
                    columns: 2
                    columnSpacing: 8
                    rowSpacing: 6

                    Label { text: qsTr("Control port") }
                    SpinBox {
                        from: 1
                        to: 65535
                        value: dialog.app.controlPort
                        onValueModified: dialog.app.controlPort = value
                        editable: true
                        Accessible.name: qsTr("Loader control port")
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("The port the loader listens on (59 unless it has to move); the target is told when it is not 59, which matters when the target runs on this machine")
                    }
                    Label { text: qsTr("Download directory") }
                    TextField {
                        Layout.preferredWidth: 320
                        text: dialog.app.downloadDirectory
                        onTextEdited: dialog.app.downloadDirectory = text
                        Accessible.name: qsTr("Download directory")
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Where a download is to be written; reserved for the Download operation")
                    }
                }
            }
        }

        GroupBox {
            title: qsTr("Log")
            Layout.fillWidth: true

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                GridLayout {
                    columns: 2
                    columnSpacing: 8
                    rowSpacing: 6

                    Label { text: qsTr("Lines kept") }
                    SpinBox {
                        from: 100
                        to: 1000000
                        value: dialog.app.logCapacity
                        onValueModified: dialog.app.logCapacity = value
                        editable: true
                        Accessible.name: qsTr("Activity log capacity")
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("The most lines the activity log keeps; older ones are dropped")
                    }
                }
            }
        }
    }
}

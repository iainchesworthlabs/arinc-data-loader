import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: window

    // The MockTargetModel (src/mock_target/core), handed over by main.cpp.
    required property var app

    readonly property bool dark: palette.window.hslLightness < 0.5
    readonly property color okColour: dark ? "#70d090" : "#1b6e3a"
    readonly property color failColour: dark ? "#ff7070" : "#b00020"

    title: qsTr("ARINC Mock Target")
    width: 1100
    height: 700
    minimumWidth: 1000
    minimumHeight: 640
    visible: true

    FolderDialog {
        id: storageDialog

        title: qsTr("Choose the storage directory")
        onAccepted: window.app.storageDirectory = selectedFolder
    }

    SplitView {
        anchors.fill: parent
        anchors.margins: 12
        orientation: Qt.Horizontal

        // ---- what the operator sets up and starts ----
        ScrollView {
            SplitView.preferredWidth: 460
            SplitView.minimumWidth: 380
            contentWidth: availableWidth
            clip: true

            ColumnLayout {
                width: parent.width
                spacing: 12

                Frame {
                    Layout.fillWidth: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8

                        Label {
                            text: qsTr("Identity")
                            font.bold: true
                        }
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("What the target answers to FIND and how its protocol files are named.")
                            wrapMode: Text.Wrap
                            opacity: 0.7
                        }
                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 8
                            rowSpacing: 6

                            Label { text: qsTr("Hardware ID") }
                            TextField {
                                objectName: "hardwareIdField"
                                Layout.fillWidth: true
                                placeholderText: qsTr("HNPFMS")
                                text: window.app.hardwareId
                                enabled: !window.app.running
                                onTextEdited: window.app.hardwareId = text
                                Accessible.name: qsTr("Target hardware identifier")
                            }
                            Label { text: qsTr("Type name") }
                            TextField {
                                Layout.fillWidth: true
                                placeholderText: qsTr("FMS")
                                text: window.app.typeName
                                enabled: !window.app.running
                                onTextEdited: window.app.typeName = text
                                Accessible.name: qsTr("Target type name")
                            }
                            Label { text: qsTr("Position") }
                            TextField {
                                Layout.fillWidth: true
                                placeholderText: qsTr("L")
                                text: window.app.position
                                enabled: !window.app.running
                                onTextEdited: window.app.position = text
                                Accessible.name: qsTr("Target position")
                            }
                            Label { text: qsTr("Literal name") }
                            TextField {
                                Layout.fillWidth: true
                                placeholderText: qsTr("FMS LEFT")
                                text: window.app.literalName
                                enabled: !window.app.running
                                onTextEdited: window.app.literalName = text
                                Accessible.name: qsTr("Target literal name")
                            }
                            Label { text: qsTr("Manufacturer code") }
                            TextField {
                                Layout.fillWidth: true
                                placeholderText: qsTr("HNP")
                                text: window.app.manufacturerCode
                                enabled: !window.app.running
                                onTextEdited: window.app.manufacturerCode = text
                                Accessible.name: qsTr("Target manufacturer code")
                            }
                        }
                    }
                }

                Frame {
                    Layout.fillWidth: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8

                        Label {
                            text: qsTr("Ports")
                            font.bold: true
                        }
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("The loader asks for the TFTP port; FIND is broadcast on its port (1001).")
                            wrapMode: Text.Wrap
                            opacity: 0.7
                        }
                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 8
                            rowSpacing: 6

                            Label { text: qsTr("TFTP control port") }
                            SpinBox {
                                objectName: "tftpPortField"
                                from: 1
                                to: 65535
                                value: window.app.tftpPort
                                editable: true
                                enabled: !window.app.running
                                onValueModified: window.app.tftpPort = value
                                Accessible.name: qsTr("TFTP control port")
                            }
                            Label { text: qsTr("FIND port") }
                            SpinBox {
                                objectName: "findPortField"
                                from: 1
                                to: 65535
                                value: window.app.findPort
                                editable: true
                                enabled: !window.app.running
                                onValueModified: window.app.findPort = value
                                Accessible.name: qsTr("FIND port")
                            }
                        }
                    }
                }

                Frame {
                    Layout.fillWidth: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8

                        Label {
                            text: qsTr("Storage")
                            font.bold: true
                        }
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Where uploaded files are written (one directory per load). Files put in its \"holds\" directory are what a download serves.")
                            wrapMode: Text.Wrap
                            opacity: 0.7
                        }
                        RowLayout {
                            Button {
                                objectName: "storageButton"
                                text: qsTr("Choose directory...")
                                enabled: !window.app.running
                                onClicked: storageDialog.open()
                            }
                            Button {
                                text: qsTr("Open folder")
                                enabled: window.app.storageDirectory.length > 0
                                onClicked: Qt.openUrlExternally(window.app.storageDirectory)
                                Accessible.name: qsTr("Open the storage directory")
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            text: window.app.storageDirectory.length > 0
                                  ? window.app.storageDirectory : qsTr("No storage directory chosen.")
                            elide: Text.ElideMiddle
                            opacity: 0.7
                        }
                    }
                }

                Frame {
                    Layout.fillWidth: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8

                        Label {
                            text: qsTr("Run")
                            font.bold: true
                        }
                        RowLayout {
                            Button {
                                objectName: "startButton"
                                text: qsTr("Start")
                                enabled: !window.app.running
                                onClicked: window.app.start()
                                Accessible.name: qsTr("Start the mock target")
                            }
                            Button {
                                objectName: "stopButton"
                                text: qsTr("Stop")
                                enabled: window.app.running
                                onClicked: window.app.stop()
                                Accessible.name: qsTr("Stop the mock target")
                            }
                        }
                        Label {
                            objectName: "errorBanner"
                            Layout.fillWidth: true
                            visible: window.app.errorText.length > 0
                            text: window.app.errorText
                            color: window.failColour
                            wrapMode: Text.Wrap
                            Accessible.role: Accessible.StaticText
                            Accessible.name: qsTr("Error: %1").arg(text)
                        }
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Starts serving on the ports above until it is stopped. The data loader application can then FIND it and run the Information, Upload and Download operations against it.")
                            wrapMode: Text.Wrap
                            opacity: 0.7
                        }
                    }
                }

                Frame {
                    Layout.fillWidth: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8

                        Label {
                            text: qsTr("Injections")
                            font.bold: true
                        }
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Ways to make the target misbehave, for demonstrations. Everything here defaults to behaving.")
                            wrapMode: Text.Wrap
                            opacity: 0.7
                        }

                        RowLayout {
                            Label { text: qsTr("Answer WAIT (s)") }
                            SpinBox {
                                objectName: "waitFirstField"
                                from: 0
                                to: 65535
                                value: window.app.waitFirstSeconds
                                editable: true
                                enabled: !window.app.running
                                onValueModified: window.app.waitFirstSeconds = value
                                Accessible.name: qsTr("Seconds to answer the first request with WAIT")
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("The first request for an initialization file is answered with WAIT:x, so the loader waits and asks again. 0 behaves.")
                            }
                            CheckBox {
                                text: qsTr("every request")
                                checked: window.app.waitEveryRequest
                                enabled: !window.app.running
                                onToggled: window.app.waitEveryRequest = checked
                                Accessible.name: qsTr("Answer every initialization request with WAIT")
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Answers every request with WAIT, not just the first.")
                            }
                        }

                        ComboBox {
                            id: refuseBox

                            Layout.fillWidth: true
                            textRole: "label"
                            valueRole: "value"
                            model: [
                                { label: qsTr("Refuse nothing"), value: "" },
                                { label: qsTr("Refuse Information"), value: "information" },
                                { label: qsTr("Refuse Upload"), value: "upload" },
                                { label: qsTr("Refuse Download (media defined)"), value: "downloadMedia" },
                                { label: qsTr("Refuse Download (operator defined)"), value: "downloadOperator" }
                            ]
                            enabled: !window.app.running
                            onActivated: window.app.refuseOperation = currentValue
                            Component.onCompleted: currentIndex = indexOfValue(window.app.refuseOperation)
                            Accessible.name: qsTr("Operation to refuse")
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("The chosen operation's initialization file answers 0x1000 (denied), which is what the loader reports.")
                        }

                        RowLayout {
                            Label { text: qsTr("Drop status") }
                            SpinBox {
                                from: 0
                                to: 99
                                value: window.app.dropStatusEvery
                                editable: true
                                enabled: !window.app.running
                                onValueModified: window.app.dropStatusEvery = value
                                Accessible.name: qsTr("Drop every Nth status file")
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Every Nth status file is never sent, so the loader reports the gap by the counter. 0 behaves.")
                            }
                            Label { text: qsTr("th file") }
                        }

                        CheckBox {
                            text: qsTr("Corrupt served files")
                            checked: window.app.corruptServedFile
                            enabled: !window.app.running
                            onToggled: window.app.corruptServedFile = checked
                            Accessible.name: qsTr("Flip a bit in every served file")
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("A bit of every file the target serves (the initialization file, a download's files) is flipped.")
                        }

                        CheckBox {
                            text: qsTr("Send a wrong checksum option")
                            checked: window.app.wrongChecksum
                            enabled: !window.app.running
                            onToggled: window.app.wrongChecksum = checked
                            Accessible.name: qsTr("Send a wrong checksum option")
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("The check value option on what the target writes is one bit off, so the loader reports the mismatch (it still keeps the file).")
                        }

                        RowLayout {
                            Label { text: qsTr("Abort after") }
                            SpinBox {
                                from: 0
                                to: 99
                                value: window.app.abortAfterStatuses
                                editable: true
                                enabled: !window.app.running
                                onValueModified: window.app.abortAfterStatuses = value
                                Accessible.name: qsTr("Abort after this many status files")
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("The target aborts the operation (0x1003) after this many status files. 0 behaves.")
                            }
                            Label { text: qsTr("status files") }
                        }

                        CheckBox {
                            text: qsTr("Go silent after the first status file")
                            checked: window.app.silent
                            enabled: !window.app.running
                            onToggled: window.app.silent = checked
                            Accessible.name: qsTr("Go silent after the first status file")
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("The target answers nothing after the first status file, so the loader hits its data load protocol time-out.")
                        }
                    }
                }
            }
        }

        // ---- what is happening ----
        ColumnLayout {
            SplitView.fillWidth: true
            SplitView.minimumWidth: 380
            spacing: 12

            Frame {
                Layout.fillWidth: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8

                    Label {
                        text: qsTr("Operation")
                        font.bold: true
                    }
                    Label {
                        Layout.fillWidth: true
                        text: window.app.currentOperation.length > 0
                              ? window.app.currentOperation : qsTr("Serving (no operation).")
                        font.bold: true
                    }
                    ProgressBar {
                        objectName: "progressBar"
                        Layout.fillWidth: true
                        visible: window.app.running || window.app.progress >= 0
                        from: 0
                        to: 100
                        indeterminate: window.app.running && window.app.progress < 0
                        value: Math.max(window.app.progress, 0)
                        Accessible.name: qsTr("Progress of the operation")
                    }
                    Label {
                        Layout.fillWidth: true
                        text: window.app.currentOperation.length > 0
                              ? (window.app.progress >= 0
                                 ? window.app.currentOperation + "  " + window.app.progress + "%"
                                 : window.app.currentOperation)
                              : ""
                    }
                }
            }

            Frame {
                Layout.fillHeight: true
                Layout.preferredHeight: 1

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 6

                    Label {
                        text: qsTr("Activity")
                        font.bold: true
                    }
                    LogView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        events: window.app.activity
                    }
                    Button {
                        text: qsTr("Clear")
                        onClicked: window.app.clearLog()
                        Accessible.name: qsTr("Clear the activity log")
                    }
                }
            }

            Frame {
                Layout.fillHeight: true
                Layout.preferredHeight: 1

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 6

                    Label {
                        text: qsTr("Stored files")
                        font.bold: true
                    }
                    ListView {
                        id: storedList

                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: window.app.storedFiles
                        ScrollBar.vertical: ScrollBar {}
                        Accessible.role: Accessible.List
                        Accessible.name: qsTr("Files stored from uploads")

                        Label {
                            anchors.centerIn: parent
                            visible: storedList.count === 0
                            text: qsTr("Nothing stored yet. The files of an Upload are written under the storage directory.")
                            wrapMode: Text.Wrap
                            opacity: 0.6
                        }
                        delegate: Label {
                            id: storedRow

                            required property string name
                            required property int size
                            required property string path

                            width: ListView.view.width
                            leftPadding: 4
                            rightPadding: 4
                            topPadding: 2
                            bottomPadding: 2
                            wrapMode: Text.Wrap
                            text: name + "  (" + size + " bytes)"
                            ToolTip.visible: storedList.hoveredItem === storedRow
                            ToolTip.text: path
                        }
                    }
                }
            }
        }
    }
}

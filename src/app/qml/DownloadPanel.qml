pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The Download operation (5.4.4): the mode, the files, and the start button. In the media
// defined mode the operator names the files the target is to send; in the operator defined mode
// the target's list of files is shown to be ticked, and the choice goes back to it. The files
// land in a new DNLD_DATA_<THW_ID_POS>_<number> directory under the download directory
// (Preferences). The overall progress, the result and the Abort button are the Operation
// panel's; each row's own progress is here.
Panel {
    id: panel

    required property var app
    required property color failColour

    // Adds what the operator typed as a file of a media defined download.
    function addName() {
        const name = downloadNameField.text.trim()
        if (name.length > 0) {
            panel.app.addDownloadFile(name)
            downloadNameField.text = ""
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        RowLayout {
            RadioButton {
                objectName: "downloadModeMedia"

                text: qsTr("Media defined")
                checked: panel.app.downloadMode === 0
                enabled: !panel.app.busy
                onToggled: if (checked) panel.app.downloadMode = 0
                Accessible.name: qsTr("Media defined download")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("The operator names the files the target is to send (ARINC 615A 5.4.4, media defined)")
            }
            RadioButton {
                objectName: "downloadModeOperator"

                text: qsTr("Operator defined")
                checked: panel.app.downloadMode === 1
                enabled: !panel.app.busy
                onToggled: if (checked) panel.app.downloadMode = 1
                Accessible.name: qsTr("Operator defined download")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("The target sends its list of files, the operator chooses, and the choice goes back (ARINC 615A 5.4.4, operator defined)")
            }
        }

        // Media defined: the operator's own list of file names.
        ColumnLayout {
            visible: panel.app.downloadMode === 0
            spacing: 6

            RowLayout {
                TextField {
                    id: downloadNameField

                    objectName: "downloadFileName"
                    Layout.fillWidth: true
                    placeholderText: qsTr("File name")
                    enabled: !panel.app.busy
                    onAccepted: panel.addName()
                    Accessible.name: qsTr("Download file name")
                }
                Button {
                    objectName: "addDownloadButton"

                    text: qsTr("Add")
                    enabled: !panel.app.busy && downloadNameField.text.trim().length > 0
                    onClicked: panel.addName()
                    Accessible.name: qsTr("Add the file name to the download")
                }
            }
            Label {
                Layout.fillWidth: true
                visible: panel.app.downloadMode === 0 && downloadList.count === 0
                text: qsTr("No file named. Name each file the target is to send.")
                wrapMode: Text.Wrap
                opacity: 0.7
            }
        }

        // Operator defined: waiting for the target's list, then the choice.
        ColumnLayout {
            visible: panel.app.downloadMode === 1
            spacing: 6

            Label {
                objectName: "downloadWaiting"

                Layout.fillWidth: true
                visible: panel.app.downloadMode === 1 && !panel.app.downloadChoicePending
                         && downloadList.count === 0
                text: panel.app.busy
                      ? qsTr("Waiting for the target's list of files...")
                      : qsTr("Nothing to choose from yet. Start the download: the target sends its list of files, the operator ticks what to take, and the choice goes back.")
                wrapMode: Text.Wrap
                opacity: 0.7
            }
            RowLayout {
                visible: panel.app.downloadChoicePending

                Button {
                    objectName: "chooseNoneButton"

                    text: qsTr("Choose none")
                    onClicked: panel.app.chooseNoDownloadFiles()
                    Accessible.name: qsTr("Choose none of the files")
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Nothing is downloaded: the operation is cancelled")
                }
                Button {
                    objectName: "confirmDownloadButton"

                    text: qsTr("Download selected")
                    enabled: panel.app.downloadSelectedCount > 0
                    onClicked: panel.app.confirmDownloadSelection()
                    Accessible.name: qsTr("Download the ticked files")
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("%n file(s) ticked", "", panel.app.downloadSelectedCount)
                    opacity: 0.7
                    elide: Text.ElideRight
                }
            }
        }

        // The files, with their own progress while a download runs.
        Repeater {
            id: downloadList

            model: panel.app.downloads
            delegate: ColumnLayout {
                id: downloadRow

                required property int index
                required property var model
                required property string name
                required property string description
                required property bool checked
                required property int ratio
                required property string fileStatus
                required property bool failed
                required property string problem

                Layout.fillWidth: true
                spacing: 2

                RowLayout {
                    CheckBox {
                        visible: panel.app.downloadMode === 1
                        objectName: "downloadChoiceCheck"
                        Layout.fillWidth: true
                        text: downloadRow.name
                        checked: downloadRow.checked
                        enabled: panel.app.downloadChoicePending
                        onToggled: downloadRow.model.checked = checked
                        Accessible.name: qsTr("Download %1").arg(downloadRow.name)
                    }
                    Label {
                        visible: panel.app.downloadMode === 0
                        Layout.fillWidth: true
                        text: downloadRow.name
                    }
                    Button {
                        visible: panel.app.downloadMode === 0 && !panel.app.busy
                        text: qsTr("Remove")
                        onClicked: panel.app.removeDownloadFile(downloadRow.index)
                        Accessible.name: qsTr("Remove %1 from the download").arg(downloadRow.name)
                    }
                }
                Label {
                    Layout.fillWidth: true
                    Layout.leftMargin: 28
                    visible: downloadRow.description.length > 0
                    text: downloadRow.description
                    opacity: 0.7
                    wrapMode: Text.Wrap
                }
                ProgressBar {
                    Layout.fillWidth: true
                    Layout.leftMargin: 28
                    visible: downloadRow.ratio >= 0
                    from: 0
                    to: 100
                    value: downloadRow.ratio
                    Accessible.name: qsTr("Download progress of %1").arg(downloadRow.name)
                }
                Label {
                    Layout.fillWidth: true
                    Layout.leftMargin: 28
                    visible: downloadRow.ratio >= 0
                    text: downloadRow.ratio + "% " + downloadRow.fileStatus
                          + (downloadRow.problem.length > 0 ? ": " + downloadRow.problem : "")
                    color: downloadRow.failed ? panel.failColour : panel.palette.windowText
                    wrapMode: Text.Wrap
                }
            }
        }

        RowLayout {
            Button {
                objectName: "downloadButton"

                text: qsTr("Download")
                enabled: panel.app.canDownload
                onClicked: panel.app.downloadSelected()
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Downloads into a new DNLD_DATA directory under the download directory (Preferences)")
            }
        }
        Label {
            Layout.fillWidth: true
            visible: text.length > 0
            text: qsTr("Download directory: %1").arg(panel.app.lastDownloadDirectory)
            elide: Text.ElideMiddle
            opacity: 0.7
        }
    }
}

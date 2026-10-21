pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// The download plan: the mode, the files and, before anything starts, where the result will be
// written. The action itself is the window's fixed bar.
Item {
    id: root
    objectName: "downloadPanel"

    required property var app

    readonly property string targetName: root.app.thwIdPos.length > 0
                                         ? root.app.thwIdPos
                                         : (root.app.targetAddress.length > 0
                                            ? root.app.targetAddress : qsTr("no target"))

    function addName() {
        const name = downloadNameField.text.trim();
        if (name.length > 0) {
            root.app.addDownloadFile(name);
            downloadNameField.text = "";
        }
    }

    FolderDialog {
        id: destinationDialog

        title: qsTr("Choose the download folder")
        onAccepted: root.app.downloadDirectory = selectedFolder
    }

    // Tab to a control below the fold and the page follows it.
    RevealOnFocus {
        flickable: scroll.contentItem
    }

    ScrollView {
        id: scroll
        objectName: "downloadScroll"

        anchors.fill: parent
        contentWidth: availableWidth
        clip: true
        ScrollBar.vertical: AppScrollBar { inScrollView: true }

        Item {
            width: scroll.availableWidth
            implicitHeight: content.implicitHeight + Theme.space6 * 2

            ColumnLayout {
                id: content

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Theme.space6
                spacing: Theme.space6

                // ---- the plan ----
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space1

                    Text {
                        text: qsTr("THE DOWNLOAD")
                        color: Theme.accentInk
                        font.pixelSize: Theme.fontMicro
                        font.family: Theme.monoFamily
                        font.letterSpacing: 1.2
                        font.bold: true
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Download from %1").arg(root.targetName)
                        color: Theme.text
                        font.pixelSize: Theme.fontHeadline
                        font.family: Theme.headingFamily
                        font.weight: Font.ExtraBold
                        wrapMode: Text.WordWrap
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Each download writes a new DNLD_DATA_<THW_ID_POS>_<n> folder.")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontBody
                            elide: Text.ElideRight
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 2
                    color: Theme.divider
                }

                // ---- mode ----
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space2

                    SectionHeader {
                        label: qsTr("Mode")
                        summary: root.app.downloadMode === 0 ? qsTr("MEDIA DEFINED")
                                                             : qsTr("OPERATOR DEFINED")
                    }

                    SegmentedControl {
                        objectName: "downloadMode"
                        segments: [
                            {
                                value: 0,
                                label: qsTr("Media defined"),
                                name: "downloadModeMedia"
                            },
                            {
                                value: 1,
                                label: qsTr("Operator defined"),
                                name: "downloadModeOperator"
                            }
                        ]
                        selectedValue: root.app.downloadMode
                        onSegmentClicked: (value) => root.app.downloadMode = value
                        enabled: !root.app.busy
                        Accessible.name: qsTr("Download mode")
                    }

                    Text {
                        Layout.fillWidth: true
                        text: root.app.downloadMode === 0
                              ? qsTr("Name the files the target is to send; only those are requested.")
                              : qsTr("The target sends its list; tick the files to take, then send the choice back.")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontBody
                        wrapMode: Text.WordWrap
                    }
                }

                // ---- media defined ----
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: root.app.downloadMode === 0
                    spacing: Theme.space2

                    SectionHeader {
                        label: qsTr("Files")
                        summary: Theme.quantity(mediaDownloadList.count,
                                                qsTr("file"), qsTr("files"))
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2

                        AppTextField {
                            id: downloadNameField

                            objectName: "downloadFileName"
                            Layout.fillWidth: true
                            placeholderText: qsTr("File name, for example CONFIG.LDR")
                            enabled: !root.app.busy
                            onAccepted: root.addName()
                            Accessible.name: qsTr("Download file name")
                        }

                        AppButton {
                            objectName: "addDownloadButton"
                            text: qsTr("Add")
                            enabled: !root.app.busy && downloadNameField.text.trim().length > 0
                            onClicked: root.addName()
                            Accessible.description: qsTr("Add the file name to the download")
                        }
                    }

                    EmptyState {
                        visible: mediaDownloadList.count === 0
                        title: qsTr("No file named yet")
                        body: qsTr("Name each file the target is to send. The target's own list is only used in the operator defined mode.")
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        Repeater {
                            id: mediaDownloadList

                            model: root.app.downloads

                            delegate: ColumnLayout {
                                id: nameRow

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
                                spacing: Theme.space1

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.space2

                                    Text {
                                        Layout.fillWidth: true
                                        text: nameRow.name
                                        color: Theme.text
                                        font.pixelSize: Theme.fontBody
                                        font.family: Theme.monoFamily
                                        elide: Text.ElideRight
                                    }

                                    StatusChip {
                                        visible: nameRow.ratio >= 0
                                        text: nameRow.ratio >= 0
                                              ? nameRow.ratio + "% " + nameRow.fileStatus
                                              : nameRow.fileStatus
                                        tone: nameRow.failed ? "fail"
                                                             : nameRow.ratio >= 100 ? "ok" : "accent"
                                    }

                                    AppButton {
                                        visible: !root.app.busy
                                        text: qsTr("Remove")
                                        onClicked: root.app.removeDownloadFile(nameRow.index)
                                        Accessible.description: qsTr("Remove %1 from the download").arg(nameRow.name)
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    visible: nameRow.description.length > 0
                                    text: nameRow.description
                                    color: Theme.textMuted
                                    font.pixelSize: Theme.fontSmall
                                    wrapMode: Text.WordWrap
                                }

                                TaskProgress {
                                    Layout.fillWidth: true
                                    visible: nameRow.ratio >= 0
                                    value: Math.max(0, nameRow.ratio)
                                    indeterminate: false
                                }

                                // Why this file failed, in words, under its bar.
                                Text {
                                    Layout.fillWidth: true
                                    visible: nameRow.failed && nameRow.problem.length > 0
                                    text: nameRow.problem
                                    color: Theme.accentInk
                                    font.pixelSize: Theme.fontSmall
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                }

                // ---- operator defined ----
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: root.app.downloadMode === 1
                    spacing: Theme.space2

                    SectionHeader {
                        label: qsTr("The target's list")
                        summary: Theme.quantity(root.app.downloadSelectedCount,
                                                qsTr("file ticked"),
                                                qsTr("files ticked"))
                    }

                    Text {
                        objectName: "downloadWaiting"

                        Layout.fillWidth: true
                        visible: !root.app.downloadChoicePending && operatorDownloadList.count === 0
                        text: root.app.busy
                              ? qsTr("Waiting for the target's list of files...")
                              : qsTr("Start the download: the target sends its list, tick what to take, and send the choice back.")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontBody
                        wrapMode: Text.WordWrap
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        visible: root.app.downloadChoicePending

                        AppButton {
                            objectName: "chooseNoneButton"
                            text: qsTr("Choose none")
                            onClicked: root.app.chooseNoDownloadFiles()
                            Accessible.description: qsTr("Choose none of the files")
                        }

                        AppButton {
                            objectName: "confirmDownloadButton"
                            primary: true
                            text: qsTr("Download selected")
                            enabled: root.app.downloadSelectedCount > 0
                            onClicked: root.app.confirmDownloadSelection()
                            Accessible.description: qsTr("Download the ticked files")
                        }

                        Text {
                            Layout.fillWidth: true
                            text: Theme.quantity(root.app.downloadSelectedCount,
                                                 qsTr("file ticked"), qsTr("files ticked"))
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontBody
                            elide: Text.ElideRight
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        Repeater {
                            id: operatorDownloadList
                            model: root.app.downloads

                            delegate: ColumnLayout {
                                id: choiceRow

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
                                spacing: Theme.space1

                                AppCheckBox {
                                    objectName: "downloadChoiceCheck"
                                    Layout.fillWidth: true
                                    text: choiceRow.name
                                    checked: choiceRow.checked
                                    enabled: root.app.downloadChoicePending
                                    onToggled: (value) => choiceRow.model.checked = value
                                    Accessible.description: qsTr("Download %1").arg(choiceRow.name)
                                }

                                Text {
                                    Layout.fillWidth: true
                                    Layout.leftMargin: 26
                                    visible: choiceRow.description.length > 0
                                    text: choiceRow.description
                                    color: Theme.textMuted
                                    font.pixelSize: Theme.fontSmall
                                    wrapMode: Text.WordWrap
                                }

                                TaskProgress {
                                    Layout.fillWidth: true
                                    Layout.leftMargin: 26
                                    visible: choiceRow.ratio >= 0
                                    value: Math.max(0, choiceRow.ratio)
                                    indeterminate: false
                                }

                                // What happened to this file, and why when it failed.
                                Text {
                                    Layout.fillWidth: true
                                    Layout.leftMargin: 26
                                    visible: choiceRow.ratio >= 0 || choiceRow.failed
                                    text: Math.max(0, choiceRow.ratio) + "% " + choiceRow.fileStatus
                                          + (choiceRow.problem.length > 0 ? ": " + choiceRow.problem : "")
                                    color: choiceRow.failed ? Theme.accentInk : Theme.text
                                    font.pixelSize: Theme.fontSmall
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                }

                // ---- destination ----
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space2

                    SectionHeader {
                        label: qsTr("Destination")
                        summary: root.app.lastDownloadDirectory.length > 0
                                 ? qsTr("LAST DOWNLOAD") : qsTr("NOT WRITTEN YET")
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2

                        Text {
                            Layout.fillWidth: true
                            text: root.app.downloadDirectory.length > 0
                                  ? root.app.downloadDirectory : qsTr("No download folder set")
                            color: Theme.text
                            font.pixelSize: Theme.fontSmall
                            font.family: Theme.monoFamily
                            elide: Text.ElideMiddle
                        }

                        AppButton {
                            text: qsTr("Choose folder...")
                            enabled: !root.app.busy
                            onClicked: destinationDialog.open()
                            Accessible.description: qsTr("Choose the download folder")
                        }

                        AppButton {
                            visible: root.app.lastDownloadDirectory.length > 0
                            text: qsTr("Open folder")
                            onClicked: Qt.openUrlExternally(Theme.fileUrl(root.app.lastDownloadDirectory))
                            Accessible.description: qsTr("Open the last download folder")
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: root.app.lastDownloadDirectory.length > 0
                              ? qsTr("The last download was written to %1.").arg(root.app.lastDownloadDirectory)
                              : qsTr("A new numbered folder is made for every download, even one that is aborted.")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                    }
                }

                // ---- the receipt of the last download ----
                Rectangle {
                    Layout.fillWidth: true
                    visible: root.app.resultIsTransfer && root.app.resultWasDownload
                             && root.app.resultState !== 0 && !root.app.busy
                    implicitHeight: downloadReceipt.implicitHeight + Theme.space4 * 2
                    color: Theme.neutral100
                    border.color: Theme.divider
                    border.width: 1

                    ColumnLayout {
                        id: downloadReceipt

                        anchors.fill: parent
                        anchors.margins: Theme.space4
                        spacing: Theme.space2

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space2

                            StatusChip {
                                text: root.app.resultState === 1 ? qsTr("COMPLETE") : qsTr("FAILED")
                                tone: root.app.resultState === 1 ? "ok" : "fail"
                            }

                            Text {
                                Layout.fillWidth: true
                                text: root.app.resultText
                                color: Theme.text
                                font.pixelSize: Theme.fontSubheading
                                font.family: Theme.headingFamily
                                font.weight: Font.DemiBold
                                wrapMode: Text.WordWrap
                            }

                            AppButton {
                                visible: root.app.lastDownloadDirectory.length > 0
                                text: qsTr("Open folder")
                                onClicked: Qt.openUrlExternally(Theme.fileUrl(root.app.lastDownloadDirectory))
                                Accessible.description: qsTr("Open the download folder")
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: root.app.resultDetail
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontBody
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Target %1 · %2")
                                  .arg(root.targetName)
                                  .arg(root.app.lastDownloadDirectory.length > 0
                                       ? root.app.lastDownloadDirectory : qsTr("no folder"))
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSmall
                            font.family: Theme.monoFamily
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
        }
    }
}

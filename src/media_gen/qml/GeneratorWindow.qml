pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: window

    required property var app
    property bool renderDark: false

    readonly property string statusText: app.busy ? qsTr("GENERATING")
                                                  : app.resultState === 1 ? qsTr("GENERATED")
                                                                          : app.canGenerate ? qsTr("READY")
                                                                                            : qsTr("INCOMPLETE")
    readonly property string statusTone: app.busy ? "accent"
                                                  : app.resultState === 1 ? "ok"
                                                                          : app.canGenerate ? "ok" : "warn"
    readonly property bool noCheckValues: app.fileCheckTypeIndex === 0
                                          && app.filesListCheckTypeIndex === 0
                                          && app.loadCheckTypeIndex === 0

    title: qsTr("ARINC Media Set Generator")
    width: 1280
    height: 900
    minimumWidth: 1120
    minimumHeight: 760
    visible: true
    color: Theme.bg

    palette.window: Theme.bg
    palette.windowText: Theme.text
    palette.base: Theme.neutral100
    palette.alternateBase: Theme.neutral200
    palette.text: Theme.text
    palette.button: Theme.surface
    palette.buttonText: Theme.text
    palette.brightText: Theme.accentText
    palette.highlight: Theme.accentFill
    palette.highlightedText: Theme.accentText
    palette.mid: Theme.divider
    palette.dark: Theme.neutral900
    palette.light: Theme.neutral100
    palette.placeholderText: Theme.textMuted
    palette.toolTipBase: Theme.surface
    palette.toolTipText: Theme.text
    palette.link: Theme.accentInk

    Component.onCompleted: Theme.forcedDark = renderDark;

    Shortcut {
        sequence: StandardKey.Quit
        onActivated: window.close()
    }

    header: AppHeader {
        appCode: "GEN"
        title: qsTr("ARINC Media Set Generator")
        subtitle: qsTr("ARINC 665 · media set, load, files, batch")

        StatusChip {
            text: window.statusText
            tone: window.statusTone
        }

        AppButton {
            text: qsTr("About")
            onClicked: aboutDialog.open()
            Accessible.name: qsTr("About the generator")
        }
    }

    footer: AppStatusBar {
        Rectangle {
            width: 6
            height: 6
            radius: 3
            color: window.app.busy ? Theme.accent : Theme.neutral600
        }

        Text {
            text: window.app.busy
                  ? (window.app.progressPercent < 0
                     ? qsTr("Building and verifying...")
                     : qsTr("Reading files... %1%").arg(window.app.progressPercent))
                  : window.app.canGenerate ? qsTr("Ready") : qsTr("Incomplete")
            color: window.app.canGenerate ? Theme.text : Theme.textMuted
            font.pixelSize: Theme.fontMono
            font.family: Theme.monoFamily
            font.bold: window.app.busy
        }

        Text {
            text: "·"
            color: Theme.textMuted
            font.pixelSize: Theme.fontMono
        }

        Text {
            Layout.fillWidth: true
            text: window.app.outputDirectory.length > 0
                  ? window.app.outputDirectory : qsTr("no output directory")
            color: Theme.textMuted
            font.pixelSize: Theme.fontMono
            font.family: Theme.monoFamily
            elide: Text.ElideMiddle
        }

        Text {
            text: qsTr("v%1").arg(Qt.application.version)
            color: Theme.textMuted
            font.pixelSize: Theme.fontMono
            font.family: Theme.monoFamily
        }
    }

    FolderDialog {
        id: outputDialog

        title: qsTr("Choose the output directory")
        onAccepted: window.app.chooseOutputDirectory(selectedFolder)
    }

    FileDialog {
        id: filesDialog

        title: qsTr("Choose the files for the media set")
        fileMode: FileDialog.OpenFiles
        onAccepted: window.app.addFiles(selectedFiles)
    }

    AppDialog {
        id: aboutDialog

        heading: qsTr("About the Media Set Generator")
        primaryText: qsTr("Close")
        width: 540

        content: ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space3

            Text {
                text: "GEN"
                color: Theme.accent
                font.pixelSize: Theme.fontDisplay
                font.family: Theme.headingFamily
                font.weight: Font.ExtraBold
                font.letterSpacing: Theme.trackingTitle
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Turns files chosen from disk into an ARINC 665 load and media set, written as the directory tree the data loader opens, and verifies it by reading it back.")
                color: Theme.textMuted
                font.pixelSize: Theme.fontBody
                wrapMode: Text.WordWrap
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Version %1 · GPL-3.0-or-later").arg(Qt.application.version)
                color: Theme.text
                font.pixelSize: Theme.fontSmall
                font.family: Theme.monoFamily
            }
        }

        onPrimaryClicked: aboutDialog.close()
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ---- the form, in numbered blocks ----
        ScrollView {
            id: formRail

            Layout.preferredWidth: 388
            Layout.minimumWidth: 348
            Layout.maximumWidth: 460
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: formRail.availableWidth
                spacing: 0

                RailBlock {
                    Layout.margins: Theme.space4
                    ordinal: "01"
                    label: qsTr("MEDIA SET")

                    Text {
                        text: qsTr("PART NUMBER")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontMicro
                        font.family: Theme.monoFamily
                        font.letterSpacing: 0.8
                    }

                    AppTextField {
                        id: mediaSetPartNumberField

                        Layout.fillWidth: true
                        text: window.app.mediaSetPartNumber
                        placeholderText: qsTr("ACM-SET-0001")
                        enabled: !window.app.busy
                        onTextEdited: window.app.mediaSetPartNumber = text
                        Accessible.name: qsTr("Media set part number")
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: mediaSetPartNumberField.text.length > 0
                                 && window.app.mediaSetPartNumberError.length > 0
                        text: window.app.mediaSetPartNumberError
                        color: Theme.accentInk
                        font.pixelSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2

                        Text {
                            text: qsTr("MEMBERS")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontMicro
                            font.family: Theme.monoFamily
                            font.letterSpacing: 0.8
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        AppSpinBox {
                            id: memberCount

                            Layout.preferredWidth: 110
                            from: 1
                            to: 255
                            value: window.app.memberCount
                            enabled: !window.app.busy
                            onValueModified: window.app.memberCount = value
                            Accessible.name: qsTr("Number of media set members")
                        }
                    }

                    Text {
                        text: qsTr("CHECK VALUES")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontMicro
                        font.family: Theme.monoFamily
                        font.letterSpacing: 0.8
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space1

                        Text {
                            text: qsTr("Each listed file")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSmall
                        }

                        AppComboBox {
                            Layout.fillWidth: true
                            model: window.app.checkValueOptions()
                            currentIndex: window.app.fileCheckTypeIndex
                            enabled: !window.app.busy
                            onActivated: window.app.fileCheckTypeIndex = currentIndex
                            Accessible.name: qsTr("Check value for each listed file")
                        }

                        Text {
                            text: qsTr("FILES.LUM itself")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSmall
                        }

                        AppComboBox {
                            Layout.fillWidth: true
                            model: window.app.checkValueOptions()
                            currentIndex: window.app.filesListCheckTypeIndex
                            enabled: !window.app.busy
                            onActivated: window.app.filesListCheckTypeIndex = currentIndex
                            Accessible.name: qsTr("Check value for FILES.LUM itself")
                        }
                    }

                    Text {
                        text: qsTr("OUTPUT")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontMicro
                        font.family: Theme.monoFamily
                        font.letterSpacing: 0.8
                    }

                    AppButton {
                        Layout.fillWidth: true
                        text: qsTr("Choose output directory...")
                        enabled: !window.app.busy
                        onClicked: outputDialog.open()
                        Accessible.name: qsTr("Choose the output directory")
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: window.app.outputDirectory.length > 0
                        text: window.app.outputDirectory
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSmall
                        font.family: Theme.monoFamily
                        elide: Text.ElideMiddle
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: window.app.outputDirectory.length > 0
                                 && window.app.outputDirectoryError.length > 0
                        text: window.app.outputDirectoryError
                        color: Theme.accentInk
                        font.pixelSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: window.app.outputDirectory.length === 0
                        text: qsTr("The set is written below this directory, in a directory named for its part number.")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    Layout.leftMargin: Theme.space4
                    Layout.rightMargin: Theme.space4
                    color: Theme.divider
                }

                RailBlock {
                    Layout.margins: Theme.space4
                    ordinal: "02"
                    label: qsTr("LOAD")

                    Text {
                        text: qsTr("PART NUMBER")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontMicro
                        font.family: Theme.monoFamily
                        font.letterSpacing: 0.8
                    }

                    AppTextField {
                        id: loadPartNumberField

                        Layout.fillWidth: true
                        text: window.app.loadPartNumber
                        placeholderText: qsTr("ACM47-1234-5678")
                        enabled: !window.app.busy
                        onTextEdited: window.app.loadPartNumber = text
                        Accessible.name: qsTr("Load part number")
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: loadPartNumberField.text.length > 0
                                 && window.app.loadPartNumberError.length > 0
                        text: window.app.loadPartNumberError
                        color: Theme.accentInk
                        font.pixelSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        text: qsTr("HEADER FILE")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontMicro
                        font.family: Theme.monoFamily
                        font.letterSpacing: 0.8
                    }

                    AppTextField {
                        id: headerFileField

                        Layout.fillWidth: true
                        text: window.app.headerFileName
                        placeholderText: qsTr("ACM47.LUH")
                        enabled: !window.app.busy
                        onTextEdited: window.app.headerFileName = text
                        Accessible.name: qsTr("Header file name")
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: headerFileField.text.length > 0
                                 && window.app.headerFileNameError.length > 0
                        text: window.app.headerFileNameError
                        color: Theme.accentInk
                        font.pixelSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        text: qsTr("LOAD TYPE")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontMicro
                        font.family: Theme.monoFamily
                        font.letterSpacing: 0.8
                    }

                    AppComboBox {
                        Layout.fillWidth: true
                        model: window.app.loadTypeOptions()
                        currentIndex: window.app.loadTypeIndex + 1
                        enabled: !window.app.busy
                        onActivated: window.app.loadTypeIndex = currentIndex - 1
                        Accessible.name: qsTr("Load type")
                    }

                    Text {
                        text: qsTr("LOAD CHECK VALUE")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontMicro
                        font.family: Theme.monoFamily
                        font.letterSpacing: 0.8
                    }

                    AppComboBox {
                        Layout.fillWidth: true
                        model: window.app.checkValueOptions()
                        currentIndex: window.app.loadCheckTypeIndex
                        enabled: !window.app.busy
                        onActivated: window.app.loadCheckTypeIndex = currentIndex
                        Accessible.name: qsTr("Check value of the load header")
                    }

                    Text {
                        text: qsTr("TARGET HARDWARE IDS")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontMicro
                        font.family: Theme.monoFamily
                        font.letterSpacing: 0.8
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space1

                        Repeater {
                            model: window.app.targetHardwareIds

                            delegate: RowLayout {
                                id: targetIdRow

                                required property int index
                                required property string modelData

                                Layout.fillWidth: true
                                spacing: Theme.space2

                                Text {
                                    Layout.fillWidth: true
                                    text: targetIdRow.modelData
                                    color: Theme.text
                                    font.pixelSize: Theme.fontBody
                                    font.family: Theme.monoFamily
                                    elide: Text.ElideRight
                                }

                                AppButton {
                                    text: qsTr("Remove")
                                    enabled: !window.app.busy
                                    onClicked: window.app.removeTargetHardwareId(targetIdRow.index)
                                    Accessible.name: qsTr("Remove target hardware ID %1")
                                        .arg(targetIdRow.modelData)
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space2

                            AppTextField {
                                id: newTargetId

                                Layout.fillWidth: true
                                placeholderText: qsTr("ACME")
                                enabled: !window.app.busy
                                onAccepted: {
                                    window.app.addTargetHardwareId(text);
                                    text = "";
                                }
                                Accessible.name: qsTr("New target hardware ID")
                            }

                            AppButton {
                                text: qsTr("Add")
                                enabled: !window.app.busy && newTargetId.text.length > 0
                                onClicked: {
                                    window.app.addTargetHardwareId(newTargetId.text);
                                    newTargetId.text = "";
                                }
                                Accessible.name: qsTr("Add target hardware ID")
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: window.app.targetHardwareIdsError.length > 0
                            text: window.app.targetHardwareIdsError
                            color: Theme.accentInk
                            font.pixelSize: Theme.fontSmall
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    Layout.leftMargin: Theme.space4
                    Layout.rightMargin: Theme.space4
                    color: Theme.divider
                }

                RailBlock {
                    Layout.margins: Theme.space4
                    ordinal: "03"
                    label: qsTr("BATCH")

                    AppCheckBox {
                        Layout.fillWidth: true
                        text: qsTr("Include a batch file")
                        note: qsTr("One BATCH file that names every load in the set.")
                        checked: window.app.includeBatch
                        enabled: !window.app.busy
                        onToggled: (value) => window.app.includeBatch = value
                    }

                    AppTextField {
                        Layout.fillWidth: true
                        text: window.app.batchPartNumber
                        placeholderText: qsTr("ACM4C-0100-0200")
                        enabled: window.app.includeBatch && !window.app.busy
                        onTextEdited: window.app.batchPartNumber = text
                        Accessible.name: qsTr("Batch part number")
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: window.app.includeBatch
                        text: qsTr("Written as %1.LUB").arg(window.app.batchPartNumber.slice(0, 3))
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSmall
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: window.app.includeBatch && window.app.batchPartNumberError.length > 0
                        text: window.app.batchPartNumberError
                        color: Theme.accentInk
                        font.pixelSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                    }
                }

                Item {
                    Layout.preferredHeight: Theme.space6
                }
            }
        }

        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 2
            color: Theme.divider
        }

        // ---- the files and the action ----
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            ColumnLayout {
                Layout.fillWidth: true
                Layout.margins: Theme.space6
                spacing: Theme.space1

                Text {
                    text: qsTr("THE MEDIA SET")
                    color: Theme.accentInk
                    font.pixelSize: Theme.fontMicro
                    font.family: Theme.monoFamily
                    font.letterSpacing: 1.2
                    font.bold: true
                }

                Text {
                    Layout.fillWidth: true
                    text: window.app.mediaSetPartNumber.length > 0
                          ? window.app.mediaSetPartNumber : qsTr("New media set")
                    color: Theme.text
                    font.pixelSize: Theme.fontHeadline
                    font.family: Theme.headingFamily
                    font.weight: Font.ExtraBold
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space2

                    StatusChip {
                        text: window.app.canGenerate ? qsTr("READY") : qsTr("INCOMPLETE")
                        tone: window.app.canGenerate ? "ok" : "warn"
                    }

                    Text {
                        Layout.fillWidth: true
                        text: Theme.quantity(window.app.memberCount, qsTr("member"), qsTr("members"))
                              + " · "
                              + Theme.quantity(filesList.count, qsTr("file"), qsTr("files"))
                              + " · 1 load"
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontBody
                        elide: Text.ElideRight
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: window.noCheckValues
                    spacing: Theme.space2

                    StatusChip {
                        text: qsTr("NO CHECK VALUES")
                        tone: "warn"
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("The set is written, but the loader cannot verify its files against the header. Choose a check value unless the format requires none.")
                        color: Theme.accentInk
                        font.pixelSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 2
                color: Theme.divider
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: Theme.space6
                Layout.rightMargin: Theme.space6
                Layout.topMargin: Theme.space4
                spacing: Theme.space2

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space2

                    AppButton {
                        text: qsTr("Add files...")
                        enabled: !window.app.busy
                        onClicked: filesDialog.open()
                        Accessible.name: qsTr("Add files to the media set")
                    }

                    AppButton {
                        text: qsTr("Move up")
                        enabled: !window.app.busy && filesList.currentIndex > 0
                        onClicked: window.app.moveFileUp(filesList.currentIndex)
                        Accessible.name: qsTr("Move the chosen file up")
                    }

                    AppButton {
                        text: qsTr("Move down")
                        enabled: !window.app.busy
                                 && filesList.currentIndex >= 0
                                 && filesList.currentIndex < filesList.count - 1
                        onClicked: window.app.moveFileDown(filesList.currentIndex)
                        Accessible.name: qsTr("Move the chosen file down")
                    }

                    AppButton {
                        text: qsTr("Clear")
                        enabled: !window.app.busy && filesList.count > 0
                        onClicked: window.app.clearFiles()
                        Accessible.name: qsTr("Clear the file list")
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Text {
                        text: Theme.quantity(filesList.count, qsTr("file"), qsTr("files"))
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSmall
                        font.family: Theme.monoFamily
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: Theme.space2
                    spacing: Theme.space2

                    Text {
                        Layout.preferredWidth: 290
                        text: qsTr("NAME ON THE MEDIA SET")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontMicro
                        font.family: Theme.monoFamily
                        font.letterSpacing: 0.8
                    }

                    Text {
                        Layout.preferredWidth: 110
                        text: qsTr("KIND")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontMicro
                        font.family: Theme.monoFamily
                        font.letterSpacing: 0.8
                    }

                    Text {
                        Layout.preferredWidth: 150
                        text: qsTr("CHECK VALUE")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontMicro
                        font.family: Theme.monoFamily
                        font.letterSpacing: 0.8
                    }

                    Text {
                        Layout.preferredWidth: 80
                        horizontalAlignment: Text.AlignRight
                        text: qsTr("SIZE")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontMicro
                        font.family: Theme.monoFamily
                        font.letterSpacing: 0.8
                    }

                    Item {
                        Layout.fillWidth: true
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Theme.divider
                }

                ListView {
                    id: filesList

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: window.app.files
                    currentIndex: -1
                    keyNavigationEnabled: true
                    ScrollBar.vertical: ScrollBar {}
                    Accessible.role: Accessible.List
                    Accessible.name: qsTr("Files of the media set")

                    delegate: Rectangle {
                        id: fileRow

                        required property var model
                        required property string sourcePath
                        required property string fileName
                        required property string kind
                        required property int checkType
                        required property string size
                        required property string message
                        required property int index

                        width: ListView.view.width
                        implicitHeight: fileLayout.implicitHeight + Theme.space2

                        color: filesList.currentIndex === fileRow.index
                               ? Theme.neutral200 : "transparent"

                        RowLayout {
                            id: fileLayout

                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: Theme.space2
                            spacing: Theme.space2

                            ColumnLayout {
                                Layout.preferredWidth: 290
                                spacing: 1

                                AppTextField {
                                    Layout.fillWidth: true
                                    text: fileRow.fileName
                                    enabled: !window.app.busy
                                    onTextEdited: fileRow.model.fileName = text
                                    onActiveFocusChanged: if (activeFocus)
                                                              filesList.currentIndex = fileRow.index
                                    Accessible.name: qsTr("Name of %1 on the media set")
                                        .arg(fileRow.sourcePath)
                                }

                                Text {
                                    Layout.fillWidth: true
                                    visible: fileRow.message.length > 0
                                    text: fileRow.message
                                    color: Theme.accentInk
                                    font.pixelSize: Theme.fontMicro
                                    elide: Text.ElideRight
                                }
                            }

                            AppComboBox {
                                Layout.preferredWidth: 110
                                model: ["data", "support"]
                                currentIndex: fileRow.kind === "support" ? 1 : 0
                                enabled: !window.app.busy
                                onActivated: fileRow.model.kind = currentIndex === 1
                                                                   ? "support" : "data"
                                Accessible.name: qsTr("Kind of %1").arg(fileRow.fileName)
                            }

                            AppComboBox {
                                Layout.preferredWidth: 150
                                model: window.app.checkValueOptions()
                                currentIndex: fileRow.checkType
                                enabled: !window.app.busy
                                onActivated: fileRow.model.checkType = currentIndex
                                Accessible.name: qsTr("Check value of %1").arg(fileRow.fileName)
                            }

                            Text {
                                Layout.preferredWidth: 80
                                horizontalAlignment: Text.AlignRight
                                text: fileRow.size
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontSmall
                                font.family: Theme.monoFamily
                            }

                            Item {
                                Layout.fillWidth: true
                            }

                            AppButton {
                                text: qsTr("Remove")
                                enabled: !window.app.busy
                                onClicked: window.app.removeFile(fileRow.index)
                                Accessible.name: qsTr("Remove %1").arg(fileRow.fileName)
                            }
                        }
                    }
                }

                EmptyState {
                    Layout.fillWidth: true
                    visible: filesList.count === 0
                    title: qsTr("No files chosen")
                    body: qsTr("Data files are the load's payload; support files carry it, for example CONFIG.LDR. The order is the order the target reads them.")

                    AppButton {
                        text: qsTr("Add files...")
                        enabled: !window.app.busy
                        onClicked: filesDialog.open()
                        Accessible.name: qsTr("Add files to the media set")
                    }
                }

                Text {
                    Layout.fillWidth: true
                    visible: window.app.filesError.length > 0 && filesList.count > 0
                    text: window.app.filesError
                    color: Theme.accentInk
                    font.pixelSize: Theme.fontSmall
                    wrapMode: Text.WordWrap
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.space6
                Layout.rightMargin: Theme.space6
                Layout.topMargin: Theme.space2
                visible: window.app.resultState !== 0
                implicitHeight: resultContent.implicitHeight + Theme.space3 * 2
                color: Theme.neutral100
                border.color: Theme.divider
                border.width: 1

                ColumnLayout {
                    id: resultContent

                    anchors.fill: parent
                    anchors.margins: Theme.space3
                    spacing: Theme.space1

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2

                        StatusChip {
                            objectName: "resultBanner"
                            text: window.app.resultState === 1
                                  ? qsTr("COMPLETE")
                                  : window.app.resultText.toLowerCase().indexOf("cancel") >= 0
                                    ? qsTr("CANCELLED") : qsTr("FAILED")
                            tone: window.app.resultState === 1
                                  ? "ok"
                                  : window.app.resultText.toLowerCase().indexOf("cancel") >= 0
                                    ? "neutral" : "fail"
                        }

                        Text {
                            Layout.fillWidth: true
                            text: window.app.resultText
                            color: Theme.text
                            font.pixelSize: Theme.fontBody
                            font.family: Theme.headingFamily
                            font.weight: Font.DemiBold
                            wrapMode: Text.WordWrap
                        }

                        AppButton {
                            visible: window.app.outputPath.length > 0
                            text: qsTr("Open folder")
                            onClicked: Qt.openUrlExternally(window.app.outputPath)
                            Accessible.name: qsTr("Open the generated media set")
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: window.app.resultDetail.length > 0
                        text: window.app.resultDetail
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSmall
                        font.family: Theme.monoFamily
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 76
                color: Theme.surface

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: 1
                    color: Theme.divider
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.space4
                    anchors.rightMargin: Theme.space4
                    anchors.topMargin: Theme.space2
                    anchors.bottomMargin: Theme.space2
                    spacing: Theme.space2

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space1

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space2

                            BusyMark {
                                visible: window.app.busy
                            }

                            Text {
                                Layout.fillWidth: true
                                text: window.app.busy
                                      ? (window.app.progressPercent < 0
                                         ? qsTr("Building and verifying the media set...")
                                         : window.app.progressPercent >= 100
                                           ? qsTr("Building and verifying the media set...")
                                           : qsTr("Reading the files... %1%").arg(window.app.progressPercent))
                                      : window.app.canGenerate
                                        ? (filesList.count === 1
                                           ? qsTr("Ready: 1 file to write and verify")
                                           : qsTr("Ready: %1 files to write and verify")
                                                 .arg(filesList.count))
                                        : qsTr("Complete the required fields: part numbers, header file, output directory and at least one file.")
                                color: Theme.text
                                font.pixelSize: Theme.fontBody
                                elide: Text.ElideRight
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            visible: window.app.busy
                            spacing: Theme.space2

                            TaskProgress {
                                Layout.fillWidth: true
                                value: Math.max(0, window.app.progressPercent)
                                indeterminate: window.app.progressPercent < 0
                            }

                            Text {
                                text: window.app.progressPercent >= 0
                                      ? window.app.progressPercent + "%" : qsTr("working")
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontMono
                                font.family: Theme.monoFamily
                            }
                        }
                    }

                    AppButton {
                        visible: window.app.busy
                        text: qsTr("Cancel")
                        onClicked: window.app.cancel()
                        Accessible.name: qsTr("Cancel the generation")
                    }

                    AppButton {
                        objectName: "generateButton"
                        primary: true
                        text: window.app.busy ? qsTr("Generating...")
                                              : qsTr("Generate media set")
                        enabled: window.app.canGenerate
                        onClicked: window.app.generate()
                        Accessible.name: qsTr("Generate the media set")
                    }
                }
            }
        }
    }
}

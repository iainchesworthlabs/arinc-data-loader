pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: window

    // The GeneratorModel (src/media_gen/core), handed over by main.cpp.
    required property var app

    // The tests render the window in the dark scheme (ADL_DARK=1): the offscreen platform
    // theme does not follow the color scheme hint, so the palette is set by hand, exactly as a
    // dark desktop theme supplies it.
    property bool renderDark: false

    function applyDarkPalette() {
        palette.window = "#2d2d30"
        palette.windowText = "#f0f0f0"
        palette.base = "#1e1e1e"
        palette.alternateBase = "#2d2d30"
        palette.text = "#f0f0f0"
        palette.placeholderText = "#999999"
        palette.button = "#3c3c3c"
        palette.buttonText = "#f0f0f0"
        palette.brightText = "#ff7070"
        palette.highlight = "#3574f0"
        palette.highlightedText = "#ffffff"
        palette.mid = "#555555"
        palette.dark = "#1e1e1e"
        palette.light = "#4a4a4a"
        palette.toolTipBase = "#2d2d30"
        palette.toolTipText = "#f0f0f0"
        palette.link = "#70b0ff"
    }

    readonly property bool dark: palette.window.hslLightness < 0.5
    readonly property color okColour: dark ? "#70d090" : "#1b6e3a"
    readonly property color failColour: dark ? "#ff7070" : "#b00020"
    readonly property color warnColour: dark ? "#e0b060" : "#8a5a00"

    title: qsTr("ARINC Media Set Generator")
    width: 1000
    height: 680
    minimumWidth: 900
    minimumHeight: 600
    visible: true

    Component.onCompleted: if (renderDark) applyDarkPalette()

    // A validation message of the form, or nothing (and then the label is not shown).
    component ErrorLabel: Label {
        required property string message

        visible: message.length > 0
        text: message
        color: window.failColour
        wrapMode: Text.Wrap
    }

    menuBar: MenuBar {
        Menu {
            title: qsTr("&File")

            Action {
                text: qsTr("&Quit")
                shortcut: StandardKey.Quit
                onTriggered: Qt.quit()
            }
        }
        Menu {
            title: qsTr("&Help")

            Action {
                text: qsTr("&About")
                onTriggered: aboutDialog.open()
            }
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

    MessageDialog {
        id: aboutDialog

        title: qsTr("About")
        text: qsTr("ARINC Media Set Generator\n\n"
                   + "Picks files and generates an ARINC 665 media set as the directory tree "
                   + "the ARINC Data Loader uploads to a target.")
    }

    ScrollView {
        anchors.fill: parent
        anchors.margins: 12
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: 12

            GroupBox {
                title: qsTr("Media set")
                Layout.fillWidth: true

                GridLayout {
                    anchors.fill: parent
                    columns: 2
                    columnSpacing: 12
                    rowSpacing: 6

                    Label {
                        text: qsTr("Part number")
                        opacity: 0.7
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        TextField {
                            Layout.fillWidth: true
                            text: window.app.mediaSetPartNumber
                            placeholderText: qsTr("ACM-SET-0001")
                            enabled: !window.app.busy
                            onTextEdited: window.app.mediaSetPartNumber = text
                            Accessible.name: qsTr("Media set part number")
                            Component.onCompleted: forceActiveFocus()
                        }
                        ErrorLabel {
                            message: window.app.mediaSetPartNumberError
                        }
                    }
                    Label {
                        text: qsTr("Members")
                        opacity: 0.7
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            text: qsTr("Number of media set members (1 to 255)")
                            opacity: 0.7
                        }
                        SpinBox {
                            from: 1
                            to: 255
                            value: window.app.memberCount
                            enabled: !window.app.busy
                            onValueModified: window.app.memberCount = value
                            Accessible.name: qsTr("Number of media set members")
                        }
                    }
                    Label {
                        text: qsTr("Check values")
                        opacity: 0.7
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Label {
                                    text: qsTr("For each listed file")
                                    opacity: 0.7
                                }
                                ComboBox {
                                    Layout.fillWidth: true
                                    model: window.app.checkValueOptions()
                                    currentIndex: window.app.fileCheckTypeIndex
                                    enabled: !window.app.busy
                                    onActivated: window.app.fileCheckTypeIndex = currentIndex
                                    Accessible.name: qsTr("Check value for each listed file")
                                }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Label {
                                    text: qsTr("For FILES.LUM itself")
                                    opacity: 0.7
                                }
                                ComboBox {
                                    Layout.fillWidth: true
                                    model: window.app.checkValueOptions()
                                    currentIndex: window.app.filesListCheckTypeIndex
                                    enabled: !window.app.busy
                                    onActivated: window.app.filesListCheckTypeIndex = currentIndex
                                    Accessible.name: qsTr("Check value for FILES.LUM itself")
                                }
                            }
                        }
                    }
                    Label {
                        text: qsTr("Output")
                        opacity: 0.7
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Button {
                                text: qsTr("Choose directory...")
                                enabled: !window.app.busy
                                onClicked: outputDialog.open()
                                Accessible.name: qsTr("Choose the output directory")
                            }
                            Label {
                                Layout.fillWidth: true
                                text: window.app.outputDirectory
                                elide: Text.ElideMiddle
                                opacity: 0.7
                            }
                        }
                        ErrorLabel {
                            message: window.app.outputDirectoryError
                        }
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("The media set is written to a directory of its part "
                                       + "number below this one; an existing set is never "
                                       + "overwritten (a counter is appended).")
                            wrapMode: Text.Wrap
                            opacity: 0.7
                        }
                    }
                }
            }

            GroupBox {
                title: qsTr("Load")
                Layout.fillWidth: true

                GridLayout {
                    anchors.fill: parent
                    columns: 2
                    columnSpacing: 12
                    rowSpacing: 6

                    Label {
                        text: qsTr("Load part number")
                        opacity: 0.7
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        TextField {
                            Layout.fillWidth: true
                            text: window.app.loadPartNumber
                            placeholderText: qsTr("ACM47-1234-5678")
                            enabled: !window.app.busy
                            onTextEdited: window.app.loadPartNumber = text
                            Accessible.name: qsTr("Load part number")
                        }
                        ErrorLabel {
                            message: window.app.loadPartNumberError
                        }
                    }
                    Label {
                        text: qsTr("Header file")
                        opacity: 0.7
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        TextField {
                            Layout.fillWidth: true
                            text: window.app.headerFileName
                            placeholderText: qsTr("ACM47.LUH")
                            enabled: !window.app.busy
                            onTextEdited: window.app.headerFileName = text
                            Accessible.name: qsTr("Header file name")
                        }
                        ErrorLabel {
                            message: window.app.headerFileNameError
                        }
                    }
                    Label {
                        text: qsTr("Load type")
                        opacity: 0.7
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            text: qsTr("Load type (what the load carries)")
                            opacity: 0.7
                        }
                        ComboBox {
                            Layout.fillWidth: true
                            model: window.app.loadTypeOptions()
                            currentIndex: window.app.loadTypeIndex + 1
                            enabled: !window.app.busy
                            onActivated: window.app.loadTypeIndex = currentIndex - 1
                            Accessible.name: qsTr("Load type")
                        }
                    }
                    Label {
                        text: qsTr("Targets")
                        opacity: 0.7
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            text: qsTr("Target hardware IDs (at least one recommended)")
                            opacity: 0.7
                        }
                        ListView {
                            id: targetList

                            Layout.fillWidth: true
                            Layout.preferredHeight: Math.max(60,
                                                      Math.min(140, targetList.count * 36))
                            visible: count > 0
                            clip: true
                            model: window.app.targetHardwareIds
                            Accessible.role: Accessible.List
                            Accessible.name: qsTr("Target hardware IDs")

                            delegate: RowLayout {
                                id: targetRow

                                required property int index
                                required property string modelData

                                width: ListView.view.width
                                spacing: 8

                                Label {
                                    Layout.fillWidth: true
                                    text: targetRow.modelData
                                    elide: Text.ElideRight
                                }
                                Button {
                                    text: qsTr("Remove")
                                    enabled: !window.app.busy
                                    onClicked: window.app.removeTargetHardwareId(targetRow.index)
                                    Accessible.name: qsTr("Remove target hardware ID %1")
                                        .arg(targetRow.modelData)
                                }
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            TextField {
                                id: newTargetId

                                Layout.fillWidth: true
                                placeholderText: qsTr("ACME")
                                enabled: !window.app.busy
                                onAccepted: {
                                    window.app.addTargetHardwareId(text)
                                    text = ""
                                }
                                Accessible.name: qsTr("New target hardware ID")
                            }
                            Button {
                                text: qsTr("Add")
                                enabled: !window.app.busy && newTargetId.text.length > 0
                                onClicked: {
                                    window.app.addTargetHardwareId(newTargetId.text)
                                    newTargetId.text = ""
                                }
                                Accessible.name: qsTr("Add target hardware ID")
                            }
                        }
                        ErrorLabel {
                            message: window.app.targetHardwareIdsError
                        }
                    }
                }
            }

            GroupBox {
                title: qsTr("Batch")
                Layout.fillWidth: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 6

                    CheckBox {
                        text: qsTr("Include a batch file (all the set's loads in one)")
                        checked: window.app.includeBatch
                        enabled: !window.app.busy
                        onToggled: window.app.includeBatch = checked
                        Accessible.name: qsTr("Include a batch file")
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            text: qsTr("Batch part number")
                            opacity: 0.7
                        }
                        TextField {
                            Layout.fillWidth: true
                            text: window.app.batchPartNumber
                            placeholderText: qsTr("ACM4C-0100-0200")
                            enabled: window.app.includeBatch && !window.app.busy
                            onTextEdited: window.app.batchPartNumber = text
                            Accessible.name: qsTr("Batch part number")
                        }
                        Label {
                            visible: window.app.includeBatch
                                     && window.app.batchPartNumber.length > 0
                            text: qsTr("written as %1.LUB")
                                .arg(window.app.batchPartNumber.slice(0, 3))
                            opacity: 0.7
                        }
                    }
                    ErrorLabel {
                        message: window.app.batchPartNumberError
                    }
                }
            }

            GroupBox {
                title: qsTr("Files")
                Layout.fillWidth: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Button {
                            text: qsTr("Add files...")
                            enabled: !window.app.busy
                            onClicked: filesDialog.open()
                            Accessible.name: qsTr("Add files to the media set")
                        }
                        Button {
                            text: qsTr("Move up")
                            enabled: !window.app.busy && filesList.currentIndex > 0
                            onClicked: window.app.moveFileUp(filesList.currentIndex)
                            Accessible.name: qsTr("Move the chosen file up")
                        }
                        Button {
                            text: qsTr("Move down")
                            enabled: !window.app.busy
                                     && filesList.currentIndex >= 0
                                     && filesList.currentIndex < filesList.count - 1
                            onClicked: window.app.moveFileDown(filesList.currentIndex)
                            Accessible.name: qsTr("Move the chosen file down")
                        }
                        Button {
                            text: qsTr("Clear")
                            enabled: !window.app.busy && filesList.count > 0
                            onClicked: window.app.clearFiles()
                            Accessible.name: qsTr("Clear the file list")
                        }
                        Label {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignRight
                            text: qsTr("%n file(s)", "", filesList.count)
                            opacity: 0.7
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Name on the media set")
                            opacity: 0.7
                        }
                        Label {
                            Layout.preferredWidth: 90
                            text: qsTr("Kind")
                            opacity: 0.7
                        }
                        Label {
                            Layout.preferredWidth: 120
                            text: qsTr("Check value")
                            opacity: 0.7
                        }
                        Label {
                            Layout.preferredWidth: 80
                            text: qsTr("Size")
                            opacity: 0.7
                        }
                        Label {
                            Layout.preferredWidth: 90
                            text: qsTr("")
                        }
                    }

                    ListView {
                        id: filesList

                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.max(120,
                                                  Math.min(320, filesList.contentHeight))
                        visible: count > 0
                        clip: true
                        model: window.app.files
                        currentIndex: -1
                        keyNavigationEnabled: true
                        Accessible.role: Accessible.List
                        Accessible.name: qsTr("Files of the media set")

                        // Which row Move up/Move down act on, shown for the keyboard too.
                        highlight: Rectangle {
                            color: window.dark ? "#33ffffff" : "#22000000"
                        }
                        highlightMoveDuration: 0

                        delegate: Item {
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
                            height: 56

                            MouseArea {
                                anchors.fill: parent
                                onClicked: filesList.currentIndex = fileRow.index
                            }
                            RowLayout {
                                anchors.fill: parent
                                spacing: 8

                                TextField {
                                    Layout.fillWidth: true
                                    text: fileRow.fileName
                                    enabled: !window.app.busy
                                    onTextEdited: fileRow.model.fileName = text
                                    Accessible.name: qsTr("Name of %1 on the media set")
                                        .arg(fileRow.sourcePath)
                                }
                                ComboBox {
                                    Layout.preferredWidth: 90
                                    // The values are the model's kinds, not display text.
                                    model: ["data", "support"]
                                    currentIndex: fileRow.kind === "support" ? 1 : 0
                                    enabled: !window.app.busy
                                    onActivated: fileRow.model.kind = currentIndex === 1 ? "support"
                                                                                         : "data"
                                    Accessible.name: qsTr("Kind of %1").arg(fileRow.fileName)
                                }
                                ComboBox {
                                    Layout.preferredWidth: 120
                                    model: window.app.checkValueOptions()
                                    currentIndex: fileRow.checkType
                                    enabled: !window.app.busy
                                    onActivated: fileRow.model.checkType = currentIndex
                                    Accessible.name: qsTr("Check value of %1").arg(fileRow.fileName)
                                }
                                Label {
                                    Layout.preferredWidth: 80
                                    text: fileRow.size
                                    opacity: 0.7
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: fileRow.message
                                    color: window.failColour
                                    wrapMode: Text.Wrap
                                    maximumLineCount: 2
                                    elide: Text.ElideRight
                                }
                                Button {
                                    text: qsTr("Remove")
                                    enabled: !window.app.busy
                                    onClicked: window.app.removeFile(fileRow.index)
                                    Accessible.name: qsTr("Remove %1").arg(fileRow.fileName)
                                }
                            }
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: filesList.count === 0
                        text: qsTr("No files chosen. Data files are the load's payload; "
                                   + "support files carry it, for example CONFIG.LDR.")
                        wrapMode: Text.Wrap
                        opacity: 0.7
                    }
                    ErrorLabel {
                        message: window.app.filesError
                    }
                }
            }

            GroupBox {
                title: qsTr("Generate")
                Layout.fillWidth: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Button {
                            objectName: "generateButton"
                            text: qsTr("Generate")
                            enabled: window.app.canGenerate
                            onClicked: window.app.generate()
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Reads the files, builds the media set, writes "
                                               + "it and reads it back to check it")
                            Accessible.name: qsTr("Generate the media set")
                        }
                        Button {
                            text: qsTr("Cancel")
                            enabled: window.app.busy
                            onClicked: window.app.cancel()
                            Accessible.name: qsTr("Cancel the generation")
                        }
                    }
                    ProgressBar {
                        Layout.fillWidth: true
                        visible: window.app.busy
                        from: 0
                        to: 100
                        indeterminate: window.app.progressPercent < 0
                        value: Math.max(window.app.progressPercent, 0)
                        Accessible.name: qsTr("Progress of the generation")
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: window.app.busy
                        text: window.app.progressPercent < 0
                              ? qsTr("Building and verifying...")
                              : qsTr("Reading the files... %1%").arg(window.app.progressPercent)
                    }
                    Label {
                        id: resultBanner

                        objectName: "resultBanner"
                        Layout.fillWidth: true
                        visible: window.app.resultState !== 0
                        text: window.app.resultText
                        font.bold: true
                        wrapMode: Text.Wrap
                        color: window.app.resultState === 1 ? window.okColour : window.failColour
                        Accessible.role: Accessible.StaticText
                        Accessible.name: qsTr("Result: %1").arg(text)
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: text.length > 0
                        text: window.app.resultDetail
                        wrapMode: Text.Wrap
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: window.app.validationFindings.length > 0
                        text: qsTr("%n difference(s) from the standard's recommendations "
                                   + "(warnings, not failures)", "",
                                   window.app.validationFindings.length)
                        color: window.warnColour
                    }
                    Repeater {
                        model: window.app.validationFindings

                        delegate: Label {
                            id: findingRow

                            required property string modelData

                            Layout.fillWidth: true
                            text: findingRow.modelData
                            wrapMode: Text.Wrap
                            opacity: 0.8
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("The generated directory is the media set: open it with the "
                                   + "ARINC Data Loader's 'Choose directory...'.")
                        wrapMode: Text.Wrap
                        opacity: 0.7
                    }
                }
            }
        }
    }
}

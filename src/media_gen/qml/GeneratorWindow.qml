pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: window

    required property var app
    property bool renderDark: false

    // Set once the operator has asked to generate an incomplete form: from then on every
    // required field says what is wrong with it, touched or not.
    property bool attempted: false

    // The file table's columns, shared by its header and its rows.
    readonly property int colNameMin: 160
    readonly property int colKind: 110
    readonly property int colCheck: 150
    readonly property int colSize: 80
    readonly property int colRemove: 96

    // What still has to be right before a set can be written, in the order the form asks for it.
    readonly property var needs: {
        const list = [];
        if (app.mediaSetPartNumberError.length > 0)
            list.push(qsTr("media set part number"));
        if (app.loadPartNumberError.length > 0)
            list.push(qsTr("load part number"));
        if (app.headerFileNameError.length > 0)
            list.push(qsTr("header file name"));
        if (app.includeBatch && app.batchPartNumberError.length > 0)
            list.push(qsTr("batch part number"));
        if (app.targetHardwareIdsError.length > 0)
            list.push(qsTr("target hardware IDs"));
        if (app.outputDirectoryError.length > 0)
            list.push(qsTr("output folder"));
        if (app.filesError.length > 0)
            list.push(qsTr("a data file"));
        return list;
    }
    // Nothing has been entered yet: the window is waiting, not complaining.
    readonly property bool pristine: app.mediaSetPartNumber.length === 0
                                     && app.loadPartNumber.length === 0
                                     && app.headerFileName.length === 0
                                     && app.targetHardwareIds.length === 0
                                     && app.outputDirectory.length === 0
                                     && !app.includeBatch
                                     && filesList.count === 0

    // The state, once, in the header. The panel, the footer and the action bar say other things.
    readonly property string statusText: app.busy ? qsTr("GENERATING")
                                                  : app.resultState === 1 ? qsTr("GENERATED")
                                                  : app.resultState === 2 ? qsTr("FAILED")
                                                  : app.resultState === 3 ? qsTr("CANCELLED")
                                                  : app.canGenerate ? qsTr("READY")
                                                  : pristine ? qsTr("NOT STARTED")
                                                             : qsTr("INCOMPLETE")
    readonly property string statusTone: app.busy ? "accent"
                                                  : app.resultState === 1 ? "ok"
                                                  : app.resultState === 2 ? "fail"
                                                  : app.resultState === 3 ? "neutral"
                                                  : app.canGenerate ? "ok" : "neutral"
    readonly property bool noCheckValues: app.fileCheckTypeIndex === 0
                                          && app.filesListCheckTypeIndex === 0
                                          && app.loadCheckTypeIndex === 0
    readonly property bool modalOpen: aboutDialog.opened || licencesDialog.opened

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

    // The message under a field. It is shown for a field the operator has been in, or for all of
    // them once Generate was pressed on an incomplete form; a fresh form shows none.
    component FieldError: Text {
        required property string message
        property bool touched: false

        Layout.fillWidth: true
        visible: (touched || window.attempted) && message.length > 0
        text: message
        color: Theme.accentInk
        font.pixelSize: Theme.fontSmall
        wrapMode: Text.WordWrap
    }

    Shortcut {
        sequence: StandardKey.Quit
        onActivated: window.close()
    }

    header: AppHeader {
        appCode: "GEN"
        title: qsTr("ARINC Media Set Generator")
        subtitle: qsTr("ARINC 665 · media set, load, files, batch")

        StatusChip {
            objectName: "statusChip"
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
                  : window.needs.length > 0
                    ? Theme.quantity(window.needs.length, qsTr("required item left"),
                                   qsTr("required items left"))
                    : window.app.resultState === 1 ? qsTr("Written and verified")
                    : window.app.resultState !== 0 ? qsTr("Nothing was written")
                                                   : qsTr("All required items are filled in")
            color: window.needs.length === 0 ? Theme.text : Theme.textMuted
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
                  ? window.app.outputDirectory : qsTr("No output folder chosen")
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

        title: qsTr("Choose the output folder")
        onAccepted: window.app.chooseOutputDirectory(selectedFolder)
    }

    FileDialog {
        id: filesDialog

        title: qsTr("Choose the files for the media set")
        fileMode: FileDialog.OpenFiles
        onAccepted: window.app.addFiles(selectedFiles)
    }

    AboutDialog {
        id: aboutDialog

        appCode: "GEN"
        appName: qsTr("ARINC Media Set Generator")
        blurb: qsTr("Turns files chosen from disk into an ARINC 665 load and media set, written as the folder tree the data loader opens, and verifies it by reading it back.")
        onShowLicences: licencesDialog.open()
    }

    LicencesDialog {
        id: licencesDialog
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ---- the form, in numbered blocks ----
        ScrollView {
            id: formRail

            Layout.preferredWidth: Theme.railWidth
            Layout.minimumWidth: Theme.railMinWidth
            Layout.maximumWidth: Theme.railMaxWidth
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
                        property bool touched: false

                        placeholderText: qsTr("e.g. ACM-SET-0001")
                        enabled: !window.app.busy
                        onTextEdited: {
                            touched = true;
                            window.app.mediaSetPartNumber = text;
                        }
                        onActiveFocusChanged: if (!activeFocus) touched = true
                        Accessible.name: qsTr("Media set part number")
                    }

                    FieldError {
                        touched: mediaSetPartNumberField.touched
                        message: window.app.mediaSetPartNumberError
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
                        text: qsTr("Choose folder...")
                        enabled: !window.app.busy
                        onClicked: outputDialog.open()
                        Accessible.name: qsTr("Choose the output folder")
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

                    FieldError {
                        // A folder that was chosen and is gone is wrong at once; one not chosen
                        // yet is only wrong when the operator tries to generate.
                        touched: window.app.outputDirectory.length > 0
                        message: window.app.outputDirectoryError
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: window.app.outputDirectory.length === 0
                        text: qsTr("The set is written below this folder, in a folder named for its part number.")
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
                        property bool touched: false

                        placeholderText: qsTr("e.g. ACM47-1234-5678")
                        enabled: !window.app.busy
                        onTextEdited: {
                            touched = true;
                            window.app.loadPartNumber = text;
                        }
                        onActiveFocusChanged: if (!activeFocus) touched = true
                        Accessible.name: qsTr("Load part number")
                    }

                    FieldError {
                        touched: loadPartNumberField.touched
                        message: window.app.loadPartNumberError
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
                        property bool touched: false

                        placeholderText: qsTr("e.g. ACM47.LUH")
                        enabled: !window.app.busy
                        onTextEdited: {
                            touched = true;
                            window.app.headerFileName = text;
                        }
                        onActiveFocusChanged: if (!activeFocus) touched = true
                        Accessible.name: qsTr("Header file name")
                    }

                    FieldError {
                        touched: headerFileField.touched
                        message: window.app.headerFileNameError
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
                                placeholderText: qsTr("e.g. ACME")
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

                        // An identifier that was added and is wrong says so at once.
                        FieldError {
                            touched: true
                            message: window.app.targetHardwareIdsError
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
                        id: batchPartNumberField

                        property bool touched: false

                        Layout.fillWidth: true
                        text: window.app.batchPartNumber
                        placeholderText: qsTr("e.g. ACM4C-0100-0200")
                        enabled: window.app.includeBatch && !window.app.busy
                        onTextEdited: {
                            touched = true;
                            window.app.batchPartNumber = text;
                        }
                        onActiveFocusChanged: if (!activeFocus) touched = true
                        Accessible.name: qsTr("Batch part number")
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: window.app.includeBatch && window.app.batchPartNumber.length >= 3
                        text: qsTr("Written as %1.LUB").arg(window.app.batchPartNumber.slice(0, 3))
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSmall
                    }

                    FieldError {
                        touched: batchPartNumberField.touched && window.app.includeBatch
                        message: window.app.includeBatch ? window.app.batchPartNumberError : ""
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

                Text {
                    Layout.fillWidth: true
                    text: qsTr("%1 · %2 · %3")
                          .arg(Theme.quantity(window.app.memberCount, qsTr("member"), qsTr("members")))
                          .arg(Theme.quantity(filesList.count, qsTr("file"), qsTr("files")))
                          .arg(Theme.quantity(1, qsTr("load"), qsTr("loads")))
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontBody
                    elide: Text.ElideRight
                }

                // Only once there is something to write: a form with no files has no check
                // values to miss yet.
                RowLayout {
                    Layout.fillWidth: true
                    visible: window.noCheckValues && filesList.count > 0
                    spacing: Theme.space2

                    StatusChip {
                        text: qsTr("NO CHECK VALUES")
                        tone: "warn"
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("With no check value the loader cannot verify these files against the header. Choose one unless the format requires none.")
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

                // The header and the rows lay out the same columns: the name takes what is left,
                // the others are fixed (window.colKind and the like), and the margins match.
                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: Theme.space2
                    Layout.leftMargin: Theme.space2
                    Layout.rightMargin: Theme.space2
                    visible: filesList.count > 0
                    spacing: Theme.space2

                    Text {
                        Layout.fillWidth: true
                        Layout.minimumWidth: window.colNameMin
                        text: qsTr("NAME ON THE MEDIA SET")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontMicro
                        font.family: Theme.monoFamily
                        font.letterSpacing: 0.8
                    }

                    Text {
                        Layout.preferredWidth: window.colKind
                        Layout.minimumWidth: window.colKind
                        text: qsTr("KIND")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontMicro
                        font.family: Theme.monoFamily
                        font.letterSpacing: 0.8
                    }

                    Text {
                        Layout.preferredWidth: window.colCheck
                        Layout.minimumWidth: window.colCheck
                        text: qsTr("CHECK VALUE")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontMicro
                        font.family: Theme.monoFamily
                        font.letterSpacing: 0.8
                    }

                    Text {
                        Layout.preferredWidth: window.colSize
                        Layout.minimumWidth: window.colSize
                        horizontalAlignment: Text.AlignRight
                        text: qsTr("SIZE")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontMicro
                        font.family: Theme.monoFamily
                        font.letterSpacing: 0.8
                    }

                    Item {
                        Layout.preferredWidth: window.colRemove
                        Layout.minimumWidth: window.colRemove
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    visible: filesList.count > 0
                    color: Theme.divider
                }

                ListView {
                    id: filesList

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: count > 0
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
                                Layout.fillWidth: true
                                Layout.minimumWidth: window.colNameMin
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
                                Layout.preferredWidth: window.colKind
                                Layout.minimumWidth: window.colKind
                                model: [qsTr("data"), qsTr("support")]
                                currentIndex: fileRow.kind === "support" ? 1 : 0
                                enabled: !window.app.busy
                                onActivated: fileRow.model.kind = currentIndex === 1
                                                                   ? "support" : "data"
                                Accessible.name: qsTr("Kind of %1").arg(fileRow.fileName)
                            }

                            AppComboBox {
                                Layout.preferredWidth: window.colCheck
                                Layout.minimumWidth: window.colCheck
                                model: window.app.checkValueOptions()
                                currentIndex: fileRow.checkType
                                enabled: !window.app.busy
                                onActivated: fileRow.model.checkType = currentIndex
                                Accessible.name: qsTr("Check value of %1").arg(fileRow.fileName)
                            }

                            Text {
                                Layout.preferredWidth: window.colSize
                                Layout.minimumWidth: window.colSize
                                horizontalAlignment: Text.AlignRight
                                text: fileRow.size
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontSmall
                                font.family: Theme.monoFamily
                            }

                            AppButton {
                                Layout.preferredWidth: window.colRemove
                                Layout.minimumWidth: window.colRemove
                                text: qsTr("Remove")
                                enabled: !window.app.busy
                                onClicked: window.app.removeFile(fileRow.index)
                                Accessible.name: qsTr("Remove %1").arg(fileRow.fileName)
                            }
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: filesList.count === 0

                    EmptyState {
                        anchors.centerIn: parent
                        width: Math.min(parent.width, 520)
                        title: qsTr("No files chosen")
                        body: qsTr("Data files are the load's payload; support files carry it, for example CONFIG.LDR. The order is the order the target reads them.")

                        AppButton {
                            text: qsTr("Add files...")
                            enabled: !window.app.busy
                            onClicked: filesDialog.open()
                            Accessible.name: qsTr("Add files to the media set")
                        }
                    }
                }

                FieldError {
                    // The empty list has its own empty state; this is for files that are there.
                    touched: filesList.count > 0
                    message: window.app.filesError
                }
            }

            // Generate was pressed on a form that is not complete: say what is left.
            AppBanner {
                objectName: "attemptBanner"
                Layout.fillWidth: true
                Layout.leftMargin: Theme.space6
                Layout.rightMargin: Theme.space6
                Layout.topMargin: Theme.space2
                visible: window.attempted && !window.app.canGenerate && !window.app.busy
                tone: "warning"
                text: qsTr("The media set cannot be generated yet.")
                detail: qsTr("Still needed: %1.").arg(window.needs.join(", "))
            }

            // How the last generation ended. The state word is in the header; this says what
            // happened, and where.
            AppBanner {
                objectName: "resultBanner"
                Layout.fillWidth: true
                Layout.leftMargin: Theme.space6
                Layout.rightMargin: Theme.space6
                Layout.topMargin: Theme.space2
                visible: window.app.resultState !== 0 && !window.app.busy
                tone: window.app.resultState === 1 ? "success"
                                                   : window.app.resultState === 3 ? "info" : "error"
                text: window.app.resultText
                detail: window.app.resultDetail

                AppButton {
                    visible: window.app.outputPath.length > 0
                    text: qsTr("Open folder")
                    onClicked: Qt.openUrlExternally(window.app.outputPath)
                    Accessible.name: qsTr("Open the generated media set")
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
                                        ? filesList.count === 1 ? qsTr("1 file will be written and verified")
                                              : qsTr("%1 files will be written and verified")
                                                    .arg(filesList.count)
                                        : window.pristine
                                          ? qsTr("Nothing entered yet. Fill in the form and add the files.")
                                          : qsTr("Still needed: %1.").arg(window.needs.join(", "))
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
                        // Pressed on an incomplete form it marks what is missing, instead of
                        // being dead without saying why.
                        enabled: !window.app.busy
                        onClicked: {
                            if (window.app.canGenerate) {
                                window.attempted = false;
                                window.app.generate();
                            } else {
                                window.attempted = true;
                            }
                        }
                        Accessible.name: qsTr("Generate the media set")
                        Accessible.description: window.app.canGenerate
                                                ? "" : qsTr("Required items are still missing")
                    }
                }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        z: 900
        visible: window.modalOpen
        color: Theme.dark ? Qt.rgba(0, 0, 0, 0.62) : Qt.rgba(0, 0, 0, 0.32)

        MouseArea {
            anchors.fill: parent
        }
    }
}

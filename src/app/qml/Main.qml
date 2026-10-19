import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: window

    // The LoaderModel (src/app/core), handed over by main.cpp.
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
    readonly property color warnColour: dark ? "#ffb060" : "#a04000"

    title: qsTr("ARINC Data Loader")
    width: 1100
    height: 760
    minimumWidth: 760
    minimumHeight: 520
    visible: true

    Component.onCompleted: if (renderDark) applyDarkPalette()

    menuBar: MenuBar {
        Menu {
            title: qsTr("&File")
            Action {
                text: qsTr("&Preferences...")
                onTriggered: preferencesDialog.open()
            }
            MenuSeparator {}
            Action {
                text: qsTr("&Quit")
                shortcut: StandardKey.Quit
                onTriggered: window.close()
            }
        }
        Menu {
            title: qsTr("&Target")
            Action {
                text: qsTr("&Find targets")
                enabled: !window.app.busy
                onTriggered: window.app.findTargets()
            }
            Action {
                text: qsTr("&Read information")
                enabled: window.app.canReadInformation
                onTriggered: window.app.readInformation()
            }
            Action {
                text: qsTr("&Upload")
                enabled: window.app.canUpload
                onTriggered: window.app.uploadSelected()
            }
        }
        Menu {
            title: qsTr("&Media set")
            Action {
                text: qsTr("&Choose directory...")
                enabled: !window.app.busy
                onTriggered: mediaSetDialog.open()
            }
            Action {
                text: qsTr("&Check media set")
                enabled: !window.app.busy && window.app.mediaSetPath.length > 0
                onTriggered: window.app.checkMediaSet()
            }
            Action {
                text: qsTr("&Properties...")
                enabled: window.app.mediaSetPath.length > 0
                onTriggered: mediaSetPropertiesDialog.open()
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
        id: mediaSetDialog

        title: qsTr("Choose the media set directory")
        onAccepted: window.app.chooseMediaSet(selectedFolder)
    }

    PreferencesDialog {
        id: preferencesDialog

        objectName: "preferencesDialog"
        app: window.app
    }

    MediaSetPropertiesDialog {
        id: mediaSetPropertiesDialog

        objectName: "mediaSetPropertiesDialog"
        app: window.app
    }

    TargetPropertiesDialog {
        id: targetPropertiesDialog

        objectName: "targetPropertiesDialog"
        app: window.app
    }

    AboutDialog {
        id: aboutDialog

        objectName: "aboutDialog"
    }

    // One dialog per failed job: what happened, what it means, and what to do when the
    // application knows. `dismissError` acknowledges it, and the next failure opens it again.
    // A Qt Quick dialog rather than a native message box, like the rest of the window, so it can
    // be read and dismissed by the UI suite (a native modal runs its own event loop).
    Dialog {
        id: errorDialog

        objectName: "errorDialog"
        title: qsTr("The operation failed")
        modal: true
        standardButtons: Dialog.Ok
        width: 560

        property alias text: errorText.text

        contentItem: Label {
            id: errorText

            width: errorDialog.availableWidth
            wrapMode: Text.Wrap
        }

        onAccepted: window.app.dismissError()
        onRejected: window.app.dismissError()
    }

    Connections {
        target: window.app
        function onErrorChanged() {
            if (window.app.lastErrorText.length > 0) {
                errorDialog.text = window.app.lastErrorText
                        + (window.app.lastErrorDetail.length > 0
                           ? "\n\n" + window.app.lastErrorDetail : "")
                        + (window.app.lastErrorHint.length > 0
                           ? "\n\n" + window.app.lastErrorHint : "")
                errorDialog.open()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 12
            orientation: Qt.Horizontal

            // ---- what the operator sets up and starts ----
            ScrollView {
                SplitView.preferredWidth: 440
                SplitView.minimumWidth: 340
                contentWidth: availableWidth
                clip: true

                ColumnLayout {
                    width: parent.width
                    spacing: 12

                    Panel {
                        title: qsTr("Media set")

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 8

                            RowLayout {
                                Button {
                                    objectName: "mediaSetChooseButton"

                                    text: qsTr("Choose directory...")
                                    enabled: !window.app.busy
                                    onClicked: {
                                        if (window.app.lastMediaSetDirectory.length > 0)
                                            mediaSetDialog.currentFolder
                                                    = window.app.lastMediaSetDirectory
                                        mediaSetDialog.open()
                                    }
                                }
                                Button {
                                    objectName: "mediaSetCheckButton"

                                    text: qsTr("Check media set")
                                    enabled: !window.app.busy
                                             && window.app.mediaSetPath.length > 0
                                    onClicked: window.app.checkMediaSet()
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Reads the set again and checks it; an upload needs a set that has been checked valid")
                                }
                                Button {
                                    objectName: "mediaSetPropertiesButton"

                                    text: qsTr("Properties...")
                                    enabled: window.app.mediaSetPath.length > 0
                                    onClicked: mediaSetPropertiesDialog.open()
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: window.app.mediaSetPath
                                    elide: Text.ElideMiddle
                                    opacity: 0.7
                                }
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: window.app.mediaSetPath.length === 0
                                text: qsTr("No media set chosen. A media set is a directory with one directory per member.")
                                wrapMode: Text.Wrap
                                opacity: 0.7
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: text.length > 0
                                text: window.app.mediaSetError
                                color: window.failColour
                                wrapMode: Text.Wrap
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: text.length > 0
                                text: window.app.mediaSetTitle
                                font.bold: true
                                wrapMode: Text.Wrap
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: window.app.validationState === 1
                                text: qsTr("Checking the media set...")
                                wrapMode: Text.Wrap
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: window.app.validationState === 2
                                text: qsTr("Media set checked: %1").arg(window.app.validationDetail)
                                color: window.okColour
                                wrapMode: Text.Wrap
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: window.app.validationState === 3
                                text: qsTr("The media set is not valid: %1").arg(window.app.validationError)
                                color: window.failColour
                                wrapMode: Text.Wrap
                            }
                            RowLayout {
                                visible: loadList.count > 0
                                Button {
                                    objectName: "tickAllLoadsButton"

                                    text: qsTr("Tick all")
                                    enabled: !window.app.busy
                                    onClicked: window.app.selectAllLoads(true)
                                }
                                Button {
                                    objectName: "tickNoLoadsButton"

                                    text: qsTr("Tick none")
                                    enabled: !window.app.busy
                                    onClicked: window.app.selectAllLoads(false)
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("%n load(s) ticked for upload", "", window.app.selectedLoadCount)
                                    opacity: 0.7
                                    elide: Text.ElideRight
                                }
                            }
                            Repeater {
                                id: loadList

                                objectName: "loadList"
                                model: window.app.loads
                                delegate: ColumnLayout {
                                    id: loadRow

                                    required property var model
                                    required property string partNumber
                                    required property string detail
                                    required property bool checked
                                    required property int ratio
                                    required property string loadStatus
                                    required property bool failed
                                    required property string problem

                                    Layout.fillWidth: true
                                    spacing: 2

                                    CheckBox {
                                        objectName: "loadCheck"
                                        Layout.fillWidth: true
                                        text: loadRow.partNumber
                                        checked: loadRow.checked
                                        enabled: !window.app.busy
                                        onToggled: loadRow.model.checked = checked
                                        Accessible.name: qsTr("Upload load %1").arg(loadRow.partNumber)
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        Layout.leftMargin: 28
                                        text: loadRow.detail
                                        opacity: 0.7
                                        wrapMode: Text.Wrap
                                    }
                                    ProgressBar {
                                        Layout.fillWidth: true
                                        Layout.leftMargin: 28
                                        visible: loadRow.ratio >= 0
                                        from: 0
                                        to: 100
                                        value: loadRow.ratio
                                        Accessible.name: qsTr("Upload progress of %1").arg(loadRow.partNumber)
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        Layout.leftMargin: 28
                                        visible: loadRow.ratio >= 0
                                        text: loadRow.ratio + "% " + loadRow.loadStatus
                                              + (loadRow.problem.length > 0 ? ": " + loadRow.problem : "")
                                        color: loadRow.failed ? window.failColour : window.palette.windowText
                                        wrapMode: Text.Wrap
                                    }
                                }
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: window.app.mediaSetFindings.length > 0
                                text: qsTr("%n warning(s) against the standard's recommendations", "",
                                           window.app.mediaSetFindings.length)
                                color: window.warnColour
                            }
                            Repeater {
                                model: window.app.mediaSetFindings
                                delegate: Label {
                                    required property string modelData

                                    Layout.fillWidth: true
                                    text: modelData
                                    wrapMode: Text.Wrap
                                    opacity: 0.8
                                }
                            }
                        }
                    }

                    Panel {
                        title: qsTr("Target")

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 8

                            Button {
                                objectName: "findTargetsButton"

                                text: qsTr("Find targets")
                                enabled: !window.app.busy
                                onClicked: window.app.findTargets()
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Broadcasts FIND and listens for three seconds")
                            }
                            ListView {
                                id: targetList

                                objectName: "targetList"
                                Layout.fillWidth: true
                                Layout.preferredHeight: Math.min(contentHeight, 140)
                                visible: count > 0
                                clip: true
                                model: window.app.targets
                                Accessible.role: Accessible.List
                                Accessible.name: qsTr("Targets found")
                                delegate: ItemDelegate {
                                    required property int index
                                    required property string summary

                                    objectName: "targetDelegate"
                                    width: ListView.view.width
                                    text: summary
                                    highlighted: ListView.isCurrentItem
                                    onClicked: {
                                        targetList.currentIndex = index
                                        window.app.selectTarget(index)
                                    }
                                }
                            }
                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                columnSpacing: 8
                                rowSpacing: 6

                                Label { text: qsTr("Address") }
                                TextField {
                                    objectName: "targetAddressField"

                                    Layout.fillWidth: true
                                    placeholderText: qsTr("10.0.0.2")
                                    text: window.app.targetAddress
                                    enabled: !window.app.busy
                                    onTextEdited: window.app.targetAddress = text
                                    Accessible.name: qsTr("Target address")
                                    Component.onCompleted: forceActiveFocus()
                                }
                                Label { text: qsTr("THW_ID_POS") }
                                TextField {
                                    objectName: "thwIdPosField"

                                    Layout.fillWidth: true
                                    placeholderText: qsTr("HNPFMS_L")
                                    text: window.app.thwIdPos
                                    enabled: !window.app.busy
                                    onTextEdited: window.app.thwIdPos = text
                                    Accessible.name: qsTr("Target hardware identifier and position")
                                }
                            }
                            Button {
                                objectName: "targetPropertiesButton"

                                text: qsTr("Properties...")
                                enabled: window.app.selectedTargetIndex >= 0
                                         || window.app.targetAddress.length > 0
                                onClicked: targetPropertiesDialog.open()
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: !window.app.targetValid
                                text: qsTr("Choose a target, or enter its IPv4 address and THW_ID_POS.")
                                wrapMode: Text.Wrap
                                opacity: 0.7
                            }
                        }
                    }

                    DownloadPanel {
                        title: qsTr("Download")
                        app: window.app
                        failColour: window.failColour
                    }

                    Panel {
                        title: qsTr("Operation")

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 8

                            RowLayout {
                                Button {
                                    objectName: "readInformationButton"

                                    text: qsTr("Read information")
                                    enabled: window.app.canReadInformation
                                    onClicked: window.app.readInformation()
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Asks the target for its configuration (ARINC 615A Information)")
                                }
                                Button {
                                    objectName: "uploadButton"
                                    text: qsTr("Upload")
                                    enabled: window.app.canUpload
                                    onClicked: window.app.uploadSelected()
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Loads the ticked loads of the media set to the target (ARINC 615A Upload)")
                                }
                                Button {
                                    objectName: "abortButton"

                                    text: qsTr("Abort")
                                    enabled: window.app.canAbort
                                    onClicked: window.app.abort()
                                }
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: !window.app.canUpload
                                         && window.app.mediaSetPath.length > 0
                                         && window.app.selectedLoadCount > 0
                                         && window.app.targetValid
                                         && window.app.validationState !== 2
                                text: window.app.validationState === 3
                                      ? qsTr("Upload is refused: the media set is not valid.")
                                      : qsTr("Upload is refused until the media set has been checked.")
                                wrapMode: Text.Wrap
                                opacity: 0.7
                            }
                            ProgressBar {
                                objectName: "operationProgress"

                                Layout.fillWidth: true
                                visible: window.app.busy
                                from: 0
                                to: 100
                                // The target reports a figure for an upload or a download only;
                                // for the rest, and before the first status file, the bar only
                                // shows that it is busy.
                                indeterminate: window.app.progress < 0
                                value: Math.max(window.app.progress, 0)
                                Accessible.name: qsTr("Progress of the operation")
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: window.app.busy
                                text: window.app.progress < 0
                                      ? window.app.activity
                                      : window.app.activity + " " + window.app.progress + "%"
                            }
                            Label {
                                objectName: "resultLabel"

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
                        }
                    }
                }
            }

            // ---- what came back ----
            ColumnLayout {
                SplitView.fillWidth: true
                SplitView.minimumWidth: 320
                spacing: 12

                Panel {
                    title: qsTr("Target configuration")
                    Layout.fillHeight: true
                    Layout.preferredHeight: 1

                    ListView {
                        id: configurationList

                        objectName: "configurationList"
                        anchors.fill: parent
                        clip: true
                        model: window.app.configuration
                        Accessible.role: Accessible.List
                        Accessible.name: qsTr("Target configuration")
                        ScrollBar.vertical: ScrollBar {}

                        Label {
                            anchors.centerIn: parent
                            visible: configurationList.count === 0
                            text: qsTr("Nothing read yet.")
                            opacity: 0.6
                        }
                        delegate: ColumnLayout {
                            required property int level
                            required property string title
                            required property string detail

                            width: ListView.view.width
                            spacing: 0
                            Label {
                                Layout.leftMargin: level === 0 ? 0 : 24
                                Layout.topMargin: level === 0 ? 6 : 0
                                text: title
                                font.bold: level === 0
                            }
                            Label {
                                Layout.leftMargin: level === 0 ? 0 : 24
                                text: detail
                                opacity: 0.7
                            }
                        }
                    }
                }

                Panel {
                    title: qsTr("Activity")
                    Layout.fillHeight: true
                    Layout.preferredHeight: 1

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 6

                        LogView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            events: window.app.events
                        }
                        Button {
                            objectName: "clearLogButton"

                            text: qsTr("Clear")
                            onClicked: window.app.clearLog()
                        }
                    }
                }
            }
        }

        // ---- the status bar ----
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: window.palette.mid
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.topMargin: 4
            Layout.bottomMargin: 4
            spacing: 8

            Label {
                id: statusState

                objectName: "statusState"
                text: window.app.busy ? window.app.activity : qsTr("Idle")
                font.bold: window.app.busy
            }
            Label {
                text: "·"
                opacity: 0.5
            }
            Label {
                id: statusTarget

                objectName: "statusTarget"
                text: window.app.targetAddress.length > 0
                      ? window.app.targetAddress : qsTr("no target")
                opacity: 0.8
            }
            Label {
                text: "·"
                opacity: 0.5
            }
            Label {
                Layout.fillWidth: true
                text: window.app.mediaSetPath.length > 0
                      ? window.app.mediaSetPath : qsTr("no media set")
                elide: Text.ElideMiddle
                opacity: 0.8
            }
        }
    }
}

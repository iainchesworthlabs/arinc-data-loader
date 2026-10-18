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

    title: qsTr("ARINC Data Loader")
    width: 1100
    height: 760
    minimumWidth: 760
    minimumHeight: 520
    visible: true

    // The design tokens follow the palette Qt's Fusion style draws the standard controls from:
    // the dark scheme is the desktop's, not a second opinion. Set once, after the palette is.
    Component.onCompleted: {
        if (renderDark)
            applyDarkPalette();
        Theme.dark = palette.window.hslLightness < 0.5;
    }

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
            enabled: !window.app.firstRun
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
            enabled: !window.app.firstRun
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

        app: window.app
    }

    MediaSetPropertiesDialog {
        id: mediaSetPropertiesDialog

        app: window.app
    }

    TargetPropertiesDialog {
        id: targetPropertiesDialog

        app: window.app
    }

    AboutDialog {
        id: aboutDialog
    }

    // One dialog per failed job: what happened, what it means, and what to do when the
    // application knows. `dismissError` acknowledges it, and the next failure opens it again.
    MessageDialog {
        id: errorDialog

        title: qsTr("The operation failed")
        buttons: MessageDialog.Ok
        onButtonClicked: window.app.dismissError()
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

    // ---- the first launch ----
    FirstRunScreen {
        id: firstRun

        anchors.fill: parent
        visible: window.app.firstRun
        app: window.app
    }

    // ---- the workbench ----
    ColumnLayout {
        anchors.fill: parent
        visible: !window.app.firstRun
        spacing: 0

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: Theme.space3
            orientation: Qt.Horizontal

            // ---- what the operator sets up and starts ----
            ScrollView {
                SplitView.preferredWidth: 460
                SplitView.minimumWidth: 360
                contentWidth: availableWidth
                clip: true

                ColumnLayout {
                    width: parent.width
                    spacing: Theme.space3

                    Panel {
                        title: qsTr("Media set")

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: Theme.space2

                            RowLayout {
                                AppButton {
                                    text: qsTr("Choose directory...")
                                    enabled: !window.app.busy
                                    onClicked: {
                                        if (window.app.lastMediaSetDirectory.length > 0)
                                            mediaSetDialog.currentFolder
                                                    = window.app.lastMediaSetDirectory
                                        mediaSetDialog.open()
                                    }
                                    Accessible.name: qsTr("Choose the media set directory")
                                }
                                AppButton {
                                    text: qsTr("Check media set")
                                    enabled: !window.app.busy
                                             && window.app.mediaSetPath.length > 0
                                    onClicked: window.app.checkMediaSet()
                                    Accessible.name: qsTr("Check the media set")
                                    ToolTip.visible: hovered
                                    ToolTip.text: qsTr("Reads the set again and checks it; an upload needs a set that has been checked valid")
                                }
                                AppButton {
                                    text: qsTr("Properties...")
                                    enabled: window.app.mediaSetPath.length > 0
                                    onClicked: mediaSetPropertiesDialog.open()
                                    Accessible.name: qsTr("Show the media set properties")
                                }
                            }
                            Label {
                                Layout.fillWidth: true
                                text: window.app.mediaSetPath
                                elide: Text.ElideMiddle
                                color: Theme.muted
                                visible: text.length > 0
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: window.app.mediaSetPath.length === 0
                                text: qsTr("No media set chosen. A media set is a directory with one directory per member.")
                                wrapMode: Text.Wrap
                                color: Theme.muted
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: text.length > 0
                                text: window.app.mediaSetError
                                color: Theme.fail
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
                                color: Theme.muted
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: window.app.validationState === 2
                                text: qsTr("Media set checked: %1").arg(window.app.validationDetail)
                                color: Theme.ok
                                wrapMode: Text.Wrap
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: window.app.validationState === 3
                                text: qsTr("The media set is not valid: %1").arg(window.app.validationError)
                                color: Theme.fail
                                wrapMode: Text.Wrap
                            }
                            RowLayout {
                                visible: loadList.count > 0

                                AppButton {
                                    text: qsTr("Tick all")
                                    enabled: !window.app.busy
                                    onClicked: window.app.selectAllLoads(true)
                                    Accessible.name: qsTr("Tick every load for upload")
                                }
                                AppButton {
                                    text: qsTr("Tick none")
                                    enabled: !window.app.busy
                                    onClicked: window.app.selectAllLoads(false)
                                    Accessible.name: qsTr("Untick every load")
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("%n load(s) ticked for upload", "", window.app.selectedLoadCount)
                                    color: Theme.muted
                                    elide: Text.ElideRight
                                }
                            }
                            Repeater {
                                id: loadList

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
                                        color: Theme.muted
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
                                        color: loadRow.failed ? Theme.fail : Theme.ink
                                        wrapMode: Text.Wrap
                                    }
                                }
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: window.app.mediaSetFindings.length > 0
                                text: qsTr("%n warning(s) against the standard's recommendations", "",
                                           window.app.mediaSetFindings.length)
                                color: Theme.warn
                            }
                            Repeater {
                                model: window.app.mediaSetFindings
                                delegate: Label {
                                    required property string modelData

                                    Layout.fillWidth: true
                                    text: modelData
                                    wrapMode: Text.Wrap
                                    color: Theme.muted
                                }
                            }
                        }
                    }

                    Panel {
                        title: qsTr("Target")

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: Theme.space2

                            AppButton {
                                text: qsTr("Find targets")
                                enabled: !window.app.busy
                                onClicked: window.app.findTargets()
                                Accessible.name: qsTr("Find targets")
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Broadcasts FIND and listens for three seconds")
                            }
                            ListView {
                                id: targetList

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

                                    width: ListView.view.width
                                    text: summary
                                    highlighted: ListView.isCurrentItem
                                    onClicked: {
                                        targetList.currentIndex = index;
                                        window.app.selectTarget(index);
                                    }
                                }
                            }
                            GridLayout {
                                Layout.fillWidth: true
                                columns: 2
                                columnSpacing: Theme.space2
                                rowSpacing: Theme.space1

                                Label {
                                    text: qsTr("Address")
                                    color: Theme.muted
                                }
                                TextField {
                                    Layout.fillWidth: true
                                    placeholderText: qsTr("10.0.0.2")
                                    text: window.app.targetAddress
                                    enabled: !window.app.busy
                                    onTextEdited: window.app.targetAddress = text
                                    Accessible.name: qsTr("Target address")
                                    Component.onCompleted: forceActiveFocus()
                                }
                                Label {
                                    text: qsTr("THW_ID_POS")
                                    color: Theme.muted
                                }
                                TextField {
                                    Layout.fillWidth: true
                                    placeholderText: qsTr("HNPFMS_L")
                                    text: window.app.thwIdPos
                                    enabled: !window.app.busy
                                    onTextEdited: window.app.thwIdPos = text
                                    Accessible.name: qsTr("Target hardware identifier and position")
                                }
                            }
                            AppButton {
                                text: qsTr("Properties...")
                                enabled: window.app.selectedTargetIndex >= 0
                                         || window.app.targetAddress.length > 0
                                onClicked: targetPropertiesDialog.open()
                                Accessible.name: qsTr("Show the target properties")
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: !window.app.targetValid
                                text: qsTr("Choose a target, or enter its IPv4 address and THW_ID_POS.")
                                wrapMode: Text.Wrap
                                color: Theme.muted
                            }
                        }
                    }

                    DownloadPanel {
                        title: qsTr("Download")
                        app: window.app
                    }
                }
            }

            // ---- what came back ----
            ColumnLayout {
                SplitView.fillWidth: true
                SplitView.minimumWidth: 320
                spacing: Theme.space3

                Panel {
                    title: qsTr("Target configuration")
                    Layout.fillHeight: true
                    Layout.preferredHeight: 1

                    ListView {
                        id: configurationList

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
                            color: Theme.muted
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
                                color: Theme.muted
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
                        spacing: Theme.space1

                        LogView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            events: window.app.events
                        }
                        AppButton {
                            text: qsTr("Clear")
                            onClicked: window.app.clearLog()
                            Accessible.name: qsTr("Clear the activity log")
                        }
                    }
                }
            }
        }

        // ---- the operation: the buttons, the progress and the result, always in reach ----
        Panel {
            title: qsTr("Operation")
            Layout.leftMargin: Theme.space3
            Layout.rightMargin: Theme.space3

            ColumnLayout {
                anchors.fill: parent
                spacing: Theme.space2

                RowLayout {
                    spacing: Theme.space2

                    AppButton {
                        text: qsTr("Read information")
                        enabled: window.app.canReadInformation
                        onClicked: window.app.readInformation()
                        Accessible.name: qsTr("Read the target information")
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Asks the target for its configuration (ARINC 615A Information)")
                    }
                    AppButton {
                        objectName: "uploadButton"

                        primary: true
                        text: qsTr("Upload")
                        enabled: window.app.canUpload
                        onClicked: window.app.uploadSelected()
                        Accessible.name: qsTr("Upload the ticked loads")
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Loads the ticked loads of the media set to the target (ARINC 615A Upload)")
                    }
                    AppButton {
                        text: qsTr("Abort")
                        enabled: window.app.canAbort
                        onClicked: window.app.abort()
                        Accessible.name: qsTr("Abort the operation")
                    }
                    ProgressBar {
                        Layout.fillWidth: true
                        visible: window.app.busy
                        from: 0
                        to: 100
                        // The target reports a figure for an upload or a download only; for the
                        // rest, and before the first status file, the bar only shows that it is
                        // busy.
                        indeterminate: window.app.progress < 0
                        value: Math.max(window.app.progress, 0)
                        Accessible.name: qsTr("Progress of the operation")
                    }
                    Label {
                        visible: window.app.busy
                        text: window.app.progress < 0
                              ? window.app.activity
                              : window.app.activity + " " + window.app.progress + "%"
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
                    color: Theme.muted
                }
                Label {
                    Layout.fillWidth: true
                    visible: window.app.resultState !== 0
                    text: window.app.resultText
                    font.bold: true
                    wrapMode: Text.Wrap
                    color: window.app.resultState === 1 ? Theme.ok : Theme.fail
                    Accessible.role: Accessible.StaticText
                    Accessible.name: qsTr("Result: %1").arg(text)
                }
                Label {
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: window.app.resultDetail
                    wrapMode: Text.Wrap
                    color: Theme.muted
                }
            }
        }

        // ---- the status bar ----
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.border
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.space3
            Layout.rightMargin: Theme.space3
            Layout.topMargin: Theme.space1
            Layout.bottomMargin: Theme.space1
            spacing: Theme.space2

            Label {
                text: window.app.busy ? window.app.activity : qsTr("Idle")
                font.bold: window.app.busy
            }
            Label {
                text: "·"
                color: Theme.muted
            }
            Label {
                text: window.app.targetAddress.length > 0
                      ? window.app.targetAddress : qsTr("no target")
                color: Theme.muted
            }
            Label {
                text: "·"
                color: Theme.muted
            }
            Label {
                Layout.fillWidth: true
                text: window.app.mediaSetPath.length > 0
                      ? window.app.mediaSetPath : qsTr("no media set")
                elide: Text.ElideMiddle
                color: Theme.muted
            }
        }
    }
}

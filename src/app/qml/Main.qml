pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: window

    // The LoaderModel (src/app/core), handed over by main.cpp.
    required property var app

    // The tests render the window in the dark scheme (ADL_DARK=1); the screenshot runs do the
    // same. The application otherwise follows the desktop's colour scheme.
    property bool renderDark: false

    // 0 upload, 1 download. The two operations share the target and the log but not the plan;
    // keeping the switch here means Download is never below a scroll fold.
    property int mode: 0

    // Closing during a job asks first, then waits for the target to confirm the abort.
    property bool closeRequested: false
    property bool closeAfterStop: false

    readonly property string statusText: app.busy ? qsTr("WORKING")
                                                  : app.firstRun ? qsTr("SETUP")
                                                                 : app.canUpload || app.canDownload
                                                                   ? qsTr("READY") : qsTr("NOT READY")
    readonly property string statusTone: app.busy ? "accent"
                                                  : app.canUpload || app.canDownload ? "ok" : "warn"
    readonly property string readinessText: {
        if (window.app.busy)
            return window.app.activity;
        if (window.mode === 0) {
            if (window.app.canUpload)
                return window.app.selectedLoadCount === 1
                       ? qsTr("Ready: 1 load to %1").arg(window.app.thwIdPos)
                       : qsTr("Ready: %1 loads to %2")
                             .arg(window.app.selectedLoadCount)
                             .arg(window.app.thwIdPos);
            return qsTr("Upload needs a checked media set, at least one ticked load and a target.");
        }
        if (window.app.canDownload)
            return qsTr("Ready to download from %1.").arg(window.app.thwIdPos);
        return qsTr("Download needs a target, a destination and - in media defined mode - a file name.");
    }

    title: qsTr("ARINC Data Loader")
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

    onClosing: function(close) {
        if (window.app.busy && !window.closeAfterStop) {
            close.accepted = false;
            closeDialog.open();
        }
    }

    Shortcut {
        sequence: StandardKey.Preferences
        onActivated: preferencesDialog.open()
    }

    Shortcut {
        sequence: StandardKey.Quit
        onActivated: window.close()
    }

    Shortcut {
        sequence: "Ctrl+F"
        onActivated: if (!window.app.busy) window.app.findTargets()
    }

    header: AppHeader {
        appCode: "ADL"
        title: qsTr("ARINC Data Loader")
        subtitle: qsTr("ARINC 615A · ARINC 665 · engineering load console")

        StatusChip {
            text: window.statusText
            tone: window.statusTone
        }

        AppButton {
            text: qsTr("Preferences")
            onClicked: preferencesDialog.open()
            Accessible.name: qsTr("Open preferences")
        }

        AppButton {
            text: qsTr("About")
            onClicked: aboutDialog.open()
            Accessible.name: qsTr("About the data loader")
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
            text: window.app.busy ? window.app.activity
                                  : window.app.resultIsTransfer && window.app.resultState === 1
                                    ? qsTr("Complete")
                                    : window.app.resultIsTransfer && window.app.resultState === 2
                                      ? qsTr("Failed") : qsTr("Idle")
            color: window.app.busy ? Theme.accentInk : Theme.text
            font.pixelSize: Theme.fontMono
            font.family: Theme.monoFamily
            font.bold: window.app.busy
            elide: Text.ElideRight
            Layout.maximumWidth: 320
        }

        Text {
            text: "·"
            color: Theme.textMuted
            font.pixelSize: Theme.fontMono
        }

        Text {
            text: window.app.targetValid ? window.app.thwIdPos : qsTr("no target")
            color: Theme.textMuted
            font.pixelSize: Theme.fontMono
            font.family: Theme.monoFamily
        }

        Text {
            text: "·"
            color: Theme.textMuted
            font.pixelSize: Theme.fontMono
        }

        Text {
            Layout.fillWidth: true
            text: window.app.mediaSetPath.length > 0
                  ? window.app.mediaSetPath : qsTr("no media set")
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
        id: mediaSetDialog

        title: qsTr("Choose the media set directory")
        onAccepted: {
            if (window.app.firstRun)
                window.app.completeFirstRun();
            window.app.chooseMediaSet(selectedFolder);
        }
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

    AppDialog {
        id: closeDialog

        heading: qsTr("An operation is running")
        primaryText: qsTr("Stop and close")
        secondaryText: qsTr("Keep working")

        content: ColumnLayout {
            Layout.fillWidth: true

            Text {
                Layout.fillWidth: true
                text: qsTr("Closing now asks the target to stop. The target may take up to 13 seconds to confirm, and the window will stay open until it does.")
                color: Theme.textMuted
                font.pixelSize: Theme.fontBody
                wrapMode: Text.WordWrap
            }
        }

        onPrimaryClicked: {
            window.closeAfterStop = true;
            window.app.abort();
            window.closeRequested = true;
            close();
        }

        onSecondaryClicked: close()
    }

    AppDialog {
        id: errorDialog

        heading: qsTr("The operation failed")
        primaryText: qsTr("OK")

        content: ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space2

            Text {
                Layout.fillWidth: true
                text: window.app.lastErrorText
                color: Theme.text
                font.pixelSize: Theme.fontSubheading
                font.family: Theme.headingFamily
                font.weight: Font.DemiBold
                wrapMode: Text.WordWrap
            }

            Text {
                Layout.fillWidth: true
                visible: window.app.lastErrorDetail.length > 0
                text: window.app.lastErrorDetail
                color: Theme.textMuted
                font.pixelSize: Theme.fontBody
                wrapMode: Text.WordWrap
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                visible: window.app.lastErrorHint.length > 0
                color: Theme.divider
            }

            Text {
                Layout.fillWidth: true
                visible: window.app.lastErrorHint.length > 0
                text: window.app.lastErrorHint
                color: Theme.accentInk
                font.pixelSize: Theme.fontBody
                wrapMode: Text.WordWrap
            }
        }

        onPrimaryClicked: {
            window.app.dismissError();
            close();
        }
    }

    Connections {
        target: window.app

        function onErrorChanged() {
            if (window.app.lastErrorText.length > 0)
                errorDialog.open();
        }

        function onBusyChanged() {
            if (!window.app.busy && window.closeRequested) {
                window.closeRequested = false;
                window.close();
            }
        }
    }

    FirstRunScreen {
        anchors.fill: parent
        visible: window.app.firstRun
        app: window.app

        onChooseMediaSetRequested: mediaSetDialog.open()
    }

    RowLayout {
        anchors.fill: parent
        visible: !window.app.firstRun
        spacing: 0

        // ---- the context rail: target, then media set ----
        ScrollView {
            id: rail

            Layout.preferredWidth: 388
            Layout.minimumWidth: 348
            Layout.maximumWidth: 460
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: rail.availableWidth
                spacing: 0

                RailBlock {
                    Layout.margins: Theme.space4
                    ordinal: "01"
                    label: qsTr("TARGET")

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2

                        StatusChip {
                            text: window.app.targetValid ? qsTr("SET") : qsTr("NOT SET")
                            tone: window.app.targetValid ? "ok" : "warn"
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        AppButton {
                            text: qsTr("Find")
                            enabled: !window.app.busy
                            onClicked: window.app.findTargets()
                            Accessible.name: qsTr("Find targets")
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space1

                        Text {
                            text: qsTr("ADDRESS")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontMicro
                            font.family: Theme.monoFamily
                            font.letterSpacing: 0.8
                        }

                        AppTextField {
                            Layout.fillWidth: true
                            placeholderText: qsTr("10.0.0.2")
                            text: window.app.targetAddress
                            enabled: !window.app.busy
                            onTextEdited: window.app.targetAddress = text
                            Accessible.name: qsTr("Target address")
                        }

                        Text {
                            text: qsTr("THW_ID_POS")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontMicro
                            font.family: Theme.monoFamily
                            font.letterSpacing: 0.8
                        }

                        AppTextField {
                            Layout.fillWidth: true
                            placeholderText: qsTr("HNPFMS_L")
                            text: window.app.thwIdPos
                            enabled: !window.app.busy
                            onTextEdited: window.app.thwIdPos = text
                            Accessible.name: qsTr("Target hardware identifier and position")
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: targetRepeater.count > 0
                        spacing: Theme.space1

                        Text {
                            text: qsTr("FOUND")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontMicro
                            font.family: Theme.monoFamily
                            font.letterSpacing: 0.8
                        }

                        Repeater {
                            id: targetRepeater

                            model: window.app.targets

                            delegate: Rectangle {
                                id: targetRow

                                required property int index
                                required property string thwIdPos
                                required property string address
                                required property string typeName
                                required property string position

                                Layout.fillWidth: true
                                implicitHeight: targetContent.implicitHeight + Theme.space2 * 2
                                color: window.app.selectedTargetIndex === targetRow.index
                                       ? Theme.accent100 : "transparent"
                                border.color: window.app.selectedTargetIndex === targetRow.index
                                              ? Theme.accent300 : Theme.divider
                                border.width: 1

                                RowLayout {
                                    id: targetContent

                                    anchors.fill: parent
                                    anchors.margins: Theme.space2
                                    spacing: Theme.space2

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 0

                                        Text {
                                            text: targetRow.thwIdPos.length > 0
                                                  ? targetRow.thwIdPos : targetRow.address
                                            color: Theme.text
                                            font.pixelSize: Theme.fontBody
                                            font.family: Theme.monoFamily
                                            font.bold: true
                                        }

                                        Text {
                                            text: targetRow.address + " · "
                                                  + targetRow.typeName + " " + targetRow.position
                                            color: Theme.textMuted
                                            font.pixelSize: Theme.fontSmall
                                            elide: Text.ElideRight
                                        }
                                    }

                                    StatusChip {
                                        visible: window.app.selectedTargetIndex === targetRow.index
                                        text: qsTr("SELECTED")
                                        tone: "ok"
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    enabled: !window.app.busy
                                    onClicked: window.app.selectTarget(targetRow.index)
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2

                        AppButton {
                            text: qsTr("Properties...")
                            enabled: window.app.selectedTargetIndex >= 0
                                     || window.app.targetAddress.length > 0
                            onClicked: targetPropertiesDialog.open()
                            Accessible.name: qsTr("Show the target properties")
                        }

                        Text {
                            Layout.fillWidth: true
                            text: window.app.targetValid
                                  ? qsTr("Ready to address.")
                                  : qsTr("Type an address or use FIND.")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSmall
                            elide: Text.ElideRight
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
                    id: mediaSetRail

                    property bool findingsExpanded: false

                    Layout.margins: Theme.space4
                    ordinal: "02"
                    label: qsTr("MEDIA SET")

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2

                        StatusChip {
                            text: window.app.mediaSetPath.length === 0 ? qsTr("NOT CHOSEN")
                                                                       : window.app.validationState === 1
                                                                         ? qsTr("CHECKING")
                                                                         : window.app.validationState === 2
                                                                           ? qsTr("CHECKED")
                                                                           : window.app.validationState === 3
                                                                             ? qsTr("INVALID") : qsTr("UNCHECKED")
                            tone: window.app.mediaSetPath.length === 0 ? "neutral"
                                                                       : window.app.validationState === 2
                                                                         ? "ok"
                                                                         : window.app.validationState === 3
                                                                           ? "fail" : "warn"
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        AppButton {
                            text: window.app.mediaSetPath.length === 0
                                  ? qsTr("Choose...") : qsTr("Change...")
                            enabled: !window.app.busy
                            onClicked: mediaSetDialog.open()
                            Accessible.name: qsTr("Choose the media set directory")
                        }
                    }

                    EmptyState {
                        visible: window.app.mediaSetPath.length === 0
                        title: qsTr("No media set chosen")
                        body: qsTr("Choose a directory with one directory per member. It is read and checked before an upload.")
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: window.app.mediaSetPath.length > 0
                        spacing: Theme.space1

                        Text {
                            Layout.fillWidth: true
                            text: window.app.mediaSetPath
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSmall
                            font.family: Theme.monoFamily
                            elide: Text.ElideMiddle
                        }

                        Text {
                            Layout.fillWidth: true
                            text: window.app.mediaSetPartNumber + " · "
                                  + Theme.quantity(window.app.mediaSetMemberCount,
                                                   qsTr("member"), qsTr("members"))
                                  + " · "
                                  + Theme.quantity(loadRepeater.count, qsTr("load"), qsTr("loads"))
                            color: Theme.text
                            font.pixelSize: Theme.fontSubheading
                            font.family: Theme.headingFamily
                            font.weight: Font.DemiBold
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: window.app.mediaSetError.length > 0
                            text: window.app.mediaSetError
                            color: Theme.accentInk
                            font.pixelSize: Theme.fontSmall
                            wrapMode: Text.WordWrap
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space2

                            AppButton {
                                text: qsTr("Re-check")
                                enabled: !window.app.busy && window.app.mediaSetPath.length > 0
                                onClicked: window.app.checkMediaSet()
                                Accessible.name: qsTr("Check the media set again")
                            }

                            AppButton {
                                text: qsTr("Properties...")
                                enabled: window.app.mediaSetPath.length > 0
                                onClicked: mediaSetPropertiesDialog.open()
                                Accessible.name: qsTr("Show the media set properties")
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.topMargin: Theme.space2
                            spacing: Theme.space1

                            Text {
                                text: qsTr("LOADS")
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontMicro
                                font.family: Theme.monoFamily
                                font.letterSpacing: 0.8
                            }

                            Item {
                                Layout.fillWidth: true
                            }

                            AppButton {
                                text: qsTr("All")
                                enabled: !window.app.busy && loadRepeater.count > 0
                                onClicked: window.app.selectAllLoads(true)
                                Accessible.name: qsTr("Tick every load")
                            }

                            AppButton {
                                text: qsTr("None")
                                enabled: !window.app.busy && loadRepeater.count > 0
                                onClicked: window.app.selectAllLoads(false)
                                Accessible.name: qsTr("Untick every load")
                            }
                        }

                        Repeater {
                            id: loadRepeater

                            model: window.app.loads

                            delegate: Rectangle {
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
                                implicitHeight: loadContent.implicitHeight + Theme.space2 * 2
                                color: loadRow.checked ? Theme.accent100 : "transparent"
                                border.color: loadRow.checked ? Theme.accent300 : Theme.divider
                                border.width: 1

                                ColumnLayout {
                                    id: loadContent

                                    anchors.fill: parent
                                    anchors.margins: Theme.space2
                                    spacing: Theme.space1

                                    AppCheckBox {
                                        objectName: "loadCheck"
                                        Layout.fillWidth: true
                                        text: loadRow.partNumber
                                        checked: loadRow.checked
                                        enabled: !window.app.busy
                                        onToggled: (value) => loadRow.model.checked = value
                                        Accessible.name: qsTr("Upload load %1").arg(loadRow.partNumber)
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        Layout.leftMargin: 26
                                        text: loadRow.detail
                                        color: Theme.textMuted
                                        font.pixelSize: Theme.fontSmall
                                        wrapMode: Text.WordWrap
                                    }

                                    TaskProgress {
                                        Layout.fillWidth: true
                                        Layout.leftMargin: 26
                                        visible: loadRow.ratio >= 0
                                        value: Math.max(0, loadRow.ratio)
                                        indeterminate: false
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        Layout.leftMargin: 26
                                        visible: loadRow.ratio >= 0
                                        text: loadRow.ratio + "% " + loadRow.loadStatus
                                              + (loadRow.problem.length > 0 ? ": " + loadRow.problem : "")
                                        color: loadRow.failed ? Theme.accentInk : Theme.text
                                        font.pixelSize: Theme.fontSmall
                                        wrapMode: Text.WordWrap
                                    }
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            visible: window.app.mediaSetFindings.length > 0
                            spacing: Theme.space2

                            StatusChip {
                                text: Theme.quantity(window.app.mediaSetFindings.length,
                                                     qsTr("WARNING"), qsTr("WARNINGS"))
                                tone: "warn"
                            }

                            Item {
                                Layout.fillWidth: true
                            }
                        }

                        Repeater {
                            model: window.app.mediaSetFindings

                            delegate: Text {
                                id: findingRow

                                required property int index
                                required property string modelData

                                Layout.fillWidth: true
                                visible: findingRow.index < 3 || mediaSetRail.findingsExpanded
                                text: "· " + findingRow.modelData
                                color: Theme.accentInk
                                font.pixelSize: Theme.fontSmall
                                wrapMode: Text.WordWrap
                            }
                        }

                        AppButton {
                            visible: window.app.mediaSetFindings.length > 3
                            text: mediaSetRail.findingsExpanded ? qsTr("Show fewer")
                                                                : qsTr("Show all warnings")
                            onClicked: mediaSetRail.findingsExpanded = !mediaSetRail.findingsExpanded
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        visible: window.app.validationState === 1
                        spacing: Theme.space2

                        BusyMark {}

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Reading and checking the set...")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontBody
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: window.app.validationState === 2
                        text: qsTr("Every file and check value was read and verified.")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: window.app.validationState === 3
                        text: window.app.validationError
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

        // ---- the work area ----
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                color: Theme.surface

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.space4
                    anchors.rightMargin: Theme.space4
                    spacing: Theme.space3

                    Text {
                        text: qsTr("TRANSFER")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontMicro
                        font.family: Theme.monoFamily
                        font.letterSpacing: 1.0
                    }

                    SegmentedControl {
                        segments: [
                            { value: 0, label: qsTr("Upload") },
                            { value: 1, label: qsTr("Download") }
                        ]
                        selectedValue: window.mode
                        onSegmentClicked: (value) => window.mode = value
                        enabled: !window.app.busy
                        Accessible.name: qsTr("Transfer operation")
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    StatusChip {
                        text: window.app.busy ? qsTr("RUNNING")
                                              : window.app.resultIsTransfer && window.app.resultState === 1
                                                ? qsTr("COMPLETE")
                                                : window.app.resultIsTransfer && window.app.resultState === 2
                                                  ? qsTr("FAILED")
                                                  : window.app.canUpload || window.app.canDownload
                                                    ? qsTr("READY") : qsTr("NOT READY")
                        tone: window.app.busy ? "accent"
                                              : window.app.resultIsTransfer && window.app.resultState === 1
                                                ? "ok"
                                                : window.app.resultIsTransfer && window.app.resultState === 2
                                                  ? "fail"
                                                  : window.app.canUpload || window.app.canDownload
                                                    ? "ok" : "warn"
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 2
                    color: Theme.divider
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: window.mode

                UploadPanel {
                    app: window.app

                    onChooseMediaSetRequested: mediaSetDialog.open()
                    onCheckMediaSetRequested: window.app.checkMediaSet()
                    onFindTargetsRequested: window.app.findTargets()
                }

                DownloadPanel {
                    app: window.app
                }
            }

            ActivityDock {
                id: activityDock

                Layout.fillWidth: true
                Layout.preferredHeight: activityDock.expanded ? 260 : 44
                app: window.app
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
                                text: window.readinessText
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
                                value: Math.max(0, window.app.progress)
                                indeterminate: window.app.progress < 0
                            }

                            Text {
                                text: window.app.progress >= 0
                                      ? window.app.progress + "%" : qsTr("working")
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontMono
                                font.family: Theme.monoFamily
                            }
                        }
                    }

                    AppButton {
                        visible: window.mode === 0
                        text: qsTr("Read information")
                        enabled: window.app.canReadInformation
                        onClicked: window.app.readInformation()
                        Accessible.name: qsTr("Read the target information")
                    }

                    AppButton {
                        objectName: "uploadButton"
                        visible: window.mode === 0
                        primary: true
                        text: window.app.busy ? qsTr("Uploading...") : qsTr("Upload")
                        enabled: window.app.canUpload
                        onClicked: window.app.uploadSelected()
                        Accessible.name: qsTr("Upload the ticked loads")
                    }

                    AppButton {
                        objectName: "downloadButton"
                        visible: window.mode === 1
                        primary: true
                        text: window.app.busy ? qsTr("Downloading...") : qsTr("Download")
                        enabled: window.app.canDownload
                        onClicked: window.app.downloadSelected()
                        Accessible.name: qsTr("Download from the target")
                    }

                    AppButton {
                        visible: window.app.busy
                        text: qsTr("Abort")
                        enabled: window.app.canAbort
                        onClicked: window.app.abort()
                        Accessible.name: qsTr("Abort the operation")
                    }
                }
            }
        }
    }
}

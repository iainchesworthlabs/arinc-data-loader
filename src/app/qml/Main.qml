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

    // One state for the whole window: the header chip, the transfer strip's chip and the status bar
    // all read it, so they cannot disagree (the header said READY while the strip said FAILED).
    readonly property bool lastFailed: app.resultIsTransfer && app.resultState === 2
    readonly property bool lastComplete: app.resultIsTransfer && app.resultState === 1
    readonly property string statusText: app.busy ? qsTr("RUNNING")
                                                  : app.firstRun ? qsTr("SETUP")
                                                  : lastFailed ? qsTr("FAILED")
                                                  : lastComplete ? qsTr("COMPLETE")
                                                  : app.canUpload || app.canDownload
                                                    ? qsTr("READY") : qsTr("NOT READY")
    readonly property string statusTone: app.busy ? "accent"
                                                  : app.firstRun ? "neutral"
                                                  : lastFailed ? "fail"
                                                  : lastComplete ? "ok"
                                                  : app.canUpload || app.canDownload ? "ok" : "warn"
    readonly property bool modalOpen: preferencesDialog.opened || aboutDialog.opened || licencesDialog.opened
                                      || mediaSetPropertiesDialog.opened
                                      || targetPropertiesDialog.opened || errorDialog.opened
                                      || closeDialog.opened
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
    // 1000 x 620 leaves the header, the action bar and the status bar on screen on a 1366 x 768
    // display at 125% scaling and on a 1920 x 1080 display at 150%.
    minimumWidth: 1000
    minimumHeight: 620
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
            objectName: "headerStatusChip"
            text: window.statusText
            tone: window.statusTone
        }

        AppButton {
            objectName: "preferencesButton"
            text: qsTr("Preferences")
            onClicked: preferencesDialog.open()
            Accessible.description: qsTr("Open preferences")
        }

        AppButton {
            objectName: "aboutButton"
            text: qsTr("About")
            onClicked: aboutDialog.open()
            Accessible.description: qsTr("About the data loader")
        }
    }

    footer: AppStatusBar {
        // The state mark: the same glyph the chips use for the same state, so the status bar does
        // not rely on a coloured dot.
        ToneGlyph {
            Layout.preferredWidth: 10
            Layout.preferredHeight: 10
            glyph: window.app.busy ? "busy"
                                   : window.lastFailed ? "cross"
                                                       : window.lastComplete ? "check" : "dot"
            color: window.app.busy ? Theme.accentInk
                                   : window.lastFailed ? Theme.fail : Theme.neutral700
        }

        Text {
            objectName: "statusState"

            text: window.app.busy ? window.app.activity
                                  : window.lastComplete
                                    ? qsTr("Complete")
                                    : window.lastFailed
                                      ? qsTr("Failed") : qsTr("Idle")
            color: window.app.busy ? Theme.accentInk
                                   : window.lastFailed ? Theme.fail : Theme.text
            font.pixelSize: Theme.fontMono
            font.family: Theme.monoFamily
            font.bold: window.app.busy || window.lastFailed
            elide: Text.ElideRight
            Layout.maximumWidth: 320
        }

        Text {
            text: "·"
            color: Theme.textMuted
            font.pixelSize: Theme.fontMono
        }

        Text {
            objectName: "statusTarget"

            text: window.app.targetValid
                  ? window.app.thwIdPos
                  : window.app.targetAddress.length > 0
                    ? window.app.targetAddress : qsTr("no target")
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
        objectName: "preferencesDialog"

        title: qsTr("Preferences")
        app: window.app
    }

    MediaSetPropertiesDialog {
        id: mediaSetPropertiesDialog
        objectName: "mediaSetPropertiesDialog"

        title: qsTr("Media set properties")
        app: window.app
    }

    TargetPropertiesDialog {
        id: targetPropertiesDialog
        objectName: "targetPropertiesDialog"

        title: qsTr("Target properties")
        app: window.app
    }

    AboutDialog {
        id: aboutDialog
        objectName: "aboutDialog"

        title: qsTr("About ARINC Data Loader")
        onShowLicences: licencesDialog.open()
    }

    LicencesDialog {
        id: licencesDialog
    }

    AppDialog {
        id: closeDialog

        objectName: "closeDialog"
        heading: qsTr("An operation is running")
        primaryText: qsTr("Stop and close")
        secondaryText: qsTr("Keep working")
        // The primary stops a running transfer, so it is not the default: the focus starts on
        // "Keep working" and Enter does not press "Stop and close" from elsewhere in the dialog.
        initialFocus: "secondary"
        enterAccepts: false

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
            // The job may have ended while the dialog was open: no busyChanged would follow.
            if (window.app.busy)
                window.closeRequested = true;
            else
                window.close();
            close();
        }

        onSecondaryClicked: close()
    }

    AppDialog {
        id: errorDialog

        objectName: "errorDialog"
        heading: qsTr("The operation failed")
        primaryText: qsTr("OK")

        // The UI suite reads the whole failure as text; the body below lays the same failure out.
        property string text: window.app.lastErrorText

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

        onPrimaryClicked: close()

        // OK, Esc and any other way of closing the dialog acknowledge the failure: Esc used to
        // close it and leave the error standing in the model.
        onClosed: window.app.dismissError()
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
        // Tab into a control below the fold and the rail follows it.
        RevealOnFocus {
            flickable: rail.contentItem
        }

        ScrollView {
            id: rail
            objectName: "railScroll"

            Layout.preferredWidth: 388
            Layout.minimumWidth: 348
            Layout.maximumWidth: 460
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ScrollBar.vertical: AppScrollBar { inScrollView: true }

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
                            objectName: "findTargetsButton"

                            text: qsTr("Find")
                            enabled: !window.app.busy
                            onClicked: window.app.findTargets()
                            Accessible.description: qsTr("Find targets")
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
                            objectName: "targetAddressField"

                            Layout.fillWidth: true
                            placeholderText: qsTr("10.0.0.2")
                            text: window.app.targetAddress
                            enabled: !window.app.busy
                            onTextCommitted: (value) => window.app.targetAddress = value
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
                            objectName: "thwIdPosField"

                            Layout.fillWidth: true
                            placeholderText: qsTr("HNPFMS_L")
                            text: window.app.thwIdPos
                            enabled: !window.app.busy
                            onTextCommitted: (value) => window.app.thwIdPos = value
                            Accessible.name: qsTr("THW_ID_POS")
                            Accessible.description: qsTr("Target hardware identifier and position")
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

                            objectName: "targetList"
                            model: window.app.targets

                            delegate: Rectangle {
                                id: targetRow

                                objectName: "targetDelegate"

                                required property int index
                                required property string thwIdPos
                                required property string address
                                required property string typeName
                                required property string position

                                readonly property bool chosen: window.app.selectedTargetIndex === targetRow.index

                                // The FIND answers are one choice, read like a radio group: one tab
                                // stop (the chosen answer, the first when none is), Up and Down
                                // choose the neighbour and move the focus to it, Enter or Space
                                // chooses the focused one.
                                function chooseRow(position) {
                                    const row = targetRepeater.itemAt(position);
                                    if (row === null)
                                        return;
                                    window.app.selectTarget(position);
                                    row.forceActiveFocus(Qt.TabFocusReason);
                                }

                                Layout.fillWidth: true
                                implicitHeight: targetContent.implicitHeight + Theme.space2 * 2
                                color: targetRow.chosen ? Theme.selectedFill : "transparent"
                                border.color: targetRow.chosen ? Theme.selectedBorder : Theme.divider
                                border.width: targetRow.chosen ? Theme.selectedBorderWidth : 1
                                enabled: !window.app.busy
                                activeFocusOnTab: targetRow.index
                                                  === (window.app.selectedTargetIndex >= 0
                                                       ? window.app.selectedTargetIndex : 0)
                                                  || targetRow.activeFocus
                                Accessible.role: Accessible.RadioButton
                                Accessible.name: (targetRow.thwIdPos.length > 0
                                                  ? targetRow.thwIdPos : targetRow.address)
                                Accessible.description: targetRow.address + ", " + targetRow.typeName
                                                        + " " + targetRow.position
                                Accessible.checkable: true
                                Accessible.checked: targetRow.chosen
                                Accessible.focusable: true
                                Accessible.onPressAction: window.app.selectTarget(targetRow.index)

                                Keys.onPressed: function(event) {
                                    const count = targetRepeater.count;
                                    if (event.key === Qt.Key_Up || event.key === Qt.Key_Left) {
                                        targetRow.chooseRow((targetRow.index - 1 + count) % count);
                                        event.accepted = true;
                                    } else if (event.key === Qt.Key_Down || event.key === Qt.Key_Right) {
                                        targetRow.chooseRow((targetRow.index + 1) % count);
                                        event.accepted = true;
                                    } else if (event.key === Qt.Key_Home) {
                                        targetRow.chooseRow(0);
                                        event.accepted = true;
                                    } else if (event.key === Qt.Key_End) {
                                        targetRow.chooseRow(count - 1);
                                        event.accepted = true;
                                    } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter
                                               || event.key === Qt.Key_Space) {
                                        window.app.selectTarget(targetRow.index);
                                        event.accepted = true;
                                    }
                                }

                                FocusRing {
                                    active: targetRow.activeFocus
                                }

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
                                    onClicked: {
                                        window.app.selectTarget(targetRow.index);
                                        targetRow.forceActiveFocus(Qt.MouseFocusReason);
                                    }
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2

                        AppButton {
                            objectName: "targetPropertiesButton"

                            text: qsTr("Properties...")
                            enabled: window.app.selectedTargetIndex >= 0
                                     || window.app.targetAddress.length > 0
                            onClicked: targetPropertiesDialog.open()
                            Accessible.description: qsTr("Show the target properties")
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
                                                                       : window.app.validationState === 1
                                                                         ? "accent"
                                                                         : window.app.validationState === 2
                                                                           ? "ok"
                                                                           : window.app.validationState === 3
                                                                             ? "fail" : "warn"
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        AppButton {
                            objectName: "mediaSetChooseButton"

                            text: window.app.mediaSetPath.length === 0
                                  ? qsTr("Choose...") : qsTr("Change...")
                            enabled: !window.app.busy
                            onClicked: mediaSetDialog.open()
                            Accessible.description: qsTr("Choose the media set directory")
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

                        ToneMessage {
                            visible: window.app.mediaSetError.length > 0
                            tone: "fail"
                            text: window.app.mediaSetError
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space2

                            AppButton {
                                objectName: "mediaSetCheckButton"

                                text: qsTr("Re-check")
                                enabled: !window.app.busy && window.app.mediaSetPath.length > 0
                                onClicked: window.app.checkMediaSet()
                                Accessible.description: qsTr("Check the media set again")
                            }

                            AppButton {
                                objectName: "mediaSetPropertiesButton"

                                text: qsTr("Properties...")
                                enabled: window.app.mediaSetPath.length > 0
                                onClicked: mediaSetPropertiesDialog.open()
                                Accessible.description: qsTr("Show the media set properties")
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
                                objectName: "tickAllLoadsButton"

                                text: qsTr("All")
                                enabled: !window.app.busy && loadRepeater.count > 0
                                onClicked: window.app.selectAllLoads(true)
                                Accessible.description: qsTr("Tick every load")
                            }

                            AppButton {
                                objectName: "tickNoLoadsButton"

                                text: qsTr("None")
                                enabled: !window.app.busy && loadRepeater.count > 0
                                onClicked: window.app.selectAllLoads(false)
                                Accessible.description: qsTr("Untick every load")
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
                                color: loadRow.checked ? Theme.selectedFill : "transparent"
                                border.color: loadRow.checked ? Theme.selectedBorder : Theme.divider
                                border.width: loadRow.checked ? Theme.selectedBorderWidth : 1

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
                                        Accessible.description: qsTr("Upload load %1").arg(loadRow.partNumber)
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

                                    ToneMessage {
                                        Layout.leftMargin: 26
                                        visible: loadRow.ratio >= 0
                                        tone: loadRow.failed ? "fail"
                                                             : loadRow.ratio >= 100 ? "ok" : "neutral"
                                        text: loadRow.ratio + "% " + loadRow.loadStatus
                                              + (loadRow.problem.length > 0 ? ": " + loadRow.problem : "")
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

                            // A warning from the check reads in the warning ink with a triangle,
                            // not in the error's red: it does not stop an upload.
                            delegate: ToneMessage {
                                id: findingRow

                                required property int index
                                required property string modelData

                                visible: findingRow.index < 3 || mediaSetRail.findingsExpanded
                                tone: "warn"
                                text: findingRow.modelData
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

                    ToneMessage {
                        visible: window.app.validationState === 3
                        tone: "fail"
                        text: window.app.validationError
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
                            { value: 0, label: qsTr("Upload"), name: "transferUpload" },
                            { value: 1, label: qsTr("Download"), name: "transferDownload" }
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
                        objectName: "stripStatusChip"
                        text: window.statusText
                        tone: window.statusTone
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
                    onTickAllLoadsRequested: window.app.selectAllLoads(true)
                }

                DownloadPanel {
                    app: window.app
                }
            }

            ActivityDock {
                id: activityDock
                objectName: "activityDock"

                Layout.fillWidth: true
                // A share of the window rather than a fixed 260 px: at the minimum size the plan
                // above it would otherwise be left with about 280 px.
                Layout.preferredHeight: activityDock.expanded
                                        ? Math.max(150, Math.min(260, Math.round(window.height * 0.26)))
                                        : 44
                app: window.app

                // A short window starts with the dock folded away; the operator opens it with Show.
                Component.onCompleted: expanded = window.height >= 700
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
                                objectName: "operationProgress"

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
                        objectName: "readInformationButton"

                        visible: window.mode === 0
                        text: qsTr("Read information")
                        enabled: window.app.canReadInformation
                        onClicked: window.app.readInformation()
                        Accessible.description: qsTr("Read the target information")
                    }

                    AppButton {
                        objectName: "uploadButton"
                        visible: window.mode === 0
                        primary: true
                        text: window.app.busy ? qsTr("Uploading...") : qsTr("Upload")
                        enabled: window.app.canUpload
                        onClicked: window.app.uploadSelected()
                        Accessible.description: qsTr("Upload the ticked loads")
                    }

                    AppButton {
                        objectName: "downloadButton"
                        visible: window.mode === 1
                        primary: true
                        text: window.app.busy ? qsTr("Downloading...") : qsTr("Download")
                        enabled: window.app.canDownload
                        onClicked: window.app.downloadSelected()
                        Accessible.description: qsTr("Download from the target")
                    }

                    AppButton {
                        objectName: "abortButton"

                        visible: window.app.busy
                        text: qsTr("Abort")
                        enabled: window.app.canAbort
                        onClicked: window.app.abort()
                        Accessible.description: qsTr("Abort the operation")
                    }
                }
            }
        }
    }

    // A modal page recedes the work behind it. The popup draws above this scrim; the window's
    // work sits below it, so the dialog reads as a page rather than a floating panel.
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

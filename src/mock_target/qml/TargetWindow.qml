pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: window

    required property var app
    property bool renderDark: false

    readonly property string derivedThwIdPos: app.hardwareId.length > 0
                                              ? app.hardwareId
                                                + (app.position.length > 0 ? "_" + app.position : "")
                                              : qsTr("not named")
    readonly property int activeInjections: (app.waitFirstSeconds > 0 ? 1 : 0)
                                             + (app.waitEveryRequest ? 1 : 0)
                                             + (app.refuseOperation.length > 0 ? 1 : 0)
                                             + (app.dropStatusEvery > 0 ? 1 : 0)
                                             + (app.corruptServedFile ? 1 : 0)
                                             + (app.wrongChecksum ? 1 : 0)
                                             + (app.abortAfterStatuses > 0 ? 1 : 0)
                                             + (app.silent ? 1 : 0)
    readonly property string versionDetails: typeof appVersionDetails === "string"
                                             ? appVersionDetails
                                             : qsTr("ARINC Mock Target %1")
                                                   .arg(Qt.application.version)
    readonly property bool modalOpen: aboutDialog.opened || licencesDialog.opened

    title: qsTr("ARINC Mock Target")
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
        appCode: "MOCK"
        title: qsTr("ARINC Mock Target")
        subtitle: qsTr("ARINC 615A target · real sockets · fault injection")

        StatusChip {
            text: window.app.running ? qsTr("SERVING") : qsTr("STOPPED")
            tone: window.app.running ? "accent" : "neutral"
        }

        AppButton {
            objectName: "startButton"
            primary: !window.app.running
            text: qsTr("Start")
            enabled: !window.app.running && window.app.hardwareId.length > 0
            onClicked: window.app.start()
            Accessible.name: qsTr("Start the mock target")
        }

        AppButton {
            objectName: "stopButton"
            text: qsTr("Stop")
            enabled: window.app.running
            onClicked: window.app.stop()
            Accessible.name: qsTr("Stop the mock target")
        }

        AppButton {
            text: qsTr("About")
            onClicked: aboutDialog.open()
            Accessible.name: qsTr("About the mock target")
        }
    }

    footer: AppStatusBar {
        Rectangle {
            width: 6
            height: 6
            radius: 3
            color: window.app.running ? Theme.accent : Theme.neutral600
        }

        Text {
            text: window.app.running ? qsTr("Serving") : qsTr("Stopped")
            color: window.app.running ? Theme.accentInk : Theme.text
            font.pixelSize: Theme.fontMono
            font.family: Theme.monoFamily
            font.bold: window.app.running
        }

        Text {
            text: "·"
            color: Theme.textMuted
            font.pixelSize: Theme.fontMono
        }

        Text {
            text: qsTr("TFTP %1 · FIND %2").arg(window.app.tftpPort).arg(window.app.findPort)
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
            text: window.derivedThwIdPos
            color: Theme.textMuted
            font.pixelSize: Theme.fontMono
            font.family: Theme.monoFamily
            elide: Text.ElideRight
        }

        Text {
            text: qsTr("v%1").arg(Qt.application.version)
            color: Theme.textMuted
            font.pixelSize: Theme.fontMono
            font.family: Theme.monoFamily
        }
    }

    FolderDialog {
        id: storageDialog

        title: qsTr("Choose the storage directory")
        onAccepted: window.app.storageDirectory = selectedFolder
    }

    AppDialog {
        id: aboutDialog

        heading: qsTr("About the ARINC Mock Target")
        primaryText: qsTr("Close")
        secondaryText: qsTr("Licences")
        width: 620

        content: ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space3

            Text {
                text: "MOCK"
                color: Theme.accent
                font.pixelSize: Theme.fontDisplay
                font.family: Theme.headingFamily
                font.weight: Font.ExtraBold
                font.letterSpacing: Theme.trackingTitle
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("A 615A target over real UDP sockets, for demonstrating the loader end to end without an embedded box. FIND, Information, Upload and Download run against it; the fault injections make it misbehave on purpose.")
                color: Theme.textMuted
                font.pixelSize: Theme.fontBody
                wrapMode: Text.WordWrap
            }

            SectionHeader {
                label: qsTr("Version")
            }

            Text {
                Layout.fillWidth: true
                text: window.versionDetails
                color: Theme.text
                font.pixelSize: Theme.fontSmall
                font.family: Theme.monoFamily
                wrapMode: Text.WordWrap
            }

            SectionHeader {
                label: qsTr("Licences")
            }

            Text {
                Layout.fillWidth: true
                textFormat: Text.RichText
                linkColor: Theme.accentInk
                text: qsTr("GNU General Public License, version 3 or later. Third-party components - Qt (LGPL 3), the Archivo typeface (SIL Open Font License 1.1) and others - are listed, with their licences, under Licences. <a href=\"https://github.com/iainchesworthlabs/arinc-data-loader\">Repository</a>.")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSmall
                wrapMode: Text.WordWrap
                onLinkActivated: (link) => Qt.openUrlExternally(link)
            }
        }

        onSecondaryClicked: licencesDialog.open()
        onPrimaryClicked: aboutDialog.close()
    }

    LicencesDialog {
        id: licencesDialog
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ---- configuration ----
        ScrollView {
            id: rail

            Layout.preferredWidth: 396
            Layout.minimumWidth: 352
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
                    label: qsTr("IDENTITY")

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("What the target answers to FIND and how its protocol files are named.")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        text: qsTr("HARDWARE ID")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontMicro
                        font.family: Theme.monoFamily
                        font.letterSpacing: 0.8
                    }

                    AppTextField {
                        objectName: "hardwareIdField"
                        Layout.fillWidth: true
                        placeholderText: qsTr("HNPFMS")
                        text: window.app.hardwareId
                        enabled: !window.app.running
                        onTextEdited: window.app.hardwareId = text
                        Accessible.name: qsTr("Target hardware identifier")
                    }

                    Text {
                        text: qsTr("TYPE NAME")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontMicro
                        font.family: Theme.monoFamily
                        font.letterSpacing: 0.8
                    }

                    AppTextField {
                        Layout.fillWidth: true
                        placeholderText: qsTr("FMS")
                        text: window.app.typeName
                        enabled: !window.app.running
                        onTextEdited: window.app.typeName = text
                        Accessible.name: qsTr("Target type name")
                    }

                    Text {
                        text: qsTr("POSITION")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontMicro
                        font.family: Theme.monoFamily
                        font.letterSpacing: 0.8
                    }

                    AppTextField {
                        Layout.fillWidth: true
                        placeholderText: qsTr("L")
                        text: window.app.position
                        enabled: !window.app.running
                        onTextEdited: window.app.position = text
                        Accessible.name: qsTr("Target position")
                    }

                    Text {
                        text: qsTr("LITERAL NAME")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontMicro
                        font.family: Theme.monoFamily
                        font.letterSpacing: 0.8
                    }

                    AppTextField {
                        Layout.fillWidth: true
                        placeholderText: qsTr("FMS LEFT")
                        text: window.app.literalName
                        enabled: !window.app.running
                        onTextEdited: window.app.literalName = text
                        Accessible.name: qsTr("Target literal name")
                    }

                    Text {
                        text: qsTr("MANUFACTURER CODE")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontMicro
                        font.family: Theme.monoFamily
                        font.letterSpacing: 0.8
                    }

                    AppTextField {
                        Layout.fillWidth: true
                        placeholderText: qsTr("HNP")
                        text: window.app.manufacturerCode
                        enabled: !window.app.running
                        onTextEdited: window.app.manufacturerCode = text
                        Accessible.name: qsTr("Target manufacturer code")
                    }

                    Text {
                        text: qsTr("THW_ID_POS")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontMicro
                        font.family: Theme.monoFamily
                        font.letterSpacing: 0.8
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2

                        AppTextField {
                            id: derivedField

                            Layout.fillWidth: true
                            readOnly: true
                            text: window.derivedThwIdPos
                            Accessible.name: qsTr("Derived target hardware identifier and position")
                        }

                        AppButton {
                            text: qsTr("Copy")
                            enabled: window.app.hardwareId.length > 0
                            onClicked: {
                                derivedField.selectAll();
                                derivedField.copy();
                            }
                            Accessible.name: qsTr("Copy the derived THW_ID_POS")
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Hardware ID, an underscore and the position. The loader addresses the target by this.")
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
                    label: qsTr("PORTS")

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("The loader asks for the TFTP port; FIND is broadcast on its port.")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("TFTP control port")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSmall
                        }

                        AppSpinBox {
                            objectName: "tftpPortField"
                            Layout.preferredWidth: 120
                            from: 1
                            to: 65535
                            value: window.app.tftpPort
                            editable: true
                            enabled: !window.app.running
                            onValueModified: window.app.tftpPort = value
                            Accessible.name: qsTr("TFTP control port")
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("FIND port")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSmall
                        }

                        AppSpinBox {
                            objectName: "findPortField"
                            Layout.preferredWidth: 120
                            from: 1
                            to: 65535
                            value: window.app.findPort
                            editable: true
                            enabled: !window.app.running
                            onValueModified: window.app.findPort = value
                            Accessible.name: qsTr("FIND port")
                        }
                    }

                    AppCheckBox {
                        objectName: "loopbackOnlyField"
                        Layout.fillWidth: true
                        text: qsTr("This computer only (127.0.0.1)")
                        checked: window.app.loopbackOnly
                        enabled: !window.app.running
                        onToggled: (value) => window.app.loopbackOnly = value
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Listens on no network, so the firewall does not ask. A loader on another computer cannot reach it.")
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
                    ordinal: "03"
                    label: qsTr("STORAGE")

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Uploaded files are written under one directory per load. Files in its 'holds' directory are what a download serves.")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2

                        AppButton {
                            objectName: "storageButton"
                            text: qsTr("Choose directory...")
                            enabled: !window.app.running
                            onClicked: storageDialog.open()
                            Accessible.name: qsTr("Choose the storage directory")
                        }

                        AppButton {
                            text: qsTr("Open folder")
                            enabled: window.app.storageDirectory.length > 0
                            onClicked: Qt.openUrlExternally(Theme.fileUrl(window.app.storageDirectory))
                            Accessible.name: qsTr("Open the storage directory")
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: window.app.storageDirectory.length > 0
                              ? window.app.storageDirectory
                              : qsTr("No storage directory chosen. The target still serves; an upload is not kept.")
                        color: window.app.storageDirectory.length > 0 ? Theme.textMuted : Theme.accentInk
                        font.pixelSize: Theme.fontSmall
                        font.family: window.app.storageDirectory.length > 0
                                     ? Theme.monoFamily : Theme.headingFamily
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
                    ordinal: "04"
                    label: qsTr("BEHAVIOUR")

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Think time")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSmall
                        }

                        AppSpinBox {
                            Layout.preferredWidth: 120
                            from: 0
                            to: 60000
                            value: window.app.thinkTimeMs
                            editable: true
                            enabled: !window.app.running
                            onValueModified: window.app.thinkTimeMs = value
                            Accessible.name: qsTr("Target think time in milliseconds")
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Status period")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSmall
                        }

                        AppSpinBox {
                            Layout.preferredWidth: 120
                            from: 0
                            to: 3600
                            value: window.app.statusPeriodSec
                            editable: true
                            enabled: !window.app.running
                            onValueModified: window.app.statusPeriodSec = value
                            Accessible.name: qsTr("Status file period in seconds")
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
                    ordinal: "05"
                    label: qsTr("FAULT INJECTION")

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: injectionHeader.implicitHeight + Theme.space3 * 2
                        color: window.activeInjections > 0 ? Theme.accent100 : Theme.neutral100
                        border.color: window.activeInjections > 0 ? Theme.accent300 : Theme.divider
                        border.width: 1

                        RowLayout {
                            id: injectionHeader

                            anchors.fill: parent
                            anchors.margins: Theme.space3
                            spacing: Theme.space2

                            StatusChip {
                                text: window.activeInjections > 0
                                      ? qsTr("%n ACTIVE", "", window.activeInjections)
                                      : qsTr("BEHAVING")
                                tone: window.activeInjections > 0 ? "warn" : "ok"
                            }

                            Text {
                                Layout.fillWidth: true
                                text: qsTr("These change how the target misbehaves, for demonstrations. Everything defaults to behaving.")
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontSmall
                                wrapMode: Text.WordWrap
                            }

                            AppButton {
                                text: qsTr("Reset all")
                                enabled: !window.app.running && window.activeInjections > 0
                                onClicked: {
                                    window.app.waitFirstSeconds = 0;
                                    window.app.waitEveryRequest = false;
                                    window.app.refuseOperation = "";
                                    window.app.dropStatusEvery = 0;
                                    window.app.corruptServedFile = false;
                                    window.app.wrongChecksum = false;
                                    window.app.abortAfterStatuses = 0;
                                    window.app.silent = false;
                                }
                                Accessible.name: qsTr("Reset every fault injection")
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Answer WAIT")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSmall
                        }

                        AppSpinBox {
                            objectName: "waitFirstField"
                            Layout.preferredWidth: 100
                            from: 0
                            to: 65535
                            value: window.app.waitFirstSeconds
                            editable: true
                            enabled: !window.app.running
                            onValueModified: window.app.waitFirstSeconds = value
                            Accessible.name: qsTr("Seconds to answer the first request with WAIT")
                        }

                        Text {
                            text: qsTr("s")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSmall
                        }
                    }

                    AppCheckBox {
                        Layout.fillWidth: true
                        text: qsTr("Answer WAIT every request")
                        checked: window.app.waitEveryRequest
                        enabled: !window.app.running
                        onToggled: (value) => window.app.waitEveryRequest = value
                    }

                    Text {
                        text: qsTr("REFUSE")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontMicro
                        font.family: Theme.monoFamily
                        font.letterSpacing: 0.8
                    }

                    AppComboBox {
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
                        Component.onCompleted: currentIndex = indexOfValue(window.app.refuseOperation)
                        onActivated: window.app.refuseOperation = currentValue
                        Accessible.name: qsTr("Operation to refuse")
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Drop every Nth status file")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSmall
                        }

                        AppSpinBox {
                            Layout.preferredWidth: 100
                            from: 0
                            to: 99
                            value: window.app.dropStatusEvery
                            editable: true
                            enabled: !window.app.running
                            onValueModified: window.app.dropStatusEvery = value
                            Accessible.name: qsTr("Drop every Nth status file")
                        }
                    }

                    AppCheckBox {
                        Layout.fillWidth: true
                        text: qsTr("Corrupt served files")
                        checked: window.app.corruptServedFile
                        enabled: !window.app.running
                        onToggled: (value) => window.app.corruptServedFile = value
                    }

                    AppCheckBox {
                        Layout.fillWidth: true
                        text: qsTr("Send a wrong checksum option")
                        checked: window.app.wrongChecksum
                        enabled: !window.app.running
                        onToggled: (value) => window.app.wrongChecksum = value
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Abort after N status files")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSmall
                        }

                        AppSpinBox {
                            Layout.preferredWidth: 100
                            from: 0
                            to: 99
                            value: window.app.abortAfterStatuses
                            editable: true
                            enabled: !window.app.running
                            onValueModified: window.app.abortAfterStatuses = value
                            Accessible.name: qsTr("Abort after this many status files")
                        }
                    }

                    AppCheckBox {
                        Layout.fillWidth: true
                        text: qsTr("Go silent after the first status file")
                        checked: window.app.silent
                        enabled: !window.app.running
                        onToggled: (value) => window.app.silent = value
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

        // ---- what the target is doing ----
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            ColumnLayout {
                Layout.fillWidth: true
                Layout.margins: Theme.space6
                spacing: Theme.space2

                Text {
                    text: qsTr("OPERATION")
                    color: Theme.accentInk
                    font.pixelSize: Theme.fontMicro
                    font.family: Theme.monoFamily
                    font.letterSpacing: 1.2
                    font.bold: true
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space3

                    Text {
                        Layout.fillWidth: true
                        text: window.app.currentOperation.length > 0
                              ? window.app.currentOperation
                              : window.app.running ? qsTr("Serving — no operation")
                                                   : qsTr("Stopped")
                        color: Theme.text
                        font.pixelSize: Theme.fontHeadline
                        font.family: Theme.headingFamily
                        font.weight: Font.ExtraBold
                        wrapMode: Text.WordWrap
                    }

                    StatusChip {
                        text: window.app.running ? qsTr("SERVING") : qsTr("STOPPED")
                        tone: window.app.running ? "accent" : "neutral"
                    }
                }

                TaskProgress {
                    Layout.fillWidth: true
                    visible: window.app.running || window.app.progress >= 0
                    value: Math.max(0, window.app.progress)
                    indeterminate: window.app.running && window.app.progress < 0
                }

                Text {
                    Layout.fillWidth: true
                    visible: window.app.progress >= 0
                    text: qsTr("%1% of the target's list").arg(window.app.progress)
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                    font.family: Theme.monoFamily
                }

                Text {
                    Layout.fillWidth: true
                    visible: window.app.errorText.length > 0
                    text: window.app.errorText
                    color: Theme.accentInk
                    font.pixelSize: Theme.fontBody
                    wrapMode: Text.WordWrap
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
                Layout.topMargin: Theme.space3
                spacing: Theme.space1

                SectionHeader {
                    label: qsTr("Activity")
                    summary: Theme.quantity(activityList.count, qsTr("line"), qsTr("lines"))
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space2

                    Item {
                        Layout.fillWidth: true
                    }

                    AppButton {
                        text: qsTr("Clear")
                        onClicked: window.app.clearLog()
                        Accessible.name: qsTr("Clear the activity log")
                    }
                }

                LogView {
                    id: activityList

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    events: window.app.activity
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 2
                color: Theme.divider
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 240
                Layout.leftMargin: Theme.space6
                Layout.rightMargin: Theme.space6
                Layout.topMargin: Theme.space3
                spacing: Theme.space1

                SectionHeader {
                    label: qsTr("Stored files")
                    summary: Theme.quantity(storedList.count, qsTr("file"), qsTr("files"))
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space2

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("NAME")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontMicro
                        font.family: Theme.monoFamily
                    }

                    Text {
                        Layout.preferredWidth: 100
                        horizontalAlignment: Text.AlignRight
                        text: qsTr("BYTES")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontMicro
                        font.family: Theme.monoFamily
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Theme.divider
                }

                ListView {
                    id: storedList

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: storedList.count > 0
                    clip: true
                    model: window.app.storedFiles
                    ScrollBar.vertical: ScrollBar {}
                    Accessible.role: Accessible.List
                    Accessible.name: qsTr("Files stored from uploads")

                    delegate: RowLayout {
                        id: storedRow

                        required property string name
                        required property int size
                        required property string path

                        width: ListView.view.width
                        spacing: Theme.space2

                        Text {
                            Layout.fillWidth: true
                            text: storedRow.name
                            color: Theme.text
                            font.pixelSize: Theme.fontSmall
                            font.family: Theme.monoFamily
                            elide: Text.ElideRight
                            ToolTip.visible: storedHover.hovered
                            ToolTip.text: storedRow.path
                        }

                        Text {
                            Layout.preferredWidth: 100
                            horizontalAlignment: Text.AlignRight
                            text: String(storedRow.size)
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSmall
                            font.family: Theme.monoFamily
                        }

                        HoverHandler {
                            id: storedHover
                        }
                    }
                }

                EmptyState {
                    Layout.fillWidth: true
                    visible: storedList.count === 0
                    title: qsTr("Nothing stored yet")
                    body: qsTr("The files of an upload are written under the storage directory, one directory per load.")
                }
            }

            Item {
                Layout.preferredHeight: Theme.space4
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

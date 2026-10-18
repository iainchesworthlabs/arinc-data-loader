import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: window

    // The LoaderModel (src/app/core), handed over by main.cpp.
    required property var app

    readonly property bool dark: palette.window.hslLightness < 0.5
    readonly property color okColour: dark ? "#70d090" : "#1b6e3a"
    readonly property color failColour: dark ? "#ff7070" : "#b00020"

    title: qsTr("ARINC Data Loader")
    width: 1100
    height: 760
    minimumWidth: 760
    minimumHeight: 520
    visible: true

    FolderDialog {
        id: mediaSetDialog

        title: qsTr("Choose the media set directory")
        onAccepted: window.app.chooseMediaSet(selectedFolder)
    }

    SplitView {
        anchors.fill: parent
        anchors.margins: 12
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
                                text: qsTr("Choose directory...")
                                enabled: !window.app.busy
                                onClicked: mediaSetDialog.open()
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
                        Repeater {
                            model: window.app.loads
                            delegate: ColumnLayout {
                                required property string partNumber
                                required property string detail

                                Layout.fillWidth: true
                                spacing: 0
                                Label { text: partNumber }
                                Label {
                                    Layout.fillWidth: true
                                    text: detail
                                    opacity: 0.7
                                    wrapMode: Text.Wrap
                                }
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: window.app.mediaSetFindings.length > 0
                            text: qsTr("%n difference(s) from the standard", "",
                                       window.app.mediaSetFindings.length)
                            color: window.failColour
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
                            text: qsTr("Find targets")
                            enabled: !window.app.busy
                            onClicked: window.app.findTargets()
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
                                Layout.fillWidth: true
                                placeholderText: qsTr("10.0.0.2")
                                text: window.app.targetAddress
                                enabled: !window.app.busy
                                onTextEdited: window.app.targetAddress = text
                                Accessible.name: qsTr("Target address")
                            }
                            Label { text: qsTr("THW_ID_POS") }
                            TextField {
                                Layout.fillWidth: true
                                placeholderText: qsTr("HNPFMS_L")
                                text: window.app.thwIdPos
                                enabled: !window.app.busy
                                onTextEdited: window.app.thwIdPos = text
                                Accessible.name: qsTr("Target hardware identifier and position")
                            }
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

                Panel {
                    title: qsTr("Operation")

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 8

                        RowLayout {
                            Button {
                                text: qsTr("Read information")
                                enabled: window.app.canReadInformation
                                onClicked: window.app.readInformation()
                                ToolTip.visible: hovered
                                ToolTip.text: qsTr("Asks the target for its configuration (ARINC 615A Information)")
                            }
                            Button {
                                text: qsTr("Abort")
                                enabled: window.app.canAbort
                                onClicked: window.app.abort()
                            }
                        }
                        ProgressBar {
                            Layout.fillWidth: true
                            visible: window.app.busy
                            indeterminate: true
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: window.app.busy
                            text: window.app.activity
                        }
                        Label {
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
                        text: qsTr("Clear")
                        onClicked: window.app.clearLog()
                    }
                }
            }
        }
    }
}

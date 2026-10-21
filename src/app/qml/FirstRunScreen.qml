import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// The first launch. The shape is the family's: one job, every route visible, one primary, and
// the mental model taught beside it rather than in a manual. The defaults are optional and
// folded away - a person who does not know what a THW_ID_POS is should not be asked for one
// before they have seen the workbench.
Item {
    id: root

    required property var app
    property bool showDefaults: false

    signal started()
    signal chooseMediaSetRequested()

    // The first screen opens with the keyboard on its one primary, so Enter starts.
    onVisibleChanged: if (visible) startButton.forceActiveFocus(Qt.TabFocusReason)
    Component.onCompleted: if (visible) startButton.forceActiveFocus(Qt.TabFocusReason)

    FolderDialog {
        id: downloadDirectoryDialog

        title: qsTr("Choose the download folder")
        onAccepted: root.app.downloadDirectory = selectedFolder
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.space8
        spacing: Theme.space8

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.space4

            Text {
                text: qsTr("FIRST RUN")
                color: Theme.accentInk
                font.pixelSize: Theme.fontMicro
                font.family: Theme.monoFamily
                font.letterSpacing: 1.4
                font.bold: true
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Load a media set to a target.")
                color: Theme.text
                font.pixelSize: Theme.fontDisplay
                font.family: Theme.headingFamily
                font.weight: Font.ExtraBold
                font.letterSpacing: Theme.trackingTitle
                wrapMode: Text.WordWrap
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("The loader reads an ARINC 665 media set, checks every file and its check value, and moves the loads you tick to an ARINC 615A target. Nothing is sent until you ask it to.")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSubheading
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Theme.space2
                spacing: Theme.space2

                AppButton {
                    id: startButton

                    objectName: "firstRunStart"
                    primary: true
                    text: qsTr("Start loading")
                    onClicked: {
                        root.app.completeFirstRun();
                        root.started();
                    }
                    Accessible.description: qsTr("Start the loader")
                }

                AppButton {
                    text: qsTr("Choose a media set...")
                    onClicked: root.chooseMediaSetRequested()
                    Accessible.description: qsTr("Choose the first media set")
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Opens the media set chooser and the workbench together")
                }
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Start loading opens the workbench. The default address and THW_ID_POS start empty, and downloads go to the folder shown under Defaults; set them there or later in Preferences. A media set can be chosen there, or here.")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSmall
                wrapMode: Text.WordWrap
            }

            Item {
                Layout.fillHeight: true
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.divider
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space2

                Text {
                    Layout.fillWidth: true
                    text: qsTr("Defaults")
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                    font.family: Theme.headingFamily
                    font.weight: Font.DemiBold
                }

                AppButton {
                    text: root.showDefaults ? qsTr("Hide") : qsTr("Set up defaults")
                    onClicked: root.showDefaults = !root.showDefaults
                    Accessible.description: root.showDefaults ? qsTr("Hide the default settings")
                                                        : qsTr("Show the default settings")
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                visible: root.showDefaults
                spacing: Theme.space2

                Text {
                    Layout.fillWidth: true
                    text: qsTr("These are the values the workbench starts from. They can be changed later in Preferences.")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                    wrapMode: Text.WordWrap
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: Theme.space3
                    rowSpacing: Theme.space2

                    Text {
                        text: qsTr("Default address")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSmall
                    }

                    AppTextField {
                        objectName: "firstRunAddress"
                        Layout.fillWidth: true
                        placeholderText: qsTr("e.g. 10.0.0.2")
                        text: root.app.defaultTargetAddress
                        onTextCommitted: (value) => root.app.defaultTargetAddress = value
                        Accessible.name: qsTr("Default address")
                        Accessible.description: qsTr("Default target address")
                    }

                    Text {
                        text: qsTr("Default THW_ID_POS")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSmall
                    }

                    AppTextField {
                        objectName: "firstRunThwIdPos"
                        Layout.fillWidth: true
                        placeholderText: qsTr("e.g. HNPFMS_L")
                        text: root.app.defaultThwIdPos
                        onTextCommitted: (value) => root.app.defaultThwIdPos = value
                        Accessible.name: qsTr("Default THW_ID_POS")
                        Accessible.description: qsTr("Default target hardware identifier and position")
                    }

                    Text {
                        text: qsTr("Download folder")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSmall
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space1

                        AppTextField {
                            objectName: "firstRunDownloadDirectory"
                            Layout.fillWidth: true
                            text: root.app.downloadDirectory
                            onTextCommitted: (value) => root.app.downloadDirectory = value
                            Accessible.name: qsTr("Download folder")
                        }

                        AppButton {
                            text: qsTr("Choose folder...")
                            onClicked: downloadDirectoryDialog.open()
                            Accessible.description: qsTr("Choose the download folder")
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: 380
            Layout.fillHeight: true
            color: Theme.neutral100
            border.color: Theme.divider
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.space6
                spacing: Theme.space4

                Text {
                    text: qsTr("WHAT THIS WINDOW DOES")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontMicro
                    font.family: Theme.monoFamily
                    font.letterSpacing: 1.2
                    font.bold: true
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 2
                    color: Theme.divider
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space2

                    RowLayout {
                        spacing: Theme.space2

                        Text {
                            text: "01"
                            color: Theme.accentInk
                            font.pixelSize: Theme.fontMono
                            font.family: Theme.monoFamily
                            font.bold: true
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Choose a media set")
                            color: Theme.text
                            font.pixelSize: Theme.fontSubheading
                            font.family: Theme.headingFamily
                            font.weight: Font.DemiBold
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("A folder with one folder per member. Its loads, files and findings are read and checked.")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSmall
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
                    spacing: Theme.space2

                    RowLayout {
                        spacing: Theme.space2

                        Text {
                            text: "02"
                            color: Theme.accentInk
                            font.pixelSize: Theme.fontMono
                            font.family: Theme.monoFamily
                            font.bold: true
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Find the target")
                            color: Theme.text
                            font.pixelSize: Theme.fontSubheading
                            font.family: Theme.headingFamily
                            font.weight: Font.DemiBold
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("FIND broadcasts for three seconds. A target that does not answer can be named by IPv4 address and THW_ID_POS.")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSmall
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
                    spacing: Theme.space2

                    RowLayout {
                        spacing: Theme.space2

                        Text {
                            text: "03"
                            color: Theme.accentInk
                            font.pixelSize: Theme.fontMono
                            font.family: Theme.monoFamily
                            font.bold: true
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Upload or download")
                            color: Theme.text
                            font.pixelSize: Theme.fontSubheading
                            font.family: Theme.headingFamily
                            font.weight: Font.DemiBold
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("The operations run on the engine's own thread, with progress, an abort and an activity log that keeps what the target said.")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSmall
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
                    spacing: Theme.space2

                    Text {
                        text: qsTr("THE THREE TOOLS")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontMicro
                        font.family: Theme.monoFamily
                        font.letterSpacing: 1.2
                        font.bold: true
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2

                        Text {
                            text: "ADL"
                            color: Theme.accentInk
                            font.pixelSize: Theme.fontMono
                            font.family: Theme.monoFamily
                            font.bold: true
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("the loader")
                            color: Theme.text
                            font.pixelSize: Theme.fontSmall
                            font.family: Theme.headingFamily
                            font.weight: Font.DemiBold
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Reads a media set, talks to the target, and keeps the evidence.")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2

                        Text {
                            text: "GEN"
                            color: Theme.accentInk
                            font.pixelSize: Theme.fontMono
                            font.family: Theme.monoFamily
                            font.bold: true
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("the media set generator")
                            color: Theme.text
                            font.pixelSize: Theme.fontSmall
                            font.family: Theme.headingFamily
                            font.weight: Font.DemiBold
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Turns files into the ARINC 665 tree this window opens.")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2

                        Text {
                            text: "MOCK"
                            color: Theme.accentInk
                            font.pixelSize: Theme.fontMono
                            font.family: Theme.monoFamily
                            font.bold: true
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("the target")
                            color: Theme.text
                            font.pixelSize: Theme.fontSmall
                            font.family: Theme.headingFamily
                            font.weight: Font.DemiBold
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: qsTr("Serves a 615A dialogue over real sockets, and can be made to misbehave.")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontSmall
                        wrapMode: Text.WordWrap
                    }
                }

                Item {
                    Layout.fillHeight: true
                }

                Text {
                    Layout.fillWidth: true
                    text: qsTr("The same system draws the media set generator and the mock target, so the three tools read alike.")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}

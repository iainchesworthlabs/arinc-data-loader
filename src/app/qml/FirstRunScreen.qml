import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// The first launch. Both sibling applications open on a first-run surface before their workbench
// (forge-gui's FirstRunScreen, the Countdown Solver's defaults from the OS): what the window is
// for, the three settings it needs before the operator can do anything, and one way on. The
// fields write through LoaderModel's preferences as they are edited, exactly like the Preferences
// dialog; "Start" records that the first run is done and hands over to the workbench.
Item {
    id: root

    required property var app

    signal started()

    FolderDialog {
        id: downloadDirectoryDialog

        title: qsTr("Choose the download directory")
        onAccepted: root.app.downloadDirectory = selectedFolder
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.pad * 2
        spacing: Theme.pad

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.pad

            // ---- what to set up ----
            Panel {
                Layout.fillHeight: true
                Layout.preferredWidth: 3
                title: qsTr("Before the first operation")

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Theme.space3

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("The loader needs to know where a target is, and where a download is to be written. Both can be changed later in File → Preferences.")
                        wrapMode: Text.Wrap
                        color: Theme.muted
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: Theme.space2
                        rowSpacing: Theme.space2

                        Label {
                            text: qsTr("Default address")
                            color: Theme.muted
                        }
                        TextField {
                            objectName: "firstRunAddress"
                            Layout.fillWidth: true
                            placeholderText: qsTr("10.0.0.2")
                            text: root.app.defaultTargetAddress
                            onTextEdited: root.app.defaultTargetAddress = text
                            Accessible.name: qsTr("Default target address")
                        }
                        Label {
                            text: qsTr("Default THW_ID_POS")
                            color: Theme.muted
                        }
                        TextField {
                            objectName: "firstRunThwIdPos"
                            Layout.fillWidth: true
                            placeholderText: qsTr("HNPFMS_L")
                            text: root.app.defaultThwIdPos
                            onTextEdited: root.app.defaultThwIdPos = text
                            Accessible.name: qsTr("Default target hardware identifier and position")
                        }
                        Label {
                            text: qsTr("Download directory")
                            color: Theme.muted
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space1

                            TextField {
                                objectName: "firstRunDownloadDirectory"
                                Layout.fillWidth: true
                                text: root.app.downloadDirectory
                                onTextEdited: root.app.downloadDirectory = text
                                Accessible.name: qsTr("Download directory")
                            }
                            Button {
                                text: qsTr("Browse...")
                                onClicked: downloadDirectoryDialog.open()
                                Accessible.name: qsTr("Choose the download directory")
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2

                        AppButton {
                            objectName: "firstRunStart"
                            primary: true
                            text: qsTr("Start")
                            onClicked: {
                                root.app.completeFirstRun();
                                root.started();
                            }
                            Accessible.name: qsTr("Start the loader")
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Saves the settings above and opens the window")
                        }
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("The media set is chosen in the window, then checked before an upload.")
                            wrapMode: Text.Wrap
                            color: Theme.muted
                        }
                    }
                }
            }

            // ---- what the window does ----
            Panel {
                Layout.fillHeight: true
                Layout.preferredWidth: 2
                title: qsTr("What this window does")

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Theme.space3

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("01  Choose a media set")
                        font.pixelSize: Theme.fontHeading
                        font.bold: true
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("A directory with one directory per member: its loads, files and findings are read and checked.")
                        wrapMode: Text.Wrap
                        color: Theme.muted
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("02  Find the target")
                        font.pixelSize: Theme.fontHeading
                        font.bold: true
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("FIND broadcasts for three seconds; a target that does not answer can be named by address and THW_ID_POS.")
                        wrapMode: Text.Wrap
                        color: Theme.muted
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("03  Upload or download")
                        font.pixelSize: Theme.fontHeading
                        font.bold: true
                    }
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("The information, upload and download operations run on the engine's own thread, with progress, an abort and an activity log.")
                        wrapMode: Text.Wrap
                        color: Theme.muted
                    }
                    Item { Layout.fillHeight: true }
                }
            }
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// What the chosen media set holds: its identity, its loads, every file from FILES.LUM and where
// the set differs from the standard's recommendations.
AppDialog {
    id: dialog

    required property var app

    heading: qsTr("Media set properties")
    primaryText: qsTr("Close")
    width: Math.min(940, Overlay.overlay ? Overlay.overlay.width - 48 : 940)
    height: Math.min(720, Overlay.overlay ? Overlay.overlay.height - 48 : 720)

    content: ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: Theme.space3

        GridLayout {
            Layout.fillWidth: true
            columns: 5
            columnSpacing: Theme.space2
            rowSpacing: Theme.space2

            StatTile {
                label: qsTr("Part number")
                valueObjectName: "mediaSetPartNumberLabel"
                value: dialog.app.mediaSetPartNumber.length > 0
                       ? dialog.app.mediaSetPartNumber : qsTr("not read")
            }
            StatTile {
                label: qsTr("Members")
                value: String(dialog.app.mediaSetMemberCount)
            }
            StatTile {
                label: qsTr("Loads")
                valueObjectName: "mediaSetLoadCountLabel"
                value: String(dialog.app.loadCount)
            }
            StatTile {
                label: qsTr("Files")
                valueObjectName: "mediaSetFileCountLabel"
                value: String(dialog.app.fileCount)
            }
            StatTile {
                label: qsTr("Batches")
                value: String(dialog.app.mediaSetBatchCount)
            }
        }

        Text {
            Layout.fillWidth: true
            text: dialog.app.mediaSetPath.length > 0
                  ? dialog.app.mediaSetPath : qsTr("No media set chosen")
            color: Theme.textMuted
            font.pixelSize: Theme.fontSmall
            font.family: Theme.monoFamily
            elide: Text.ElideMiddle
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 2
            color: Theme.divider
        }

        SectionHeader {
            label: qsTr("Loads")
            summary: Theme.quantity(dialog.app.loadCount, qsTr("load"), qsTr("loads"))
        }

        ListView {
            id: loadsList

            Layout.fillWidth: true
            Layout.preferredHeight: 96
            clip: true
            model: dialog.app.loads
            ScrollBar.vertical: AppScrollBar {}
            boundsBehavior: Flickable.StopAtBounds
            activeFocusOnTab: true
            Accessible.role: Accessible.List
            Accessible.focusable: true
            Keys.onPressed: (event) => loadsListKeys.handle(event)

            KeyScroll {
                id: loadsListKeys

                flickable: loadsList
            }

            // Inside the edge: the list clips, so a ring outside it would be cut off.
            FocusRing {
                parent: loadsList
                anchors.margins: 0
                active: loadsList.activeFocus
            }

            delegate: RowLayout {
                required property string partNumber
                required property string detail

                width: ListView.view.width
                spacing: Theme.space2

                Text {
                    Layout.preferredWidth: 210
                    text: partNumber
                    color: Theme.text
                    font.pixelSize: Theme.fontBody
                    font.family: Theme.monoFamily
                    font.bold: true
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    text: detail
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                    elide: Text.ElideRight
                }
            }
        }

        SectionHeader {
            label: qsTr("Files")
            summary: Theme.quantity(dialog.app.fileCount,
                                    qsTr("file from FILES.LUM"), qsTr("files from FILES.LUM"))
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2

            Text {
                Layout.preferredWidth: 220
                text: qsTr("NAME")
                color: Theme.textMuted
                font.pixelSize: Theme.fontMicro
                font.family: Theme.monoFamily
            }
            Text {
                Layout.preferredWidth: 72
                text: qsTr("MEMBER")
                color: Theme.textMuted
                font.pixelSize: Theme.fontMicro
                font.family: Theme.monoFamily
            }
            Text {
                Layout.fillWidth: true
                text: qsTr("PATH")
                color: Theme.textMuted
                font.pixelSize: Theme.fontMicro
                font.family: Theme.monoFamily
            }
            Text {
                Layout.preferredWidth: 64
                horizontalAlignment: Text.AlignRight
                text: qsTr("CRC")
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
            id: filesList

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: dialog.app.files
            ScrollBar.vertical: AppScrollBar {}
            boundsBehavior: Flickable.StopAtBounds
            activeFocusOnTab: true
            Accessible.role: Accessible.List
            Accessible.focusable: true
            Keys.onPressed: (event) => filesListKeys.handle(event)

            KeyScroll {
                id: filesListKeys

                flickable: filesList
            }

            // Inside the edge: the list clips, so a ring outside it would be cut off.
            FocusRing {
                parent: filesList
                anchors.margins: 0
                active: filesList.activeFocus
            }

            delegate: RowLayout {
                required property string name
                required property int member
                required property string pathname
                required property string crc

                width: ListView.view.width
                spacing: Theme.space2

                Text {
                    Layout.preferredWidth: 220
                    text: name
                    color: Theme.text
                    font.pixelSize: Theme.fontSmall
                    font.family: Theme.monoFamily
                    elide: Text.ElideRight
                }
                Text {
                    Layout.preferredWidth: 72
                    text: String(member)
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                    font.family: Theme.monoFamily
                }
                Text {
                    Layout.fillWidth: true
                    text: pathname
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                    elide: Text.ElideMiddle
                }
                Text {
                    Layout.preferredWidth: 64
                    horizontalAlignment: Text.AlignRight
                    text: crc
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                    font.family: Theme.monoFamily
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: dialog.app.mediaSetFindings.length > 0
            spacing: Theme.space2

            StatusChip {
                text: Theme.quantity(dialog.app.mediaSetFindings.length,
                                     qsTr("WARNING"), qsTr("WARNINGS"))
                tone: "warn"
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Deviations from the standard's recommendations. They are shown, and do not block an upload.")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSmall
                wrapMode: Text.WordWrap
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: dialog.app.mediaSetFindings.length > 0
            spacing: 0

            Repeater {
                model: dialog.app.mediaSetFindings

                delegate: ToneMessage {
                    id: finding

                    required property string modelData

                    tone: "warn"
                    text: finding.modelData
                }
            }
        }
    }

    onPrimaryClicked: dialog.close()
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// What the chosen media set holds: its identity, its loads, every file of it (from FILES.LUM)
// and where it differs from the standard's recommendations.
Dialog {
    id: dialog

    required property var app

    title: qsTr("Media set properties")
    modal: true
    standardButtons: Dialog.Close
    width: 720
    height: 600

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.space2

        GridLayout {
            columns: 2
            columnSpacing: Theme.space3
            rowSpacing: Theme.space1

            Label {
                text: qsTr("Part number")
                color: Theme.muted
            }
            Label {
                text: dialog.app.mediaSetPartNumber.length > 0
                      ? dialog.app.mediaSetPartNumber : qsTr("not read")
                font.bold: true
            }
            Label {
                text: qsTr("Members")
                color: Theme.muted
            }
            Label { text: dialog.app.mediaSetMemberCount }
            Label {
                text: qsTr("Batches")
                color: Theme.muted
            }
            Label { text: dialog.app.mediaSetBatchCount }
            Label {
                text: qsTr("Directory")
                color: Theme.muted
            }
            Label {
                text: dialog.app.mediaSetPath.length > 0
                      ? dialog.app.mediaSetPath : qsTr("not chosen")
                elide: Text.ElideMiddle
                Layout.maximumWidth: 520
            }
        }

        Label {
            text: qsTr("%n load(s)", "", dialog.app.loads.rowCount())
            font.bold: true
        }
        ListView {
            id: loadsList

            Layout.fillWidth: true
            Layout.preferredHeight: 110
            clip: true
            model: dialog.app.loads
            ScrollBar.vertical: ScrollBar {}

            delegate: RowLayout {
                required property string partNumber
                required property string detail

                width: ListView.view.width
                spacing: Theme.space2

                Label {
                    Layout.preferredWidth: 180
                    text: partNumber
                    elide: Text.ElideRight
                    font.bold: true
                }
                Label {
                    Layout.fillWidth: true
                    text: detail
                    color: Theme.muted
                    elide: Text.ElideRight
                }
            }
        }

        Label {
            text: qsTr("%n file(s), from FILES.LUM", "", dialog.app.files.rowCount())
            font.bold: true
        }
        ListView {
            id: filesList

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: dialog.app.files
            ScrollBar.vertical: ScrollBar {}

            delegate: RowLayout {
                required property string name
                required property int member
                required property string pathname
                required property string crc

                width: ListView.view.width
                spacing: Theme.space2

                Label {
                    Layout.preferredWidth: 220
                    text: name
                    elide: Text.ElideRight
                }
                Label {
                    Layout.preferredWidth: 80
                    text: qsTr("member %1").arg(member)
                    color: Theme.muted
                }
                Label {
                    Layout.fillWidth: true
                    text: pathname
                    elide: Text.ElideMiddle
                    color: Theme.muted
                }
                Label {
                    Layout.preferredWidth: 44
                    horizontalAlignment: Text.AlignRight
                    text: crc
                    font.family: Theme.mono
                    font.pixelSize: Theme.fontSmall
                    color: Theme.muted
                }
            }
        }

        Label {
            visible: dialog.app.mediaSetFindings.length > 0
            text: qsTr("%n warning(s) against the standard's recommendations", "",
                       dialog.app.mediaSetFindings.length)
            font.bold: true
            color: Theme.warn
        }
        Repeater {
            model: dialog.app.mediaSetFindings
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

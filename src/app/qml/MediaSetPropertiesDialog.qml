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
    width: 680
    height: 560

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        GridLayout {
            columns: 2
            columnSpacing: 12
            rowSpacing: 4

            Label {
                text: qsTr("Part number")
                opacity: 0.7
            }
            Label {
                objectName: "mediaSetPartNumberLabel"

                text: dialog.app.mediaSetPartNumber.length > 0
                      ? dialog.app.mediaSetPartNumber : qsTr("not read")
                font.bold: true
            }
            Label {
                text: qsTr("Members")
                opacity: 0.7
            }
            Label { text: dialog.app.mediaSetMemberCount }
            Label {
                text: qsTr("Batches")
                opacity: 0.7
            }
            Label { text: dialog.app.mediaSetBatchCount }
            Label {
                text: qsTr("Directory")
                opacity: 0.7
            }
            Label {
                text: dialog.app.mediaSetPath.length > 0
                      ? dialog.app.mediaSetPath : qsTr("not chosen")
                elide: Text.ElideMiddle
                Layout.maximumWidth: 480
            }
        }

        Label {
            objectName: "mediaSetLoadCountLabel"

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
                spacing: 8

                Label {
                    Layout.preferredWidth: 180
                    text: partNumber
                    elide: Text.ElideRight
                    font.bold: true
                }
                Label {
                    Layout.fillWidth: true
                    text: detail
                    opacity: 0.7
                    elide: Text.ElideRight
                }
            }
        }

        Label {
            objectName: "mediaSetFileCountLabel"

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
                spacing: 8

                Label {
                    Layout.preferredWidth: 220
                    text: name
                    elide: Text.ElideRight
                }
                Label {
                    Layout.preferredWidth: 72
                    text: qsTr("member %1").arg(member)
                    opacity: 0.7
                }
                Label {
                    Layout.fillWidth: true
                    text: pathname
                    elide: Text.ElideMiddle
                    opacity: 0.7
                }
                Label {
                    Layout.preferredWidth: 44
                    horizontalAlignment: Text.AlignRight
                    text: crc
                    font.family: "Consolas"
                    opacity: 0.85
                }
            }
        }

        Label {
            visible: dialog.app.mediaSetFindings.length > 0
            text: qsTr("%n warning(s) against the standard's recommendations", "",
                       dialog.app.mediaSetFindings.length)
            font.bold: true
        }
        Repeater {
            model: dialog.app.mediaSetFindings
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

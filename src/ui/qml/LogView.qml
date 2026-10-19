import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The activity log: what the target said and what the loader did, newest last. Time in mono,
// message in the body face, severity carried by the word in the message and by the accent.
Item {
    id: root

    required property var events
    property bool showTime: true
    readonly property int count: list.count

    function colourOf(severity) {
        switch (severity) {
        case 2:
        case 1:
            return Theme.accentInk;
        case 3:
            return Theme.accent;
        default:
            return Theme.text;
        }
    }

    ListView {
        id: list

        property bool following: true

        anchors.fill: parent
        clip: true
        model: root.events
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        Accessible.role: Accessible.List
        Accessible.name: qsTr("Activity log")

        delegate: RowLayout {
            id: row

            required property string time
            required property string message
            required property int severity

            width: ListView.view.width
            spacing: Theme.space3

            Text {
                visible: root.showTime
                text: row.time
                color: Theme.textMuted
                font.pixelSize: Theme.fontMono
                font.family: Theme.monoFamily
                Layout.preferredWidth: 62
                Layout.alignment: Qt.AlignTop
            }

            Text {
                Layout.fillWidth: true
                text: row.message
                textFormat: Text.PlainText
                color: root.colourOf(row.severity)
                font.pixelSize: Theme.fontSmall
                wrapMode: Text.Wrap
                Layout.alignment: Qt.AlignTop
            }
        }

        onMovementEnded: following = atYEnd
        onCountChanged: if (following) Qt.callLater(positionViewAtEnd)
    }

    Label {
        anchors.centerIn: parent
        visible: list.count === 0
        text: qsTr("No activity yet.")
        color: Theme.textMuted
        font.pixelSize: Theme.fontBody
    }
}

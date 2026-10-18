import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The activity log: what the target said and what the loader did. Follows the newest line unless
// the operator has scrolled up to read.
Frame {
    id: root

    required property var events

    function colourOf(severity) {
        switch (severity) {
        case 1: return Theme.warn;  // notice
        case 2: return Theme.warn;  // warning
        case 3: return Theme.fail;  // error
        default: return Theme.ink;
        }
    }

    padding: 0
    background: Rectangle {
        color: "transparent"
    }

    ListView {
        id: list

        // False once the operator scrolls away from the end; true again at the end.
        property bool following: true

        anchors.fill: parent
        clip: true
        model: root.events
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        Accessible.role: Accessible.List
        Accessible.name: qsTr("Activity log")

        delegate: Label {
            required property string time
            required property string message
            required property int severity

            width: ListView.view.width
            leftPadding: Theme.space2
            rightPadding: Theme.space2
            topPadding: 2
            bottomPadding: 2
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            color: root.colourOf(severity)
            font.pixelSize: Theme.fontSmall
            text: time + "  " + message
        }

        onMovementEnded: following = atYEnd
        onCountChanged: if (following) Qt.callLater(positionViewAtEnd)
    }
}

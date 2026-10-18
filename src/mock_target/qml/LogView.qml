import QtQuick
import QtQuick.Controls

// The activity log: what the mock target did, with the severity in colour. Follows the newest
// line unless the operator has scrolled up to read.
Frame {
    id: root

    required property var events

    readonly property bool dark: palette.window.hslLightness < 0.5

    function colourOf(severity) {
        switch (severity) {
        case 1: return dark ? "#e0c060" : "#7a5c00"   // notice
        case 2: return dark ? "#ffb060" : "#a04000"   // warning
        case 3: return dark ? "#ff7070" : "#b00020"   // error
        default: return palette.text
        }
    }

    padding: 0

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
            required property int severity
            required property string time
            required property string message

            width: ListView.view.width
            leftPadding: 8
            rightPadding: 8
            topPadding: 2
            bottomPadding: 2
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            color: root.colourOf(severity)
            font.pixelSize: 12
            text: time + "  " + message
        }

        onMovementEnded: following = atYEnd
        onCountChanged: if (following) Qt.callLater(positionViewAtEnd)
    }
}

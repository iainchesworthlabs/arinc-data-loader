import QtQuick
import QtQuick.Layouts

// The empty state of a panel: a short title, one sentence that says what will appear and how to
// make it appear, and the actions that do it. A quiet neutral surface, not a blank box.
Rectangle {
    id: root

    property string title: ""
    property string body: ""
    default property alias actions: actionRow.data

    Layout.fillWidth: true
    implicitHeight: content.implicitHeight + Theme.space4 * 2
    color: Theme.neutral100
    border.color: Theme.divider
    border.width: 1
    radius: Theme.radiusControl
    Accessible.role: Accessible.Grouping
    Accessible.name: root.title

    ColumnLayout {
        id: content

        anchors.fill: parent
        anchors.margins: Theme.space4
        spacing: Theme.space2

        Rectangle {
            Layout.preferredWidth: 24
            Layout.preferredHeight: 2
            color: Theme.accent
        }

        Text {
            Layout.fillWidth: true
            text: root.title
            color: Theme.text
            font.pixelSize: Theme.fontSubheading
            font.family: Theme.headingFamily
            font.weight: Font.DemiBold
            wrapMode: Text.WordWrap
        }

        Text {
            Layout.fillWidth: true
            visible: root.body.length > 0
            text: root.body
            color: Theme.textMuted
            font.pixelSize: Theme.fontBody
            wrapMode: Text.WordWrap
        }

        RowLayout {
            id: actionRow

            Layout.fillWidth: true
            spacing: Theme.space2
        }
    }
}

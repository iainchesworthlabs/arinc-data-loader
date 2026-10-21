import QtQuick
import QtQuick.Layouts

// One preflight check: what is being checked, the value it currently has, a supporting line and
// the verdict. The verdict is a word as well as a tone, so the row does not rely on colour.
Rectangle {
    id: root

    property string label: ""
    property string value: ""
    property string detail: ""
    property string chipText: ""
    property string tone: "neutral"

    Layout.fillWidth: true
    implicitHeight: content.implicitHeight + Theme.space3 * 2
    color: Theme.neutral100
    border.color: Theme.divider
    border.width: 1
    radius: Theme.radiusControl
    Accessible.role: Accessible.Grouping
    Accessible.name: root.label + ": " + root.value + ", " + root.chipText

    RowLayout {
        id: content

        anchors.fill: parent
        anchors.margins: Theme.space3
        spacing: Theme.space2

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                text: root.label.toUpperCase()
                color: Theme.textMuted
                font.pixelSize: Theme.fontMicro
                font.family: Theme.monoFamily
                font.letterSpacing: 0.8
            }

            Text {
                Layout.fillWidth: true
                text: root.value.length > 0 ? root.value : qsTr("Not set")
                color: Theme.text
                font.pixelSize: Theme.fontNormal
                font.family: Theme.monoFamily
                font.bold: true
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                objectName: "checkTileDetail"
                visible: root.detail.length > 0
                text: root.detail
                color: Theme.textMuted
                font.pixelSize: Theme.fontSmall
                // Two lines, then an ellipsis: "FIND or type an address" must not be cut to
                // "FIND or type an a..." in a narrow tile.
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }
        }

        StatusChip {
            text: root.chipText
            tone: root.tone
        }
    }
}

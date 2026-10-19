import QtQuick
import QtQuick.Controls as QQC

// A small exclusive choice: a row of 30 px segments, the chosen one filled with accentFill and
// lettered in accentText, the rest transparent with a 1 px divider. One tab stop for the group;
// Left/Right/Home/End move it. Each segment is a Button so `objectName` and `click()` remain what
// the window tests drive.
Item {
    id: root

    property var segments: []
    property int selectedValue: 0
    signal segmentClicked(int value)

    implicitWidth: row.implicitWidth
    implicitHeight: row.implicitHeight
    opacity: enabled ? 1.0 : 0.45
    Accessible.role: Accessible.Grouping
    Accessible.name: ""

    function choose(value) {
        if (value !== root.selectedValue)
            root.segmentClicked(value);
    }

    Row {
        id: row

        anchors.fill: parent
        spacing: 0

        Repeater {
            model: root.segments

            delegate: QQC.Button {
                id: segment

                required property var modelData
                required property int index

                objectName: modelData.name || ""
                text: modelData.label
                checkable: true
                checked: root.selectedValue === modelData.value
                enabled: root.enabled
                padding: Theme.space2
                implicitHeight: Math.max(30, contentItem.implicitHeight + 12)
                implicitWidth: Math.max(90, contentItem.implicitWidth + 24)
                activeFocusOnTab: index === 0
                Accessible.role: Accessible.RadioButton
                Accessible.name: modelData.label
                Accessible.checked: checked

                background: Rectangle {
                    color: segment.checked
                           ? Theme.accentFill
                           : segment.pressed ? Theme.neutral300
                                             : segment.hovered ? Theme.neutral200 : "transparent"
                    border.width: segment.checked ? 0 : 1
                    border.color: Theme.divider
                    radius: Theme.radiusControl
                }

                contentItem: Text {
                    text: segment.text
                    color: segment.checked ? Theme.accentText : Theme.text
                    font.pixelSize: Theme.fontBody
                    font.family: Theme.headingFamily
                    font.weight: segment.checked ? Font.DemiBold : Font.Normal
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                onClicked: root.choose(modelData.value)

                Keys.onPressed: function(event) {
                    if (event.key === Qt.Key_Left || event.key === Qt.Key_Up) {
                        const next = (index - 1 + root.segments.length) % root.segments.length;
                        root.choose(root.segments[next].value);
                        event.accepted = true;
                    } else if (event.key === Qt.Key_Right || event.key === Qt.Key_Down) {
                        const next = (index + 1) % root.segments.length;
                        root.choose(root.segments[next].value);
                        event.accepted = true;
                    } else if (event.key === Qt.Key_Home) {
                        root.choose(root.segments[0].value);
                        event.accepted = true;
                    } else if (event.key === Qt.Key_End) {
                        root.choose(root.segments[root.segments.length - 1].value);
                        event.accepted = true;
                    }
                }

                FocusRing {
                    active: segment.visualFocus
                }
            }
        }
    }
}

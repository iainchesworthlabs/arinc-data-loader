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

    // The segment that is the group's one tab stop: the chosen one (the first when none is).
    readonly property int currentIndex: {
        for (let i = 0; i < root.segments.length; ++i) {
            if (root.segments[i].value === root.selectedValue)
                return i;
        }
        return 0;
    }

    function choose(value) {
        if (value !== root.selectedValue)
            root.segmentClicked(value);
    }

    // Chooses the segment at `position` and moves the keyboard focus onto it, so the next arrow
    // key continues from there.
    function moveTo(position) {
        root.choose(root.segments[position].value);
        repeater.itemAt(position).forceActiveFocus();
    }

    Row {
        id: row

        anchors.fill: parent
        spacing: 0

        Repeater {
            id: repeater

            model: root.segments

            delegate: QQC.Button {
                id: segment

                required property var modelData
                required property int index

                objectName: modelData.name || ""
                text: modelData.label
                // Not checkable: a click on the chosen segment must not clear it.
                checked: root.selectedValue === modelData.value
                enabled: root.enabled
                padding: Theme.space2
                implicitHeight: Math.max(30, contentItem.implicitHeight + 12)
                implicitWidth: Math.max(90, contentItem.implicitWidth + 24)
                activeFocusOnTab: index === root.currentIndex
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
                    const count = root.segments.length;
                    // Left and Right swap in a right-to-left layout; Up and Down never do.
                    const previousKey = segment.mirrored ? Qt.Key_Right : Qt.Key_Left;
                    const nextKey = segment.mirrored ? Qt.Key_Left : Qt.Key_Right;
                    if (event.key === previousKey || event.key === Qt.Key_Up) {
                        root.moveTo((index - 1 + count) % count);
                        event.accepted = true;
                    } else if (event.key === nextKey || event.key === Qt.Key_Down) {
                        root.moveTo((index + 1) % count);
                        event.accepted = true;
                    } else if (event.key === Qt.Key_Home) {
                        root.moveTo(0);
                        event.accepted = true;
                    } else if (event.key === Qt.Key_End) {
                        root.moveTo(count - 1);
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

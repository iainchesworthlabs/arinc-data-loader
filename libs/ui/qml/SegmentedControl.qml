// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC

// A small exclusive choice: a row of 30 px segments, the chosen one filled with accentFill and
// lettered in accentText, the rest transparent with a 1 px divider. One tab stop for the group (the
// chosen segment); Left/Right (swapped in a right-to-left layout), Up/Down, Home and End choose the
// neighbouring segment and move the focus onto it, wrapping at the ends. A segment cannot be
// unchecked by clicking it. Each segment is a Button so `objectName` and `click()` remain what the
// window tests drive.
//
// `tabs: true` draws the same control as a tab strip: 42 px tall, no boxes, the chosen tab
// underlined by 3 px of accent and lettered in the full text colour. Same keys, same one tab stop.
Item {
    id: root

    property var segments: []
    property int selectedValue: 0
    // The tab-strip look, for the activity dock.
    property bool tabs: false
    signal segmentClicked(int value)

    implicitWidth: row.implicitWidth
    implicitHeight: row.implicitHeight
    opacity: enabled ? 1.0 : Theme.disabledOpacity
    Accessible.role: root.tabs ? Accessible.PageTabList : Accessible.Grouping
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
        repeater.itemAt(position).forceActiveFocus(Qt.TabFocusReason);
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
                padding: root.tabs ? Theme.space3 : Theme.space2
                implicitHeight: root.tabs ? 42 : Math.max(30, contentItem.implicitHeight + 12)
                implicitWidth: root.tabs ? contentItem.implicitWidth + padding * 2
                                         : Math.max(90, contentItem.implicitWidth + 24)
                // The item holding the focus keeps its tab stop until the focus leaves it: Qt refuses
                // to clear activeFocusOnTab on the active item, and the choice changes first.
                activeFocusOnTab: index === root.currentIndex || activeFocus
                Accessible.role: root.tabs ? Accessible.PageTab : Accessible.RadioButton
                Accessible.name: modelData.label
                Accessible.checked: checked

                background: Rectangle {
                    color: root.tabs
                           ? (segment.hovered && !segment.checked ? Theme.neutral200 : "transparent")
                           : segment.checked
                             ? Theme.accentFill
                             : segment.pressed ? Theme.neutral300
                                               : segment.hovered ? Theme.neutral200 : "transparent"
                    border.width: !root.tabs && !segment.checked ? 1 : 0
                    border.color: Theme.divider
                    radius: Theme.radiusControl

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 3
                        visible: root.tabs && segment.checked
                        color: Theme.accent
                    }
                }

                contentItem: Text {
                    text: segment.text
                    color: root.tabs
                           ? (segment.checked ? Theme.text : Theme.textMuted)
                           : segment.checked ? Theme.accentText : Theme.text
                    font.pixelSize: root.tabs ? Theme.fontSmall : Theme.fontBody
                    font.family: Theme.headingFamily
                    font.weight: segment.checked ? Font.DemiBold : (root.tabs ? Font.DemiBold : Font.Normal)
                    font.letterSpacing: root.tabs ? 0.6 : 0
                    horizontalAlignment: root.tabs ? Text.AlignLeft : Text.AlignHCenter
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
                    // Inside the strip's own height, so the dock's 42 px band does not clip it.
                    anchors.margins: root.tabs ? 2 : -Theme.focusRingOffset
                    active: segment.visualFocus
                }
            }
        }
    }
}

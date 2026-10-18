import QtQuick
import QtQuick.Controls

// A row of exclusive segments for a small, fixed choice (the Download mode). The selection is
// bound in from outside (`selectedValue`); a click or an arrow key only reports the choice
// (`segmentClicked`), so the caller's binding survives and the component never writes state it
// does not own. Each segment is a Button, so it keeps the keyboard behaviour, `click()` for the
// window's offscreen test, and the objectName that test looks for.
Item {
    id: root

    // [{ value: <int>, label: <string>, name: <objectName, optional> }]
    property var segments: []
    property int selectedValue: 0

    signal segmentClicked(int value)

    implicitWidth: row.implicitWidth
    implicitHeight: row.implicitHeight

    function choose(value) {
        if (value !== selectedValue) {
            segmentClicked(value);
        }
    }

    function move(from, delta) {
        if (segments.length === 0) {
            return;
        }
        const next = (from + delta + segments.length) % segments.length;
        segmentClicked(segments[next].value);
        const item = segmentRepeater.itemAt(next);
        if (item) {
            item.forceActiveFocus();
        }
    }

    Row {
        id: row

        anchors.fill: parent
        spacing: Theme.space1

        Repeater {
            id: segmentRepeater

            model: root.segments
            delegate: Button {
                id: segment

                required property var modelData
                required property int index

                objectName: modelData.name || ""
                text: modelData.label
                checkable: true
                checked: root.selectedValue === modelData.value
                padding: Theme.space2
                implicitHeight: 32
                implicitWidth: Math.max(120, contentItem.implicitWidth + padding * 2)
                // One tab stop for the group, as forge-gui's SegmentedControl does it: the arrow
                // keys move the choice inside it.
                activeFocusOnTab: index === 0
                Accessible.role: Accessible.RadioButton
                Accessible.name: modelData.label
                Accessible.checked: checked

                onClicked: root.choose(modelData.value)
                Keys.onPressed: (event) => {
                    if (event.key === Qt.Key_Left) {
                        root.move(index, -1);
                        event.accepted = true;
                    } else if (event.key === Qt.Key_Right) {
                        root.move(index, 1);
                        event.accepted = true;
                    } else if (event.key === Qt.Key_Home) {
                        root.move(0, 0);
                        event.accepted = true;
                    } else if (event.key === Qt.Key_End) {
                        root.move(root.segments.length - 1, 0);
                        event.accepted = true;
                    }
                }

                background: Rectangle {
                    radius: Theme.radiusControl
                    color: segment.checked
                           ? Theme.accent
                           : segment.hovered ? Theme.panel : "transparent"
                    border.width: segment.checked ? 0 : 1
                    border.color: Theme.border
                }

                contentItem: Text {
                    text: segment.text
                    font.pixelSize: Theme.fontBody
                    color: segment.checked ? Theme.accentInk : Theme.ink
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                FocusRing {}
            }
        }
    }
}

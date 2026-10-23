// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// A three-square busy mark. The squares light in sequence; under reduced motion they are all
// still at full strength, because the mark is the squares and the motion is only the second
// reading of it.
Item {
    id: root

    implicitWidth: 24
    implicitHeight: 24

    property real phase: 0

    NumberAnimation on phase {
        running: !Theme.reducedMotion && root.visible
        from: 0
        to: 3
        duration: 900
        loops: Animation.Infinite
    }

    Row {
        anchors.centerIn: parent
        spacing: 3

        Repeater {
            model: 3

            delegate: Rectangle {
                required property int index

                width: 5
                height: 5
                color: Theme.neutral600
                opacity: Theme.reducedMotion
                         ? 1.0
                         : 0.28 + 0.72 * Math.max(0.0, 1.0 - Math.abs(root.phase - index))
            }
        }
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts

// The window footer: a quiet mono line of state. Children are placed left to right; use an Item
// with Layout.fillWidth in the caller for a right-aligned group.
Rectangle {
    id: root

    default property alias content: row.data

    implicitHeight: 28
    color: Theme.surface
    border.color: Theme.divider
    border.width: 0

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: Theme.divider
    }

    RowLayout {
        id: row

        anchors.fill: parent
        anchors.leftMargin: Theme.space4
        anchors.rightMargin: Theme.space4
        spacing: Theme.space2
    }
}

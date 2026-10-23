// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

// The keyboard focus ring: 2 px of accent ink, two pixels outside the control's own border. Put
// one inside a hand-drawn control; it draws while `active` is true. A Button passes visualFocus
// so a mouse click does not ring.
Rectangle {
    id: ring

    property bool active: parent ? parent.activeFocus : false

    anchors.fill: parent
    anchors.margins: -Theme.focusRingOffset
    visible: active
    color: "transparent"
    border.color: Theme.focusRing
    border.width: Theme.focusRingWidth
    radius: Theme.radius
    z: 100
    Accessible.ignored: true
}

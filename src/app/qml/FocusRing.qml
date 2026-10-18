import QtQuick

// The keyboard focus ring. Place it as a child of the control it belongs to: it fills the parent,
// sits just outside the parent's own border while the parent has the keyboard focus, and is
// invisible otherwise (a mouse press does not put the ring on, because Button's visualFocus only
// follows the keyboard).
Rectangle {
    id: ring

    anchors.fill: parent
    anchors.margins: -(Theme.focusRingOffset + Theme.focusRingWidth)
    color: "transparent"
    border.color: Theme.focusRing
    border.width: Theme.focusRingWidth
    radius: (parent && parent.radius !== undefined ? parent.radius : 0)
            + Theme.focusRingOffset + Theme.focusRingWidth
    visible: parent !== null && parent.visualFocus === true
    z: 100

    Accessible.ignored: true
}

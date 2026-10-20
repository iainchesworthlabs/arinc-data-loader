import QtQuick
import QtQuick.Controls as QQC

// A text field drawn to the family: 30 px floor, neutral100 fill, a 1 px divider border, mono
// body for identifiers and numbers. The border becomes the focus ring colour while the keyboard
// is inside it. `unit` draws a muted suffix inside the box.
QQC.TextField {
    id: control

    property string unit: ""

    implicitHeight: Math.max(30, contentHeight + 10)
    leftPadding: Theme.space3
    rightPadding: unit.length > 0 ? unitLabel.implicitWidth + Theme.space4 : Theme.space3
    color: enabled ? Theme.text : Theme.textMuted
    placeholderTextColor: Theme.textMuted
    selectionColor: Theme.accentFill
    selectedTextColor: Theme.accentText
    font.pixelSize: Theme.fontBody
    font.family: Theme.monoFamily
    verticalAlignment: TextInput.AlignVCenter
    selectByMouse: true

    background: Rectangle {
        color: Theme.neutral100
        border.color: control.activeFocus ? Theme.focusRing : Theme.divider
        border.width: 1
        radius: Theme.radiusControl
    }

    Text {
        id: unitLabel

        visible: control.unit.length > 0
        anchors.right: parent.right
        anchors.rightMargin: Theme.space3
        anchors.verticalCenter: parent.verticalCenter
        text: control.unit
        color: Theme.textMuted
        font.family: Theme.monoFamily
        font.pixelSize: Theme.fontSmall
    }
}

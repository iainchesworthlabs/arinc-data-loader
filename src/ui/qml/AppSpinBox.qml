import QtQuick
import QtQuick.Controls as QQC

// A spin box drawn to the family: neutral100, a 1 px divider, square stepper cells on the right.
QQC.SpinBox {
    id: control

    implicitHeight: 30
    rightPadding: 26
    font.pixelSize: Theme.fontBody
    font.family: Theme.monoFamily
    editable: true
    // Plain digits: the default formats with the locale's grouping, which shows a port as "1,001".
    textFromValue: function(value) { return String(value) }
    background: Rectangle {
        color: Theme.neutral100
        border.color: control.activeFocus ? Theme.focusRing : Theme.divider
        border.width: 1
        radius: Theme.radiusControl
    }

    up.indicator: Rectangle {
        x: control.width - width
        y: 1
        width: 24
        height: (control.height - 2) / 2
        color: control.up.pressed ? Theme.neutral300
                                  : control.up.hovered ? Theme.neutral200 : Theme.neutral100
        border.color: Theme.divider
        border.width: 1

        Text {
            anchors.centerIn: parent
            text: "+"
            color: Theme.textMuted
            font.pixelSize: Theme.fontSmall
        }
    }

    down.indicator: Rectangle {
        x: control.width - width
        y: height + 1
        width: 24
        height: (control.height - 2) / 2
        color: control.down.pressed ? Theme.neutral300
                                    : control.down.hovered ? Theme.neutral200 : Theme.neutral100
        border.color: Theme.divider
        border.width: 1

        Text {
            anchors.centerIn: parent
            text: "\u2212"
            color: Theme.textMuted
            font.pixelSize: Theme.fontSmall
        }
    }
}

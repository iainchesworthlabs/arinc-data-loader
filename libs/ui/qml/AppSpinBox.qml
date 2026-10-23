// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC

// A spin box drawn to the family: neutral100, a 1 px divider, and two steppers side by side on the
// right (minus, then plus), each 30 px wide and as tall as the box, so each clears the 24 px
// minimum target (the stacked 24x14 cells did not). The 2 px focus ring sits outside the box.
//
// The value editor carries the control's accessible name, so a screen reader or voice control
// names the number field and not an unlabelled text input.
QQC.SpinBox {
    id: control

    readonly property int stepperWidth: 30

    implicitHeight: 32
    leftPadding: control.mirrored ? control.stepperWidth * 2 + Theme.space1 : 0
    rightPadding: control.mirrored ? 0 : control.stepperWidth * 2 + Theme.space1
    font.pixelSize: Theme.fontBody
    font.family: Theme.monoFamily
    editable: true
    opacity: enabled ? 1.0 : Theme.disabledOpacity
    // Plain digits: the default formats with the locale's grouping, which shows a port as "1,001".
    textFromValue: function(value) { return String(value) }

    background: Rectangle {
        color: Theme.neutral100
        border.color: control.activeFocus ? Theme.focusRing : Theme.divider
        border.width: 1
        radius: Theme.radiusControl
    }

    contentItem: TextInput {
        z: 2
        text: control.displayText
        font: control.font
        color: control.enabled ? Theme.text : Theme.textMuted
        selectionColor: Theme.accentFill
        selectedTextColor: Theme.accentText
        horizontalAlignment: Qt.AlignLeft
        verticalAlignment: Qt.AlignVCenter
        // The gap is at the start edge (the physical left in a left-to-right window, the right in a
        // mirrored one: the alignment mirrors by itself, a padding does not).
        leftPadding: control.mirrored ? 0 : Theme.space3
        rightPadding: control.mirrored ? Theme.space3 : 0
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: Qt.ImhFormattedNumbersOnly
        selectByMouse: true
        Accessible.role: Accessible.EditableText
        Accessible.name: control.Accessible.name
    }

    // The minus cell is the outer-left of the pair and the plus cell the outer-right, mirrored in a
    // right-to-left layout.
    down.indicator: Rectangle {
        x: control.mirrored ? 1 : control.width - control.stepperWidth * 2 - 1
        y: 1
        width: control.stepperWidth
        height: control.height - 2
        color: control.down.pressed ? Theme.neutral300
                                    : control.down.hovered ? Theme.neutral200 : Theme.neutral100
        border.color: Theme.divider
        border.width: 1
        opacity: control.value > control.from ? 1.0 : Theme.disabledOpacity

        Text {
            anchors.centerIn: parent
            text: "−"
            color: Theme.text
            font.pixelSize: Theme.fontNormal
        }
    }

    up.indicator: Rectangle {
        x: control.mirrored ? control.stepperWidth + 1 : control.width - control.stepperWidth - 1
        y: 1
        width: control.stepperWidth
        height: control.height - 2
        color: control.up.pressed ? Theme.neutral300
                                  : control.up.hovered ? Theme.neutral200 : Theme.neutral100
        border.color: Theme.divider
        border.width: 1
        opacity: control.value < control.to ? 1.0 : Theme.disabledOpacity

        Text {
            anchors.centerIn: parent
            text: "+"
            color: Theme.text
            font.pixelSize: Theme.fontNormal
        }
    }

    FocusRing {
        active: control.activeFocus
    }
}

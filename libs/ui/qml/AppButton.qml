// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC

// The flat, bordered action button of the family: 30 px floor, a 1 px divider border, accent
// fill when primary, dimmed when disabled, and a 2 px focus ring outside the border. A Button
// underneath keeps the standard keyboard, accessibility and click() behaviour.
QQC.Button {
    id: control

    property bool primary: false

    padding: Theme.space3
    implicitWidth: Math.max(0, contentItem.implicitWidth + padding * 2)
    implicitHeight: Math.max(30, contentItem.implicitHeight + 12)
    hoverEnabled: true
    opacity: enabled ? 1.0 : Theme.disabledOpacity
    Accessible.name: text

    // Enter and Return press the focused button, as Space does (the standard Button handles only
    // Space).
    Keys.onReturnPressed: if (control.enabled) control.click()
    Keys.onEnterPressed: if (control.enabled) control.click()

    background: Rectangle {
        radius: Theme.radiusControl
        color: {
            if (control.primary) {
                if (control.pressed)
                    return Qt.darker(Theme.accentFill, 1.15);
                return control.hovered ? Qt.lighter(Theme.accentFill, 1.08) : Theme.accentFill;
            }
            if (control.pressed)
                return Theme.neutral300;
            return control.hovered ? Theme.neutral200 : "transparent";
        }
        border.width: control.primary ? 0 : 1
        border.color: Theme.divider
    }

    contentItem: Text {
        text: control.text
        font.pixelSize: Theme.fontBody
        font.family: Theme.headingFamily
        font.weight: control.primary ? Font.DemiBold : Font.Normal
        color: control.primary ? Theme.accentText : Theme.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    FocusRing {
        active: control.visualFocus
    }
}

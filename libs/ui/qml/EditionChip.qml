// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC

// The edition chip of a window's status bar: the edition, and when the licence is ending or has ended, how
// much is left. It is a button, so the keyboard reaches it and a screen reader says what it opens; the
// words and the tone are the licence model's (LicenceModel::chipText, chipTone, chipName), the chip is
// text and a glyph and never a colour alone. Activating it asks the window to open the Licence dialog.
QQC.AbstractButton {
    id: root

    property string tone: "neutral"
    property string description: ""

    implicitWidth: chip.implicitWidth
    implicitHeight: chip.implicitHeight
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    Accessible.role: Accessible.Button
    Accessible.name: root.description.length > 0 ? root.description : root.text

    Keys.onReturnPressed: root.click()
    Keys.onEnterPressed: root.click()

    contentItem: StatusChip {
        id: chip

        text: root.text
        tone: root.tone
        opacity: root.hovered || root.visualFocus ? 1.0 : 0.92
    }

    background: Item {
        FocusRing {
            active: root.visualFocus
        }
    }
}

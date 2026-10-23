// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts

// A section header: a mono accent ordinal, an uppercase tracked label, a 2 px rule and an
// optional mono summary on the right.
RowLayout {
    id: root

    property alias label: labelText.text
    property alias summary: summaryText.text
    property string ordinal: ""
    property bool rule: true

    Layout.fillWidth: true
    spacing: Theme.space2
    visible: labelText.text.length > 0

    Text {
        visible: root.ordinal.length > 0
        text: root.ordinal
        color: Theme.accentInk
        font.family: Theme.monoFamily
        font.pixelSize: Theme.fontSmall
        font.bold: true
    }

    Text {
        id: labelText

        color: Theme.text
        font.pixelSize: Theme.fontSmall
        font.bold: true
        font.capitalization: Font.AllUppercase
        font.letterSpacing: Theme.trackingWide
        elide: Text.ElideRight
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        Layout.maximumWidth: Math.ceil(implicitWidth)
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 2
        color: root.rule ? Theme.divider : "transparent"
    }

    Text {
        id: summaryText

        visible: text.length > 0
        color: Theme.textMuted
        font.family: Theme.monoFamily
        font.pixelSize: Theme.fontSmall
        elide: Text.ElideRight
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        Layout.maximumWidth: Math.ceil(implicitWidth)
    }
}

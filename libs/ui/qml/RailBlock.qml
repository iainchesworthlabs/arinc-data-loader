// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts

// One numbered block of a rail: a mono accent ordinal, an uppercase tracked label, a 2 px rule
// and the block's content beneath.
ColumnLayout {
    id: root

    property string ordinal: "01"
    property string label: ""
    default property alias content: body.data

    spacing: Theme.space3

    Accessible.role: Accessible.Grouping
    Accessible.name: root.label

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.space2

        Text {
            text: root.ordinal
            color: Theme.accentInk
            font.pixelSize: Theme.fontMono
            font.family: Theme.monoFamily
        }

        Text {
            text: root.label
            color: Theme.text
            font.pixelSize: Theme.fontMono
            font.family: Theme.headingFamily
            font.letterSpacing: 1.3
            font.weight: Font.DemiBold
            font.capitalization: Font.AllUppercase
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 2
            color: Theme.divider
        }
    }

    ColumnLayout {
        id: body

        Layout.fillWidth: true
        spacing: Theme.space3
    }
}

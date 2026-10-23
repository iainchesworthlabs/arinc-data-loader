// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts

// The application header: the family's wordmark, the product name and subtitle, and the actions
// that belong to the window rather than to a panel. A 2 px rule closes it.
Rectangle {
    id: root

    property string appCode: "ADL"
    property string title: ""
    property string subtitle: ""
    default property alias actions: actionRow.data

    implicitHeight: 64
    color: Theme.surface

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space6
        anchors.rightMargin: Theme.space6
        anchors.topMargin: Theme.space2
        anchors.bottomMargin: Theme.space2
        spacing: Theme.space3

        Rectangle {
            Layout.preferredWidth: 4
            Layout.preferredHeight: 30
            color: Theme.accent
        }

        Text {
            text: root.appCode
            color: Theme.text
            font.pixelSize: Theme.fontTitle
            font.family: Theme.headingFamily
            font.weight: Font.ExtraBold
            font.letterSpacing: Theme.trackingTitle
        }

        ColumnLayout {
            spacing: 0

            Text {
                text: root.title
                color: Theme.text
                font.pixelSize: Theme.fontBody
                font.family: Theme.headingFamily
                font.weight: Font.DemiBold
            }

            Text {
                text: root.subtitle
                color: Theme.textMuted
                font.pixelSize: Theme.fontMicro
                font.family: Theme.monoFamily
                font.letterSpacing: 0.9
                font.capitalization: Font.AllUppercase
            }
        }

        Item {
            Layout.fillWidth: true
        }

        RowLayout {
            id: actionRow

            spacing: Theme.space2
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 2
        color: Theme.divider
    }
}

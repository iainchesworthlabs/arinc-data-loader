// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Layouts

// One boxed statistic: a muted micro label over a bold mono value.
Rectangle {
    id: root

    property alias label: labelText.text
    property alias value: valueText.text
    property string valueObjectName: ""

    Layout.fillWidth: true
    implicitHeight: content.implicitHeight + Theme.space4
    color: Theme.neutral100
    border.color: Theme.divider
    border.width: 1
    radius: Theme.radiusControl

    ColumnLayout {
        id: content

        anchors.fill: parent
        anchors.margins: Theme.space2
        spacing: 2

        Text {
            id: labelText

            color: Theme.textMuted
            font.pixelSize: Theme.fontMicro
            font.family: Theme.monoFamily
            font.letterSpacing: 0.8
            font.capitalization: Font.AllUppercase
        }

        Text {
            id: valueText

            objectName: root.valueObjectName
            color: Theme.text
            font.pixelSize: Theme.fontNormal
            font.family: Theme.monoFamily
            font.bold: true
            elide: Text.ElideRight
        }
    }
}

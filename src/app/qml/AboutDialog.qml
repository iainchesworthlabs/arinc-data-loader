import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// What the program is.
AppDialog {
    id: dialog

    heading: qsTr("About ARINC Data Loader")
    primaryText: qsTr("Close")
    width: 540

    content: ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.space3

        Text {
            text: "ADL"
            color: Theme.accent
            font.pixelSize: Theme.fontDisplay
            font.family: Theme.headingFamily
            font.weight: Font.ExtraBold
            font.letterSpacing: Theme.trackingTitle
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("An ARINC 615A and ARINC 665 engineering load console: media sets, targets, the Information, Upload and Download operations, and the evidence of what happened.")
            color: Theme.textMuted
            font.pixelSize: Theme.fontBody
            wrapMode: Text.WordWrap
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 2
            color: Theme.divider
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Version %1 · GPL-3.0-or-later").arg(Qt.application.version)
            color: Theme.text
            font.pixelSize: Theme.fontSmall
            font.family: Theme.monoFamily
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Uses Qt and the Archivo typeface (SIL OFL 1.1).")
            color: Theme.textMuted
            font.pixelSize: Theme.fontSmall
            wrapMode: Text.WordWrap
        }
    }

    onPrimaryClicked: dialog.close()
}

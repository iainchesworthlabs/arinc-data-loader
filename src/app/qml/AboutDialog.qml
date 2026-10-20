import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// What the program is. The version block is the same build provenance the other ICL tools show:
// version, git describe, full commit, branch, build target and Qt version (appVersionDetails is
// set by main.cpp from adl::ui::version_details(); the fallback is for a harness that does not
// register it).
AppDialog {
    id: dialog

    readonly property string details: typeof appVersionDetails === "string"
                                      ? appVersionDetails
                                      : qsTr("ARINC Data Loader %1").arg(Qt.application.version)

    // Licences opens the third-party notices; the window shows them (Main.qml).
    signal showLicences()

    heading: qsTr("About ARINC Data Loader")
    primaryText: qsTr("Close")
    secondaryText: qsTr("Licences")
    width: 620

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

        SectionHeader {
            label: qsTr("Version")
        }

        Text {
            objectName: "aboutVersionText"
            Layout.fillWidth: true
            text: dialog.details
            color: Theme.text
            font.pixelSize: Theme.fontSmall
            font.family: Theme.monoFamily
            wrapMode: Text.WordWrap
        }

        SectionHeader {
            label: qsTr("Licences")
        }

        Text {
            Layout.fillWidth: true
            textFormat: Text.RichText
            linkColor: Theme.accentInk
            text: qsTr("Free software under the GNU General Public License, version 3 or later. It is distributed WITHOUT ANY WARRANTY; see the <a href=\"https://www.gnu.org/licenses/gpl-3.0.html\">GNU General Public License</a>.")
            color: Theme.textMuted
            font.pixelSize: Theme.fontSmall
            wrapMode: Text.WordWrap
            onLinkActivated: (link) => Qt.openUrlExternally(link)
        }

        Text {
            Layout.fillWidth: true
            text: qsTr("Third-party components - Qt (LGPL 3), the Archivo typeface (SIL Open Font License 1.1) and others - are listed, with their licences, under Licences.")
            color: Theme.textMuted
            font.pixelSize: Theme.fontSmall
            wrapMode: Text.WordWrap
        }

        Text {
            Layout.fillWidth: true
            textFormat: Text.RichText
            linkColor: Theme.accentInk
            text: "<a href=\"https://github.com/iainchesworthlabs/arinc-data-loader\">github.com/iainchesworthlabs/arinc-data-loader</a>"
            color: Theme.textMuted
            font.pixelSize: Theme.fontSmall
            onLinkActivated: (link) => Qt.openUrlExternally(link)
        }
    }

    onSecondaryClicked: dialog.showLicences()
    onPrimaryClicked: dialog.close()
}

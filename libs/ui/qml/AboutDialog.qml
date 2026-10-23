import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// What a program is, the same page in all three: the wordmark, one paragraph, the build
// provenance (version, git describe, full commit, branch, build target and Qt version; the
// window passes appVersionDetails, which main.cpp sets from adl::ui::version_details()), the
// licence statement and a Licences button that opens the third-party notices. A window with
// something more to say puts it inside, below the licence block.
AppDialog {
    id: dialog

    // The wordmark ("ADL"), the product name and the one-paragraph description.
    property string appCode: "ADL"
    property string appName: ""
    property string blurb: ""
    // The window's provenance text; the fallback is for a harness that does not register it.
    property string details: typeof appVersionDetails === "string"
                             ? appVersionDetails
                             : qsTr("%1 %2").arg(dialog.appName).arg(Qt.application.version)
    default property alias extra: extraSection.data

    // Licences opens the third-party notices; the window shows them.
    signal showLicences()

    title: dialog.heading
    heading: qsTr("About %1").arg(dialog.appName)
    primaryText: qsTr("Close")
    secondaryText: qsTr("Licences")
    width: 620

    content: ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.space3

        Text {
            text: dialog.appCode
            color: Theme.accent
            font.pixelSize: Theme.fontDisplay
            font.family: Theme.headingFamily
            font.weight: Font.ExtraBold
            font.letterSpacing: Theme.trackingTitle
        }

        Text {
            Layout.fillWidth: true
            text: dialog.blurb
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

        ColumnLayout {
            id: extraSection

            Layout.fillWidth: true
            spacing: Theme.space3
        }
    }

    onSecondaryClicked: dialog.showLicences()
    onPrimaryClicked: dialog.close()
}

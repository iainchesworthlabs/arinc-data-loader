import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// What the program is.
Dialog {
    id: dialog

    title: qsTr("About ARINC Data Loader")
    modal: true
    standardButtons: Dialog.Close
    width: 420

    ColumnLayout {
        spacing: 10

        Label {
            text: qsTr("ARINC Data Loader")
            font.pixelSize: 18
            font.bold: true
        }
        Label {
            Layout.fillWidth: true
            text: qsTr("A data loader for ARINC 615A targets and ARINC 665 media sets.")
            wrapMode: Text.Wrap
        }
        Label {
            text: qsTr("Version %1").arg(Qt.application.version.length > 0
                                         ? Qt.application.version : qsTr("1.0"))
            opacity: 0.7
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A modal dialog to the family: a square surface, a 2 px heading rule, 24 px padding and a
// footer with one primary. `content` holds the body; a caller sets `heading`, `primaryText` and
// optionally `secondaryText`, and handles primaryClicked/secondaryClicked.
Dialog {
    id: root

    property string heading: ""
    property string primaryText: qsTr("OK")
    property string secondaryText: ""
    property alias content: body.data

    signal primaryClicked()
    signal secondaryClicked()

    modal: true
    closePolicy: Popup.CloseOnEscape
    padding: Theme.space6
    width: 640
    background: Rectangle {
        color: Theme.surface
        border.color: Theme.divider
        border.width: 1
        radius: Theme.radius
    }

    header: Item {
        implicitHeight: 56

        Text {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: Theme.space6
            text: root.heading
            color: Theme.text
            font.pixelSize: Theme.fontHeading
            font.family: Theme.headingFamily
            font.weight: Font.DemiBold
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 2
            color: Theme.divider
        }
    }

    contentItem: ColumnLayout {
        id: body

        spacing: Theme.space3
    }

    footer: Item {
        implicitHeight: 64

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.space6
            anchors.rightMargin: Theme.space6
            spacing: Theme.space2

            Item {
                Layout.fillWidth: true
            }

            AppButton {
                visible: root.secondaryText.length > 0
                text: root.secondaryText
                onClicked: root.secondaryClicked()
            }

            AppButton {
                primary: true
                text: root.primaryText
                onClicked: root.primaryClicked()
            }
        }
    }
}

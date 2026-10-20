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

    // The standard-button roles a Qt Quick Test or a keyboard user may look for. The dialog has
    // two buttons, not a QDialogButtonBox, so the mapping is explicit: the affirmative roles are
    // the primary (Save / Close / OK) and Cancel is the secondary.
    function standardButton(role) {
        if (role === Dialog.Ok || role === Dialog.Save || role === Dialog.Apply
                || role === Dialog.Close || role === Dialog.Yes)
            return primaryButton;
        if (role === Dialog.Cancel || role === Dialog.No)
            return secondaryButton;
        return null;
    }

    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: Overlay.overlay
    z: 1

    // A modal page reads as a page because the work behind it recedes; the window draws that
    // scrim itself, above its content and below the popup overlay.
    padding: Theme.space6
    width: 640
    background: Rectangle {
        color: Theme.surface
        border.color: Theme.text
        border.width: 2
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
                id: secondaryButton

                visible: root.secondaryText.length > 0
                text: root.secondaryText
                onClicked: root.secondaryClicked()
            }

            AppButton {
                id: primaryButton

                primary: true
                text: root.primaryText
                onClicked: root.primaryClicked()
            }
        }
    }
}

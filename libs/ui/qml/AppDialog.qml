// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A modal dialog to the family: a square surface, a 2 px heading rule, 24 px padding and a
// footer with one primary. `content` holds the body; a caller sets `heading`, `primaryText` and
// optionally `secondaryText`, and handles primaryClicked/secondaryClicked.
//
// Keyboard: the dialog opens with the focus inside it, not on the window behind. By default the
// focus is on the primary button; a dialog with fields names the first one in `initialFocusItem`,
// and one whose primary is not the safe answer (a stop-and-close) sets `initialFocus:
// "secondary"` and `enterAccepts: false`. While `enterAccepts` is true, Return or Enter anywhere in
// the body presses the primary, as a default button does (a field's own Return, a check box's
// toggle and a focused button's press come first). Esc closes the dialog and, when it closes
// by any route, the focus goes back to the control that opened it.
Dialog {
    id: root

    property string heading: ""
    property string primaryText: qsTr("OK")
    property string secondaryText: ""
    property alias content: body.data

    // "primary" or "secondary": the button the focus starts on when initialFocusItem is not set.
    property string initialFocus: "primary"
    // The field or control to start on, when the body has one that should take the first Tab.
    property Item initialFocusItem: null
    // Whether Return or Enter in the body presses the primary.
    property bool enterAccepts: true

    signal primaryClicked()
    signal secondaryClicked()

    readonly property Item primaryItem: primaryButton
    readonly property Item secondaryItem: secondaryButton

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

    property Item _returnFocusTo: null

    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: Overlay.overlay
    z: 1

    onAboutToShow: {
        const window = root.parent ? root.parent.Window.window : null;
        root._returnFocusTo = window ? window.activeFocusItem : null;
    }

    onOpened: {
        let target = root.initialFocusItem;
        if (!target || !target.visible || !target.enabled) {
            target = root.initialFocus === "secondary" && secondaryButton.visible
                     ? secondaryButton : primaryButton;
        }
        target.forceActiveFocus(Qt.TabFocusReason);
    }

    onClosed: {
        const back = root._returnFocusTo;
        root._returnFocusTo = null;
        if (back && back.visible && back.enabled)
            back.forceActiveFocus(Qt.OtherFocusReason);
    }

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

        Keys.onReturnPressed: function(event) {
            if (root.enterAccepts && primaryButton.enabled) {
                primaryButton.click();
                event.accepted = true;
            }
        }
        Keys.onEnterPressed: function(event) {
            if (root.enterAccepts && primaryButton.enabled) {
                primaryButton.click();
                event.accepted = true;
            }
        }
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

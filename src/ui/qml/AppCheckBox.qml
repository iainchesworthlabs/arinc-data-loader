import QtQuick
import QtQuick.Controls as QQC

// A checkbox to the family: a 16 px square that fills with accentFill when checked, a struck
// accentText mark, a wrapped label beside it and an optional note beneath. A Rectangle rather
// than QQC.CheckBox because the standard indicator is not themed by the palette.
Item {
    id: root

    property string text: ""
    property string note: ""
    property bool checked: false
    signal toggled(bool value)

    implicitWidth: Math.max(220, Math.min(560, label.implicitWidth + 26))
    implicitHeight: column.implicitHeight
    opacity: enabled ? 1.0 : 0.45
    activeFocusOnTab: enabled
    Accessible.role: Accessible.CheckBox
    Accessible.name: root.text
    Accessible.checked: root.checked
    Accessible.focusable: root.enabled
    Accessible.onPressAction: if (root.enabled) root.toggled(!root.checked)

    onEnabledChanged: if (!root.enabled && root.activeFocus) root.focus = false;

    // The window tests and keyboard both use this: a click is a toggle.
    function click() {
        if (root.enabled)
            root.toggled(!root.checked);
    }

    Keys.onPressed: function(event) {
        if (!root.enabled)
            return;
        if (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            root.toggled(!root.checked);
            event.accepted = true;
        }
    }

    Column {
        id: column

        width: root.width
        spacing: Theme.space1

        Row {
            spacing: 10

            Rectangle {
                id: box

                width: 16
                height: 16
                anchors.verticalCenter: parent.verticalCenter
                color: root.checked ? Theme.accentFill : "transparent"
                border.color: Theme.divider
                border.width: 1

                Canvas {
                    id: tick

                    anchors.fill: parent
                    visible: root.checked
                    onPaint: {
                        const c = getContext("2d");
                        c.clearRect(0, 0, width, height);
                        c.strokeStyle = Theme.accentText;
                        c.lineWidth = 1.8;
                        c.beginPath();
                        c.moveTo(3, 8.5);
                        c.lineTo(6.5, 12);
                        c.lineTo(13, 4);
                        c.stroke();
                    }
                    Connections {
                        target: Theme
                        function onDarkChanged() { tick.requestPaint(); }
                    }
                }

                FocusRing {
                    active: root.activeFocus
                }
            }

            Text {
                id: label

                width: Math.max(0, root.width - 26)
                text: root.text
                color: Theme.text
                font.pixelSize: Theme.fontNormal
                wrapMode: Text.WordWrap
            }
        }

        Text {
            visible: root.note.length > 0
            width: Math.max(0, root.width - 26)
            x: 26
            text: root.note
            color: Theme.textMuted
            font.pixelSize: Theme.fontSmall
            wrapMode: Text.WordWrap
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.enabled
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: root.toggled(!root.checked)
    }
}

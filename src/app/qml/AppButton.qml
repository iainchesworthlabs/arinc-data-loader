import QtQuick
import QtQuick.Controls

// The window's action button: drawn from the design tokens rather than by the platform style, so
// a primary action (Upload, Download) reads the same on the three desktops under Fusion. It is a
// Button on purpose, so it keeps the keyboard behaviour, the accessible button role and the
// `click()` the window's offscreen test drives.
Button {
    id: control

    // The one action a panel is for (accent fill); every other button is secondary.
    property bool primary: false

    padding: Theme.space3
    implicitWidth: Math.max(120, contentItem.implicitWidth + padding * 2)
    implicitHeight: Math.max(36, contentItem.implicitHeight + padding * 2)
    Accessible.name: text

    background: Rectangle {
        radius: Theme.radiusControl
        color: {
            if (!control.enabled) {
                return control.primary ? Qt.rgba(Theme.muted.r, Theme.muted.g, Theme.muted.b, 0.30)
                                       : "transparent";
            }
            if (control.primary) {
                return control.pressed
                       ? Qt.darker(Theme.accent, 1.15)
                       : control.hovered ? Qt.lighter(Theme.accent, 1.10) : Theme.accent;
            }
            return control.pressed || control.hovered ? Theme.bg : Theme.panel;
        }
        border.width: control.primary ? 0 : 1
        border.color: control.enabled
                      ? Theme.border
                      : Qt.rgba(Theme.border.r, Theme.border.g, Theme.border.b, 0.5)
    }

    contentItem: Text {
        text: control.text
        font.pixelSize: Theme.fontBody
        font.bold: control.primary
        color: !control.enabled
               ? Qt.rgba(Theme.ink.r, Theme.ink.g, Theme.ink.b, 0.45)
               : control.primary ? Theme.accentInk : Theme.ink
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    FocusRing {}
}

import QtQuick
import QtQuick.Layouts

// A compact status token: an uppercase mono word, a 1 px border and, optionally, a filled
// surface. The family carries success as neutral, warning and failure as the accent; the word
// does the work, not a second hue.
Rectangle {
    id: root

    property string text: ""
    // neutral | accent | warn | fail | ok
    property string tone: "neutral"
    property bool dot: true

    readonly property bool filled: tone === "accent" || tone === "fail"
    readonly property color toneColour: {
        if (tone === "warn")
            return Theme.accentInk;
        if (tone === "ok")
            return Theme.neutral900;
        if (tone === "accent" || tone === "fail")
            return Theme.accentText;
        return Theme.textMuted;
    }

    implicitWidth: chip.implicitWidth + Theme.space3 * 2
    implicitHeight: Math.max(20, chip.implicitHeight + 6)
    Layout.minimumWidth: implicitWidth
    Layout.preferredWidth: implicitWidth
    color: filled ? Theme.accentFill : "transparent"
    border.width: 1
    border.color: filled ? Theme.accentFill
                         : tone === "warn" ? Theme.accentInk : Theme.divider
    radius: Theme.radiusControl
    Accessible.role: Accessible.StaticText
    Accessible.name: root.text

    RowLayout {
        id: chip

        anchors.centerIn: parent
        spacing: Theme.space1

        Rectangle {
            visible: root.dot
            width: 4
            height: 4
            radius: 2
            color: root.toneColour
        }

        Text {
            text: root.text.toUpperCase()
            color: root.toneColour
            font.pixelSize: Theme.fontMicro
            font.family: Theme.monoFamily
            font.letterSpacing: 0.8
            font.bold: true
        }
    }
}

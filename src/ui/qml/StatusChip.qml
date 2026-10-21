import QtQuick
import QtQuick.Layouts

// A compact status token: a glyph and an uppercase mono word on a 1 px border. The family has no
// green, so each tone is told apart by its glyph and its surface as well as its ink (Theme.tone*):
//
//   neutral  a dot, muted word, outline only
//   ok       a check, neutral900 word on a neutral200 tint (success is neutral, with a mark)
//   warn     a warning triangle, accent800 word on an accent100 tint
//   fail     a cross, accentText word on an accentFill surface
//   accent   a play mark, accentInk word, accentInk outline: work in progress or a service running
//
// The word is 12 px bold mono: text that carries a state is not set in 9 or 10 px.
Rectangle {
    id: root

    property string text: ""
    // neutral | ok | warn | fail | accent ("accent" is also what the busy states use)
    property string tone: "neutral"
    // false removes the glyph.
    property bool dot: true

    readonly property string glyph: Theme.toneGlyph(root.tone)
    readonly property color toneColour: Theme.toneInk(root.tone)
    readonly property bool filled: Theme.knownTone(root.tone) === "fail"

    implicitWidth: chip.implicitWidth + Theme.space3 * 2
    implicitHeight: Math.max(22, chip.implicitHeight + 6)
    color: Theme.toneFill(root.tone)
    border.width: 1
    border.color: Theme.toneBorder(root.tone)
    radius: Theme.radiusControl
    Accessible.role: Accessible.StaticText
    Accessible.name: root.text

    RowLayout {
        id: chip

        anchors.centerIn: parent
        spacing: Theme.space1 + 2

        ToneGlyph {
            visible: root.dot
            Layout.preferredWidth: 10
            Layout.preferredHeight: 10
            glyph: root.glyph
            color: root.toneColour
        }

        Text {
            text: root.text.toUpperCase()
            color: root.toneColour
            font.pixelSize: Theme.fontSmall
            font.family: Theme.monoFamily
            font.letterSpacing: 0.6
            font.bold: true
        }
    }
}

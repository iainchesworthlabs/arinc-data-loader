import QtQuick
import QtQuick.Layouts

// A line of prose that carries a state: a glyph (triangle for a warning, cross for a failure, check
// for success) and wrapped text in the tone's ink. A warning and an error used to be the same red
// text; here they differ in ink, in glyph and, when the caller says so, in a leading word. A
// neutral tone shows no glyph and reads as ordinary muted text.
RowLayout {
    id: root

    property string text: ""
    // neutral | ok | warn | fail | accent
    property string tone: "neutral"
    // The text size; a message in the rail is small, a message in a panel is body size.
    property int pixelSize: Theme.fontSmall
    // A leading word ("Warning", "Error") so the line reads without colour or glyph; empty for none.
    property string lead: ""

    readonly property bool hasGlyph: Theme.knownTone(root.tone) !== "neutral"

    Layout.fillWidth: true
    spacing: Theme.space2

    ToneGlyph {
        visible: root.hasGlyph
        Layout.preferredWidth: 10
        Layout.preferredHeight: 10
        Layout.alignment: Qt.AlignTop
        Layout.topMargin: Math.max(0, (message.font.pixelSize * 1.2 - 10) / 2)
        glyph: Theme.toneGlyph(root.tone)
        color: Theme.toneTextInk(root.tone)
    }

    Text {
        id: message

        Layout.fillWidth: true
        text: root.lead.length > 0 ? root.lead + ": " + root.text : root.text
        color: root.hasGlyph ? Theme.toneTextInk(root.tone) : Theme.text
        textFormat: Text.PlainText
        font.pixelSize: root.pixelSize
        wrapMode: Text.WordWrap
    }
}

import QtQuick
import QtQuick.Layouts

// An in-window message that stays where it is until the cause goes away or it is dismissed: the
// one idiom for an error, a warning, a note or a confirmation that belongs to the window rather
// than to a modal question. `text` is the sentence, `detail` an optional second line in mono
// (a path, a clause, a count), and anything placed inside is an action on the right (Open folder,
// Retry). The tone sets the rule at the left edge, the surface and a glyph, so a banner is never
// told apart by colour alone (the tones are Theme's: error is `fail`, warning `warn`, success `ok`
// and info `neutral`); the words do the rest.
//
//   error    something failed or cannot continue    cross, accent tint, heavy accent rule
//   warning  allowed, but the operator should look  warning triangle, heavy rule in the warning ink
//   info     a neutral note about what happened     no glyph, neutral surface, light rule
//   success  something finished as intended        check, neutral surface, dark rule
Rectangle {
    id: root

    property string text: ""
    property string detail: ""
    // error | warning | info | success
    property string tone: "error"
    property bool dismissable: false
    default property alias actions: actionRow.data

    signal dismissed()

    // The Theme tone this banner's tone stands for.
    readonly property string themeTone: tone === "error" ? "fail"
                                        : tone === "warning" ? "warn"
                                        : tone === "success" ? "ok" : "neutral"

    readonly property color ruleColour: {
        if (tone === "error")
            return Theme.accentInk;
        if (tone === "warning")
            return Theme.warn;
        if (tone === "success")
            return Theme.ok;
        return Theme.divider;
    }

    implicitHeight: row.implicitHeight + Theme.space3 * 2
    color: tone === "error" ? Theme.accent100 : Theme.neutral100
    border.width: 1
    border.color: tone === "error" ? Theme.accentInk : tone === "warning" ? Theme.warn : Theme.divider
    Accessible.role: Accessible.AlertMessage
    Accessible.name: root.detail.length > 0 ? root.text + " " + root.detail : root.text

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: root.tone === "info" ? 2 : 4
        color: root.ruleColour
    }

    RowLayout {
        id: row

        anchors.fill: parent
        anchors.leftMargin: Theme.space4
        anchors.rightMargin: Theme.space3
        anchors.topMargin: Theme.space3
        anchors.bottomMargin: Theme.space3
        spacing: Theme.space3

        ToneGlyph {
            visible: root.themeTone !== "neutral"
            Layout.preferredWidth: 14
            Layout.preferredHeight: 14
            Layout.alignment: Qt.AlignTop
            Layout.topMargin: 1
            glyph: Theme.toneGlyph(root.themeTone)
            color: Theme.toneTextInk(root.themeTone)
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space1

            Text {
                Layout.fillWidth: true
                text: root.text
                color: Theme.text
                font.pixelSize: Theme.fontBody
                font.family: Theme.headingFamily
                font.weight: Font.DemiBold
                wrapMode: Text.WordWrap
            }

            Text {
                Layout.fillWidth: true
                visible: root.detail.length > 0
                text: root.detail
                color: Theme.textMuted
                font.pixelSize: Theme.fontSmall
                font.family: Theme.monoFamily
                wrapMode: Text.WordWrap
            }
        }

        RowLayout {
            id: actionRow

            spacing: Theme.space2
        }

        AppButton {
            visible: root.dismissable
            text: qsTr("Dismiss")
            onClicked: root.dismissed()
            Accessible.name: qsTr("Dismiss the message")
        }
    }
}

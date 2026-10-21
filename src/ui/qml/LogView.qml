import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The activity log: what the target said and what the loader did, newest last. Time in mono, then
// the severity as a word and a glyph (INFO, NOTE, WARN with a triangle, ERROR with a cross), then
// the message. Severity is never colour alone: warnings read in the warning ink and errors in the
// accent ink as well, but the word is there in a plain log copied to a file.
//
// The log is a tab stop. Up, Down, PageUp, PageDown, Home and End scroll it, and Ctrl+C copies every
// line to the clipboard as "time  SEVERITY  message".
Item {
    id: root

    required property var events
    property bool showTime: true
    readonly property int count: list.count

    // The roles of adl::app::EventModel (Qt::UserRole + 1, + 2, + 3).
    readonly property int severityRole: 257
    readonly property int textRole: 258
    readonly property int timeRole: 259

    function severityWord(severity) {
        switch (severity) {
        case 3: return qsTr("ERROR");
        case 2: return qsTr("WARN");
        case 1: return qsTr("NOTE");
        default: return qsTr("INFO");
        }
    }

    function colourOf(severity) {
        return Theme.toneTextInk(Theme.severityTone(severity));
    }

    // Every line, as plain text: what Ctrl+C puts on the clipboard.
    function plainText() {
        const lines = [];
        const rows = root.events.rowCount();
        for (let i = 0; i < rows; ++i) {
            const at = root.events.index(i, 0);
            const parts = [];
            if (root.showTime)
                parts.push(root.events.data(at, root.timeRole));
            parts.push(root.severityWord(root.events.data(at, root.severityRole)));
            parts.push(root.events.data(at, root.textRole));
            lines.push(parts.join("  "));
        }
        return lines.join("\n");
    }

    function copyAll() {
        clipboard.text = root.plainText();
        clipboard.selectAll();
        clipboard.copy();
    }

    // QML has no clipboard object; a TextEdit that is never shown can copy its own text.
    TextEdit {
        id: clipboard

        visible: false
        Accessible.ignored: true
    }

    KeyScroll {
        id: keyScroll

        flickable: list
    }

    ListView {
        id: list
        objectName: "logList"

        property bool following: true
        property bool scrollQueued: false

        function follow() {
            if (!following)
                return;
            if (scrollQueued)
                return;
            scrollQueued = true;
            Qt.callLater(function() {
                scrollQueued = false;
                if (following)
                    positionViewAtEnd();
            });
        }

        anchors.fill: parent
        clip: true
        model: root.events
        boundsBehavior: Flickable.StopAtBounds
        activeFocusOnTab: true
        ScrollBar.vertical: AppScrollBar {}

        Accessible.role: Accessible.List
        Accessible.name: qsTr("Activity log")
        Accessible.description: qsTr("Up and Down scroll; Ctrl+C copies every line")
        Accessible.focusable: true

        Keys.onPressed: function(event) {
            if (event.matches(StandardKey.Copy)) {
                root.copyAll();
                event.accepted = true;
            } else if (keyScroll.handle(event)) {
                following = atYEnd;
            }
        }

        delegate: RowLayout {
            id: row

            required property string time
            required property string message
            required property int severity

            width: ListView.view.width - Theme.space3
            spacing: Theme.space3

            Text {
                visible: root.showTime
                text: row.time
                color: Theme.textMuted
                font.pixelSize: Theme.fontMono
                font.family: Theme.monoFamily
                Layout.preferredWidth: 62
                Layout.alignment: Qt.AlignTop
            }

            RowLayout {
                Layout.preferredWidth: 58
                Layout.alignment: Qt.AlignTop
                spacing: Theme.space1

                ToneGlyph {
                    Layout.preferredWidth: 10
                    Layout.preferredHeight: 10
                    Layout.alignment: Qt.AlignVCenter
                    visible: row.severity >= 2
                    glyph: Theme.toneGlyph(Theme.severityTone(row.severity))
                    color: root.colourOf(row.severity)
                }

                Text {
                    text: root.severityWord(row.severity)
                    color: row.severity >= 2 ? root.colourOf(row.severity) : Theme.textMuted
                    font.pixelSize: Theme.fontMono
                    font.family: Theme.monoFamily
                    font.bold: row.severity >= 1
                }
            }

            Text {
                Layout.fillWidth: true
                text: row.message
                textFormat: Text.PlainText
                color: root.colourOf(row.severity)
                font.pixelSize: Theme.fontSmall
                wrapMode: Text.Wrap
                Layout.alignment: Qt.AlignTop
            }
        }

        onMovementEnded: following = atYEnd
        onCountChanged: list.follow()
    }

    FocusRing {
        anchors.fill: list
        anchors.margins: -Theme.focusRingOffset
        active: list.activeFocus
    }

    Label {
        anchors.centerIn: parent
        visible: list.count === 0
        text: qsTr("No activity yet.")
        color: Theme.textMuted
        font.pixelSize: Theme.fontBody
    }
}

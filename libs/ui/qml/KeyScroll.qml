import QtQuick

// The keys that scroll a read-only list or log: Up and Down by a line, PageUp and PageDown by a
// page, Home and End to the ends. A ListView that is only read has no key navigation of its own,
// so without this it takes the focus (or not) and then cannot be moved by keyboard.
//
//   ListView { activeFocusOnTab: true; Keys.onPressed: (event) => keyScroll.handle(event) }
//   KeyScroll { id: keyScroll; flickable: list }
QtObject {
    id: root

    required property Flickable flickable
    // Pixels per Up or Down.
    property int line: 24

    // True when the key moved the view (the caller marks the event accepted from the result).
    function handle(event) {
        const view = root.flickable;
        const maxY = Math.max(0, view.contentHeight - view.height) + view.originY;
        const minY = view.originY;
        let target = view.contentY;
        switch (event.key) {
        case Qt.Key_Up: target -= root.line; break;
        case Qt.Key_Down: target += root.line; break;
        case Qt.Key_PageUp: target -= view.height - root.line; break;
        case Qt.Key_PageDown: target += view.height - root.line; break;
        case Qt.Key_Home: target = minY; break;
        case Qt.Key_End: target = maxY; break;
        default: return false;
        }
        view.contentY = Math.max(minY, Math.min(maxY, target));
        view.returnToBounds();
        event.accepted = true;
        return true;
    }
}

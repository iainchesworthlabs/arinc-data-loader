import QtQuick

// Scrolls a view so the item holding the keyboard focus is on screen. A Flickable does not follow
// focus by itself, so Tab into a field below the fold would otherwise leave the operator typing
// into something they cannot see. Put one inside the ScrollView (or Flickable) it serves:
//
//   ScrollView { id: scroll; RevealOnFocus { flickable: scroll.contentItem } ... }
Item {
    id: root

    // The Flickable that scrolls (a ScrollView's contentItem).
    required property Flickable flickable
    // Space left between the focused item and the view's edge once it has been scrolled to.
    property int margin: Theme.space3

    visible: false
    width: 0
    height: 0

    function contains(item) {
        for (let node = item; node !== null && node !== undefined; node = node.parent) {
            if (node === root.flickable.contentItem)
                return true;
        }
        return false;
    }

    function reveal(item) {
        if (!item || !root.contains(item))
            return;
        const top = item.mapToItem(root.flickable.contentItem, 0, 0).y;
        const bottom = top + item.height;
        const view = root.flickable.height;
        const maxY = Math.max(0, root.flickable.contentHeight - view);
        if (top - root.margin < root.flickable.contentY)
            root.flickable.contentY = Math.max(0, top - root.margin);
        else if (bottom + root.margin > root.flickable.contentY + view)
            root.flickable.contentY = Math.min(maxY, bottom + root.margin - view);
    }

    Connections {
        target: root.Window.window

        function onActiveFocusItemChanged() {
            root.reveal(root.Window.window.activeFocusItem);
        }
    }
}

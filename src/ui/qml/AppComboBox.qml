import QtQuick
import QtQuick.Controls as QQC

// A combo box drawn to the family: the same 30 px neutral100 box as AppTextField, a drawn
// chevron instead of the style's image, and a 1 px accent border while focused.
QQC.ComboBox {
    id: control

    implicitHeight: 30
    font.pixelSize: Theme.fontBody

    background: Rectangle {
        color: Theme.neutral100
        border.color: control.activeFocus ? Theme.focusRing : Theme.divider
        border.width: 1
        radius: Theme.radiusControl
    }

    contentItem: Text {
        leftPadding: control.mirrored ? 26 : Theme.space3
        rightPadding: control.mirrored ? Theme.space3 : 26
        text: control.displayText
        color: control.enabled ? Theme.text : Theme.textMuted
        font: control.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Canvas {
        // At the end edge: the right in a left-to-right window, the left in a mirrored one.
        x: control.mirrored ? Theme.space3 : control.width - width - Theme.space3
        y: control.topPadding + (control.availableHeight - height) / 2
        width: 10
        height: 6
        contextType: "2d"

        onPaint: {
            const c = getContext("2d");
            c.reset();
            c.strokeStyle = Theme.textMuted;
            c.lineWidth = 1.5;
            c.beginPath();
            c.moveTo(0, 0);
            c.lineTo(width / 2, height);
            c.lineTo(width, 0);
            c.stroke();
        }
    }

    popup.background: Rectangle {
        color: Theme.surface
        border.color: Theme.divider
        border.width: 1
        radius: Theme.radiusControl
    }

    delegate: QQC.ItemDelegate {
        id: item

        required property int index

        width: control.width
        highlighted: control.highlightedIndex === index

        contentItem: Text {
            // textAt() reads the row whatever the model is: a list of strings, of objects with a
            // role, or an item model such as a QStringListModel.
            text: control.textAt(item.index)
            color: Theme.text
            font.pixelSize: Theme.fontBody
            elide: Text.ElideRight
        }

        background: Rectangle {
            color: item.highlighted ? Theme.neutral200 : "transparent"
        }
    }
}

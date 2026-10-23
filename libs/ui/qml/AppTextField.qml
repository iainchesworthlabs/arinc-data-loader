// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls as QQC

// A text field drawn to the family: 30 px floor, neutral100 fill, a 1 px divider border, mono
// body for identifiers and numbers. The keyboard focus is the 2 px ring outside the box (the
// border change alone was 2.1:1 against the unfocused border in light and 1.3:1 in dark), and the
// border takes the ring colour as well. `unit` draws a muted suffix inside the box.
//
// A placeholder is an example, not a value: it is drawn in the heading face, in italics and in
// the muted tone, never in the mono face that real identifiers and numbers use, so a hint such
// as "e.g. 10.0.0.2" cannot be taken for something that was entered. Write the hint as "e.g. ..."
// at the call site.
//
// `textCommitted` is what a view binds to write the text to its model. It fires on every edit, when
// the field is left or accepted, and when the text is set from outside while the field is not
// being typed in (UI Automation's value pattern and voice control set the text this way, which
// raises neither textEdited nor editingFinished).
QQC.TextField {
    id: control

    property string unit: ""

    signal textCommitted(string value)

    implicitHeight: Math.max(30, contentHeight + 10)
    // The room the unit takes at the end edge: the right in a left-to-right window, the left in a
    // mirrored one.
    readonly property real endPadding: unit.length > 0 ? unitLabel.implicitWidth + Theme.space4
                                                       : Theme.space3
    leftPadding: control.mirrored ? endPadding : Theme.space3
    rightPadding: control.mirrored ? Theme.space3 : endPadding
    color: enabled ? Theme.text : Theme.textMuted
    // The control's own placeholder would use the mono face; the example below replaces it.
    placeholderTextColor: "transparent"
    selectionColor: Theme.accentFill
    selectedTextColor: Theme.accentText
    font.pixelSize: Theme.fontBody
    font.family: Theme.monoFamily
    verticalAlignment: TextInput.AlignVCenter
    selectByMouse: true
    opacity: enabled ? 1.0 : Theme.disabledOpacity

    // The text last handed to textCommitted. Leaving a field does not hand it over again when it
    // has not changed since: a view's model may have moved on (a FIND answer chosen with the mouse
    // changes the address while the field holds what was typed before), and the field's old text
    // must not be written back over it.
    property string _committed: ""

    function _commit() {
        control._committed = control.text;
        control.textCommitted(control.text);
    }

    Component.onCompleted: control._committed = control.text

    onTextEdited: control._commit()
    onEditingFinished: {
        if (control.text !== control._committed)
            control._commit();
    }
    onTextChanged: {
        if (!control.activeFocus)
            control._commit();
    }

    background: Rectangle {
        color: Theme.neutral100
        border.color: control.activeFocus ? Theme.focusRing : Theme.divider
        border.width: 1
        radius: Theme.radiusControl
    }

    FocusRing {
        active: control.activeFocus
    }

    Text {
        id: placeholderLabel

        visible: control.text.length === 0 && control.preeditText.length === 0
        anchors.left: parent.left
        anchors.leftMargin: control.leftPadding
        anchors.right: parent.right
        anchors.rightMargin: control.rightPadding
        anchors.verticalCenter: parent.verticalCenter
        text: control.placeholderText
        color: Theme.textMuted
        font.pixelSize: Theme.fontBody
        font.family: Theme.headingFamily
        font.italic: true
        elide: Text.ElideRight
    }

    Text {
        id: unitLabel

        visible: control.unit.length > 0
        anchors.right: parent.right
        anchors.rightMargin: Theme.space3
        anchors.verticalCenter: parent.verticalCenter
        text: control.unit
        color: Theme.textMuted
        font.family: Theme.monoFamily
        font.pixelSize: Theme.fontSmall
    }
}

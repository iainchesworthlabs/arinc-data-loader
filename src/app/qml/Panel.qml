import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A titled group of controls on its own card: the window's section, as Panel is used everywhere.
// The frame and the section label are drawn from the design tokens, so the three desktops agree,
// while GroupBox keeps the API every use already has (the title and the default property as the
// content). The label is placed by the style at `topPadding - implicitLabelHeight - spacing`, so
// topPadding has to be computed from the label it is given (the Fusion style's own formula); too
// small a number puts the label above the card and clips it.
GroupBox {
    id: root

    Layout.fillWidth: true
    padding: Theme.pad
    spacing: Theme.space1
    topPadding: padding + implicitLabelHeight + spacing

    background: Rectangle {
        color: Theme.panel
        border.width: 1
        border.color: Theme.border
        radius: Theme.radiusCard
    }

    label: Label {
        text: root.title.toUpperCase()
        color: Theme.muted
        font.pixelSize: Theme.fontSmall
        font.bold: true
        font.letterSpacing: 1.1
    }
}

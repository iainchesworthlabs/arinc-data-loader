import QtQuick
import QtQuick.Controls as QQC

// A 4 px progress bar in the family's accent, with a still 30% mark under reduced motion or
// before a figure is known.
QQC.ProgressBar {
    id: control

    implicitHeight: 4
    from: 0
    to: 100

    background: Rectangle {
        color: Theme.neutral300
        height: 4
    }

    contentItem: Item {
        id: track

        property real phase: 0

        Rectangle {
            width: control.indeterminate ? track.width * 0.3
                                         : Math.max(0, track.width * control.visualPosition)
            height: 4
            color: Theme.accent
            x: control.indeterminate
               ? (track.width - width) * track.phase
               : 0
        }

        NumberAnimation on phase {
            running: control.indeterminate && !Theme.reducedMotion && control.visible
            from: 0
            to: 1
            duration: 1200
            loops: Animation.Infinite
        }
    }
}

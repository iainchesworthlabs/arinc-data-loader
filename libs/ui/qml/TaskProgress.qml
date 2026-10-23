import QtQuick
import QtQuick.Controls as QQC

// A 4 px progress bar: the accentFill fill on a neutral300 track (4.4:1 in light, 4.2:1 in dark),
// with a still 30% mark under reduced motion or before a figure is known. It fills from the start
// edge: the left in a left-to-right window, the right in a mirrored one (so the width follows
// `position`, not `visualPosition`, which a mirrored bar already flips, and the bar is placed at
// the start edge itself).
QQC.ProgressBar {
    id: control

    implicitHeight: 4
    from: 0
    to: 100

    background: Rectangle {
        color: Theme.progressTrack
        height: 4
    }

    contentItem: Item {
        id: track

        property real phase: 0

        Rectangle {
            width: control.indeterminate ? track.width * 0.3
                                         : Math.max(0, track.width * control.position)
            height: 4
            color: Theme.progressFill
            // Where along the free space the bar sits, from the start edge: nowhere for a
            // figure, travelling for the indeterminate one.
            readonly property real fromStart: control.indeterminate ? track.phase : 0
            x: (track.width - width) * (control.mirrored ? 1 - fromStart : fromStart)
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

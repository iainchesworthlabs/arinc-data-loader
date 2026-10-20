import QtQuick
import QtQuick.Controls as QQC

// A scroll bar that is there when there is something to scroll to. The style's own bar fades out
// until the pointer moves, which leaves a keyboard or touch operator with no sign that a page
// continues below the fold. This one is drawn whenever the content is longer than the view: a
// 12 px neutral200 track and a neutral600 thumb (3.9:1 against the track in light).
QQC.ScrollBar {
    id: control

    policy: QQC.ScrollBar.AlwaysOn
    visible: control.size < 1.0
    minimumSize: 0.08
    implicitWidth: control.orientation === Qt.Vertical ? 12 : 100
    implicitHeight: control.orientation === Qt.Horizontal ? 12 : 100
    padding: 2

    background: Rectangle {
        color: Theme.neutral200
    }

    contentItem: Rectangle {
        implicitWidth: 8
        implicitHeight: 8
        color: control.pressed ? Theme.neutral800
                               : control.hovered ? Theme.neutral700 : Theme.neutral600
    }
}

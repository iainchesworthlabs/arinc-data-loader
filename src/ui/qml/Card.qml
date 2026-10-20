import QtQuick
import QtQuick.Layouts

// A titled section on its own surface: a 1 px border, 16 px padding, a 2 px header rule and the
// numbered ordinal the family uses to make a page scannable.
//
//   default  the header and the content share one box
//   flat     no box, header and content on the page
//   framed   the header sits above the box and the box wraps only the content
Rectangle {
    id: root

    property alias title: header.label
    property alias summary: header.summary
    property alias ordinal: header.ordinal
    property alias rule: header.rule
    property bool flat: false
    property bool framed: false
    default property alias content: column.data

    readonly property bool _outerBox: !root.flat && !root.framed

    Layout.fillWidth: true
    implicitWidth: 0
    implicitHeight: layout.implicitHeight + (root._outerBox ? Theme.pad * 2 : 0)
    color: root._outerBox ? Theme.surface : "transparent"
    border.color: Theme.divider
    border.width: root._outerBox ? 1 : 0
    radius: Theme.radiusCard
    Accessible.role: Accessible.Grouping
    Accessible.name: root.title

    ColumnLayout {
        id: layout

        anchors.fill: parent
        anchors.margins: root._outerBox ? Theme.pad : 0
        spacing: Theme.gap

        SectionHeader {
            id: header
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: column.implicitHeight + (root.framed ? Theme.pad * 2 : 0)
            color: root.framed ? Theme.surface : "transparent"
            border.color: Theme.divider
            border.width: root.framed ? 1 : 0
            radius: Theme.radiusCard

            ColumnLayout {
                id: column

                anchors.fill: parent
                anchors.margins: root.framed ? Theme.pad : 0
                spacing: Theme.gap
            }
        }
    }
}

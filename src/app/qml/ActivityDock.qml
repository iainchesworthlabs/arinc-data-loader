import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The lower dock: the operator's evidence. Activity and the target's configuration share one
// lane, and the lane can be collapsed so the plan above it gets the height instead.
Item {
    id: root

    required property var app
    property bool expanded: true
    property int tab: 0

    implicitHeight: root.expanded ? 260 : 44

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            color: Theme.surface

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.space4
                anchors.rightMargin: Theme.space4
                spacing: 0

                Repeater {
                    model: [
                        { label: qsTr("ACTIVITY") },
                        { label: qsTr("TARGET CONFIGURATION") }
                    ]

                    delegate: Button {
                        id: tab

                        required property var modelData
                        required property int index

                        text: modelData.label
                        checkable: true
                        checked: root.tab === index
                        padding: Theme.space3
                        implicitHeight: 42
                        Accessible.role: Accessible.RadioButton
                        Accessible.name: modelData.label
                        Accessible.checked: checked

                        background: Rectangle {
                            color: tab.hovered && !tab.checked ? Theme.neutral200 : "transparent"

                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: 3
                                visible: tab.checked
                                color: Theme.accent
                            }
                        }

                        contentItem: Text {
                            text: tab.text
                            color: tab.checked ? Theme.text : Theme.textMuted
                            font.pixelSize: Theme.fontSmall
                            font.family: Theme.headingFamily
                            font.weight: Font.DemiBold
                            font.letterSpacing: 0.6
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: root.tab = index
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                AppButton {
                    visible: root.tab === 0
                    text: qsTr("Clear")
                    onClicked: root.app.clearLog()
                    Accessible.name: qsTr("Clear the activity log")
                }

                AppButton {
                    visible: root.tab === 1
                    text: qsTr("Read information")
                    enabled: root.app.canReadInformation
                    onClicked: root.app.readInformation()
                    Accessible.name: qsTr("Read the target information")
                }

                AppButton {
                    text: root.expanded ? qsTr("Hide") : qsTr("Show")
                    onClicked: root.expanded = !root.expanded
                    Accessible.name: root.expanded ? qsTr("Collapse the evidence panel")
                                                      : qsTr("Expand the evidence panel")
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 2
            color: Theme.divider
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.expanded
            currentIndex: root.tab

            Item {
                LogView {
                    anchors.fill: parent
                    anchors.margins: Theme.space2
                    events: root.app.events
                }
            }

            Item {
                ListView {
                    id: configurationList

                    anchors.fill: parent
                    anchors.margins: Theme.space2
                    clip: true
                    model: root.app.configuration
                    ScrollBar.vertical: ScrollBar {}
                    Accessible.role: Accessible.List
                    Accessible.name: qsTr("Target configuration")

                    delegate: ColumnLayout {
                        id: configurationRow

                        required property int level
                        required property string title
                        required property string detail

                        width: ListView.view.width
                        spacing: 0

                        Text {
                            Layout.fillWidth: true
                            Layout.leftMargin: configurationRow.level === 0 ? 0 : 24
                            Layout.topMargin: configurationRow.level === 0 ? 8 : 0
                            text: configurationRow.title
                            color: Theme.text
                            font.pixelSize: Theme.fontBody
                            font.family: Theme.headingFamily
                            font.weight: configurationRow.level === 0 ? Font.DemiBold : Font.Normal
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            Layout.fillWidth: true
                            Layout.leftMargin: configurationRow.level === 0 ? 0 : 24
                            text: configurationRow.detail
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSmall
                            font.family: Theme.monoFamily
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                EmptyState {
                    anchors.centerIn: parent
                    width: Math.min(420, parent.width - Theme.space6 * 2)
                    visible: configurationList.count === 0
                    title: qsTr("Nothing read yet")
                    body: qsTr("Read information from the target to see its hardware and parts here.")
                }
            }
        }
    }
}

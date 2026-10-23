// SPDX-License-Identifier: GPL-3.0-or-later
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

                // The two tabs are one control, like the transfer switch: one tab stop, Left/Right
                // (and Up/Down, Home, End) move and choose, and a click on the chosen tab leaves it
                // chosen.
                SegmentedControl {
                    objectName: "dockTabs"

                    Layout.alignment: Qt.AlignBottom
                    tabs: true
                    segments: [
                        { value: 0, label: qsTr("ACTIVITY"), name: "dockTabActivity" },
                        { value: 1, label: qsTr("TARGET CONFIGURATION"), name: "dockTabConfiguration" }
                    ]
                    selectedValue: root.tab
                    onSegmentClicked: (value) => root.tab = value
                    Accessible.name: qsTr("Evidence panel")
                }

                Item {
                    Layout.fillWidth: true
                }

                AppButton {
                    objectName: "clearLogButton"

                    visible: root.tab === 0
                    text: qsTr("Clear")
                    onClicked: root.app.clearLog()
                    Accessible.description: qsTr("Clear the activity log")
                }

                AppButton {
                    visible: root.tab === 1
                    text: qsTr("Read information")
                    enabled: root.app.canReadInformation
                    onClicked: root.app.readInformation()
                    Accessible.description: qsTr("Read the target information")
                }

                AppButton {
                    text: root.expanded ? qsTr("Hide") : qsTr("Show")
                    onClicked: root.expanded = !root.expanded
                    Accessible.description: root.expanded ? qsTr("Collapse the evidence panel")
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
                    objectName: "activityLog"

                    anchors.fill: parent
                    anchors.margins: Theme.space2
                    events: root.app.events
                }
            }

            Item {
                ListView {
                    id: configurationList

                    objectName: "configurationList"
                    anchors.fill: parent
                    anchors.margins: Theme.space2
                    clip: true
                    model: root.app.configuration
                    boundsBehavior: Flickable.StopAtBounds
                    activeFocusOnTab: true
                    ScrollBar.vertical: AppScrollBar {}
                    Accessible.role: Accessible.List
                    Accessible.name: qsTr("Target configuration")
                    Accessible.focusable: true

                    // Read-only, so the keys that scroll it are its whole keyboard: Up, Down, Page Up,
                    // Page Down, Home and End.
                    Keys.onPressed: (event) => configurationKeys.handle(event)

                    KeyScroll {
                        id: configurationKeys

                        flickable: configurationList
                    }

                    delegate: ColumnLayout {
                        id: configurationRow

                        required property int level
                        required property string title
                        required property string detail

                        width: ListView.view.width - Theme.space3
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

                FocusRing {
                    anchors.fill: configurationList
                    anchors.margins: -Theme.focusRingOffset
                    active: configurationList.activeFocus
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

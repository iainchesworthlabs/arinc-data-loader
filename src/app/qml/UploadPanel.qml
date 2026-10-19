pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The upload plan: what will be sent, to which target, with every prerequisite shown as a
// check rather than discovered through a disabled button. The action itself is the window's
// fixed bar, not a control at the end of this page.
Item {
    id: root

    required property var app

    signal chooseMediaSetRequested()
    signal checkMediaSetRequested()
    signal findTargetsRequested()

    readonly property string targetName: root.app.thwIdPos.length > 0
                                         ? root.app.thwIdPos
                                         : (root.app.targetAddress.length > 0
                                            ? root.app.targetAddress : qsTr("no target"))

    readonly property string checkText: {
        switch (root.app.validationState) {
        case 1: return qsTr("CHECKING");
        case 2: return qsTr("CHECKED");
        case 3: return qsTr("INVALID");
        default: return qsTr("NOT CHECKED");
        }
    }
    readonly property string checkTone: {
        switch (root.app.validationState) {
        case 1: return "accent";
        case 2: return "ok";
        case 3: return "fail";
        default: return "warn";
        }
    }

    readonly property string nextTitle: {
        if (root.app.busy)
            return qsTr("An operation is running");
        if (root.app.validationState === 1)
            return qsTr("Checking the media set");
        if (root.app.mediaSetPath.length === 0)
            return qsTr("Choose a media set first");
        if (root.app.validationState === 3)
            return qsTr("The media set is not valid");
        if (root.app.validationState !== 2)
            return qsTr("Check the media set");
        if (root.app.selectedLoadCount === 0)
            return qsTr("Tick at least one load");
        if (!root.app.targetValid && !root.app.autoFindBeforeOperation)
            return qsTr("Choose or find a target");
        return qsTr("Ready to upload");
    }
    readonly property string nextBody: {
        if (root.app.busy)
            return qsTr("Wait for it to finish, or stop it from the action bar.");
        if (root.app.validationState === 1)
            return qsTr("Every file and its check value is being read; an upload needs the check to pass.");
        if (root.app.mediaSetPath.length === 0)
            return qsTr("A media set is a directory with one directory per member. It is read and checked before an upload.");
        if (root.app.validationState === 3)
            return root.app.validationError.length > 0
                   ? root.app.validationError
                   : qsTr("Read the set again, or choose another directory.");
        if (root.app.validationState !== 2)
            return qsTr("The set has to be read again and checked before it may be uploaded.");
        if (root.app.selectedLoadCount === 0)
            return qsTr("Use the check boxes in the media set rail. Nothing is sent until at least one load is ticked.");
        if (!root.app.targetValid && !root.app.autoFindBeforeOperation)
            return qsTr("FIND, or type the target's IPv4 address and THW_ID_POS.");
        return qsTr("The target, the set, the ticked loads and the check are all in place.");
    }

    function runNextAction() {
        if (root.app.mediaSetPath.length === 0 || root.app.validationState === 3) {
            root.chooseMediaSetRequested();
        } else if (root.app.validationState !== 2) {
            root.checkMediaSetRequested();
        } else if (!root.app.targetValid && !root.app.autoFindBeforeOperation) {
            root.findTargetsRequested();
        }
    }

    ScrollView {
        id: scroll

        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        Item {
            width: scroll.availableWidth
            implicitHeight: content.implicitHeight + Theme.space6 * 2

            ColumnLayout {
                id: content

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Theme.space6
                spacing: Theme.space6

                // ---- the plan, in one sentence ----
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space1

                    Text {
                        text: qsTr("THE UPLOAD")
                        color: Theme.accentInk
                        font.pixelSize: Theme.fontMicro
                        font.family: Theme.monoFamily
                        font.letterSpacing: 1.2
                        font.bold: true
                    }

                    Text {
                        Layout.fillWidth: true
                        text: root.app.selectedLoadCount === 1
                              ? qsTr("Upload 1 load to %1").arg(root.targetName)
                              : qsTr("Upload %1 loads to %2")
                                    .arg(root.app.selectedLoadCount)
                                    .arg(root.targetName)
                        color: Theme.text
                        font.pixelSize: Theme.fontHeadline
                        font.family: Theme.headingFamily
                        font.weight: Font.ExtraBold
                        wrapMode: Text.WordWrap
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2

                        StatusChip {
                            text: root.app.busy ? qsTr("RUNNING")
                                                : root.app.canUpload ? qsTr("READY") : qsTr("NOT READY")
                            tone: root.app.busy ? "accent" : root.app.canUpload ? "ok" : "warn"
                        }

                        Text {
                            Layout.fillWidth: true
                            text: root.app.mediaSetPartNumber.length > 0
                                  ? root.app.mediaSetPartNumber + " · "
                                    + Theme.quantity(root.app.mediaSetMemberCount,
                                                     qsTr("member"), qsTr("members"))
                                    + " · "
                                    + Theme.quantity(root.app.loadCount,
                                                     qsTr("load"), qsTr("loads"))
                                  : qsTr("No media set selected")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontBody
                            elide: Text.ElideRight
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 2
                    color: Theme.divider
                }

                // ---- preflight ----
                GridLayout {
                    Layout.fillWidth: true
                    columns: 3
                    columnSpacing: Theme.space2
                    rowSpacing: Theme.space2

                    CheckTile {
                        label: qsTr("Target")
                        value: root.app.targetValid ? root.app.thwIdPos
                                                    : root.app.autoFindBeforeOperation
                                                      ? qsTr("FIND on start") : qsTr("Not chosen")
                        detail: root.app.targetValid ? root.app.targetAddress
                                                     : qsTr("FIND or type an address")
                        chipText: root.app.targetValid ? qsTr("SET")
                                                       : root.app.autoFindBeforeOperation
                                                         ? qsTr("AUTO") : qsTr("MISSING")
                        tone: root.app.targetValid ? "ok"
                                                   : root.app.autoFindBeforeOperation ? "warn" : "fail"
                    }

                    CheckTile {
                        label: qsTr("Media set")
                        value: root.app.mediaSetPartNumber.length > 0
                               ? root.app.mediaSetPartNumber : qsTr("Not chosen")
                        detail: root.app.mediaSetPartNumber.length > 0
                                ? Theme.quantity(root.app.mediaSetMemberCount,
                                                 qsTr("member"), qsTr("members"))
                                : qsTr("Choose a directory")
                        chipText: root.checkText
                        tone: root.checkTone
                    }

                    CheckTile {
                        label: qsTr("Loads")
                        value: qsTr("%1 of %2 ticked").arg(root.app.selectedLoadCount)
                               .arg(root.app.loadCount)
                        detail: Theme.quantity(root.app.mediaSetFindings.length,
                                               qsTr("warning from the check"),
                                               qsTr("warnings from the check"))
                        chipText: root.app.selectedLoadCount > 0 ? qsTr("SET") : qsTr("NONE")
                        tone: root.app.selectedLoadCount > 0 ? "ok" : "warn"
                    }
                }

                // ---- the next thing to do, or the all-clear ----
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: nextRow.implicitHeight + Theme.space3 * 2
                    color: root.app.canUpload || root.app.busy ? Theme.neutral100 : Theme.accent100
                    border.color: root.app.canUpload || root.app.busy
                                  ? Theme.divider : Theme.accent300
                    border.width: 1

                    RowLayout {
                        id: nextRow

                        anchors.fill: parent
                        anchors.margins: Theme.space3
                        spacing: Theme.space3

                        Rectangle {
                            Layout.preferredWidth: 24
                            Layout.preferredHeight: 2
                            Layout.alignment: Qt.AlignTop
                            color: root.app.canUpload || root.app.busy
                                   ? Theme.neutral600 : Theme.accentInk
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Text {
                                text: root.nextTitle
                                color: Theme.text
                                font.pixelSize: Theme.fontSubheading
                                font.family: Theme.headingFamily
                                font.weight: Font.DemiBold
                            }

                            Text {
                                Layout.fillWidth: true
                                text: root.nextBody
                                color: Theme.textMuted
                                font.pixelSize: Theme.fontBody
                                wrapMode: Text.WordWrap
                            }
                        }

                        AppButton {
                            visible: !root.app.canUpload && !root.app.busy
                                     && root.app.validationState !== 1
                                     && (root.app.mediaSetPath.length === 0
                                         || root.app.validationState === 3
                                         || root.app.validationState !== 2
                                         || (!root.app.targetValid
                                             && !root.app.autoFindBeforeOperation))
                            text: (root.app.mediaSetPath.length === 0
                                   || root.app.validationState === 3)
                                  ? qsTr("Choose media set")
                                  : root.app.validationState !== 2 ? qsTr("Check the set")
                                                                   : qsTr("Find targets")
                            onClicked: root.runNextAction()
                        }
                    }
                }

                // ---- what will actually be read ----
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space2

                    SectionHeader {
                        label: qsTr("Payload")
                        summary: Theme.quantity(root.app.selectedLoadCount,
                                                qsTr("load"), qsTr("loads"))
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2

                        Text {
                            Layout.preferredWidth: 200
                            text: qsTr("LOAD")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontMicro
                            font.family: Theme.monoFamily
                            font.letterSpacing: 0.8
                        }
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("CONTENTS")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontMicro
                            font.family: Theme.monoFamily
                            font.letterSpacing: 0.8
                        }
                        Text {
                            Layout.preferredWidth: 110
                            horizontalAlignment: Text.AlignRight
                            text: qsTr("STATUS")
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontMicro
                            font.family: Theme.monoFamily
                            font.letterSpacing: 0.8
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: Theme.divider
                    }

                    Repeater {
                        model: root.app.loads

                        delegate: ColumnLayout {
                            id: loadRow

                            required property var model
                            required property string partNumber
                            required property string headerFile
                            required property string detail
                            required property bool checked
                            required property int ratio
                            required property string loadStatus
                            required property bool failed
                            required property string problem

                            Layout.fillWidth: true
                            visible: loadRow.checked
                            spacing: Theme.space1

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.space2

                                Text {
                                    Layout.preferredWidth: 200
                                    text: loadRow.partNumber
                                    color: Theme.text
                                    font.pixelSize: Theme.fontBody
                                    font.family: Theme.monoFamily
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: loadRow.detail
                                    color: Theme.textMuted
                                    font.pixelSize: Theme.fontSmall
                                    elide: Text.ElideRight
                                }

                                StatusChip {
                                    text: loadRow.ratio >= 0
                                          ? loadRow.ratio + "% " + loadRow.loadStatus
                                          : loadRow.failed ? qsTr("FAILED") : qsTr("TICKED")
                                    tone: loadRow.failed ? "fail"
                                                         : loadRow.ratio >= 0 ? "accent" : "neutral"
                                }
                            }

                            TaskProgress {
                                Layout.fillWidth: true
                                visible: loadRow.ratio >= 0
                                value: Math.max(0, loadRow.ratio)
                                indeterminate: false
                            }

                            Text {
                                Layout.fillWidth: true
                                visible: loadRow.problem.length > 0
                                text: loadRow.problem
                                color: Theme.accentInk
                                font.pixelSize: Theme.fontSmall
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        visible: root.app.selectedLoadCount === 0
                        text: qsTr("No load is ticked. The media set rail lists every load and its check box.")
                        color: Theme.textMuted
                        font.pixelSize: Theme.fontBody
                        wrapMode: Text.WordWrap
                    }
                }

                // ---- the receipt of the last operation ----
                Rectangle {
                    Layout.fillWidth: true
                    visible: root.app.resultIsTransfer && !root.app.resultWasDownload
                             && root.app.resultState !== 0 && !root.app.busy
                    implicitHeight: receipt.implicitHeight + Theme.space4 * 2
                    color: Theme.neutral100
                    border.color: Theme.divider
                    border.width: 1

                    ColumnLayout {
                        id: receipt

                        anchors.fill: parent
                        anchors.margins: Theme.space4
                        spacing: Theme.space2

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space2

                            StatusChip {
                                text: root.app.resultState === 1 ? qsTr("COMPLETE") : qsTr("FAILED")
                                tone: root.app.resultState === 1 ? "ok" : "fail"
                            }

                            Text {
                                objectName: "resultLabel"

                                Layout.fillWidth: true
                                text: root.app.resultText
                                color: Theme.text
                                font.pixelSize: Theme.fontSubheading
                                font.family: Theme.headingFamily
                                font.weight: Font.DemiBold
                                wrapMode: Text.WordWrap
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: root.app.resultDetail
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontBody
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Target %1 · %2 · %3")
                                  .arg(root.targetName)
                                  .arg(root.app.mediaSetPartNumber.length > 0
                                       ? root.app.mediaSetPartNumber : qsTr("no media set"))
                                  .arg(Theme.quantity(root.app.selectedLoadCount,
                                                      qsTr("load ticked"),
                                                      qsTr("loads ticked")))
                            color: Theme.textMuted
                            font.pixelSize: Theme.fontSmall
                            font.family: Theme.monoFamily
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
        }
    }
}

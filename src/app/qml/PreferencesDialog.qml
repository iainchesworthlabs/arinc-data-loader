import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// The operator's preferences. The dialog edits a working copy: Save writes it, Cancel and Esc
// leave every stored value as it was. Language is the one exception, and says so, because it
// swaps the whole window's text the moment it is chosen.
//
// Two columns, not a scrolling form: the page is a small, fixed set of choices and a scroll would
// hide the second half of them for no reason.
AppDialog {
    id: dialog

    required property var app

    heading: qsTr("Preferences")
    primaryText: qsTr("Save")
    secondaryText: qsTr("Cancel")
    width: Math.min(880, Overlay.overlay ? Overlay.overlay.width - 48 : 880)
    // The first field takes the first Tab; Return in any field saves.
    initialFocusItem: languageBox.visible ? languageBox : defaultTargetAddressField

    property string draftAddress: ""
    property string draftThwIdPos: ""
    property string draftDownloadDirectory: ""
    property int draftControlPort: 59
    property int draftLogCapacity: 5000
    property bool draftAutoFind: false
    property bool draftValidateOnOpen: true

    readonly property var languages: typeof languageManager !== "undefined"
                                     && languageManager !== null
                                     ? languageManager.availableLanguages()
                                     : []
    readonly property bool hasLanguages: languages.length > 0

    function load() {
        draftAddress = app.defaultTargetAddress;
        draftThwIdPos = app.defaultThwIdPos;
        draftDownloadDirectory = app.downloadDirectory;
        draftControlPort = app.controlPort;
        draftLogCapacity = app.logCapacity;
        draftAutoFind = app.autoFindBeforeOperation;
        draftValidateOnOpen = app.validateOnOpen;
        languageBox.sync();
    }

    function apply() {
        app.defaultTargetAddress = draftAddress;
        app.defaultThwIdPos = draftThwIdPos;
        app.downloadDirectory = draftDownloadDirectory;
        app.controlPort = draftControlPort;
        app.logCapacity = draftLogCapacity;
        app.autoFindBeforeOperation = draftAutoFind;
        app.validateOnOpen = draftValidateOnOpen;
    }

    onOpened: load()

    onPrimaryClicked: {
        apply();
        close();
    }

    onSecondaryClicked: close()

    FolderDialog {
        id: downloadDirectoryDialog

        title: qsTr("Choose the download directory")
        onAccepted: dialog.draftDownloadDirectory = selectedFolder
    }

    content: RowLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: Theme.space6

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: Theme.space4

            SectionHeader {
                label: qsTr("Appearance")
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: Theme.space3
                rowSpacing: Theme.space2

                Text {
                    text: qsTr("Language")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                    visible: dialog.hasLanguages
                }

                AppComboBox {
                    id: languageBox

                    Layout.fillWidth: true
                    visible: dialog.hasLanguages
                    textRole: "name"
                    valueRole: "code"
                    model: dialog.languages
                    Accessible.name: qsTr("Language")

                    function sync() {
                        if (typeof languageManager === "undefined" || languageManager === null)
                            return;
                        const current = languageManager.currentLanguage;
                        for (let i = 0; i < dialog.languages.length; ++i) {
                            if (dialog.languages[i].code === current) {
                                currentIndex = i;
                                return;
                            }
                        }
                    }

                    onActivated: {
                        if (typeof languageManager !== "undefined" && languageManager !== null)
                            languageManager.setLanguage(currentValue);
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("The window follows the desktop's light or dark scheme. Language applies at once; everything else on Save.")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSmall
                wrapMode: Text.WordWrap
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 2
                color: Theme.divider
            }

            SectionHeader {
                label: qsTr("Target")
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: Theme.space3
                rowSpacing: Theme.space2

                Text {
                    text: qsTr("Default address")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                }

                AppTextField {
                    id: defaultTargetAddressField

                    objectName: "defaultTargetAddressField"

                    Layout.fillWidth: true
                    text: dialog.draftAddress
                    placeholderText: qsTr("10.0.0.2")
                    onTextCommitted: (value) => dialog.draftAddress = value
                    Accessible.name: qsTr("Default address")
                    Accessible.description: qsTr("Default target address")
                }

                Text {
                    text: qsTr("Default THW_ID_POS")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                }

                AppTextField {
                    objectName: "defaultThwIdPosField"

                    Layout.fillWidth: true
                    text: dialog.draftThwIdPos
                    placeholderText: qsTr("HNPFMS_L")
                    onTextCommitted: (value) => dialog.draftThwIdPos = value
                    Accessible.name: qsTr("Default THW_ID_POS")
                    Accessible.description: qsTr("Default target hardware identifier and position")
                }
            }

            AppCheckBox {
                objectName: "autoFindSwitch"

                Layout.fillWidth: true
                text: qsTr("Find the target automatically")
                note: qsTr("Before an operation with no target chosen, run FIND and take the only answer; if none or several answer, show the result and wait.")
                checked: dialog.draftAutoFind
                onToggled: (value) => dialog.draftAutoFind = value
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 2
                color: Theme.divider
            }

            SectionHeader {
                label: qsTr("Media set")
            }

            AppCheckBox {
                objectName: "validateOnOpenSwitch"

                Layout.fillWidth: true
                text: qsTr("Check the media set as soon as it is chosen")
                note: qsTr("A set has to pass the check before an upload. With this off, use 'Re-check' in the media set rail.")
                checked: dialog.draftValidateOnOpen
                onToggled: (value) => dialog.draftValidateOnOpen = value
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space2

                Text {
                    text: qsTr("Last media set")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                }

                Text {
                    Layout.fillWidth: true
                    text: dialog.app.lastMediaSetDirectory.length > 0
                          ? dialog.app.lastMediaSetDirectory : qsTr("none")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                    font.family: Theme.monoFamily
                    elide: Text.ElideMiddle
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: Theme.space4

            SectionHeader {
                label: qsTr("Loader")
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: Theme.space3
                rowSpacing: Theme.space2

                Text {
                    text: qsTr("Control port")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                }

                AppSpinBox {
                    objectName: "controlPortSpin"

                    Layout.preferredWidth: 120
                    from: 1
                    to: 65535
                    value: dialog.draftControlPort
                    onValueChanged: dialog.draftControlPort = value
                    Accessible.name: qsTr("Control port")
                    Accessible.description: qsTr("Loader control port")
                }
            }

            Text {
                text: qsTr("Download directory")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSmall
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.space2

                AppTextField {
                    objectName: "downloadDirectoryField"

                    Layout.fillWidth: true
                    text: dialog.draftDownloadDirectory
                    onTextCommitted: (value) => dialog.draftDownloadDirectory = value
                    Accessible.name: qsTr("Download directory")
                }

                AppButton {
                    text: qsTr("Browse...")
                    onClicked: downloadDirectoryDialog.open()
                    Accessible.description: qsTr("Choose the download directory")
                }
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("The directory is where each DNLD_DATA_<THW_ID_POS>_<number> directory is made.")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSmall
                wrapMode: Text.WordWrap
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 2
                color: Theme.divider
            }

            SectionHeader {
                label: qsTr("Log")
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: Theme.space3
                rowSpacing: Theme.space2

                Text {
                    text: qsTr("Activity lines kept")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                }

                AppSpinBox {
                    Layout.preferredWidth: 160
                    from: 100
                    to: 1000000
                    value: dialog.draftLogCapacity
                    onValueChanged: dialog.draftLogCapacity = value
                    Accessible.name: qsTr("Activity lines kept")
                    Accessible.description: qsTr("Activity log capacity")
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 2
                color: Theme.divider
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Save applies these choices. Cancel and Esc leave them as they were.")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSmall
                wrapMode: Text.WordWrap
            }
        }
    }
}

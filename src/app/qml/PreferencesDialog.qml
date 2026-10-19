import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// The operator's preferences. The dialog edits a working copy: Save writes it, Cancel and Esc
// leave every stored value as it was. Language is the one exception, and says so, because it
// swaps the whole window's text the moment it is chosen.
AppDialog {
    id: dialog

    required property var app

    heading: qsTr("Preferences")
    primaryText: qsTr("Save")
    secondaryText: qsTr("Cancel")
    width: 720
    height: 680

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

    content: ScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: parent.width
            spacing: Theme.space4

            Text {
                Layout.fillWidth: true
                text: qsTr("Save applies the choices below. Cancel and Esc leave them as they were. Language applies at once.")
                color: Theme.textMuted
                font.pixelSize: Theme.fontBody
                wrapMode: Text.WordWrap
            }

            SectionHeader {
                label: qsTr("Appearance")
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: Theme.space4
                rowSpacing: Theme.space2

                Text {
                    text: qsTr("Language")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                    visible: dialog.hasLanguages
                }

                AppComboBox {
                    id: languageBox

                    Layout.preferredWidth: 260
                    visible: dialog.hasLanguages
                    textRole: "name"
                    valueRole: "code"
                    model: dialog.languages
                    Accessible.name: qsTr("Language")

                    function sync() {
                        if (typeof languageManager === "undefined")
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
                        if (typeof languageManager !== "undefined")
                            languageManager.setLanguage(currentValue);
                    }
                }

                Text {
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    text: qsTr("The window follows the desktop's light or dark scheme.")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                    wrapMode: Text.WordWrap
                }
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
                columnSpacing: Theme.space4
                rowSpacing: Theme.space2

                Text {
                    text: qsTr("Default address")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                }

                AppTextField {
                    Layout.preferredWidth: 280
                    text: dialog.draftAddress
                    placeholderText: qsTr("10.0.0.2")
                    onTextEdited: dialog.draftAddress = text
                    Accessible.name: qsTr("Default target address")
                }

                Text {
                    text: qsTr("Default THW_ID_POS")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                }

                AppTextField {
                    Layout.preferredWidth: 280
                    text: dialog.draftThwIdPos
                    placeholderText: qsTr("HNPFMS_L")
                    onTextEdited: dialog.draftThwIdPos = text
                    Accessible.name: qsTr("Default target hardware identifier and position")
                }
            }

            AppCheckBox {
                Layout.fillWidth: true
                text: qsTr("Find the target automatically")
                note: qsTr("Before an operation with no target chosen, run FIND and take the only answer; if none or several answer, show the result and wait.")
                checked: dialog.draftAutoFind
                onToggled: dialog.draftAutoFind = checked
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
                Layout.fillWidth: true
                text: qsTr("Check the media set as soon as it is chosen")
                note: qsTr("A set has to pass the check before an upload. With this off, use 'Re-check' in the media set rail.")
                checked: dialog.draftValidateOnOpen
                onToggled: dialog.draftValidateOnOpen = checked
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

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 2
                color: Theme.divider
            }

            SectionHeader {
                label: qsTr("Loader")
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: Theme.space4
                rowSpacing: Theme.space2

                Text {
                    text: qsTr("Control port")
                    color: Theme.textMuted
                    font.pixelSize: Theme.fontSmall
                }

                AppSpinBox {
                    Layout.preferredWidth: 120
                    from: 1
                    to: 65535
                    value: dialog.draftControlPort
                    onValueModified: dialog.draftControlPort = value
                    Accessible.name: qsTr("Loader control port")
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
                        Layout.fillWidth: true
                        text: dialog.draftDownloadDirectory
                        onTextEdited: dialog.draftDownloadDirectory = text
                        Accessible.name: qsTr("Download directory")
                    }

                    AppButton {
                        text: qsTr("Browse...")
                        onClicked: downloadDirectoryDialog.open()
                        Accessible.name: qsTr("Choose the download directory")
                    }
                }

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
                    onValueModified: dialog.draftLogCapacity = value
                    Accessible.name: qsTr("Activity log capacity")
                }
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("The download directory is where each DNLD_DATA_<THW_ID_POS>_<number> directory is made.")
                color: Theme.textMuted
                font.pixelSize: Theme.fontSmall
                wrapMode: Text.WordWrap
            }
        }
    }
}

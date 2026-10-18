import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The operator's preferences. Every field writes through the model (and so to QSettings) as it
// changes; OK closes the dialog, Cancel closes it without changing anything back.
Dialog {
    id: dialog

    required property var app

    title: qsTr("Preferences")
    modal: true
    standardButtons: Dialog.Ok | Dialog.Cancel
    width: 620

    // The languages a catalogue exists for. Until there is a second one, the choice is the one
    // the window was written in (the source strings), which is the honest minimum: adding a
    // language is a .ts file beside the others and an entry in LanguageManager.
    readonly property var languages: typeof languageManager !== "undefined"
                                     ? languageManager.availableLanguages()
                                     : [{ "code": "en", "name": "English" }]

    ColumnLayout {
        spacing: Theme.space2

        Panel {
            title: qsTr("Appearance")
            Layout.fillWidth: true

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: Theme.space2
                rowSpacing: Theme.space1

                Label {
                    text: qsTr("Language")
                    color: Theme.muted
                }
                ComboBox {
                    id: languageBox

                    Layout.preferredWidth: 240
                    textRole: "name"
                    valueRole: "code"
                    model: dialog.languages
                    Accessible.name: qsTr("Language")

                    function sync_to_current() {
                        if (typeof languageManager === "undefined") {
                            return;
                        }
                        const current = languageManager.currentLanguage;
                        for (let i = 0; i < dialog.languages.length; ++i) {
                            if (dialog.languages[i].code === current) {
                                currentIndex = i;
                                return;
                            }
                        }
                    }

                    Component.onCompleted: sync_to_current()
                    onActivated: {
                        if (typeof languageManager !== "undefined") {
                            languageManager.setLanguage(currentValue);
                        }
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("The window is redrawn in the chosen language; the choice is kept")
                }
                Label {
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    text: qsTr("The window follows the desktop's light or dark scheme.")
                    wrapMode: Text.Wrap
                    color: Theme.muted
                }
            }
        }

        Panel {
            title: qsTr("Target")
            Layout.fillWidth: true

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space1

                GridLayout {
                    columns: 2
                    columnSpacing: Theme.space2
                    rowSpacing: Theme.space1

                    Label {
                        text: qsTr("Default address")
                        color: Theme.muted
                    }
                    TextField {
                        Layout.preferredWidth: 260
                        text: dialog.app.defaultTargetAddress
                        placeholderText: qsTr("10.0.0.2")
                        onTextEdited: dialog.app.defaultTargetAddress = text
                        Accessible.name: qsTr("Default target address")
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Prefilled into the address field at start-up, and again when the operator clears the field")
                    }
                    Label {
                        text: qsTr("Default THW_ID_POS")
                        color: Theme.muted
                    }
                    TextField {
                        Layout.preferredWidth: 260
                        text: dialog.app.defaultThwIdPos
                        placeholderText: qsTr("HNPFMS_L")
                        onTextEdited: dialog.app.defaultThwIdPos = text
                        Accessible.name: qsTr("Default target hardware identifier and position")
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Prefilled into the THW_ID_POS field at start-up, and again when the operator clears the field")
                    }
                }
                Switch {
                    text: qsTr("Find the target automatically")
                    checked: dialog.app.autoFindBeforeOperation
                    onToggled: dialog.app.autoFindBeforeOperation = checked
                    Accessible.name: text
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Before an operation with no target chosen, run FIND and, if exactly one target answers, take it and go on; otherwise show the FIND result and wait")
                }
            }
        }

        Panel {
            title: qsTr("Media set")
            Layout.fillWidth: true

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space1

                Switch {
                    text: qsTr("Check the media set as soon as it is chosen")
                    checked: dialog.app.validateOnOpen
                    onToggled: dialog.app.validateOnOpen = checked
                    Accessible.name: text
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("A set has to pass the check before an upload is allowed; with this off, choose 'Check media set' yourself")
                }
                RowLayout {
                    Label {
                        text: qsTr("Last media set")
                        color: Theme.muted
                    }
                    Label {
                        Layout.fillWidth: true
                        text: dialog.app.lastMediaSetDirectory.length > 0
                              ? dialog.app.lastMediaSetDirectory : qsTr("none")
                        elide: Text.ElideMiddle
                        color: Theme.muted
                    }
                }
            }
        }

        Panel {
            title: qsTr("Loader")
            Layout.fillWidth: true

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space1

                GridLayout {
                    columns: 2
                    columnSpacing: Theme.space2
                    rowSpacing: Theme.space1

                    Label {
                        text: qsTr("Control port")
                        color: Theme.muted
                    }
                    SpinBox {
                        from: 1
                        to: 65535
                        value: dialog.app.controlPort
                        onValueModified: dialog.app.controlPort = value
                        editable: true
                        Accessible.name: qsTr("Loader control port")
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("The port the loader listens on (59 unless it has to move); the target is told when it is not 59, which matters when the target runs on this machine")
                    }
                    Label {
                        text: qsTr("Download directory")
                        color: Theme.muted
                    }
                    TextField {
                        Layout.preferredWidth: 320
                        text: dialog.app.downloadDirectory
                        onTextEdited: dialog.app.downloadDirectory = text
                        Accessible.name: qsTr("Download directory")
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Where a download is written: each download makes its own DNLD_DATA_<THW_ID_POS>_<number> directory under this one (created when it is not there)")
                    }
                }
            }
        }

        Panel {
            title: qsTr("Log")
            Layout.fillWidth: true

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: Theme.space2
                rowSpacing: Theme.space1

                Label {
                    text: qsTr("Lines kept")
                    color: Theme.muted
                }
                SpinBox {
                    from: 100
                    to: 1000000
                    value: dialog.app.logCapacity
                    onValueModified: dialog.app.logCapacity = value
                    editable: true
                    Accessible.name: qsTr("Activity log capacity")
                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("The most lines the activity log keeps; older ones are dropped")
                }
            }
        }
    }
}

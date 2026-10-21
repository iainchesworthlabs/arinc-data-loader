import QtQuick

// The language choice every window offers, over the application's LanguageManager (the
// `languageManager` context property main.cpp registers): each language is listed by its own name
// so it can be found whatever language the window currently shows, and choosing one applies it at
// once - the translators, the layout direction and the running window. Hidden when there is no
// manager, as in a test that loads a window without one.
AppComboBox {
    id: control

    readonly property bool available: typeof languageManager !== "undefined"
                                       && languageManager !== null
    readonly property var languages: available ? languageManager.availableLanguages() : []

    visible: available && languages.length > 0
    implicitWidth: 150
    textRole: "name"
    valueRole: "code"
    model: control.languages
    Accessible.name: qsTr("Language")

    // Puts the box on the manager's current language.
    function sync() {
        if (!control.available)
            return;
        const current = languageManager.currentLanguage;
        for (let i = 0; i < control.languages.length; ++i) {
            if (control.languages[i].code === current) {
                control.currentIndex = i;
                return;
            }
        }
    }

    Component.onCompleted: control.sync()

    onActivated: {
        if (control.available)
            languageManager.setLanguage(control.currentValue);
    }

    Connections {
        target: control.available ? languageManager : null

        function onCurrentLanguageChanged() {
            control.sync();
        }
    }
}

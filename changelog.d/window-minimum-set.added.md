**The window's minimum set** ([docs/app.md](docs/app.md)): one design-token file (`qml/Theme.qml`)
is the single source of truth for colour, type and geometry, and the window is built from a small
component library over it (`Panel`, `AppButton`, `SegmentedControl`, `FocusRing`), as the sibling
applications' windows are. A first launch opens on a first-run screen (a default target, the
download directory, and what the window is for) that is stored once passed; the address and
`<THW_ID_POS>` of the last operation are remembered and put back at start-up; and the strings are
in a catalogue (`qt_add_translations`, an English `.ts`) with a `LanguageManager` that persists the
language, follows the system locale and retranslates the running window. The QML is a module
library of its own, so the window's offscreen test loads the same compiled QML the application
does.

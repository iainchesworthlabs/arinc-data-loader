pragma Singleton

import QtQuick

// The design tokens: one source of truth for colour, type and geometry, as the two sibling
// applications (Countdown Solver and forge-gui) keep theirs. Every custom-drawn surface reads a
// token and nothing outside this file knows a colour value.
//
// `dark` is set once by the window from the palette it runs under: Qt's Fusion style draws the
// standard controls from that palette, and the offscreen tests set a dark palette by hand,
// exactly as a dark desktop theme supplies it. Panel, LogView and the first-run screen draw with
// the tokens, so they follow the same scheme without asking the style anything.
QtObject {
    // Set by Main.qml after the window's palette is in place.
    property bool dark: false

    // ---- palette (light / dark) ----
    readonly property color desk: dark ? "#17191d" : "#e9ecf1"  // window backdrop
    readonly property color bg: dark ? "#1e2126" : "#f4f6f9"  // content area behind a card
    readonly property color panel: dark ? "#262a30" : "#ffffff"  // cards
    readonly property color border: dark ? "#3a414b" : "#d5dae2"
    readonly property color ink: dark ? "#e9ecf0" : "#1d2229"  // primary text
    readonly property color muted: dark ? "#a3abb7" : "#5a6473"  // secondary text
    readonly property color accent: dark ? "#6ea8ff" : "#2f5fc4"
    readonly property color accentInk: dark ? "#101827" : "#ffffff"  // text on an accent fill
    readonly property color accentSoft: dark ? "#22314b" : "#e7eefc"  // a quiet accent fill
    readonly property color ok: dark ? "#70d090" : "#1b6e3a"
    readonly property color warn: dark ? "#ffb060" : "#8a5a00"
    readonly property color fail: dark ? "#ff7070" : "#b00020"

    // ---- type ----
    // The four sizes the window uses; a literal pixelSize in a view is a size that cannot
    // follow this file.
    readonly property int fontTitle: 20
    readonly property int fontHeading: 15
    readonly property int fontBody: 14
    readonly property int fontSmall: 12
    readonly property string mono: "Consolas"

    // ---- geometry ----
    readonly property int radiusCard: 10
    readonly property int radiusControl: 6
    readonly property int gap: 8
    readonly property int pad: 16
    readonly property int space1: 4
    readonly property int space2: 8
    readonly property int space3: 12
    readonly property int space4: 16

    // ---- keyboard ----
    // Drawn just outside a control's own border, the way forge-gui's FocusRing does it (WCAG
    // 1.4.11: the ring is not the only cue, and it clears 3:1 against both surfaces).
    readonly property color focusRing: accent
    readonly property int focusRingWidth: 2
    readonly property int focusRingOffset: 2
}

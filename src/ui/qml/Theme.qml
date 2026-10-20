pragma Singleton

import QtQuick

// The one source of colour, type and geometry for the three ARINC applications.
//
// The values are ICL Forge's Signal palette and Modernist scale, so the loader, the generator and
// the mock target read as the same family as Forge: zero radius, a strict 4/8/12/16/24/32 rhythm,
// Archivo headings, monospaced identifiers, 1 px control borders, 2 px section rules and one red
// accent used for shapes; a labelled accent surface uses accentFill/accentText, which is the pair
// measured to carry its label at 4.5:1.
//
// Nothing outside this file may know a hex value. A view names a token and gets the light or dark
// value of it: the scheme follows the desktop, and a test or a screenshot can force the dark set.
QtObject {
    id: theme

    // The offscreen tests and the screenshot runs pin the scheme; the application follows the
    // desktop. Main.qml sets this before the window is shown.
    property bool forcedDark: false
    readonly property bool dark: forcedDark || Application.styleHints.colorScheme === Qt.Dark

    // ---- Signal palette, light and dark -------------------------------------
    // 100 sits nearest the background in both modes and 900 farthest, so a component's rung
    // keeps its contrast intent whichever scheme is active.
    readonly property var _signal: ({
        light: {
            bg: "#f3f2f2", surface: "#eae9e9", text: "#201e1d",
            accent: "#ec3013", fill: "#b82009", onFill: "#ffffff",
            n100: "#f8f4f4", n200: "#eae7e7", n300: "#d7d3d3", n400: "#bab6b6",
            n500: "#9b9797", n600: "#7d7979", n700: "#605d5d", n800: "#444141",
            n900: "#2d2b2b",
            a100: "#fff2ef", a200: "#ffe0d9", a300: "#ffc4b8", a400: "#ff9783",
            a500: "#ff563c", a600: "#dd2b0f", a700: "#ae1800", a800: "#7c1405",
            a900: "#4d170e"
        },
        dark: {
            bg: "#151313", surface: "#1d1a1a", text: "#ece7e5",
            accent: "#ff5c40", fill: "#ff5c40", onFill: "#151313",
            n100: "#1c1919", n200: "#272423", n300: "#353130", n400: "#4b4645",
            n500: "#6b6563", n600: "#8b8583", n700: "#aba4a1", n800: "#cfc8c5",
            n900: "#e8e2df",
            a100: "#2c150f", a200: "#3e1c14", a300: "#58261b", a400: "#8c3520",
            a500: "#cc4629", a600: "#ff5c40", a700: "#ff8368", a800: "#ffac9a",
            a900: "#ffd5cb"
        }
    })
    readonly property var _active: dark ? _signal.dark : _signal.light

    readonly property color bg: _active.bg
    readonly property color surface: _active.surface
    readonly property color text: _active.text
    readonly property color accent: _active.accent
    readonly property color accentFill: _active.fill
    readonly property color accentText: _active.onFill

    readonly property color neutral100: _active.n100
    readonly property color neutral200: _active.n200
    readonly property color neutral300: _active.n300
    readonly property color neutral400: _active.n400
    readonly property color neutral500: _active.n500
    readonly property color neutral600: _active.n600
    readonly property color neutral700: _active.n700
    readonly property color neutral800: _active.n800
    readonly property color neutral900: _active.n900

    readonly property color accent100: _active.a100
    readonly property color accent200: _active.a200
    readonly property color accent300: _active.a300
    readonly property color accent400: _active.a400
    readonly property color accent500: _active.a500
    readonly property color accent600: _active.a600
    readonly property color accent700: _active.a700
    readonly property color accent800: _active.a800
    readonly property color accent900: _active.a900

    // ---- derived tokens ----------------------------------------------------
    // Text at 50% alpha is a control boundary that clears WCAG 1.4.11's 3:1 in both modes; 68%
    // is the muted text recipe that clears 4.5:1 on the background and on a card.
    readonly property color divider: Qt.rgba(text.r, text.g, text.b, 0.5)
    readonly property color textMuted: Qt.rgba(text.r, text.g, text.b, 0.68)

    function luminance(c) {
        function channel(v) { return v <= 0.03928 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4); }
        return 0.2126 * channel(c.r) + 0.7152 * channel(c.g) + 0.0722 * channel(c.b);
    }
    function contrast(foreground, background) {
        const a = foreground.a;
        const over = Qt.rgba(foreground.r * a + background.r * (1 - a),
                             foreground.g * a + background.g * (1 - a),
                             foreground.b * a + background.b * (1 - a), 1);
        const first = luminance(over);
        const second = luminance(background);
        return (Math.max(first, second) + 0.05) / (Math.min(first, second) + 0.05);
    }

    // The accent used as ink: error text, a warning mark, a section ordinal. The accent itself
    // where it already clears 4.5:1, the 700 rung where it does not (Signal light).
    readonly property color accentInk: contrast(accent, bg) >= 4.5 ? accent : accent700
    readonly property color focusRing: accentInk
    readonly property int focusRingWidth: 2
    readonly property int focusRingOffset: 2

    // Semantic status tones. The family does not add a green: success is neutral, warning and
    // failure are the accent, and the word beside the mark carries the meaning.
    readonly property color ok: neutral800
    readonly property color warn: accentInk
    readonly property color fail: accentInk
    readonly property color info: textMuted

    // ---- motion ------------------------------------------------------------
    // System motion preference is not exposed to QML by Qt; the application uses short durations
    // and every animation goes through this so a future preference is one switch.
    property bool reducedMotion: false
    function motionDuration(ms) {
        return reducedMotion ? 0 : ms;
    }

    // ---- spacing and geometry ----------------------------------------------
    readonly property int space1: 4
    readonly property int space2: 8
    readonly property int space3: 12
    readonly property int space4: 16
    readonly property int space6: 24
    readonly property int space8: 32
    readonly property int gap: 12
    readonly property int pad: 16
    // Zero radius is deliberate: do not round a surface or a control.
    readonly property int radius: 0
    readonly property int radiusCard: 0
    readonly property int radiusControl: 0

    // ---- type ---------------------------------------------------------------
    // Every size in the three windows comes from this scale and multiplies fontScale. A literal
    // pixelSize in a view is a size that cannot follow a text-size setting.
    property real fontScale: 1.0
    readonly property int fontTitle: Math.round(22 * fontScale)
    readonly property int fontDisplay: Math.round(40 * fontScale)
    readonly property int fontHeadline: Math.round(26 * fontScale)
    readonly property int fontHeading: Math.round(18 * fontScale)
    readonly property int fontSubheading: Math.round(15 * fontScale)
    readonly property int fontNormal: Math.round(14 * fontScale)
    readonly property int fontBody: Math.round(13 * fontScale)
    readonly property int fontSmall: Math.round(12 * fontScale)
    readonly property int fontMono: Math.round(11 * fontScale)
    readonly property int fontMicro: Math.round(10 * fontScale)
    readonly property int fontFine: Math.round(9 * fontScale)
    readonly property real trackingWide: 1.2 * fontScale
    readonly property real trackingTitle: -0.3 * fontScale

    // Archivo is bundled with each application and registered before the QML engine starts; the
    // fallback keeps a build that has not registered it readable rather than blank. Archivo has no
    // Hebrew letters, so a right-to-left window - the two right-to-left languages, Hebrew and
    // Yiddish, are written in the Hebrew script - is set in the bundled Noto Sans Hebrew, which has
    // Latin letters and digits too. (Chinese and Japanese come from the system's own fonts.)
    readonly property string headingFamily: {
        const families = Qt.fontFamilies();
        if (Qt.application.layoutDirection === Qt.RightToLeft
                && families.indexOf("Noto Sans Hebrew") >= 0)
            return "Noto Sans Hebrew";
        return families.indexOf("Archivo") >= 0 ? "Archivo" : Application.font.family;
    }
    readonly property string monoFamily: {
        const installed = Qt.fontFamilies();
        if (installed.indexOf("Cascadia Mono") >= 0) return "Cascadia Mono";
        if (installed.indexOf("Consolas") >= 0) return "Consolas";
        return "monospace";
    }

    // ---- legacy names -------------------------------------------------------
    // The views written before the shared system used these names. They remain aliases so a view
    // can be moved one section at a time without a second colour vocabulary.
    readonly property color desk: bg
    readonly property color panel: surface
    readonly property color border: divider
    readonly property color ink: text
    readonly property color muted: textMuted
    readonly property color accentSoft: accent100
}

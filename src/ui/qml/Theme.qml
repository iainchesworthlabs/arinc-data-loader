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

    // Semantic status tones. The family does not add a green: success is neutral and warning and
    // failure are the accent family, so the word and a glyph carry the meaning and colour is only
    // ever the second reading. Each tone has its own ink, surface and glyph (see toneInk, toneFill,
    // toneBorder and toneGlyph below); a chip, a log line or a rail message is built from them.
    //
    //   ok      neutral900 ink on a neutral200 tint, a check
    //   warn    accent800 ink on an accent100 tint, a warning triangle
    //   fail    accentText on an accentFill surface, a cross
    //   accent  accentInk outline, a play mark: work in progress or a service running
    //   neutral textMuted outline, a dot
    readonly property color ok: neutral900
    readonly property color warn: accent800
    readonly property color fail: accentInk
    readonly property color info: textMuted

    // The tone a chip, a rail message or a result takes, by name. A name it does not know is
    // neutral. Views never choose a colour for a state; they choose one of these names.
    readonly property var tones: ["neutral", "ok", "warn", "fail", "accent"]
    function knownTone(tone) {
        return tones.indexOf(tone) >= 0 ? tone : "neutral";
    }
    // The ink for the word and glyph on the tone's own chip surface.
    function toneInk(tone) {
        switch (knownTone(tone)) {
        case "ok": return ok;
        case "warn": return warn;
        case "fail": return accentText;
        case "accent": return accentInk;
        default: return textMuted;
        }
    }
    // The ink for text that stands on the page rather than on a chip (a rail message, a log
    // line): a filled tone cannot be its own ink there, so failure reads in accentInk.
    function toneTextInk(tone) {
        return knownTone(tone) === "fail" ? fail : toneInk(tone);
    }
    function toneFill(tone) {
        switch (knownTone(tone)) {
        case "ok": return neutral200;
        case "warn": return accent100;
        case "fail": return accentFill;
        default: return Qt.rgba(0, 0, 0, 0);   // no fill: the page shows through
        }
    }
    function toneBorder(tone) {
        switch (knownTone(tone)) {
        case "warn": return warn;
        case "fail": return accentFill;
        case "accent": return accentInk;
        default: return divider;
        }
    }
    // The shape that says what the word says: a check, a warning triangle, a cross, a play mark
    // for work in progress, or a dot.
    function toneGlyph(tone) {
        switch (knownTone(tone)) {
        case "ok": return "check";
        case "warn": return "warning";
        case "fail": return "cross";
        case "accent": return "busy";
        default: return "dot";
        }
    }
    // The activity log's severities (adl::app::Severity): info, notice, warning, error.
    function severityTone(severity) {
        return severity >= 3 ? "fail" : severity === 2 ? "warn" : "neutral";
    }

    // The selected row of a list is a 2 px accentInk border (6.4:1 against the tint in light, 6.0:1
    // in dark). The 1 px accent300 border it replaced was 1.4:1, and a selection has to be seen
    // without its word.
    readonly property color selectedFill: accent100
    readonly property color selectedBorder: accentInk
    readonly property int selectedBorderWidth: 2

    // A progress bar's fill and track. The fill is the accentFill pair, not the raw accent
    // (2.8:1 against the track); accentFill holds 4.4:1 in light and 5.3:1 in dark.
    readonly property color progressFill: accentFill
    readonly property color progressTrack: neutral300

    // A disabled control is dimmed to this opacity. At 0.65 its label holds 4.5:1 against the page
    // in both schemes (0.45 was 2.75:1 in light) and the dimming is still plain.
    readonly property real disabledOpacity: 0.65

    // ---- motion ------------------------------------------------------------
    // The platform's "reduce motion" preference (Qt 6.12 exposes it as a style hint) stops the busy
    // mark and the indeterminate sweep. Every animation asks reducedMotion or goes through
    // motionDuration(); the tests and the screenshot runs can pin it with forcedReducedMotion.
    property bool forcedReducedMotion: false
    readonly property bool reducedMotion: forcedReducedMotion
        || Application.styleHints.accessibility.motionPreference === Qt.MotionPreference.ReducedMotion
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
    // The left rail of the three workbench windows: one width, one range.
    readonly property int railWidth: 388
    readonly property int railMinWidth: 348
    readonly property int railMaxWidth: 460
    // Zero radius is deliberate: do not round a surface or a control.
    readonly property int radius: 0
    readonly property int radiusCard: 0
    readonly property int radiusControl: 0

    // ---- type ---------------------------------------------------------------
    // Every size in the three windows comes from this scale; a literal pixelSize in a view is a
    // bug. Qt does not expose the platform's text-size setting, so there is no scale factor: the
    // desktop's display scaling is what makes the type larger, and the layouts wrap rather than
    // clip. Text that carries a state is never below fontMono (11).
    readonly property int fontTitle: 22
    readonly property int fontDisplay: 40
    readonly property int fontHeadline: 26
    readonly property int fontHeading: 18
    readonly property int fontSubheading: 15
    readonly property int fontNormal: 14
    readonly property int fontBody: 13
    readonly property int fontSmall: 12
    readonly property int fontMono: 11
    readonly property int fontMicro: 10
    readonly property int fontFine: 9
    readonly property real trackingWide: 1.2
    readonly property real trackingTitle: -0.3

    // The file: URL of a local path, as QUrl::fromLocalFile makes it, for Qt.openUrlExternally. A
    // native path handed to openUrlExternally is parsed as a URL, so "C:\out" becomes the scheme
    // "c" and a path with a space or a # is cut short.
    function fileUrl(path) {
        const text = String(path).split("\\").join("/");
        let prefix = "file://";
        if (text.startsWith("//"))
            prefix = "file:";          // //host/share/dir -> file://host/share/dir
        else if (!text.startsWith("/"))
            prefix = "file:///";       // C:/dir -> file:///C:/dir
        return prefix + encodeURI(text).split("#").join("%23").split("?").join("%3F");
    }

    // A count with its noun, without the "(s)" that reads as a form rather than a sentence.
    function quantity(count, singular, plural) {
        return count + " " + (count === 1 ? singular : plural);
    }

    // Archivo is bundled with each application and registered before the QML engine starts; the
    // fallback keeps a build that has not registered it readable rather than blank.
    readonly property string headingFamily: {
        return Qt.fontFamilies().indexOf("Archivo") >= 0 ? "Archivo" : Application.font.family;
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

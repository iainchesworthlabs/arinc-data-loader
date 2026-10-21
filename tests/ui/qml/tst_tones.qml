import QtQuick
import QtQuick.Controls
import QtTest

// The status vocabulary: every tone has its own glyph, ink and surface, so a state is never colour
// alone; the log says its severity in words; the contrast of what the tones carry holds in both
// schemes; and the helpers the windows share (the file URL of a path) behave.
TestCase {
    id: testCase

    name: "Tones"
    when: windowShown
    width: 600
    height: 400

    property var theme: null

    function make(source) {
        // Created against the window directory, so the singleton Theme and the shared components
        // resolve as they do in the application.
        const item = Qt.createQmlObject(source, testCase, mainQmlUrl)
        verify(item !== null)
        return item
    }

    function initTestCase() {
        theme = make('import QtQuick\nQtObject { property var theme: Theme }').theme
        verify(theme !== null, "Theme did not load")
    }

    function cleanup() {
        theme.forcedDark = false
        theme.forcedReducedMotion = false
    }

    readonly property var tones: ["neutral", "ok", "warn", "fail", "accent"]

    function test_01_every_tone_has_its_own_glyph() {
        const seen = {}
        for (const tone of tones) {
            const glyph = theme.toneGlyph(tone)
            verify(glyph.length > 0)
            verify(!(glyph in seen), tone + " shares the glyph " + glyph + " with " + seen[glyph])
            seen[glyph] = tone
        }
        compare(theme.toneGlyph("ok"), "check")
        compare(theme.toneGlyph("warn"), "warning")
        compare(theme.toneGlyph("fail"), "cross")
        // A name the vocabulary does not know is neutral.
        compare(theme.knownTone("serving"), "neutral")
        compare(theme.toneGlyph("serving"), theme.toneGlyph("neutral"))
    }

    function test_02_warning_and_failure_do_not_share_an_ink_or_a_surface() {
        verify(theme.warn.toString() !== theme.fail.toString(), "warn and fail are one colour")
        verify(theme.toneInk("warn").toString() !== theme.toneInk("fail").toString())
        verify(theme.toneFill("warn").toString() !== theme.toneFill("fail").toString())
        // Only a failure is a filled red chip; a running job and a warning are not.
        compare(theme.toneFill("accent").a, 0)
        verify(theme.toneFill("fail").toString() === theme.accentFill.toString())
        verify(theme.toneFill("warn").toString() !== theme.accentFill.toString())
        // Success is neutral: no hue to speak of.
        const ok = theme.ok
        verify(Math.abs(ok.r - ok.g) < 0.03 && Math.abs(ok.g - ok.b) < 0.03, "ok has a hue: " + ok)
    }

    function test_03_the_severity_words_and_inks_differ() {
        const log = make('import QtQuick\nLogView { events: appModel.events }')
        const words = [0, 1, 2, 3].map(function(s) { return log.severityWord(s) })
        compare(words.length, 4)
        compare(new Set(words).size, 4, "two severities share a word: " + words)
        verify(log.colourOf(2).toString() !== log.colourOf(3).toString(),
               "warning and error lines share an ink")
        verify(log.colourOf(2).toString() !== log.colourOf(0).toString())
        log.destroy()
    }

    function test_04_a_chip_shows_its_tones_glyph_and_surface() {
        for (const dark of [false, true]) {
            theme.forcedDark = dark
            for (const tone of tones) {
                const chip = make('import QtQuick\nStatusChip { text: "word"; tone: "' + tone + '" }')
                compare(chip.glyph, theme.toneGlyph(tone))
                compare(chip.color.toString(), theme.toneFill(tone).toString())
                compare(chip.border.color.toString(), theme.toneBorder(tone).toString())
                compare(chip.toneColour.toString(), theme.toneInk(tone).toString())
                chip.destroy()
            }
        }
    }

    function test_05_no_state_text_is_below_eleven_pixels() {
        verify(theme.fontSmall >= 11)
        verify(theme.fontMono >= 11)
        // The chip's word is set in fontSmall (12) and says so in its anatomy.
        const chip = make('import QtQuick\nStatusChip { text: "ready"; tone: "ok" }')
        function texts(item, found) {
            if (item.text !== undefined && item.font !== undefined && item.font.pixelSize > 0)
                found.push(item.font.pixelSize)
            for (const child of item.children)
                texts(child, found)
            return found
        }
        const sizes = texts(chip, [])
        verify(sizes.length > 0)
        for (const size of sizes)
            verify(size >= 11, "a chip word is set at " + size + " px")
        chip.destroy()
    }

    function test_06_contrast_of_what_the_tones_carry() {
        for (const dark of [false, true]) {
            theme.forcedDark = dark
            const scheme = dark ? "dark" : "light"
            for (const tone of tones) {
                const fill = theme.toneFill(tone).a === 0 ? theme.bg : theme.toneFill(tone)
                const ratio = theme.contrast(theme.toneInk(tone), fill)
                verify(ratio >= 4.5, scheme + " " + tone + " word is " + ratio.toFixed(2) + ":1")
                // The text a tone sets on the page itself (a rail message, a log line).
                const onPage = theme.contrast(theme.toneTextInk(tone), theme.bg)
                verify(onPage >= 4.5, scheme + " " + tone + " page text is " + onPage.toFixed(2) + ":1")
            }
            for (const tone of ["warn", "accent"]) {
                const edge = theme.contrast(theme.toneBorder(tone), theme.bg)
                verify(edge >= 3, scheme + " " + tone + " border is " + edge.toFixed(2) + ":1")
            }
            const selection = theme.contrast(theme.selectedBorder, theme.selectedFill)
            verify(selection >= 3, scheme + " selection border is " + selection.toFixed(2) + ":1")
            const progress = theme.contrast(theme.progressFill, theme.progressTrack)
            verify(progress >= 3, scheme + " progress fill is " + progress.toFixed(2) + ":1")
            const dimmed = Qt.rgba(theme.text.r, theme.text.g, theme.text.b, theme.disabledOpacity)
            const disabled = theme.contrast(dimmed, theme.bg)
            verify(disabled >= 4.5, scheme + " disabled label is " + disabled.toFixed(2) + ":1")
            const focus = theme.contrast(theme.focusRing, theme.bg)
            verify(focus >= 4.5, scheme + " focus ring is " + focus.toFixed(2) + ":1")
        }
    }

    function test_07_reduced_motion_is_honoured_and_can_be_forced() {
        theme.forcedReducedMotion = true
        verify(theme.reducedMotion)
        compare(theme.motionDuration(200), 0)
        theme.forcedReducedMotion = false
        compare(theme.motionDuration(200), theme.reducedMotion ? 0 : 200)
    }

    function test_08_a_native_path_becomes_a_file_url() {
        compare(theme.fileUrl("C:\\out dir\\a#b"), "file:///C:/out%20dir/a%23b")
        compare(theme.fileUrl("D:/Data/DNLD_DATA_HNPFMS_L_1"), "file:///D:/Data/DNLD_DATA_HNPFMS_L_1")
        compare(theme.fileUrl("/home/pat/media set"), "file:///home/pat/media%20set")
        compare(theme.fileUrl("\\\\server\\share\\loads"), "file://server/share/loads")
        compare(theme.fileUrl("/tmp/what?"), "file:///tmp/what%3F")
    }
}

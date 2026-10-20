# The shared design system

`src/ui` is one source of colour, type, geometry and component anatomy for `adl_app`,
`adl_media_gen` and `adl_mock_target`. It is the ICL Forge Modernist system (the Signal palette,
Archivo, zero radius, the 4/8/12/16/24/32 rhythm) applied to an avionics loading console.

A view names a token; it does not name a hex value or a pixel size. The token file is
`src/ui/qml/Theme.qml`. The components are the QML files beside it. `cmake/AdlUi.cmake` lists them
(`ADL_UI_QML_FILES`), gives each a root resource alias and passes them to each application's QML
module, and adds the bundled Archivo faces, so the same source is compiled into each window
without a second type registry. A new component has to be added to that list.

## Colour

The default palette is Signal. Light and dark are both hand-tuned; dark is not an inversion. The
scheme follows the desktop (`Application.styleHints.colorScheme`). Each window has a `renderDark`
property that it assigns to `Theme.forcedDark` when it completes; the window tests set it from
`ADL_DARK=1`, and the applications leave it false.

| Token | Light | Dark | Use |
|---|---|---|---|
| `bg` | `#f3f2f2` | `#151313` | the page behind a surface |
| `surface` | `#eae9e9` | `#1d1a1a` | header, status bar, action bar, dialogs, cards |
| `text` | `#201e1d` | `#ece7e5` | primary text |
| `accent` | `#ec3013` | `#ff5c40` | shapes: rules, the active-tab underline, markers |
| `accentFill` | `#b82009` | `#ff5c40` | a labelled accent surface |
| `accentText` | `#ffffff` | `#151313` | the label on `accentFill` |
| `neutral100` | `#f8f4f4` | `#1c1919` | field and tile fill |
| `neutral200` | `#eae7e7` | `#272423` | hover, the `ok` tint, a scroll track |
| `neutral300` | `#d7d3d3` | `#353130` | pressed, track |
| `neutral400` | `#bab6b6` | `#4b4645` | quiet marks |
| `neutral500` | `#9b9797` | `#6b6563` | disabled marks |
| `neutral600` | `#7d7979` | `#8b8583` | busy mark, scroll bar thumb |
| `neutral700` | `#605d5d` | `#aba4a1` | secondary |
| `neutral800` | `#444141` | `#cfc8c5` | strong secondary |
| `neutral900` | `#2d2b2b` | `#e8e2df` | the strongest neutral; the `ok` ink |
| `accent100` | `#fff2ef` | `#2c150f` | warning tint, selected row |
| `accent200` | `#ffe0d9` | `#3e1c14` | unused |
| `accent300` | `#ffc4b8` | `#58261b` | decoration only (1.4:1 against `accent100`) |
| `accent700` | `#ae1800` | `#ff8368` | accent as ink |
| `accent800` | `#7c1405` | `#ffac9a` | the warning ink |

`Theme` also defines `accent400` to `accent600` and `accent900`. No view uses `neutral400`,
`neutral500`, `accent200` or those accent rungs yet. `neutral900` is the `ok` ink and the window
palette's dark role, `neutral600` to `neutral800` are the scroll bar thumb, `accent800` is the
`warn` ink, `accent100` is the warning tint and the selected-row fill, and `accent300` is
decoration only (no state rests on it).

Derived tokens:

- `divider` = `text` at 50% alpha. It is the 1 px border of a control or card and is the 2 px
  section rule.
- `textMuted` = `text` at 68% alpha, for secondary prose.
- `accentInk` = the accent if it clears 4.5:1 against `bg`, otherwise `accent700`. That is
  `accent700` in light (6.41:1 on `bg`; the raw accent is 3.76:1) and the accent itself in dark
  (6.04:1). Used for error text, ordinals and the focus ring.
- `selectedFill` = `accent100` and `selectedBorder` = `accentInk`, `selectedBorderWidth` 2: the
  chosen FIND answer and the ticked loads. The border clears 3:1 against the fill in both schemes.
- `progressFill` = `accentFill` on `progressTrack` = `neutral300`: 4.4:1 in light and 5.3:1 in dark
  (the raw accent on the track was 2.8:1).
- `disabledOpacity` = 0.65. A disabled control's label then holds 4.5:1 against the page in both
  schemes (it was 2.75:1 at 0.45) and is still plainly dimmed.

Status has one vocabulary, `Theme.tone*`, and there is no green. Colour is only the second reading:
each tone has its own glyph, surface and ink, and the word says which.

| Tone | Glyph | Ink | Surface | Used for |
|---|---|---|---|---|
| `neutral` | dot | `textMuted` | outline | not yet known, not chosen, stopped |
| `ok` | check | `ok` = `neutral900` | `neutral200` tint | set, checked, complete, ready, a finished load |
| `warn` | warning triangle | `warn` = `accent800` | `accent100` tint, `warn` border | missing but recoverable, a check warning |
| `fail` | cross | `accentText` on `accentFill` (as page text, `accentInk`) | filled `accentFill` | failed, invalid, an error |
| `accent` | play mark | `accentInk` | outline, `accentInk` border | running, checking, serving |

`info` = `textMuted`. A filled red surface therefore means a failure (and, separately, a primary
button); a running job, a warning and a finished load each look different from it and from each
other. `Theme.toneGlyph`, `toneInk`, `toneTextInk`, `toneFill`, `toneBorder` and `severityTone` are
the only way a view chooses them; any other tone name draws as `neutral`. `StatusChip`, the
`ToneGlyph` and `ToneMessage` components, and the activity log are built from them.

`Theme` keeps the names used before the shared system (`desk`, `panel`, `border`, `ink`, `muted`,
`accentSoft`) as aliases of `bg`, `surface`, `divider`, `text`, `textMuted` and `accent100`. No
view uses them now. `radiusCard`, `radiusControl`, `gap` (12) and `pad` (16) are also defined.

Rules:

1. `accent` never carries a label. Signal light is 4.20:1 on white; `accentFill`/`accentText` is
   the measured pair (6.46:1 for the light fill, 6.04:1 for the dark one).
2. No second hue. Status is a word, a glyph and a tone, not a traffic light, and never colour alone.
3. The only circle is the neutral dot glyph (a 4 px disc inside a 10 px mark). Everything else is
   square.

## Type

Archivo (SIL OFL 1.1) is bundled and registered before the engine starts (`register_fonts()` in
`src/ui/fonts.cpp`, which also makes Archivo the application font); the platform UI face is the
fallback if registration fails. Identifiers, numbers and ordinals are Cascadia Mono → Consolas →
monospace.

| Token | Base px | Use |
|---|---|---|
| `fontDisplay` | 40 | first-run statement, about mark |
| `fontHeadline` | 26 | the plan line, the operation line |
| `fontTitle` | 22 | the header mark |
| `fontHeading` | 18 | dialog heading |
| `fontSubheading` | 15 | empty-state title, receipt line, media set summary |
| `fontNormal` | 14 | checkbox label, tile value |
| `fontBody` | 13 | controls, prose |
| `fontSmall` | 12 | secondary text, section labels, log messages |
| `fontMono` | 11 | rail labels and ordinals, status bar, timestamps |
| `fontMicro` | 10 | eyebrows and tile labels (never a state) |
| `fontFine` | 9 | reserved; no view uses it |

`trackingWide` = 1.2 px; `trackingTitle` = −0.3. Uppercase micro labels carry 0.6–1.4 px of
tracking, set as literals in the views. A literal `pixelSize` in a view is a bug (none remains in
`src/*/qml`). There is no scale factor: Qt does not expose the platform's text-size setting, so the
type follows the desktop's display scaling only, and the layouts wrap or scroll rather than clip.
Text that carries a state is never smaller than `fontMono` (11): a chip's word is `fontSmall`
(12, bold mono), and `fontMicro` is for labels and eyebrows that name a field, not a state. A
control's height is a floor (30 px for buttons, fields and combos, 32 for a spin box, 22 for a
chip) that grows with its label; the chrome bands under Geometry are fixed.

## Geometry

- Spacing: `space1` 4, `space2` 8, `space3` 12, `space4` 16, `space6` 24, `space8` 32.
- `radius` 0. Do not round a surface or a control.
- Borders: 1 px on controls, cards, tiles and the rule above the status bar and the action bar;
  2 px section rules, the rule under the header and under the transfer strip, the rule between the
  rail and the work area, the border of a dialog and of the chosen row of a list; 3 px active-tab
  underline (the activity dock's tabs).
- Window: all three open at 1280×900. The loader's minimum is 1000×620, so that the 76 px action
  bar stays on screen on a 1366×768 display at 125% scaling and on a 1920×1080 display at 150%
  (the generator and the mock target keep 1120×760 until they are brought down too).
  `install_window_state()` (`src/ui/window_state.cpp`) restores the saved size, clamped between the
  minimum and the screen's available area less 48 px wide and 80 px high, and restores the saved
  position only if a usable part of the window's title bar (96×24 px of its top 32 px) still lies
  on a screen's available area; otherwise the window is centred horizontally and placed a third of
  the way down. Coordinates are read as signed numbers, so a window on a monitor to the left of
  the primary one is reopened there. A maximised window is restored maximised.
- Rail: the loader's and the generator's is 388 px preferred, 348–460; the mock target's is 396
  preferred, 352–460. The rail scrolls itself and a 2 px rule separates it from the work area.
- Fixed bands: header 64, status bar 28, the loader's transfer strip 52, the loader's and the
  generator's action bar 76. The action bar never scrolls. The loader's activity dock is 44 px
  collapsed and, open, 26% of the window height between 150 and 260 px; it starts collapsed in a
  window shorter than 700 px. What is left is the plan (about 355 px at the minimum size with the
  dock collapsed, 240 px with it open); a page that is taller than that scrolls, and says so with a
  scroll bar (`AppScrollBar`) that is drawn whenever there is something to scroll to.
- Targets: a pointer target is at least 24×24 px. A check box row is 24 px high, a spin box's
  steppers are 30 px wide and as tall as the box.

## Components

| Component | Anatomy |
|---|---|
| `AppButton` | 30 px floor, 1 px divider, transparent secondary, `accentFill` primary (no border, DemiBold label), hover and pressed shades, `disabledOpacity` when disabled, 2 px focus ring outside. Its accessible name is its text; a longer phrase goes in `Accessible.description`. Return and Enter press it as Space does. |
| `AppCheckBox` | 16 px square in a row at least 24 px high (the whole row is the target), `accentFill` when checked, struck `accentText` mark, label 14, optional note 12 (its accessible description), 2 px ring around the square. `click()` for the window tests. |
| `AppTextField` | 30 px floor, `neutral100`, 1 px divider, mono 13, optional unit suffix, the 2 px focus ring outside the box (and the border in `focusRing`). `textCommitted(value)` is what a view binds to its model: it fires on every edit, when the field is left or accepted, and when the text is set from outside while the field is not being typed in (UI Automation, voice control). |
| `AppComboBox` | 30 px floor, `neutral100`, 1 px divider, drawn chevron, popup `surface`, highlighted row `neutral200`, 2 px focus ring outside the box. |
| `AppSpinBox` | 32 px, `neutral100`, mono value without digit grouping, two steppers side by side on the right (− then +), each 30 px wide and as tall as the box. The value editor carries the control's accessible name; 2 px focus ring. |
| `SegmentedControl` | Row of 30 px floor, 90 px minimum segments; the chosen one `accentFill` with an `accentText` label, the rest transparent with a 1 px divider. Each segment is a button (`objectName` from the segment's `name`). One tab stop for the group (the chosen segment); Left/Right (swapped when mirrored), Up/Down, Home and End choose the neighbour and move the focus onto it, wrapping; a click on the chosen segment does not clear it. `tabs: true` draws it as a 42 px tab strip (the activity dock): no boxes, the chosen tab underlined by 3 px of accent, roles `PageTabList` and `PageTab`. |
| `AppHeader` | 64 px on `surface`, 4×30 accent bar, mark (`appCode`, 22 px ExtraBold: `ADL`, `GEN` or `MOCK`), product name and uppercase mono 10 subtitle, right-hand actions, 2 px bottom rule. |
| `AppStatusBar` | 28 px on `surface`, 1 px top rule; the window supplies the children left to right. Each window puts a 10 px state mark (a `ToneGlyph`: busy, cross, check or dot) and a mono 11 state word first and `v<version>` last. |
| `RailBlock` | mono 11 `accentInk` ordinal, uppercase DemiBold 11 label with 1.3 px tracking, 2 px rule, content column at 12 px gaps. |
| `SectionHeader` | optional mono 12 ordinal, uppercase bold 12 label, 2 px rule (`rule: false` hides it), mono 12 summary; title and summary elide. Used in the dialogs, the upload and download panels and the mock target's right-hand pane. |
| `Card` | `surface`, 1 px border, 16 px padding; default box, `flat` or `framed`. No window uses it at present. |
| `StatusChip` | 22 px floor, 1 px border, a 10 px `ToneGlyph` and the word in bold mono 12 uppercase with 0.6 px tracking (`dot: false` removes the glyph). The tone chooses the glyph, ink, fill and border (see the tone table under Colour): `neutral` (default), `ok`, `warn`, `fail`, `accent`. Any other tone draws as `neutral`. |
| `CheckTile` | `neutral100`, 1 px border, uppercase micro label, bold mono 14 value (`Not set` when empty), 12 px detail that wraps to two lines before it elides, chip; used for the upload preflight. The tiles sit three across when the page is 720 px wide or more, and one per row below that. |
| `StatTile` | micro label over a bold mono 14 value; used in the media set properties. |
| `EmptyState` | `neutral100`, 1 px border, 24×2 accent rule, 15 px title, one sentence, actions. |
| `TaskProgress` | 4 px `progressTrack` track, `progressFill` fill; indeterminate is a 30% segment that sweeps, and stays at the left, still, under reduced motion. |
| `BusyMark` | three 5 px `neutral600` squares phased; still at full strength under reduced motion. |
| `LogView` | mono 11 time in a 62 px column (`showTime`), then the severity as a word (INFO, NOTE, WARN, ERROR) with the tone glyph for a warning or an error, then the 12 px message in the tone's page ink. A tab stop with a 2 px ring: Up, Down, Page Up, Page Down, Home and End scroll it, Ctrl+C copies every line as `time  SEVERITY  message`. Follows the end until the operator scrolls away from it; says "No activity yet." when empty. |
| `FocusRing` | 2 px `focusRing`, 2 px outside the control, z 100. Fields, combos, spin boxes, check boxes, buttons, segments, FIND answers and the scrolling lists all draw it; a list that clips draws it inside its edge. |
| `AppDialog` | 2 px `text` border on `surface`, 24 px padding, 640 px wide unless the dialog sets a width, 56 px heading band with a 2 px rule, 64 px footer with one primary and an optional secondary to its left. Content is a named `content` property. `standardButton(role)` maps the affirmative roles to the primary and Cancel and No to the secondary, for the window tests. It opens with the focus inside it: on `initialFocusItem` (the first field), else on the primary, or on the secondary with `initialFocus: "secondary"`. Return or Enter in the body presses the primary unless `enterAccepts` is false; Esc closes it; on close the focus returns to the control that opened it. |
| `ToneGlyph` | A 12 px canvas that draws the tone's shape: `check`, `warning` (triangle with a bar), `cross`, `busy` (play mark) or `dot`. Drawn as paths, since no bundled face carries them. |
| `ToneMessage` | A line of prose with its tone glyph and ink; a neutral tone shows no glyph. Rail and dialog warnings and errors use it, so a warning no longer reads as an error. |
| `AppScrollBar` | A 12 px `neutral200` track with a `neutral600` thumb, drawn whenever the content is longer than the view. |
| `RevealOnFocus` | Scrolls a `ScrollView` to the control that takes the keyboard focus. |
| `KeyScroll` | The Up, Down, Page and Home/End handling of a read-only list. |
| `LicencesDialog` | An `AppDialog` ("Third-party licences", Close) showing the embedded `NOTICES.txt` in a read-only mono 12 text area, at most 780×660. Opened from About > Licences in all three windows; says so in a sentence if the build carries no notices. |

Rules for composition:

- One primary per region. In the loader and the generator the primary lives in the fixed action
  bar; the mock target has no action bar and keeps Start in the header. Secondary actions are
  outlined.
- A modal is for a decision, not for a message. Errors use the dialog only when the operation
  failed; routine state is inline.
- Empty states say what will appear and how to make it appear. They are not a blank rectangle.
- Tables have a header row, 1 px rules and a stable column order; the first column is the object's
  identity.
- A destructive or mode-changing choice is disabled while the mode is live (the mock target's
  injections while serving), and its state is a chip, not a colour.

## Focus, keyboard and motion

- Every control is a tab stop when it can be pressed; a disabled control is not. The keyboard can
  do everything the pointer can (principle 9), and the focus is always visible.
- Buttons: Space, Return and Enter. Check boxes: Space, Return and Enter. A group that is one
  choice (the transfer switch, the dock tabs, the FIND answers) is one tab stop, the chosen item;
  Left/Right (swapped in a right-to-left layout), Up/Down, Home and End choose the neighbour and
  move the focus onto it, wrapping at the ends, and Enter or Space chooses the focused FIND answer.
  A click on the chosen tab leaves it chosen.
- A list that is only read (the activity log, the target configuration, the media set's loads and
  files) is a tab stop that scrolls with Up, Down, Page Up, Page Down, Home and End; the log also
  copies with Ctrl+C. A page that scrolls follows the focus (`RevealOnFocus`).
- Dialogs: they open with the focus inside them and give it back to the control that opened them
  when they close. The first field takes the first Tab in Preferences; the primary holds the focus
  in About, the properties dialogs and the error dialog; "Keep working" holds it in the
  stop-and-close confirmation, which is the one dialog whose primary is not the safe answer. Return
  or Enter in a dialog's body presses its primary (`enterAccepts`), except there. Esc closes the
  dialog and, for the error dialog, acknowledges the failure exactly as OK does.
- Shortcuts: the loader has Preferences (`StandardKey.Preferences`), Quit and Ctrl+F (Find); the
  generator and the mock target have Quit.
- Every control shows focus as the 2 px ring outside it (fields and combos also take the border in
  `focusRing`). The ring is `accentInk`; width 2, offset 2 (6.4:1 against the page in light and
  6.0:1 in dark; the 1 px border change it replaces was 2.1:1 and 1.3:1 against the unfocused
  border). It is never the only cue for state.
- Names: an accessible name is the text on the screen (a field's is its label: "THW_ID_POS", not
  "hardware identifier"), so a person who says what they see is understood; the longer phrase is
  the `Accessible.description`. A spin box's editor carries the box's name.
- `Theme.motionDuration()` is the one switch for animation; `Theme.reducedMotion` follows the
  platform's "reduce motion" preference (`Application.styleHints.accessibility.motionPreference`,
  Qt 6.12) and stops the busy mark and task progress motion without removing the still mark.
  `Theme.forcedReducedMotion` pins it for a test or a screenshot.
- The modal dialog traps focus as Qt's `Dialog` does; Esc closes it (the only close policy;
  clicking outside does nothing). Each window dims its content with one scrim item while a dialog
  is open (black at 32% in light, 62% in dark, above the content and below the popup overlay), and
  the dialog itself carries a 2 px border so it reads as a page rather than a floating panel. The
  scrim shows when any dialog named in the window's `modalOpen` property is open, so a new dialog
  has to be added to that list.
- Preferences is a two-column page, not a scrolling form: the set of choices is small enough to
  keep visible, and a scroll would hide half of it.

## Window chrome and first run

The header is the product's identity and the window's own actions. The loader has a readiness chip
(SETUP, RUNNING, FAILED, COMPLETE, READY or NOT READY: the same state as the transfer strip's chip
and the status bar, so they cannot disagree), Preferences and About. The generator has a chip (GENERATING,
GENERATED, READY or INCOMPLETE) and About. The mock target has a SERVING or STOPPED chip, Start,
Stop and About. The footer is the state line: the loader's is state (the running activity,
Complete, Failed or Idle), target, media set and version; the generator's is state, output
directory and version; the mock target's is state, ports, derived `THW_ID_POS` and version. The
first run, in the loader only, is the family shape: a statement, one primary ("Start loading"), one optional route ("Choose a
media set...") and a numbered mental model beside it. The defaults are folded away behind "Set up
defaults"; the workbench is reachable without answering a form.

## Implementation notes

- Shared QML is source-shared, not a separate module: each app's `qt_add_qml_module` compiles the
  files with aliases (`ArincDataLoader`, `ArincMediaGen` and `ArincMockTarget`).
  `adl_prepare_ui_qml()` must run in the app's directory before `qt_add_qml_module`;
  source-file properties are directory-scoped. It also marks `Theme.qml` as a singleton.
- `adl_add_ui_support(target)` adds the Archivo resources, the third-party notices
  (`:/adl/notices/NOTICES.txt`, written once per configure by `adl_generate_ui_notices()`),
  `fonts.cpp`, `window_state.cpp` and `version.cpp` (the About provenance), and the include
  directories, including the generated `build_info.hpp`.
- Fonts: `Archivo-Regular/Medium/SemiBold/ExtraBold.ttf`, licence `OFL.txt` beside them, in
  `src/ui/fonts`.
- Window geometry uses `QSettings` (the application's default settings) under `window/x`,
  `window/y`, `window/width`, `window/height`, `window/maximized` and `window/placed`, written when
  the window closes and when the application quits.
- QML tests load the compiled module (`loadFromModule`) and register the fonts, so the test sees
  the same module and the same typeface as the application.

# The shared design system

`src/ui` is one source of colour, type, geometry and component anatomy for `adl_app`,
`adl_media_gen` and `adl_mock_target`. It is the ICL Forge Modernist system (the Signal palette,
Archivo, zero radius, the 4/8/12/16/24/32 rhythm) applied to an avionics loading console. ICL Forge
is a separate application whose code is not in this repository.

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
| `accent300` | `#ffc4b8` | `#58261b` | the 1 px border of the mock target's armed-fault summary (1.4:1 against `accent100`, light) |
| `accent700` | `#ae1800` | `#ff8368` | accent as ink |
| `accent800` | `#7c1405` | `#ffac9a` | the warning ink |

`Theme` also defines `accent400` to `accent600` and `accent900`. No view uses `neutral400`,
`neutral500`, `accent200` or those accent rungs yet. `neutral900` is the `ok` ink and the window
palette's dark role, `neutral600` to `neutral800` are the scroll bar thumb, `accent800` is the
`warn` ink, `accent100` is the warning tint and the selected-row fill, and `accent300` is used in
one place, the border of the mock target's armed-fault summary. That border is not the only sign
of the state: the summary also carries the `N ACTIVE` chip and the header and footer show the fault
count.

Derived tokens:

- `divider` = `text` at 50% alpha. It is the 1 px border of a control or card and is the 2 px
  section rule.
- `textMuted` = `text` at 68% alpha, for secondary prose.
- `accentInk` = the accent if it clears 4.5:1 against `bg`, otherwise `accent700`. That is
  `accent700` in light (6.41:1 on `bg`; the raw accent is 3.76:1) and the accent itself in dark
  (6.04:1). Used for error text, ordinals and the focus ring.
- `selectedFill` = `accent100` and `selectedBorder` = `accentInk`, `selectedBorderWidth` 2: the
  chosen FIND answer and the ticked loads. The border clears 3:1 against the fill in both schemes.
- `progressFill` = `accentFill` on `progressTrack` = `neutral300`: 4.4:1 in light and 4.2:1 in dark
  (the raw accent on the track was 2.8:1 in light; in dark the accent and the fill are the same
  colour).
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
3. The circles are the neutral dot glyph (a 4 px disc inside a 10 px mark, drawn by `ToneGlyph` in a
   chip and in the loader's status bar) and the 6 px round dot at the start of the status bar of the
   generator and the mock target. Everything else is square.

## Type

Archivo (SIL OFL 1.1) is bundled and registered before the engine starts (`register_fonts()` in
`src/ui/fonts.cpp`, which also makes Archivo the application font); the platform UI face is the
fallback if registration fails. Identifiers, numbers and ordinals are Cascadia Mono → Consolas →
monospace.

Archivo has no Hebrew letters. In Hebrew and Yiddish `LanguageManager` makes the bundled **Noto Sans
Hebrew** (SIL OFL 1.1; four static weights cut from the variable font) the application font and
`Theme.headingFamily` follows the layout direction, so a right-to-left window is set in it
throughout (it has Latin letters and digits too). Chinese and Japanese are drawn in the system's own
CJK fonts; none is bundled ([translations](translations.md#typefaces)).

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
- Rail: one width for all three windows, `Theme.railWidth` 388 px preferred, `Theme.railMinWidth`
  348 to `Theme.railMaxWidth` 460. The rail scrolls itself and a 2 px rule separates it from the
  work area.
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
| `AppTextField` | 30 px floor, `neutral100`, 1 px divider, mono 13, optional unit suffix, the 2 px focus ring outside the box (and the border in `focusRing`). `textCommitted(value)` is what a view binds to its model: it fires on every edit, when the field is left or accepted, and when the text is set from outside while the field is not being typed in (UI Automation, voice control). A placeholder is an example, not a value: it is drawn in the heading face, italic and muted (never in the mono face of real values) and is written "e.g. ..." at the call site, so `e.g. 10.0.0.2` cannot be mistaken for an address that was entered. |
| `AppComboBox` | 30 px floor, `neutral100`, 1 px divider, drawn chevron, popup `surface`, highlighted row `neutral200`, 2 px focus ring outside the box. |
| `LanguagePicker` | An `AppComboBox` 150 px wide that lists each of the eight languages by its own name, so it can be found in any language, and applies the choice at once through the `languageManager` context property (translators, layout direction, typeface and the running window). Its accessible name is "Language". It is hidden when no language manager is registered, as in a test that loads a window without one. The generator and the mock target put it in the header, beside About; the loader's is in Preferences. |
| `AppSpinBox` | 32 px, `neutral100`, mono value without digit grouping, two steppers side by side on the right (− then +), each 30 px wide and as tall as the box. The value editor carries the control's accessible name; 2 px focus ring. |
| `SegmentedControl` | Row of 30 px floor, 90 px minimum segments; the chosen one `accentFill` with an `accentText` label, the rest transparent with a 1 px divider. Each segment is a button (`objectName` from the segment's `name`). One tab stop for the group (the chosen segment); Left/Right (swapped when mirrored), Up/Down, Home and End choose the neighbour and move the focus onto it, wrapping; a click on the chosen segment does not clear it. `tabs: true` draws it as a 42 px tab strip (the activity dock): no boxes, the chosen tab underlined by 3 px of accent, roles `PageTabList` and `PageTab`. |
| `AppHeader` | 64 px on `surface`, 4×30 accent bar, mark (`appCode`, 22 px ExtraBold: `ADL`, `GEN` or `MOCK`), product name and uppercase mono 10 subtitle, right-hand actions, 2 px bottom rule. |
| `AppStatusBar` | 28 px on `surface`, 1 px top rule; the window supplies the children left to right. The loader puts a 10 px state mark (a `ToneGlyph`: busy, cross, check or dot) and a mono 11 state word first; the generator and the mock target put a 6 px round dot (a `Rectangle`, not a `ToneGlyph`: accent while working or serving, `neutral600` otherwise) and their own mono 11 text first. Each window puts `v<version>` last. |
| `RailBlock` | mono 11 `accentInk` ordinal, uppercase DemiBold 11 label with 1.3 px tracking, 2 px rule, content column at 12 px gaps. |
| `SectionHeader` | optional mono 12 ordinal, uppercase bold 12 label, 2 px rule (`rule: false` hides it), mono 12 summary; title and summary elide. Used in the dialogs, the upload and download panels and the mock target's right-hand pane. |
| `Card` | `surface`, 1 px border, 16 px padding; default box, `flat` or `framed`. No window uses it at present. |
| `StatusChip` | 22 px floor, 1 px border, a 10 px `ToneGlyph` and the word in bold mono 12 uppercase with 0.6 px tracking (`dot: false` removes the glyph). The tone chooses the glyph, ink, fill and border (see the tone table under Colour): `neutral` (default), `ok`, `warn`, `fail`, `accent`. Any other tone draws as `neutral`. |
| `CheckTile` | `neutral100`, 1 px border, uppercase micro label, bold mono 14 value (`Not set` when empty), 12 px detail that wraps to two lines before it elides, chip; used for the upload preflight. The tiles sit three across when the page is 720 px wide or more, and one per row below that. |
| `StatTile` | micro label over a bold mono 14 value; used in the media set properties. |
| `EmptyState` | `neutral100`, 1 px border, 24×2 accent rule, 15 px title, one sentence, actions. |
| `TaskProgress` | 4 px `progressTrack` track, `progressFill` fill; indeterminate is a 30% segment that sweeps, and stays at the left, still, under reduced motion. |
| `BusyMark` | three 5 px `neutral600` squares phased; still at full strength under reduced motion. |
| `LogView` | mono 11 time in a 62 px column (`showTime`), then the severity as a word (INFO, NOTE, WARN, ERROR) with the tone glyph for a warning or an error, then the 12 px message in the tone's page ink. A tab stop with a 2 px ring: Up, Down, Page Up, Page Down, Home and End scroll it, Ctrl+C copies every line as `time  SEVERITY  message` (in the loader; see the note below the table). Follows the end until the operator scrolls away from it; says "No activity yet." when empty. |
| `FocusRing` | 2 px `focusRing`, 2 px outside the control, z 100. Fields, combos, spin boxes, check boxes, buttons, segments, FIND answers and the scrolling lists all draw it; a list that clips draws it inside its edge. |
| `AppDialog` | 2 px `text` border on `surface`, 24 px padding, 640 px wide unless the dialog sets a width, 56 px heading band with a 2 px rule, 64 px footer with one primary and an optional secondary to its left. Content is a named `content` property. `standardButton(role)` maps the affirmative roles to the primary and Cancel and No to the secondary, for the window tests. It opens with the focus inside it: on `initialFocusItem` (the first field), else on the primary, or on the secondary with `initialFocus: "secondary"`. Return or Enter in the body presses the primary unless `enterAccepts` is false; Esc closes it; on close the focus returns to the control that opened it. |
| `ToneGlyph` | A 12 px canvas that draws the tone's shape: `check`, `warning` (triangle with a bar), `cross`, `busy` (play mark) or `dot`. Drawn as paths, since no bundled face carries them. |
| `ToneMessage` | A line of prose with its tone glyph and ink; a neutral tone shows no glyph. Rail and dialog warnings and errors use it, so a warning no longer reads as an error. |
| `AppScrollBar` | A 12 px `neutral200` track with a `neutral600` thumb, drawn whenever the content is longer than the view. |
| `RevealOnFocus` | Scrolls a `ScrollView` to the control that takes the keyboard focus. |
| `KeyScroll` | The Up, Down, Page and Home/End handling of a read-only list. |
| `AppBanner` | The one inline message for the window: a sentence (`text`), an optional mono second line (`detail`), actions placed inside it, an optional Dismiss. A 4 px rule at the left edge (2 px for `info`), the surface and a `ToneGlyph` carry the tone: `error` (a cross, `accent100` surface, `accentInk` border and rule), `warning` (a warning triangle, `neutral100`, `warn` border and rule), `info` (no glyph, `neutral100`, `divider`) and `success` (a check, `neutral100`, `neutral900` rule). Its role is an alert. It never repeats the state word that is already in the header. |
| `AboutDialog` | The one About page: the wordmark, one paragraph (`blurb`), the build provenance, the licence statement, the repository link and a Licences button that opens `LicencesDialog` (`showLicences`). A window passes `appCode`, `appName` and `blurb`, and may put an extra section inside it. |
| `LicencesDialog` | An `AppDialog` ("Third-party licences", Close) showing the embedded `NOTICES.txt` in a read-only mono 12 text area, at most 780×660. Opened from About > Licences in all three windows; says so in a sentence if the build carries no notices. |

`LogView` copies by role number: severity, text and time are the loader's `EventModel` roles
`Qt::UserRole` + 1, + 2 and + 3. The mock target's `ActivityModel` numbers its roles from
`Qt::UserRole` (severity, time, text), so in the mock target Ctrl+C leaves out the time and writes
INFO for every line. The list on screen is not affected, since it reads the roles by name. Only the
loader's copy is tested (`tst_navigation.qml`).

Rules for composition:

- One primary per region. In the loader and the generator the primary lives in the fixed action
  bar; the mock target has no action bar and keeps Start and Stop in the header, and the primary
  is the one that applies: Start while the target is stopped, Stop while it is serving. Secondary
  actions are outlined.
- A modal is for a decision, not for a message. The loader's error dialog is for an operation on a
  target that failed and must be acknowledged; every other message that stays in a window is an
  `AppBanner`, and a message about one field is the small red line under that field.
- A field is not an error until the operator has been in it or has tried to go on. A fresh form
  shows no red: the state in the header says "not started" and the field messages appear for the
  fields that were touched, or for all of them once the action was pressed on an incomplete form.
- Empty states say what will appear and how to make it appear. They are not a blank rectangle.
- Tables have a header row, 1 px rules and a stable column order; the first column is the object's
  identity.
- A destructive or mode-changing choice is disabled while the mode is live (the mock target's
  injections while serving), and its state is a chip, not a colour.

## Right to left

A window root sets `LayoutMirroring.enabled` from `Qt.application.layoutDirection` and
`childrenInherit`, which is all a layout, a row, an anchor or a layout margin needs. A component
places by anchors, layouts or `mirrored` and never by a literal `x` from the left: `TaskProgress`
fills from the start edge, `AppCheckBox`'s note, `AppComboBox`'s chevron, `AppSpinBox`'s steppers and
`AppTextField`'s unit sit at the start or end edge by `mirrored`. `LanguagePicker` is the shared
language box. `tst_right_to_left.qml` checks the result ([translations](translations.md#right-to-left)).

## Focus, keyboard and motion

- Every control is a tab stop when it can be pressed; a disabled control is not. The keyboard can
  do everything the pointer can (principle 9), and the focus is always visible.
- Buttons: Space, Return and Enter. Check boxes: Space, Return and Enter. A group that is one
  choice (the transfer switch, the dock tabs, the FIND answers) is one tab stop, the chosen item.
  In the transfer switch and the dock tabs, Left/Right (swapped in a right-to-left layout),
  Up/Down, Home and End choose the neighbour and move the focus onto it, wrapping at the ends. The
  FIND answers are a vertical list: Up and Left choose the previous answer and Down and Right the
  next one, in every layout (Left and Right are not swapped there), wrapping at the ends; Home and
  End choose the first and the last, and Enter or Space chooses the focused answer. A click on the
  chosen tab leaves it chosen.
- A list that is only read (the activity log, the target configuration, the media set's loads and
  files) is a tab stop that scrolls with Up, Down, Page Up, Page Down, Home and End; the loader's
  log also copies with Ctrl+C. A page that scrolls follows the focus (`RevealOnFocus`).
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
- Names: the rule is that an accessible name is the text on the screen (a field's is its label:
  "THW_ID_POS", not "hardware identifier"), so a person who says what they see is understood; the
  longer phrase is the `Accessible.description`. A spin box's editor carries the box's name. The
  loader follows the rule for its buttons (`AppButton` takes its text as its name, and the loader
  gives a longer phrase only as a description), with exceptions among its fields (the address
  field is named "Target address" under the label ADDRESS). The generator and the mock target do
  not follow it: many of their controls have a name longer than the text on the screen ("Start the
  mock target" on Start, "Stop the mock target" on Stop, "About the mock target" and "About the
  generator" on About, "Generate the media set" on Generate media set, "Cancel the generation" on
  Cancel).
- `Theme.reducedMotion` follows the platform's "reduce motion" preference
  (`Application.styleHints.accessibility.motionPreference`, Qt 6.12) and stops the busy mark and
  task progress motion without removing the still mark (`BusyMark.qml` and `TaskProgress.qml` test
  it). `Theme.forcedReducedMotion` pins it for a test or a screenshot. `Theme.motionDuration(ms)`
  returns 0 under reduced motion, but nothing calls it at present.
- The modal dialog traps focus as Qt's `Dialog` does; Esc closes it (the only close policy;
  clicking outside does nothing). Each window dims its content with one scrim item while a dialog
  is open (black at 32% in light, 62% in dark, above the content and below the popup overlay), and
  the dialog itself carries a 2 px border so it reads as a page rather than a floating panel. The
  scrim shows when any dialog named in the window's `modalOpen` property is open, so a new dialog
  has to be added to that list.
- Preferences is a two-column page, not a scrolling form: the set of choices is small enough to
  keep visible, and a scroll would hide half of it.

## Window chrome and first run

The header is the product's identity and the window's own actions, and it is the one place a state
is named. The loader has a readiness chip (SETUP, RUNNING, FAILED, COMPLETE, READY or NOT READY: one
state that the status bar repeats, so they cannot disagree), Preferences and About; its language box
is in Preferences. The generator has a chip (NOT STARTED, INCOMPLETE, READY, GENERATING, then
GENERATED, FAILED or CANCELLED), a language box and About. The mock target has a SERVING or STOPPED
chip, Start, Stop, a language box and About, and, while any fault injection is armed, a chip that
says how many and scrolls the rail to them; the footer repeats it. The panel and the footer say
something other than the state word: the loader's footer is the running activity or the last
outcome, target, media set and version; the generator's is what is left to fill in (or what
happened), output folder and version; the mock target's is whether it listens and where, ports,
derived `THW_ID_POS`, the armed faults and version. The first run, in the loader only, is the family
shape: a statement, one primary ("Start loading"), one optional route ("Choose a media set...") and
a numbered mental model beside it. The defaults are folded away behind "Set up defaults"; the
workbench is reachable without answering a form.

## Words

One word for one thing, in the windows, the guide and the messages. The identifiers in the code
keep their own names (`outputDirectory`); this is what the person reads.

| Say | Not | Where it applies |
|---|---|---|
| **folder** | directory | Anything on the computer's file system the person chooses or opens: the output folder, the download folder, the storage folder, the media set folder ("one folder per member"). The standard's own words (the part root directory) stay when quoting the standard. Three messages of the loader still say directory: "%1 is not a directory" and "there are already %1 download directories for %2 in %3" (`src/app/core/src/download_directory.cpp`), and the `THW_ID_POS` hint "... a character a directory name cannot hold" (`src/app/core/src/engine_worker.cpp`). |
| **Choose folder...** | Browse, Change, Choose directory, Choose output directory | The button that opens a file or folder dialog. One verb, **Choose**, whether or not a value is set. A list that grows says **Add files...**. |
| **hardware ID** | hardware identifier | A target's hardware ID as the standard names it (the generator's "Target hardware IDs", the mock target's "Hardware ID", the FIND answer). |
| **THW_ID_POS** | hardware ID and position | The loader's address string, `<hardware ID>_<position>` (`HNPFMS_L`), where the person types or reads it whole. |
| **Cancel** | Abort, Stop | Ending a local job that has changed nothing yet (a generation) and dismissing a dialog. Past tense: *Cancelled*. The loader has no cancel for the read of a media set (see [the application](app.md#the-engine-and-the-window-threads)). |
| **Abort** | Cancel, Stop | Ending an operation on a target, which the target has to be told about and may take time to confirm (the loader's Abort button; *Abort and close*). |
| **Stop** | Cancel, Abort | Turning a service off: the mock target stops serving. |
| **None** | none | A value that stands alone in a field or a list ("None"). Inside a sentence the plain word is lower case ("no media set"). |
| **READY / NOT READY / NOT STARTED / INCOMPLETE / SETUP** | | The state before an operation. READY when everything it needs is there. In the loader, NOT READY when something is missing and SETUP on the first run. In the generator, NOT STARTED when nothing has been entered and INCOMPLETE when something is missing. None of them is a failure, so none uses the filled red tone (the loader's NOT READY is the warning tone, the generator's NOT STARTED and INCOMPLETE are neutral). |
| **RUNNING / GENERATING** | | While an operation is in progress, in the header: RUNNING in the loader, GENERATING in the generator. The mock target has no such state; it says SERVING. |
| **COMPLETE / FAILED / CANCELLED** | | How an operation ended. The generator's header says GENERATED, FAILED or CANCELLED; the loader's receipt says COMPLETE or FAILED. |
| **SERVING / STOPPED** | | The mock target's state, in the header only. |

A state word appears once, in the header. A second chip next to the same fact (NOT READY three
times, INCOMPLETE three times, STOPPED four times) is noise: the other places say what is missing,
what happened or where.

## Implementation notes

- Shared QML is source-shared, not a separate module: each app's `qt_add_qml_module` compiles the
  files with aliases (`ArincDataLoader`, `ArincMediaGen` and `ArincMockTarget`).
  `adl_prepare_ui_qml()` must run in the app's directory before `qt_add_qml_module`;
  source-file properties are directory-scoped. It also marks `Theme.qml` as a singleton.
- `adl_add_ui_support(target)` adds the Archivo and Noto Sans Hebrew resources, the third-party
  notices (`:/adl/notices/NOTICES.txt`, written once per configure by `adl_generate_ui_notices()`),
  the shared interface's compiled catalogue (`adl_generate_ui_translations()`), `fonts.cpp`,
  `language_manager.cpp`, `window_state.cpp`, `settings_location.cpp`, `smoke_test.cpp` and
  `version.cpp` (the About provenance), and the include directories, including the generated
  `build_info.hpp`. An application's own catalogues come from `adl_add_app_translations()`.
  `settings_location.cpp` is `ADL_SETTINGS_DIR` and `smoke_test.cpp` is `--smoke-test`
  ([the application](app.md#where-the-settings-are-kept)). `Message` (`message.cpp`) is not part of
  this list: it is the small library `adl_ui_message` that the three applications' cores link.
- Fonts: `Archivo-Regular/Medium/SemiBold/ExtraBold.ttf` with `OFL.txt`, and
  `NotoSansHebrew-Regular/Medium/SemiBold/ExtraBold.ttf` with `NotoSansHebrew-OFL.txt`, in
  `src/ui/fonts`.
- Window geometry uses `QSettings` (the application's default settings) under `window/x`,
  `window/y`, `window/width`, `window/height`, `window/maximized` and `window/placed`, written when
  the window closes and when the application quits.
- QML tests load the compiled module (`loadFromModule`) and register the fonts, so the test sees
  the same module and the same typeface as the application.

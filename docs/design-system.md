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
| `accent` | `#ec3013` | `#ff5c40` | shapes: rules, dots, progress, markers |
| `accentFill` | `#b82009` | `#ff5c40` | a labelled accent surface |
| `accentText` | `#ffffff` | `#151313` | the label on `accentFill` |
| `neutral100` | `#f8f4f4` | `#1c1919` | field and tile fill |
| `neutral200` | `#eae7e7` | `#272423` | hover, selected row |
| `neutral300` | `#d7d3d3` | `#353130` | pressed, track |
| `neutral400` | `#bab6b6` | `#4b4645` | quiet marks |
| `neutral500` | `#9b9797` | `#6b6563` | disabled marks |
| `neutral600` | `#7d7979` | `#8b8583` | busy mark, idle status dot |
| `neutral700` | `#605d5d` | `#aba4a1` | secondary |
| `neutral800` | `#444141` | `#cfc8c5` | strong secondary |
| `neutral900` | `#2d2b2b` | `#e8e2df` | the strongest neutral |
| `accent100` | `#fff2ef` | `#2c150f` | warning banner, selected row |
| `accent200` | `#ffe0d9` | `#3e1c14` | warning border |
| `accent300` | `#ffc4b8` | `#58261b` | selected-row and warning-banner border |
| `accent700` | `#ae1800` | `#ff8368` | accent as ink |

`Theme` also defines `accent400` to `accent600` and `accent800` and `accent900`. No view uses
`neutral400`, `neutral500`, `neutral700`, `neutral800`, `accent200` or any of those accent rungs
yet; `neutral900` is used by the `ok` chip tone and as the window palette's dark role.

Derived tokens:

- `divider` = `text` at 50% alpha. It is the 1 px border of a control or card and is the 2 px
  section rule.
- `textMuted` = `text` at 68% alpha, for secondary prose.
- `accentInk` = the accent if it clears 4.5:1 against `bg`, otherwise `accent700`. That is
  `accent700` in light (6.41:1 on `bg`; the raw accent is 3.76:1) and the accent itself in dark
  (6.04:1). Used for warning text, ordinals and the focus ring.
- `ok` = `neutral800`, `warn` = `accentInk`, `fail` = `accentInk`, `info` = `textMuted`. Success is
  neutral; warning and failure are the accent and the word says which. There is no green token.
  No QML view reads these four tokens: `StatusChip` chooses its own colours from the tones below.

`Theme` keeps the names used before the shared system (`desk`, `panel`, `border`, `ink`, `muted`,
`accentSoft`) as aliases of `bg`, `surface`, `divider`, `text`, `textMuted` and `accent100`. No
view uses them now. `radiusCard`, `radiusControl`, `gap` (12) and `pad` (16) are also defined.

Rules:

1. `accent` never carries a label. Signal light is 4.20:1 on white; `accentFill`/`accentText` is
   the measured pair (6.46:1 for the light fill, 6.04:1 for the dark one).
2. No second hue. Status is a word plus a tone, not a traffic light.
3. The only circles are the semantic status dots: 6 px (radius 3) in the status bar and 4 px
   (radius 2) in a `StatusChip`. Everything else is square.

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
| `fontMicro` | 10 | eyebrows, tile labels, chips |
| `fontFine` | 9 | reserved; no view uses it |

`trackingWide` = 1.2 px at 100%; `trackingTitle` = −0.3. Uppercase micro labels carry 0.6–1.4 px
of tracking, set as literals in the views (they do not follow `fontScale`). Every size is
multiplied by `Theme.fontScale`; a literal `pixelSize` in a view is a bug (none remains in
`src/*/qml`). `fontScale` is 1.0 and nothing sets it yet: there is no text-size preference. A
control's height is a floor (30 px for buttons, fields, combos and spin boxes; 20 px for a chip)
that grows with its label; the chrome bands under Geometry are fixed.

## Geometry

- Spacing: `space1` 4, `space2` 8, `space3` 12, `space4` 16, `space6` 24, `space8` 32.
- `radius` 0. Do not round a surface or a control.
- Borders: 1 px on controls, cards, tiles and the rule above the status bar and the action bar;
  2 px section rules, the rule under the header and under the transfer strip, the rule between the
  rail and the work area, and the border of a dialog; 3 px active-tab underline (the activity
  dock's tabs).
- Window: all three open at 1280×900 with a minimum of 1120×760. `install_window_state()`
  (`src/ui/window_state.cpp`) restores the saved size, clamped between the minimum and the
  screen's available area less 48 px wide and 80 px high, and restores the saved position only if
  that rectangle still meets a screen; otherwise the window is centred horizontally and placed a
  third of the way down. A maximised window is restored maximised.
- Rail: one width for all three windows, `Theme.railWidth` 388 px preferred, `Theme.railMinWidth`
  348 to `Theme.railMaxWidth` 460. The rail scrolls itself and a 2 px rule separates it from the
  work area.
- Fixed bands: header 64, status bar 28, the loader's transfer strip 52, the loader's activity dock
  260 open and 44 collapsed, the loader's and the generator's action bar 76. The action bar never
  scrolls.

## Components

| Component | Anatomy |
|---|---|
| `AppButton` | 30 px floor, 1 px divider, transparent secondary, `accentFill` primary (no border, DemiBold label), hover and pressed shades, 0.45 opacity when disabled, 2 px focus ring outside. |
| `AppCheckBox` | 16 px square, `accentFill` when checked, struck `accentText` mark, label 14, optional note 12, ring around the square. `click()` for the window tests. |
| `AppTextField` | 30 px floor, `neutral100`, 1 px divider, mono 13, optional unit suffix, focus border becomes `focusRing`. A placeholder is an example, not a value: it is drawn in the heading face, italic and muted (never in the mono face of real values) and is written "e.g. ..." at the call site, so `e.g. 10.0.0.2` cannot be mistaken for an address that was entered. |
| `AppComboBox` | 30 px floor, `neutral100`, 1 px divider, drawn chevron, popup `surface`, highlighted row `neutral200`, focus border becomes `focusRing`. |
| `AppSpinBox` | 30 px floor, `neutral100`, two 24 px square stepper cells (+ and −), mono value without digit grouping. |
| `SegmentedControl` | Row of 30 px floor, 90 px minimum segments; the chosen one `accentFill` with an `accentText` label, the rest transparent with a 1 px divider. Each segment is a button (`objectName` from the segment's `name`). One tab stop for the group. |
| `AppHeader` | 64 px on `surface`, 4×30 accent bar, mark (`appCode`, 22 px ExtraBold: `ADL`, `GEN` or `MOCK`), product name and uppercase mono 10 subtitle, right-hand actions, 2 px bottom rule. |
| `AppStatusBar` | 28 px on `surface`, 1 px top rule; the window supplies the children left to right. Each window puts a 6 px status dot and a mono 11 state word first and `v<version>` last. |
| `RailBlock` | mono 11 `accentInk` ordinal, uppercase DemiBold 11 label with 1.3 px tracking, 2 px rule, content column at 12 px gaps. |
| `SectionHeader` | optional mono 12 ordinal, uppercase bold 12 label, 2 px rule (`rule: false` hides it), mono 12 summary; title and summary elide. Used in the dialogs, the upload and download panels and the mock target's right-hand pane. |
| `Card` | `surface`, 1 px border, 16 px padding; default box, `flat` or `framed`. No window uses it at present. |
| `StatusChip` | 20 px floor, 1 px border, bold mono 10 uppercase with 0.8 px tracking, 4 px dot (`dot: false` removes it). Tones: `neutral` (default; `textMuted` word, `divider` border), `ok` (`neutral900` word and dot, `divider` border, no fill), `warn` (`accentInk` word, dot and border), `accent` and `fail` (`accentFill` fill and border, `accentText` word and dot). Any other tone draws as `neutral`. |
| `CheckTile` | `neutral100`, 1 px border, uppercase micro label, bold mono 14 value (`Not set` when empty), 12 px detail, chip; used for the upload preflight. |
| `StatTile` | micro label over a bold mono 14 value; used in the media set properties. |
| `EmptyState` | `neutral100`, 1 px border, 24×2 accent rule, 15 px title, one sentence, actions. |
| `TaskProgress` | 4 px `neutral300` track, accent fill; indeterminate is a 30% segment that sweeps, and stays at the left, still, under reduced motion. |
| `BusyMark` | three 5 px `neutral600` squares phased; still at full strength under reduced motion. |
| `LogView` | mono 11 time in a 62 px column (`showTime`), 12 px message, `accentInk` for any severity above information, follows the end until the operator scrolls away from it; says "No activity yet." when empty. |
| `FocusRing` | 2 px `focusRing`, 2 px outside the control, z 100. |
| `AppDialog` | 2 px `text` border on `surface`, 24 px padding, 640 px wide unless the dialog sets a width, 56 px heading band with a 2 px rule, 64 px footer with one primary and an optional secondary to its left. Content is a named `content` property. `standardButton(role)` maps the affirmative roles to the primary and Cancel and No to the secondary, for the window tests. |
| `AppBanner` | The one inline message for the window: a sentence (`text`), an optional mono second line (`detail`), actions placed inside it, an optional Dismiss. A 4 px rule at the left edge (2 px for `info`) and the surface carry the tone: `error` (`accent100` surface, `accentInk` border and rule), `warning` (`neutral100`, `accentInk` border and rule), `info` (`neutral100`, `divider`) and `success` (`neutral100`, `neutral900` rule). Its role is an alert. It never repeats the state word that is already in the header. |
| `AboutDialog` | The one About page: the wordmark, one paragraph (`blurb`), the build provenance, the licence statement, the repository link and a Licences button that opens `LicencesDialog` (`showLicences`). A window passes `appCode`, `appName` and `blurb`, and may put an extra section inside it. |
| `LicencesDialog` | An `AppDialog` ("Third-party licences", Close) showing the embedded `NOTICES.txt` in a read-only mono 12 text area, at most 780×660. Opened from About > Licences in all three windows; says so in a sentence if the build carries no notices. |

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

## Focus, keyboard and motion

- Every control is a tab stop when it can be pressed; a disabled control is not.
- Buttons: Space, Return and Enter. Check boxes: Space, Return and Enter. Segments: one tab stop
  for the group (the chosen segment); Left/Right/Up/Down/Home/End choose the neighbouring segment
  and move the focus onto it, wrapping at the ends (Left and Right swap in a right-to-left
  layout).
- Shortcuts: the loader has Preferences (`StandardKey.Preferences`), Quit and Ctrl+F (Find); the
  generator and the mock target have Quit.
- Text fields and combos show focus by changing their own border to `focusRing`; hand-drawn
  controls show a 2 px ring outside.
- The focus ring is `accentInk`; width 2, offset 2. It is never the only cue for state.
- `Theme.motionDuration()` is the one switch for animation; `Theme.reducedMotion` (future
  preference; nothing sets it) stops the busy mark and task progress motion without removing the
  still mark.
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
is named. The loader has a readiness chip (WORKING, SETUP, READY or NOT READY), Preferences and
About. The generator has a chip (NOT STARTED, INCOMPLETE, READY, GENERATING, then GENERATED, FAILED
or CANCELLED) and About. The mock target has a SERVING or STOPPED chip, Start, Stop and About, and,
while any fault injection is armed, a chip that says how many and scrolls the rail to them; the
footer repeats it. The panel and the footer say something other than the state word: the loader's
footer is the running activity or the last outcome, target, media set and version; the generator's
is what is left to fill in (or what happened), output folder and version; the mock target's is
whether it listens and where, ports, derived `THW_ID_POS`, the armed faults and version. The
first run, in the loader only, is the family shape: a statement, one primary ("Start loading"), one optional route ("Choose a
media set...") and a numbered mental model beside it. The defaults are folded away behind "Set up
defaults"; the workbench is reachable without answering a form.

## Words

One word for one thing, in the windows, the guide and the messages. The identifiers in the code
keep their own names (`outputDirectory`); this is what the person reads.

| Say | Not | Where it applies |
|---|---|---|
| **folder** | directory | Anything on the computer's file system the person chooses or opens: the output folder, the download folder, the storage folder, the media set folder ("one folder per member"). The standard's own words (the part root directory) stay when quoting the standard. |
| **Choose folder...** | Browse, Change, Choose directory, Choose output directory | The button that opens a file or folder dialog. One verb, **Choose**, whether or not a value is set. A list that grows says **Add files...**. |
| **hardware ID** | hardware identifier | A target's hardware ID as the standard names it (the generator's "Target hardware IDs", the mock target's "Hardware ID", the FIND answer). |
| **THW_ID_POS** | hardware ID and position | The loader's address string, `<hardware ID>_<position>` (`HNPFMS_L`), where the person types or reads it whole. |
| **Cancel** | Abort, Stop | Ending a local job that has changed nothing yet (a generation, a read of a media set) and dismissing a dialog. Past tense: *Cancelled*. |
| **Abort** | Cancel, Stop | Ending an operation on a target, which the target has to be told about and may take time to confirm (the loader's Abort button; *Abort and close*). |
| **Stop** | Cancel, Abort | Turning a service off: the mock target stops serving. |
| **None** | none | A value that stands alone in a field or a list ("None"). Inside a sentence the plain word is lower case ("no media set"). |
| **READY / NOT STARTED / INCOMPLETE** | | The state before an operation: READY when everything it needs is there, NOT STARTED when nothing has been entered, INCOMPLETE when something is missing. Not an error, so not red. |
| **WORKING** | RUNNING | While an operation is in progress, in the header, in every window (GENERATING in the generator is its own verb for the same state). |
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

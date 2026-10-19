# The shared design system

`src/ui` is one source of colour, type, geometry and component anatomy for `adl_app`,
`adl_media_gen` and `adl_mock_target`. It is the ICL Forge Modernist system (the Signal palette,
Archivo, zero radius, the 4/8/12/16/24/32 rhythm) applied to an avionics loading console.

A view names a token; it does not name a hex value or a pixel size. The token file is
`src/ui/qml/Theme.qml`. The components are the QML files beside it. `cmake/AdlUi.cmake` adds them
to each application's QML module with a root resource alias and adds the bundled Archivo faces, so
the same source is compiled into each window without a second type registry.

## Colour

The default palette is Signal. Light and dark are both hand-tuned; dark is not an inversion. The
scheme follows the desktop (`Application.styleHints.colorScheme`); the tests and screenshot runs
set `Theme.forcedDark`.

| Token | Light | Dark | Use |
|---|---|---|---|
| `bg` | `#f3f2f2` | `#151313` | the page behind a surface |
| `surface` | `#eae9e9` | `#1d1a1a` | header, cards, dialogs |
| `text` | `#201e1d` | `#ece7e5` | primary text |
| `accent` | `#ec3013` | `#ff5c40` | shapes: rules, dots, progress, markers |
| `accentFill` | `#b82009` | `#ff5c40` | a labelled accent surface |
| `accentText` | `#ffffff` | `#151313` | the label on `accentFill` |
| `neutral100` | `#f8f4f4` | `#1c1919` | field and tile fill |
| `neutral200` | `#eae7e7` | `#272423` | hover, selected row |
| `neutral300` | `#d7d3d3` | `#353130` | pressed, track |
| `neutral400` | `#bab6b6` | `#4b4645` | quiet marks |
| `neutral500` | `#9b9797` | `#6b6563` | disabled marks |
| `neutral600` | `#7d7979` | `#8b8583` | busy mark |
| `neutral700` | `#605d5d` | `#aba4a1` | secondary |
| `neutral800` | `#444141` | `#cfc8c5` | strong secondary |
| `neutral900` | `#2d2b2b` | `#e8e2df` | the strongest neutral |
| `accent100` | `#fff2ef` | `#2c150f` | warning banner, selected row |
| `accent200` | `#ffe0d9` | `#3e1c14` | warning border |
| `accent300` | `#ffc4b8` | `#58261b` | selected-row border |
| `accent700` | `#ae1800` | `#ff8368` | accent as ink |

Derived tokens:

- `divider` = `text` at 50% alpha. It is the 1 px border of a control or card and is the 2 px
  section rule.
- `textMuted` = `text` at 68% alpha, for secondary prose.
- `accentInk` = the accent if it clears 4.5:1 against `bg`, otherwise `accent700`. Used for
  warning text, ordinals and the focus ring.
- `ok` = `neutral800`, `warn` = `accentInk`, `fail` = `accentInk`. Success is neutral; warning and
  failure are the accent and the word says which. There is no green token.

Rules:

1. `accent` never carries a label. Signal light is 3.95:1 on white; `accentFill`/`accentText` is
   the measured pair (6.46:1 for the light fill).
2. No second hue. Status is a word plus a tone, not a traffic light.
3. The only circle is a semantic status dot (6 px, radius 3). Everything else is square.

## Type

Archivo (SIL OFL 1.1) is bundled and registered before the engine starts; the platform UI face is
the fallback if registration fails. Identifiers, numbers and ordinals are Cascadia Mono →
Consolas → monospace.

| Token | Base px | Use |
|---|---|---|
| `fontDisplay` | 40 | first-run statement, about mark |
| `fontHeadline` | 26 | the plan line |
| `fontTitle` | 22 | the wordmark |
| `fontHeading` | 18 | dialog heading |
| `fontSubheading` | 15 | section heading, receipt line |
| `fontNormal` | 14 | body, checkbox label |
| `fontBody` | 13 | controls, prose |
| `fontSmall` | 12 | secondary, chips, log messages |
| `fontMono` | 11 | ordinals, status bar, timestamps |
| `fontMicro` | 10 | eyebrows, tile labels, chips |
| `fontFine` | 9 | reserved |

`trackingWide` = 1.2 px at 100%; `trackingTitle` = −0.3. Uppercase micro labels carry 0.8–1.4 px
of tracking. Every size is multiplied by `Theme.fontScale`; a literal `pixelSize` in a view is a
bug. Control heights come from their labels, not a fixed box.

## Geometry

- Spacing: `space1` 4, `space2` 8, `space3` 12, `space4` 16, `space6` 24, `space8` 32.
- `radius` 0. Do not round a surface or a control.
- Borders: 1 px on controls and cards; 2 px section rules and the top/bottom of the app chrome;
  3 px active-tab underline.
- Window minimum: 1120×760; default 1280×900; restored geometry is clamped to the current screen.
- Rail: 348–460 px, preferred 388; the rail scrolls itself, the action bar never scrolls.

## Components

| Component | Anatomy |
|---|---|
| `AppButton` | 30 px floor, 1 px divider, transparent secondary, `accentFill` primary, 0.45 opacity when disabled, 2 px focus ring outside. |
| `AppCheckBox` | 16 px square, `accentFill` when checked, struck `accentText` mark, label 14, optional note 12, ring around the square. `click()` for the window tests. |
| `AppTextField` | 30 px floor, `neutral100`, 1 px divider, mono 13, optional unit suffix, focus border becomes `focusRing`. |
| `AppComboBox` | 30 px floor, `neutral100`, 1 px divider, drawn chevron, popup `surface`, highlighted row `neutral200`. |
| `AppSpinBox` | 30 px floor, `neutral100`, square stepper cells, mono value. |
| `AppHeader` | 64 px, accent bar, three-letter mark in ExtraBold, product name and mono subtitle, right-hand actions, 2 px bottom rule. |
| `AppStatusBar` | 28 px, top 1 px rule, mono 11, status dot and three fields. |
| `RailBlock` | mono accent ordinal, tracked uppercase label, 2 px rule, content column at 12 px gaps. |
| `SectionHeader` | mono accent ordinal, uppercase 12 px label, 2 px rule, mono 12 summary; title/summary elide. |
| `Card` | `surface`, 1 px border, 16 px padding; default box, flat or framed. |
| `StatusChip` | 20 px floor, 1 px border, mono 10 uppercase, 4 px dot; neutral, warn, ok, accent, fail. |
| `CheckTile` | `neutral100`, 1 px border, micro label, mono value, detail, chip; used for preflight. |
| `StatTile` | micro label over a bold mono value. |
| `EmptyState` | neutral surface, accent rule, title, one sentence, actions. |
| `TaskProgress` | 4 px track, accent fill, 30% still mark under reduced motion. |
| `BusyMark` | three 5 px squares phased; still at full strength under reduced motion. |
| `LogView` | mono time, body message, severity colour, follows the end until the operator scrolls. |
| `FocusRing` | 2 px `focusRing`, 2 px outside the control, z 100. |
| `AppDialog` | square surface, 2 px heading rule, 24 px padding, footer with one primary and optional secondary. Content is a named `content` property. |

Rules for composition:

- One primary per region. The primary lives in the fixed action bar; secondary actions are
  outlined.
- A modal is for a decision, not for a message. Errors use the dialog only when the operation
  failed; routine state is inline.
- Empty states say what will appear and how to make it appear. They are not a blank rectangle.
- Tables have a header row, 1 px rules and a stable column order; the first column is the object's
  identity.
- A destructive or mode-changing choice is disabled while the mode is live (the mock target's
  injections while serving), and its state is a chip, not a colour.

## Focus, keyboard and motion

- Every control is a tab stop when it can be pressed; a disabled control is not.
- Buttons: Space/Return. Segments: one tab stop for the group, Left/Right/Up/Down/Home/End.
- Text fields and combos show focus by changing their own border to `focusRing`; hand-drawn
  controls show a 2 px ring outside.
- The focus ring is `accentInk`; width 2, offset 2. It is never the only cue for state.
- `Theme.motionDuration()` is the one switch for animation; `Theme.reducedMotion` (future
  preference) stops the busy mark and task progress motion without removing the still mark.
- The modal dialog traps focus as Qt's `Dialog` does; Esc closes it.

## Window chrome and first run

The header is the product's identity and its global actions (Preferences, About, Start/Stop). The
footer is the state line (busy/idle/complete, target, media set, version). The first run is the
family shape: a statement, one primary, one optional route, and a numbered mental model beside it.
The defaults are folded away; the workbench is reachable without answering a form.

## Implementation notes

- Shared QML is source-shared, not a separate module: each app's `qt_add_qml_module` compiles the
  files with aliases. `adl_prepare_ui_qml()` must run in the app's directory before
  `qt_add_qml_module`; source-file properties are directory-scoped.
- `adl_add_ui_support(target)` adds the Archivo resources, `fonts.cpp` and `window_state.cpp`.
- Fonts: `Archivo-Regular/Medium/SemiBold/ExtraBold.ttf`, licence `OFL.txt` beside them.
- Window geometry uses `QSettings` under `window/` in the application's own settings file.
- QML tests load the compiled module (`loadFromModule`) and register the fonts, so the test sees
  the same module and the same typeface as the application.

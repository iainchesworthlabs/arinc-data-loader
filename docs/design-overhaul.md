# The design overhaul

This is the human-factors analysis and the design direction behind the redesign of the three Qt
applications in `src/app`, `src/media_gen` and `src/mock_target`. It records what was wrong, why it
matters for the people who load software onto avionics targets, what the new window is for, and
what remains open. The implementation is on `feature/design-overhaul`; the before-state citations
are to `feature/data-loader-ui` at `a33a9c8`, the last design pass before this one.

The short version: the applications were correct little engines wrapped in three unrelated forms.
They exposed the engine's nouns as a wall of controls, put the action at the end of a scroll, hid
the consequence of a choice, and looked like three different products. This pass gives them one
task-shaped information architecture and one design system borrowed from ICL Forge, so the loader,
the generator and the mock target read as three instruments in the same rack.

## 1. What these tools are for

The people using them are not consumers following a funnel. They are:

- **Load operators** on a bench or flight line, repeating a procedure under time pressure:
  choose a media set, confirm it is the right one, confirm the target is the right unit, tick the
  right loads, transfer, and record what happened. They need the next action and the current
  target to be unmistakable, and they need a failure to say what to do next.
- **Avionics and software engineers** building a set, reproducing a defect, or reading a protocol
  log. They need density, exact identifiers, timestamps, and the ability to see what the target
  actually said, not a summary that hides it.
- **Demonstrators and testers** running the mock target, deliberately making it misbehave, and
  proving the loader copes. They need the difference between "behaving" and "injecting a fault" to
  be visible at a glance; a demo that quietly runs with a stale injection is a false result.

These are engineering instruments. The design target is not "friendly"; it is **legible,
predictable, fast and hard to get wrong**. That is a different bar from a consumer app, and it is
the bar ICL Forge's window is built to.

## 2. Method

The analysis used four kinds of evidence:

1. **The running windows.** The before-state screenshots in
   [`design/screenshots`](design/screenshots) were captured from the `feature/data-loader-ui`
   build: the first run, the workbench with a real media set loaded, the generator after a
   successful generation, and the mock target. The after-state screenshots are the same four
   surfaces from this branch, plus [the real window](design/screenshots/after-real-window.png) at
   the size it restored on the 1200-wide primary screen (1168×939 including the frame).
2. **The code and its states.** Every finding is traced to the QML or model that produces it. The
   models were read as the source of truth for what the window can know, which is why some
   findings are marked "needs a model change" rather than solved in QML.
3. **The standards the format cites.** ARINC 615A-3 and ARINC 665-3 define the protocol and the
   package; the repository's own coverage documents say where the implementation departs from
   them. The design does not invent protocol semantics, and it does not hide the ones the engine
   knows.
4. **Human-factors frameworks.** Nielsen's ten heuristics, ISO 9241-110:2020's seven interaction
   principles, Shneiderman's eight golden rules and WCAG 2.2 AA were used as lenses, not as a
   certification. MIL-STD-1472H's human-engineering criteria were used for the parts that are
   specific to a military/avionics console: target identity, mode awareness, error tolerance and
   the cost of an unintended action.

The before-state was also driven, not only read: a real media set (`two-member`) was loaded, the
window was captured with the set checked, and the test harness drove FIND, Information, Upload and
Download against the simulated target. The QML tests were run with every engine warning an error.

## 3. What was wrong

### 3.1 Startup was a form, not an opening statement

The `feature/data-loader-ui` first run
([before](design/screenshots/before-first-run.png)) is two rounded cards: "Before the first
operation" asks for an address, a `THW_ID_POS` and a download directory before the operator has
seen what the application does; "What this window does" explains the product in a second card.
The primary action is `Start`, which is both mandatory and vague. There is no wordmark, no
statement of what the tool is for, and no way to open the workbench without answering a form.
The current first run ([after](design/screenshots/after-first-run.png)) is the family's shape: a
statement, one primary, one optional route, and the mental model beside it.

Why it matters: the first screen is where an operator learns the model of the tool. A settings
form teaches "this program is a form I fill in"; a statement plus a concrete first action teaches
"this program does a job". For a bench tool that distinction survives every later screen.

### 3.2 The workbench exposed the engine instead of the task

The before workbench ([before](design/screenshots/before-workbench.png),
[after](design/screenshots/after-workbench.png)) was a scroll of titled groups: Media set, Target,
Download, Operation. The groups were not a sequence and did not share a grammar. `Download` sat
between `Target` and `Operation` and could be pushed below the fold by a long media set;
`Operation` — the only place Upload and Abort lived — was the last thing in the scroll. At the
1100×760 default the Upload button was partly below the visible area. A disabled button at the
bottom of a scroll is not a disabled button; it is a missing button.

The new workbench is a two-pane instrument: a context rail that never scrolls away (target, then
media set) and a work area with a fixed transfer switch, a plan, a preflight, the evidence dock and
a fixed action bar. The primary action is always in the same place, and Download has a first-class
mode instead of a card below the fold.

### 3.3 Target identity was never confirmed

The before window restored the last target address and `THW_ID_POS` quietly at start-up, and a FIND
row could be selected without the row naming the `THW_ID_POS` the operation would use. Editing
either field afterwards left the FIND row selected and the old target's configuration on screen.
Only `readInformation` cleared the configuration. With automatic FIND on, a sole answer was taken
and the operation started without a confirmation step.

This is the highest-consequence defect in the suite. An upload to the wrong unit is not a typo; it
is a configuration change on an aircraft. The redesign does not add a mandatory modal on every
upload (that trains people to dismiss modals), but it does make identity a first-class object:

- the target rail always shows `THW_ID_POS` and the address as the two values the operation will
  use, with a `SET` / `NOT SET` chip;
- FIND rows show `THW_ID_POS`, address and type/position, and the chosen row is marked;
- editing either field now clears the FIND selection, the target configuration and any transfer
  receipt whose inputs no longer hold (see §8);
- the preflight repeats the target and its address immediately above the action;
- the action bar repeats the target in the readiness sentence and in the footer.

### 3.4 "Checked" was a claim without a time

`Check media set` is the read: it verifies every file and check value, and a set that reads is
structurally valid. The before window showed a green "all check values verified" with no time, and
the files are opened from disk when the target asks for them. A set edited after the check still
displayed "checked" and uploaded the new bytes. The check's findings were a second list that the
loader window did not bind, while the media set's findings were shown separately.

The redesign cannot make the file system transactional, but it makes the claim honest:

- a `CHECKED` chip sits on the set, and the rail says "Every file and check value was read and
  verified" instead of a green sentence with no provenance;
- the findings are shown once, under the set, with a count and an expander, and the same list is
  used in the properties dialog;
- a failed read clears the claim and says why; the retained loads are still visible but the
  validation state is `INVALID`, so an upload is refused.

A timestamp and an mtime/manifest guard are listed as a model follow-up in §11; they need a file
identity in `MediaSetIndex`, not a QML change.

### 3.5 The payload was invisible

The before window showed loads as a check box and a paragraph. There was no preflight that said
"this target, this set, these loads, this check". `Tick all` plus Upload was the whole plan.

The new Upload view has a three-tile preflight (target, media set, loads), a sentence naming the
count and the target, a payload table of the ticked loads with header, contents and status, and a
receipt after the operation. The left rail is still where the ticks are made, so the operator can
work in one place, but the main panel now answers "what is about to be sent" before the action.

### 3.6 Progress was one generic line; abort was a failure

The before operation panel showed one job-wide bar and a line like "Uploading..."; there was no
phase, no per-load status in the plan, and no distinction between "the transfer is running" and
"the target is confirming the abort". Closing the window during a job silently aborted and then
blocked for up to 13 seconds in the destructor, with no warning and no receipt.

The new action bar keeps the job's activity line, a 4 px progress bar and the percentage, and the
per-load rows in the rail and payload table carry their own status. Closing during a job now opens
a question: keep working, or stop and close. The second path asks the target to abort and keeps the
window open (with the normal "Stopping the operation..." state) until the engine is idle, then
closes. The old behaviour is still possible, but it is a deliberate answer to a question.

Abort and cancellation are still reported by the engine as a failure outcome (`success == false`),
so the receipt says `FAILED` unless the design distinguishes them. Changing that is a model
change: the engine has the abort reason, the window does not. §11 records it.

### 3.7 There was no receipt

A result was a green sentence at the bottom of the old Operation panel. It outlived changes to the
target and the set, and it did not record the target, the set, the ticked loads or the time. An
engineer trying to reproduce a load had to reconstruct it from the log.

The new receipt is owned by the transfer: it appears only after an Information, Upload or Download
operation (`resultIsTransfer`), names the target and the set, carries the engine's result text and
detail, and is cleared when the target changes. The activity dock keeps the full evidence,
including what the target said and what the loader did, with timestamps.

### 3.8 Download was destination-opaque

The before Download panel showed the download directory only after a download had run, had no
statement of which directory would be created, no "open folder", and switched mode by discarding
the list. `Choose none` cancelled instantly.

The new Download view names the destination before the action, says that each download creates a
new `DNLD_DATA_<THW_ID_POS>_<n>` directory, offers `Change...` and `Open folder`, and states the
consequence of each mode. It still cannot show the next number without a model property; §11 lists
that. The operator-defined list is a proper list with per-file status; the media-defined list is a
proper list with remove.

### 3.9 The generator was a long form, not a media set

The generator's before window ([before](design/screenshots/before-media-gen.png),
[after](design/screenshots/after-media-gen.png)) was a single column of group boxes: Media set,
Load, Batch, Files, Generate. The action was at the end, and the file table had no room. More
importantly, the form greeted a new operator with red "Enter the ..." errors on every untouched
field, and every check value defaulted to `none`, so "written and verified" could mean "written
and not verifiable".

The new window separates the **set** (left rail: identity, load, batch) from the **artefact**
(right: files and generate). The action is fixed at the bottom right, the file table is the centre,
and a `NO CHECK VALUES` chip appears when all three check-value choices are `none`. The red
required-field errors are no longer shown for fields that have never been touched; the action bar
says what is missing.

The generator's success detail is now prose with real plurals (one data file, two support files),
and the output path is a first-class value with `Open folder`.

### 3.10 The mock target had no mode awareness

The mock target's before window ([before](design/screenshots/before-mock.png),
[after](design/screenshots/after-mock.png)) showed Identity, Ports, Storage, Run and Injections as
five equal frames. Running was inferred from which of Start/Stop was enabled; there was no running
badge and no summary of active injections. The derived `THW_ID_POS` was left to the operator, who
had to concatenate hardware ID, underscore and position in their head. Empty storage silently
meant an upload was not kept.

The new window has a `SERVING` / `STOPPED` chip in the header, Start/Stop in the header, a derived
`THW_ID_POS` field with `Copy`, a confirmation that storage is not set but still served, and a
Fault injection block with an `N ACTIVE` / `BEHAVING` chip and `Reset all`. Every injection is
disabled while the target is running, so a mode cannot change under a live dialogue.

### 3.11 Three products, one suite

The before windows shared no design system: the loader had rounded blue cards from the UI pass, the
generator and mock target were plain Fusion group boxes with different red validation text, and the
three used different words for the same thing (`THW_ID_POS`, "Hardware ID" + "Position", "Target
hardware IDs"). That is the "styles are different" the request named. The new windows share
`src/ui`: one `Theme.qml`, one button, one field, one segmented control, one chip, one log, one
header and one footer. The screenshots show the family resemblance immediately; the consistency is
structural, not a coincidence of three copies.

### 3.12 The visual system did not match ICL Forge

ICL Forge's window is Modernist: zero radius, Archivo, a signal-red accent used for shapes, a
measured neutral ramp, 1 px control borders, 2 px section rules, 3 px active tab rules, numbered
rail blocks, a fixed action bar, and a first run that is a statement rather than a form. The
before loader used 10 px rounded cards, a blue accent, system type, and no shared header. The
redesign adopts the ICL Forge tokens and component anatomy ([`docs/design-system.md`](design-system.md)),
with one deliberate difference: each application gets a three-letter mark (`ADL`, `GEN`, `MOCK`)
and a subtitle, because the three tools are siblings, not one product.

## 4. Heuristic evaluation

Severity: **C**ritical (can cause a wrong load or an unrecoverable state), **H**igh (forces a
workaround or hides the next action), **M**edium (friction, learning cost), **L**ow (polish).

| # | Finding | Heuristic / principle | Severity | Evidence |
|---|---|---|---|---|
| 1 | Target identity was editable and implicit; a restored target could be used without confirmation. | N5 error prevention; ISO 9241-110 error tolerance; Shneiderman S5 | C | `loader_model.cpp` restore/set target; old `Main.qml` target panel; `TargetPropertiesDialog.qml` |
| 2 | Editing the target left FIND selection, configuration and result from the old target. | N1 visibility of system status; ISO self-descriptiveness | C | `loader_model.cpp` setters; old `Main.qml` configuration panel |
| 3 | "Checked" had no time and did not survive disk edits; findings were split across two lists. | N1/N5; ISO error tolerance | C | `loader_model.cpp` validation; old `Main.qml` findings |
| 4 | The primary action was at the end of a scroll and could be below the fold. | N6 recognition over recall; ISO suitability; S4 | C | old `Main.qml` ScrollView; 1100×760 window |
| 5 | Closing during a job silently aborted and blocked up to 13 s. | N3 user control; ISO controllability; S7 | C | old `Main.qml` close; `loader_model` destructor; docs/app.md |
| 6 | No preflight or payload summary; ticks were an implicit promise. | N6/N8; ISO suitability; S8 | H | old `Main.qml` media set rail/operation panel |
| 7 | Progress had no phase; abort had no receipt and was styled as failure. | N1; ISO self-descriptiveness; S3 | H | old `Main.qml` operation panel; `engine_worker.cpp` outcomes |
| 8 | Download destination and directory naming were invisible before start; mode switch discarded the list. | N1/N3; ISO self-descriptiveness | H | old `DownloadPanel.qml`; `loader_model.cpp` setDownloadMode |
| 9 | Preferences said OK/Cancel but wrote every change immediately; Cancel restored nothing. | N4 consistency; ISO conformity; S1/S6 | H | old `PreferencesDialog.qml`; model setters |
| 10 | The generator showed required-field errors on an untouched form and defaulted all check values to none. | N5/N10; ISO suitability | H | old `GeneratorWindow.qml`; `generator_model.hpp` |
| 11 | The mock target had no running badge, no active-injection summary, and no derived `THW_ID_POS`. | N1/N2; ISO self-descriptiveness | H | old `TargetWindow.qml`; `mock/model.cpp` |
| 12 | Logs were colour-only, noisy, and different in each app; no filter, copy or export. | N1/N9; ISO self-descriptiveness | M | old `LogView.qml` in app and mock |
| 13 | Three vocabularies for one concept and three error/cancel idioms across the suite. | N2/N4; ISO conformity/learnability | M | old `Main.qml`, `TargetWindow.qml`, `GeneratorWindow.qml` |
| 14 | First run was a mandatory form, not an explanation; it could not be revisited. | N5/N10; ISO learnability | M | old `FirstRunScreen.qml`; `loader_model.cpp` `firstRunComplete` |
| 15 | No text-size setting; type was literal in many views; focus and keyboard coverage were declared, not proven. | WCAG 1.4.4/2.4.7; ISO individualisation | M | old QML; docs/app.md "not checked" |

What the audit did **not** find: protocol errors. The engines are tested; the redesign is a view
layer and a small number of model-level reconciliations. The design does not change ARINC 615A or
665 behaviour.

## 5. Design direction

### 5.1 The idea

**A Modernist engineering console.** One accent, no decoration, every surface square, every
section numbered, every number monospaced, one primary action per state. The window is a rack
instrument: it tells you what it is, what it is connected to, what it will do, and what it just
did. It does not chat.

The reference is ICL Forge's `Theme.qml` and its workbench: Signal red on warm neutrals, Archivo
ExtraBold for statements, an 11 px mono for identifiers and ordinals, 4/8/12/16/24/32 spacing,
1 px control borders, 2 px section rules, 3 px active-tab rules, a fixed left rail and a fixed
bottom action bar.

### 5.2 Principles

1. **State before action.** The first thing on every surface is what is true now: target, set,
   check, tick count, run state. The action follows.
2. **One primary per state.** A filled accent button is the one thing the operator is meant to do
   next. Everything else is outlined, or a link.
3. **The primary never scrolls.** It lives in a fixed bar at the bottom of the work area. If the
   content is too tall, the content scrolls.
4. **Objects, not form fields.** A media set, a target, a load and a run are objects with identity,
   state and evidence. They are shown as such, not as a grid of labels.
5. **Preflight, then transfer, then receipt.** Every operation has the same three beats. The
   receipt is owned by the inputs and is cleared when they change.
6. **Identity is repeated, never assumed.** `THW_ID_POS` and the address appear in the rail, the
   preflight, the action sentence and the receipt.
7. **Density with rhythm.** Engineers want information; they do not want a wall. Numbered rails,
   2 px rules and a strict type scale make density scannable.
8. **Severity is a word plus a tone.** Success is neutral with a check mark; warnings and failures
   are the accent, and the word says which. Colour alone never carries a state.
9. **Keyboard is a first-class path.** Every action is reachable by Tab, Space/Return, arrows for
   segments; focus is a 2 px ring two pixels outside the control.
10. **Same family, one system.** The three windows share one token file and one component set.
    An `ADL`, `GEN` or `MOCK` mark is the only permitted difference of identity.

### 5.3 What we deliberately did not do

- No rounded cards, gradients, shadows or glass. ICL Forge is flat and square; a rounded card in
  this suite reads as a different product.
- No second accent. Warnings, errors, selected segments, progress and the wordmark all draw from
  one red ramp; success is neutral. A green "ok" token would be a second semantic colour and would
  make the family look like a monitoring dashboard.
- No wizard that hides fields. The generator and loader are for engineers; a step-by-step wizard
  would make the common case slower and the expert case impossible. We used numbered sections and
  a fixed action bar instead.
- No toast notifications. The activity log and the receipt are the record; a toast is not.

## 6. Information architecture

### 6.1 Data loader (`adl_app`)

```
┌ ADL  ARINC Data Loader ────────────────────────────── [READY] Preferences About ┐
├───────────────────────────┬──────────────────────────────────────────────────────┤
│ 01 TARGET                 │ TRANSFER  [ Upload | Download ]          [COMPLETE] │
│   status · address · THW  │                                                      │
│   FIND results            │ THE UPLOAD                                           │
│   properties              │ Upload 1 load to HNPFMS_L                            │
│                           │ [READY] ACM-SET-0001 · 2 members · 1 load             │
│ 02 MEDIA SET              │                                                      │
│   path · part · members   │ [ TARGET ] [ MEDIA SET ] [ LOADS ]                   │
│   check chip · re-check   │                                                      │
│   loads + ticks           │ Ready to upload                                      │
│   findings                │                                                      │
│                           │ PAYLOAD                                              │
│                           │ LOAD        CONTENTS                       STATUS    │
│                           │ ACM47...    ACM47.LUH · 2 data files...   100% ...   │
│                           │                                                      │
│                           │ ACTIVITY | TARGET CONFIGURATION          Clear Hide  │
│                           │ 14:38:15 ...                                         │
├───────────────────────────┴──────────────────────────────────────────────────────┤
│ Busy/ready line · progress           [Read information] [Upload] [Abort]          │
├──────────────────────────────────────────────────────────────────────────────────┤
│ ● Idle · HNPFMS_L · C:\...\two-member                                   v0.1.0    │
└──────────────────────────────────────────────────────────────────────────────────┘
```

The rail is fixed-width and scrolls itself; target is always above media set because it is the
higher-consequence object. The work area has one transfer switch (Upload/Download) because the two
share the target and the log but not the plan. The activity dock is collapsible; when it is
collapsed the plan gets the height.

### 6.2 Generator (`adl_media_gen`)

```
┌ GEN  ARINC Media Set Generator ───────────────────────── [READY] About ┐
├───────────────────────────┬──────────────────────────────────────────┤
│ 01 MEDIA SET              │ THE MEDIA SET                            │
│   part · members          │ ACM-SET-0001                             │
│   check values            │ [READY] 1 member · 2 files · 1 load       │
│   output                  │                                          │
│                           │ Add files... Move up Move down Clear     │
│ 02 LOAD                   │ NAME | KIND | CHECK VALUE | SIZE |       │
│   part · header           │ [DATA.LUP] [data] [none] 16 B  [Remove]  │
│   type · check value      │ [CONFIG.LDR] [support] [none] 12 B ...   │
│   targets                 │                                          │
│                           │                                          │
│ 03 BATCH                  │ Result receipt (when generated)          │
│   include · part          │                                          │
├───────────────────────────┴──────────────────────────────────────────┤
│ Ready: 2 files ...                       [Cancel] [Generate media set] │
└──────────────────────────────────────────────────────────────────────┘
```

The left rail is the set's definition; the right is the artefact and the action. Files are the
centre because they are the part that varies; the definition is a small, stable block.

### 6.3 Mock target (`adl_mock_target`)

```
┌ MOCK  ARINC Mock Target ──────────── [STOPPED] [Start] [Stop] About ┐
├───────────────────────────┬─────────────────────────────────────────┤
│ 01 IDENTITY               │ OPERATION                               │
│   fields · derived THW    │ Stopped                                 │
│ 02 PORTS                  │                                         │
│ 03 STORAGE                │ ACTIVITY                                │
│ 04 BEHAVIOUR              │  14:38:54 listening ...                 │
│ 05 FAULT INJECTION        │                                         │
│   [N ACTIVE] Reset all    │ STORED FILES                            │
│   wait/refuse/drop/...    │  NAME                              BYTES │
├───────────────────────────┴─────────────────────────────────────────┤
│ ● Stopped · TFTP 59 · FIND 1001 · HNPFMS_L                 v0.1.0   │
└─────────────────────────────────────────────────────────────────────┘
```

Fault injection is its own numbered block with an active count and a reset; it is visually a
warning surface, not an innocent form. Start/Stop live in the header so the run state is always
one glance away.

## 7. The design system

The implementation lives in `src/ui`:

- `qml/Theme.qml` — Signal palette, type scale, spacing, focus, motion, quantity helper.
- `qml/AppButton.qml`, `AppCheckBox.qml`, `AppComboBox.qml`, `AppSpinBox.qml`,
  `AppTextField.qml` — the standard controls.
- `qml/AppHeader.qml`, `AppStatusBar.qml`, `Card.qml`, `RailBlock.qml`, `SectionHeader.qml`,
  `StatusChip.qml`, `StatTile.qml`, `CheckTile.qml`, `EmptyState.qml`, `TaskProgress.qml`,
  `BusyMark.qml`, `LogView.qml`, `FocusRing.qml`, `AppDialog.qml` — the surfaces.
- `fonts.cpp`, `fonts/` — Archivo (SIL OFL 1.1), registered before the QML engine starts.
- `window_state.cpp` — window geometry persistence.

The files are source-shared, not a separate QML module: each application's `qt_add_qml_module`
includes them with a root resource alias, so a view names `Theme.accentFill` without an import and
there is one source of truth. `cmake/AdlUi.cmake` does the alias and font-resource work.

The full token and component reference is [`docs/design-system.md`](design-system.md). The three
rules that matter most:

1. **Zero radius.** Nothing is rounded except a semantic status dot (6 px dot, 3 px radius).
2. **One accent, two roles.** `accent` is for shapes; `accentFill`/`accentText` is for a labelled
   surface. A label never sits on the raw Signal-light accent (3.95:1); the fill is the deeper
   `#b82009` that clears 4.5:1.
3. **The scale is the layout.** No literal type sizes; no 17 px gaps; 1 px borders, 2 px section
   rules, 3 px active-tab rules.

## 8. Behaviour changes in this pass

The redesign is not only paint. These are the model-level changes that make the visual promises
true:

- **`LoaderModel::reconcile_target()`**: changing the address or `THW_ID_POS` clears the FIND
  selection, the target configuration and a transfer receipt. The log is kept. This closes the
  "old target's configuration on screen" defect.
- **`LoaderModel::resultIsTransfer` / `resultWasDownload`**: a media set read or check no longer
  looks like a completed transfer in the mode strip or the receipt, and a download's receipt is
  shown in the Download view rather than under the Upload plan. The top strip now says `READY` /
  `NOT READY` unless the last job was an Information/Upload/Download.
- **Window geometry** is restored and saved in C++ (`window_state.cpp`) for all three windows,
  clamped to the current screen. The old windows always opened at 1100×760 in the same place.
- **First run is no longer a mandatory form.** `Start loading` opens the workbench; the defaults
  are optional and folded away; choosing a media set from the first run also completes it.
- **Preferences are transactional and a page, not a floating form.** The dialog edits a working
  copy and writes on Save; Cancel and Esc leave the stored values alone. Language is the one
  immediate choice and says so. The two-column layout keeps every setting visible without a
  scroll, and the window behind it is dimmed, so it reads as a modal page rather than a panel over
  a live console ([after](design/screenshots/after-preferences.png)).
- **The About page carries the build provenance** the other ICL tools show: version, git describe,
  full commit, branch, build target, Qt version and a dirty-tree marker, generated at configure
  time by `cmake/AdlUi.cmake` and exposed as `appVersionDetails`. The licences and the repository
  link sit beside it ([after](design/screenshots/after-about.png)).
- **Close during a job asks first.** The dialog offers "Keep working" and "Stop and close"; the
  second asks the engine to abort and waits for it to be idle before closing.
- **The generator no longer shows red errors on untouched fields.** It shows an error only once a
  field has content; the action bar states what is missing. A `NO CHECK VALUES` chip makes the
  vacuous-verification case visible.
- **The mock target derives and copies `THW_ID_POS`**, distinguishes `SERVING` from `STOPPED`,
  summarises active injections and offers `Reset all`; Start is disabled without a hardware ID,
  and an unset storage directory is called out as "still serves, not kept".
- **Pluralisation** is real prose (`1 load`, `2 loads`) in the new surfaces, not `load(s)`. The
  loader's and mock target's visible log and summary lines, the find result, the media set title
  and the `adl_write_payloads` summary were swept too.

## 9. Verification

- The three windows load with **zero QML warnings**: `adl_app_qml_test`, `adl_gen_qml_test` and
  `adl_mock_qml_test` were rebuilt against the new modules and run with the offscreen platform and
  software renderer. The application test now also opens Preferences and About and fails if either
  dialog does not open, so the modal page and the provenance block are covered.
- The application test drives the real model through first run, FIND, Information, a media set
  read and check, an Upload and a media-defined Download, and asserts the outcomes. It passes.
- The Qt Quick Test UI suite added by `feature/test-suite-coverage-ui` now drives this window
  rather than the earlier one: `ui.tst_main_window` clicks the real Find, Read information,
  Re-check, All/None, Upload, Abort and Download controls, and `ui.tst_dialogs` opens
  Preferences, the media set and target properties and About and drives their fields and
  standard buttons. Both pass, and Preferences is asserted to be transactional.
- The generator test fills the form, starts a generation through the window's own button, and
  asserts a verified set; it passes.
- The mock target test starts and stops the target through the window's own buttons; it passes.
- The screenshots in [`design/screenshots`](design/screenshots) are the record of the surfaces
  before and after, including the two dialogs.

### Performance

The tests run in a Debug build; **the release build is the one to judge by feel**, and the
difference is measurable. A scripted 240-step resize at 1200×900 on the RTX 4090 machine cost
about **0.6–0.7 s of CPU in Release** and **1.9 s in Debug** over the same 6 s wall time, both with
0 ms CPU idle over 2 s. The renderer is D3D11, threaded render loop, max frame latency 2.

Two QML costs were removed in this pass: a checkbox whose label width fed its own implicit width
(a resize-time binding feedback), and one `positionViewAtEnd` call per log line (now coalesced to
one per frame). Preferences no longer builds a scrolling form that can overflow its dialog, and
the dialogs draw one scrim item rather than a full-window overlay per popup. Frame times during a
scripted resize are still around 18–22 ms at 1200×900 on this machine, so a 60 Hz live resize is
layout-bound; the release build is the honest comparison, and the remaining work is to move the
heaviest nested layouts (the rail and the payload table) to fixed lanes if that becomes the
priority.

Not verified in this pass: a screen reader, a real target over a real network, 175% text size, and
the Windows/macOS builds of the new QML. Those are in §11.

## 10. What the operator can do now that they could not before

- See the target's `THW_ID_POS` and address at the same time as the media set and the ticks.
- See why Upload is disabled, and act on the reason from the same panel.
- See exactly which loads are in the payload before starting.
- See the destination directory before a download, and open the last one afterwards.
- Stop a job and close the window without a silent 13-second hang.
- Tell a successful media set check from a successful transfer.
- See when the mock target is running, what it is doing, and which faults are armed.
- Know when a generated set has no check values.
- Work in one visual language across the loader, the generator and the mock target.

## 11. Remaining work, ranked

1. **Bind "checked" to a time and a file identity.** Add the check time and a manifest/mtime
   fingerprint to `MediaSetIndex`; mark the set stale when a file changed, and refuse or warn
   before an upload. This is the largest remaining correctness gap.
2. **Distinguish abort/cancel from failure in the outcome model.** The engine knows the abort
   reason; the receipt should say `CANCELLED` or `ABORTED BY OPERATOR` with the target's
   confirmation, not `FAILED`.
3. **Target confirmation before an automatic FIND operation.** The preflight repeats the target,
   but a sole automatic FIND still starts without an explicit confirmation. A one-line
   "HNPFMS_L answered; use it?" confirmation for the auto path would close this.
4. **Refresh the translation catalogues.** The new QML and the changed C++ strings are not in the
   eight `.ts` files; the source text falls back to English. Run lupdate, review the new strings,
   and add the plural forms the catalogues now need.
5. **Download directory preview.** Add the next free `DNLD_DATA_<THW_ID_POS>_<n>` to the model so
   the destination can be named before start, and make the aborted-directory consumption visible.
6. **Preferences: text size and motion.** Add a persisted 100/125/150/175% scale and a reduced
   motion switch to the three applications; `Theme.fontScale` and `Theme.reducedMotion` are the
   hooks. Then run the 175% layout pass.
7. **Keyboard and screen-reader pass.** The controls carry roles and names and a ring; no NVDA or
   Orca pass has been run. The activity log, stored-files list and file table need list-item names
   that include the row identity.
8. **The generator's success receipt should clear on edit.** The engine's `resultState` survives a
   field change; a dirty marker or a `resultChanged` reset on the next edit is needed.
9. **The mock target's Stop should be a state, not an instantaneous flag.** Today `running` clears
   before the engine loop returns; a `STOPPING` state and an "operation stopped" line would make
   the last frame truthful.
10. **Real target and real hardware.** The windows have not been driven against a real 615A target;
    the loopback integration test exercises the engine and the mock target, not the window.

## 12. Acceptance criteria for the next pass

The next pass is done when:

- A new operator can start the loader, choose a set, find a target and upload without reading the
  repository docs, and can say afterwards which set went to which target.
- A media set changed on disk after a check cannot be uploaded as "checked" without an explicit
  re-check or a warning naming the changed file.
- Closing during a job always states what will happen and never blocks invisibly.
- The three windows can be placed side by side and a person cannot tell which one was written
  first.
- The QML suites pass with every warning an error, at 100% and 175% text size, in light and dark,
  and a keyboard-only walk of the primary path succeeds.

## 13. Reading the code

- `src/ui/` — the design system and window chrome.
- `src/app/qml/Main.qml` — the loader shell, target and media set rails, action bar.
- `src/app/qml/UploadPanel.qml` — the upload plan, preflight, payload, receipt.
- `src/app/qml/DownloadPanel.qml` — the download modes, files and destination.
- `src/app/qml/ActivityDock.qml` — activity and target configuration.
- `src/app/qml/FirstRunScreen.qml` — the opening statement.
- `src/media_gen/qml/GeneratorWindow.qml` — the generator.
- `src/mock_target/qml/TargetWindow.qml` — the mock target.
- [`docs/design-system.md`](design-system.md) — the token and component reference.
- [`docs/design/screenshots`](design/screenshots) — before and after.

## 14. Building and looking at it

```sh
. C:\Repos\common\env.ps1
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
build/windows-msvc-debug/bin/adl_app
build/windows-msvc-debug/bin/adl_media_gen
build/windows-msvc-debug/bin/adl_mock_target
```

The window tests load the same compiled QML modules the applications do, with every engine
warning an error; they are the quickest way to see the three windows without a desktop.


# The companion seam: how a window starts an installed Pro tool

This is a plan: a design, with no code. It is not published (`plan/` is excluded from the site, `mkdocs.yml`). It is package C8 of the Pro features plan (`pro-features.md`, section 8.3), and it answers the four questions that package gives: how a window finds a companion program, how the manifest is signed and checked, what the window hands over, and what the user is asked. It also says what the window never does, what the two editions show in a window, and what is left to later packages.

Written 2026-11-20. What was read for it: sections 6 and 8.3 of `pro-features.md`; the contract of the public commands (`headless-commands.md`); the private repository (`main` at 70e1c67 for the plan, the file formats and the guides of `adl_rig`, `adl_records` and `adl_lab`, and its `agent/integration` branch for the licence page of the guide); VSS's plugin design (`plugins.md`, with its `plugin-api-reference.md`) and the signature code of its `vss_plugin` library, at its `origin/develop` (dea04617); and this repository's licence library (`libs/licence`). Nothing was built or run for this page.

**How to read the markers.** *Decided* means the owner has decided it, and the plan page or decision is named. *Proposal* means it is mine, for the owner to accept or change. Every proposal that needs an answer is collected, with a default, in [section 11](#11-decisions-for-the-owner).

## 1. In short

- **What it is.** A companion is a Pro program that is installed on the same computer as a window: `adl_rig`, `adl_records` and later `adl_lab`. A window can start one as a child program, give it files by path, and read its exit code and its `--json` events. Nothing is linked. There is no plugin, no shared memory and no connection between the two.
- **How it is found.** By a folder with a signed manifest, `companion.json`, in one of two places: beside the window's program, or in the user's data folder. The window searches nowhere else ([section 4](#4-how-a-window-finds-a-companion)).
- **How it is checked.** The manifest is signed with the signed-file envelope that the licence library already has, with the purpose `companion`. The signature names the manifest by the hash of its exact bytes, and the manifest lists every file with its size and SHA-512. The key is built into the window. This is VSS's scheme with its root key, its third-party levels, its trust store, its policy file, its sandbox and its plugin protocol left out ([section 5](#5-the-manifest-and-its-signature)).
- **What the window hands over.** Paths: a plan file, an operation record, a baseline, a folder. It never gives a key, a password or a licence ([section 6](#6-what-the-window-hands-over-and-what-it-reads-back)).
- **What the user is asked.** Almost nothing. A companion that is missing, changed, or newer than the window is said in one sentence. A missing licence is the Pro program's own sentence, shown unchanged ([section 7](#7-what-the-user-is-asked-and-what-is-said)).
- **What is never done.** No download, no connection, no count, no refusal to run. Enforcement is by the terms of the licence ([section 8](#8-what-is-never-done)).
- **Editions.** The plan's first version shows only the Licence screen. The seam adds nothing to a Community window that has no Pro package, and it never hides or greys anything because of a licence ([section 9](#9-what-the-two-editions-show-in-a-window)).
- **What it needs.** The licence library from slices 2 to 4, the operation record file (C1b and C2), and four small additions to public interfaces that the owner has not yet agreed ([section 11](#11-decisions-for-the-owner)).

## 2. What the plan has already decided

| Point | Source |
| --- | --- |
| A Pro program does not link the public libraries. It starts public commands as child programs and reads their JSON lines. | `pro-features.md`, 6.2 and decision A |
| A paid feature is a separate program, not a switch. Nothing in the three windows is locked, greyed or hidden by a licence. | 6.1 |
| Each Pro program reads the licence itself and asks for its own feature. No licence: one sentence, and it stops, with exit code 5. A run that has started finishes. A program that only reads or checks asks for no licence. | 6.3, and the private plan, section 2 |
| A wrong clock is said in one line and the program carries on. | decision G |
| A seat is one instance of the product in use, held to by the terms. Nothing counts instances or contacts a server. | decision B, and the private plan, section 1 |
| The licence library is under GPL-3.0-or-later OR BSD-2-Clause, so that a closed program can link it. | decision F |
| In the first version the windows show only the Licence screen, with a row for each pack. | 6.5 |
| If Pro is to be visible in the windows, the way is a companion seam: a window starts an installed companion and hands it what it has. Three uses: keep every receipt in the records archive, run a load plan from the loader, and compare a target with its baseline after Information. The seam is public, and a companion is found by a signed manifest, as a VSS plugin is. | 6.5 |
| It reuses VSS's manifest and signature and says what it leaves out. It is a design page, not code, and not before `adl_rig` and `adl_records` are in use. | 8.3, C8 |
| A plugin framework is not built. A plugin is for something that has to appear inside a window. | 6.6 |
| Reading what you made never needs a licence. | section 3, rule 5 |
| The program names are `adl_rig`, `adl_records`, `adl_lab`, `adl_media`. | decision C |

Everything below that is not in this table is a proposal.

## 3. Words

- **The window**: a Qt program of this repository. In the first version only the loader, `adl_app`.
- **A companion**: one installed package of Pro programs and the manifest that describes it. The word names the package, not one program. The manifest lists the programs.
- **A role**: one thing a window knows how to ask of a companion. The roles are a fixed list in the window's code. A manifest says which of its programs fills which role. It never adds a role, and it never supplies an argument or a word that the window shows as its own.
- **A root**: a folder in which the window looks for companion folders.
- **The Pro package**: the archive of the private repository's package step. It unpacks to one folder, with `bin/` for the programs, the guide and a few text files.

## 4. How a window finds a companion

Proposal. It follows VSS's three roots, with the ones ADL does not need taken out.

| Root | Where | Who writes it |
| --- | --- | --- |
| Beside the window | `companions` in the folder of the window's program (`adl::platform::executable_path()`) | The installer, or an administrator |
| For this user | `companions`, beside the user's `licences` folder: `%LOCALAPPDATA%\ARINC Data Loader\companions` on Windows, `$XDG_DATA_HOME/arinc-data-loader/companions` or `~/.local/share/arinc-data-loader/companions` on Linux, `~/Library/Application Support/ARINC Data Loader/companions` on macOS (below `adl::platform::user_data_directory()`) | The user |

- **A companion is a folder** in a root, with `companion.json`, `companion.sig` and the programs it lists. The user installs the Pro package by unpacking it into a root. The folder's own name means nothing: the manifest's `id` does.
- **`ADL_SETTINGS_DIR`** works as it does for the licence folders. When it names a folder, the window searches `companions` inside it and nowhere else, so that a test never finds a real companion.
- **Nothing else is searched.** Not the `PATH`, not the registry, not other drives, not a network share, not a folder that the manifest names. The window never makes the folders, and never writes to them.
- **When.** Once, when the window starts, on a worker thread, and again when the user asks. The window does not watch folders.
- **Limits.** At most 16 companion folders in all. A manifest of at most 64 KiB (the licence library's limit for a signed file) and at most 64 listed files. A symbolic link or a junction is not followed. An entry whose name starts with a dot is skipped.
- **The same `id` in two roots.** The first root in the table wins, and the other is listed under it as `shadowed`. A copy in the user's folder never replaces one that came with the installation.
- **No key, no search.** The window's trust list holds no key of the purpose `companion` until the owner adds one. With none, the window looks for no companion and says nothing, as a window with no licence key is the Community edition.

**Left out: the machine folder.** VSS has a third root for plugins that an administrator installs for everyone. ADL's installation folder already is that place, and `adl::platform::machine_config_directory()` is `/etc` on Linux, which is no place for programs. If an administrator wants a companion for every user, they unpack it beside the window.

**Left out: who may write the folder.** VSS reads the owner and the access rights of each root and rejects a root that others can write to. That is platform code for each system and is left to a later package. Until then the check of [5.3](#53-what-the-window-checks) is what stands between a planted file and a start, and [5.5](#55-what-a-signature-shows-and-what-it-does-not) says how far that goes.

## 5. The manifest and its signature

### 5.1 Files

```text
companion.json    the manifest, plain JSON that a person can read
companion.sig     a signed-file envelope, purpose "companion"
bin/adl_rig       the programs and every other file the manifest lists
```

The envelope is the one in `libs/licence/include/adl/licence/signed_file.hpp`: `{format, purpose, key_id, payload, signature}`, Ed25519 over `ICL-SIGNED-FILE-1`, a zero byte, the purpose, a zero byte and the payload bytes. The purpose is inside what is signed, so a licence can never pass as a manifest, nor a manifest as a licence or a revocation list. VSS has the same envelope under its own prefix, `VSS-SIGNED-FILE-1`. The two are one library after the move to a shared repository (licensing plan, section 6), and the prefix is whatever that library says.

### 5.2 An example

`companion.json`. Proposal. The key names are mine.

```json
{
  "manifest_version": 1,
  "id": "adl-pro",
  "name": "ARINC Data Loader Pro tools",
  "version": "0.1.0",
  "system": "windows-x64",
  "seam": 1,
  "critical": [],
  "tools": [
    { "name": "adl_rig",     "file": "bin/adl_rig.exe",     "feature": "pro.automation", "roles": ["run_plan"] },
    { "name": "adl_records", "file": "bin/adl_records.exe", "feature": "pro.records",    "roles": ["keep_record", "audit_record"] },
    { "name": "adl_lab",     "file": "bin/adl_lab.exe",     "feature": "pro.lab",        "roles": [] }
  ],
  "files": [
    { "path": "bin/adl_rig.exe",     "size": 1234567, "sha512": "<128 hex characters>" },
    { "path": "bin/adl_records.exe", "size": 2345678, "sha512": "<128 hex characters>" },
    { "path": "bin/adl_lab.exe",     "size": 1345678, "sha512": "<128 hex characters>" },
    { "path": "README.txt",          "size": 512,     "sha512": "<128 hex characters>" }
  ]
}
```

The payload of `companion.sig`, as the exact bytes that were signed:

```json
{ "format": 1, "id": "adl-pro", "version": "0.1.0", "issued_at": "2026-11-20T10:00:00Z",
  "manifest_sha512": "<128 hex characters: the SHA-512 of the exact bytes of companion.json>" }
```

| Key | Meaning |
| --- | --- |
| `manifest_version` | The format of this file. The window refuses a number it does not know. A change that only adds an optional key keeps the number. |
| `id` | Lower-case letters, digits and hyphens, at most 63, as every id in the Pro formats. It is the companion's identity. |
| `name`, `version` | Shown as plain text. `version` is the Pro package's version. |
| `system` | `windows-x64`, `linux-x64` or `macos-arm64`, as the package's own `VERSION.json` says. A manifest for another system is `wrong_system`, so that the user is told that and not "could not start". |
| `seam` | The number of this seam that the companion speaks. 1 here. The window knows every number up to its own. |
| `critical` | Names of keys that a reader must understand. A name the window does not know is `needs_newer_window`. This is the licence file's rule. The window ignores any other key it does not know, so a Pro package may add keys without a new window. |
| `tools[].name` | The program's name, written without `.exe` as the public contract writes it. `<name> --version` must say `<name> <version> events <n>`. |
| `tools[].file` | The path of the program in the folder, as it is on this system. |
| `tools[].feature` | The licence feature the program asks for. Shown beside the tool's name. The window never tests it. |
| `tools[].roles` | Which roles of the window the program fills: `run_plan`, `keep_record`, `audit_record`. A role the window does not know is ignored, so the tool is listed and not offered. |
| `files` | Every file of the folder, with its size and SHA-512. The package step writes this list. |

### 5.3 What the window checks

In this order, stopping at the first failure. It runs on a worker thread. Each failure is a reason, which [section 7](#7-what-the-user-is-asked-and-what-is-said) turns into a sentence.

1. **Read `companion.json` once, into memory, and do not parse it yet.** At most 64 KiB. Everything after works on these bytes, and the file is not read again for the decision.
2. **Read `companion.sig`.** The envelope's five fields, at most 64 KiB, and nothing else is read before the signature has been checked. No file: `signature_missing`.
3. **Check the signature.** The purpose is `companion`. The key is the one the envelope names, found in the window's own trust list under that purpose, never in the file being checked and never in the operating system's certificate store. The key's dates must cover the payload's `issued_at`, and the key must not be withdrawn. An unknown key is `needs_newer_window`, as it is for a licence: the window cannot tell it from a key that a newer release has. A bad signature is `signature_invalid`. A withdrawn key is `key_withdrawn`.
4. **Check the payload against the manifest.** The SHA-512 of the manifest's bytes, and its `id` and `version`, are the payload's. Otherwise `signature_invalid`.
5. **Read the manifest.** The licence library's strict JSON reader, at most depth 8: no repeated key, no comment, no byte order mark. A wrong type or an id that is not allowed is `manifest_invalid`. A `manifest_version`, a `seam` or a `critical` name that the window does not know is `needs_newer_window`. A `system` that is not this one is `wrong_system`.
6. **Check the files.** Every listed path is relative, with no `..`, no drive letter and no alternate data stream, and no two differ only in case. Each is a regular file with the listed size and SHA-512. No other file is in the folder: `files_differ`.
7. **At the first use of a tool,** run `<tool> --version` (a program that reads and checks, so it needs no licence), and compare the name and the events number: `needs_newer_window` for an events number the window does not know. This is the only step that starts a program. The window does it when the user first asks for a role, and keeps the answer until it closes.

**A start does not trust the earlier answer.** Steps 1 to 6 run again when the user asks for a role, and the program's own file is hashed once more just before it is started.

Hashing a few tens of megabytes is a fraction of a second. Steps 1 to 6 never run on the thread that draws the window.

### 5.4 What is taken from VSS, and what is left out

VSS's design is `plugins.md`, sections 5, 8, 12 and 13, and the code of its `vss_plugin` library. The reason for most of what is left out is one fact: a VSS plugin is a program that works inside the engine's traffic, and a companion is a command that runs to its end and writes a result. A companion has no channel into the window.

**Taken**

| What | Why |
| --- | --- |
| One signed-file envelope for every signed file, the purpose inside the signed message | It exists in `adl::licence` already, with its tests, its fuzz harness and its vectors. A second scheme would need all of that again. |
| The signature names the manifest by the hash of its exact bytes | There is no canonical form to get wrong, and the manifest stays a file a person can read. |
| The manifest lists every file with its size and SHA-512, and no other file is allowed | A signature over the manifest then covers the programs. |
| The key is never taken from the file being checked, and a key has one purpose | A key from the file proves nothing. A licence key must not sign a manifest. |
| Ed25519 and SHA-512 through the vendored Monocypher | Already vendored, with no preprocessor. |
| A key's dates, and a key that can be withdrawn | The licence library has both (`not_before`, `not_after`, `revoked_from`). |
| Strict reading, a size limit, a depth limit, a fuzz harness | The manifest is bytes from outside. |
| A start repeats the check | A cached answer is not a reason to start a program. |
| Test keys (`test-` prefix) that the trust list of a customer build never returns | The same rule as for licences. |
| A reason code for each failure, and a sentence for each | One state, one reason, as VSS and the licence screen do. |
| A tool's own text is shown as plain text and quoted, never mixed into the window's sentence | A sentence from another program is not the window's own words. |
| A manifest is for one system | A package is built for one. |
| The plugin folders are searched in a fixed order, a copy in a second place is `shadowed`, and a link is not followed | Less to explain, and a user's copy cannot take the place of an installed one. |

**Left out**

| What | Why, and what it would take |
| --- | --- |
| The root key and the signed key list | The trust list built into the window does the same job, and a new key reaches a computer in a new release in both designs. VSS's list could also arrive beside a plugin or as an imported file, and it built neither. Add it if keys must change without a new window. |
| Third-party, unsigned and user-trusted companions, the trust store (`trust.json`), key import and fingerprints | Only the owner's Pro package is a companion in the first version. A window that starts any program found in a folder is how a seam becomes a way in, and a decision dialog needs a store and seven catalogues of text. A site's own companion could come later, with a public key file that the site trusts by choosing it, as `adl_records verify --trust` takes a `.adlpub`. |
| The administrator's policy file | There is nothing for it to narrow. An administrator who does not want the Pro tools in a window does not unpack them into a root. |
| The sandbox, the permissions and the launcher | A companion is a program the user installed, run with the user's rights, as a command typed in a shell would be. The window gives it no secret and no channel. VSS needs a sandbox because its plugins touch live traffic. |
| The plugin protocol (`hello`, `ping`, `status`, messages), extension points, parameters, instances, supervision, restart and quarantine | A companion's protocol is its exit code and its `--json` events. Both exist and are in the public contract. |
| `not_after` and the clock ledger | A manifest that lapsed would stop tools that a bench with no network still has, and that is a refusal to run. The Pro program's own licence check, with its grace period, is where a term ends. (The key's dates are still checked.) |
| A revocation list for companions | Withdrawing a key withdraws what it signed, through the list that licences already use. Withdrawing one release of a companion is not offered. |
| The licence in the check (`disabled: not_licensed`), `requires.edition`, `released` | The window never refuses on a licence ([section 8](#8-what-is-never-done)). The Pro program decides, and the window shows the program's sentence. |
| Handing the licence file to the companion | The Pro programs read the licence themselves, from the licence folders. |
| `requires.capabilities`, `requires.runtime`, the plugin API range | There is no engine capability to ask for, and no interpreter: the programs are native. (The Python adapters are a separate wheel and not a companion.) |
| The official-only badge ("Signed by Iain Chesworth Labs") as a trust level | There is one level. The window may say who signed, from the key's record, as plain text. |

### 5.5 What a signature shows, and what it does not

It shows that the files in the folder are the files that the holder of the `companion` key signed, at the time the window checked them. It does not show that the programs are safe, and the window never says that a companion is "verified". It says who signed it.

Between the check and the start, a program that can write to the folder could replace a file. The window hashes the file just before it starts it, and a gap of milliseconds remains. A root that anyone can write to is a root that anyone with the user's rights can write to, and such a program could start the Pro tools itself. The check protects against a damaged copy, a copy from the wrong place, and a changed file. It does not protect against a hostile program that already runs as the user. VSS makes the same point about its signatures: they show who signed and that the files did not change afterwards, and not that a plugin is safe (`plugins.md`, 8.1).

A program may load libraries that are not in the folder (those of the system). The signature does not cover them. The Windows package uses a static runtime. The Linux package uses the system's glibc and libstdc++ and records the versions it needs (private plan, P7).

### 5.6 Keys

Proposal. The owner makes the keys, on their own computer, as for licences.

- A key of the purpose `companion`, apart from the licence key and the revocation key. A leak of one does not touch the others. One for use and one in reserve, as for licences.
- Its public half goes into `libs/licence/src/trusted_keys.cpp`, which today holds none. Its private half is in no repository and on no computer that builds or runs anything.
- The signing step runs on the owner's computer, after the Pro package is built. The package step writes `companion.json` and the owner signs it to make `companion.sig`, as the owner signs the checksum file of a release. The private repository never holds a key.
- A try-out package (`package_not_for_delivery` in the private repository) is not signed with a customer's key. A window with a customer's trust list says `signature_missing` for it, or `needs_newer_window` if it was signed with a key that list does not hold. A window built for tests trusts a `test-companion` key and accepts a manifest signed with it. This is the licence rule: a file made for tests is not accepted by a program built for customers.

## 6. What the window hands over, and what it reads back

**Rules for every role**

- **By path, never by linking.** The window gives a file or a folder by its full path, as UTF-8, through `QProcess`, which on Windows passes it as UTF-16. A path with any character is passed whole. Nothing is passed as a shell string: a role is a program and a fixed list of arguments that the window builds.
- **Nothing from the manifest is an argument.** The program is `file` of the manifest, joined to the companion folder. It is never found on the `PATH`.
- **The environment is the window's own,** unchanged. `ADL_SETTINGS_DIR` and `ADLPRO_LICENCE_FILE` then work for the companion as they do at a command line. The window sets neither.
- **The working directory** is a folder the window makes for the call, never the companion's folder. The Pro tools write to `runs` in the working directory when `--out` is not given. The window always gives `--out`.
- **Standard input** is closed. No Pro tool reads it.
- **What is never passed:** a licence, a licence path, a key, a passphrase, a password, a token, the name of the user. The window does not open a key file, a baseline or a plan. It passes the path of the one the user chose.
- **Output.** The window reads standard output and standard error as they come, on the thread that handles the process, and never waits for a companion on the thread that draws. A line is at most 1 MiB. The window keeps the last 2,000 lines of events and the first 64 KiB of standard error.
- **One companion call at a time** in the first version, so that two never write to one folder.
- **A loader's port.** Two loaders cannot share port 59 on one address (`headless-commands.md`, 2.1). `run_plan` starts `adl_load` through `adl_rig`, so while it runs the window starts no operation of its own, and while the window runs an operation it does not start `run_plan` and says why. `keep_record` and `audit_record` touch no target and open no port. Whether an idle window holds the control port was not checked here; the package that builds the window checks it.

**How a companion's output is read** (the public contract, section 1, and the private formats page, 1.1)

- With `--json`, standard output is JSON lines and nothing else. `event` is the first key. The first event is `started`, with `events`: the window refuses a number it does not know (`needs_newer_window`). A key it does not know is ignored. The last event is `finished`, and its `exit_code` must be the program's exit code. If they differ, or there is no `finished`, the window says that the tool did not do what its contract says, and it shows the exit code and the last lines of standard error. The window treats that as a failure and does not try again.
- A tool that stops before the run starts (exit 2 to 6) writes one sentence to standard error and no event. The window shows that sentence.
- The window takes `reason` words from its own table and writes its own, translated, sentence for each one it knows. For a word it does not know it shows the tool's `detail`, in English, as plain text and quoted.
- The licence line on standard error ("ends in 12 days", "has ended", "the clock looks wrong", "Development build") is one line of the activity log. It is not an error and the window does not act on it.

**Exit codes of every Pro tool** (private formats page, section 6)

| Code | The window says |
| --- | --- |
| 0 | Done, and the outcome is good |
| 1 | Done, and the outcome is not: a step failed, a finding. Not a fault of the tool |
| 2 | The tool did not take the options the window gave it. A fault in the window or the tool: show the details, ask for them to be copied |
| 3 | A file was read and is not valid. The tool's sentence says which and where |
| 4 | `adl_load` (for `adl_lab`, `adl_mock`) was not found, or speaks an events number the tool does not know |
| 5 | The licence does not cover this tool. The tool's own sentence, unchanged, and a button to the window's Licence screen |
| 6 | A file or folder could not be read or made. The tool's sentence |

**The roles of the first version**

| Role | Program | Feature | What the window gives | What it reads back |
| --- | --- | --- | --- | --- |
| `keep_record` | `adl_records keep <record> --archive <folder> [--key <file.adlkey>] [--signed-by <text>]` | `pro.records` | The operation record of the operation that has just ended, written by the window to a private temporary folder with the C2 writer. The archive folder and the key file are settings the user made once. | The exit code. `keep` has no `--json`. Its lines, such as `already kept`, are for a person and are not a promise, so the window shows them as plain text and decides nothing from them. The temporary file is deleted afterwards. |
| `audit_record` | `adl_records audit <baseline> --from-records <folder> --out <folder> --json` | `pro.records` | A baseline file the user chose, and a temporary folder with the record of the Information operation that has just ended. The tool touches no target. | `started`, `target_ended` (`match` or `findings`), `finished` (`result`, `reason`, `findings`, `run_folder`). Exit 0 matches, 1 findings. The window says "matches the baseline" or "N findings" and offers the report (`audit.txt`) in the run folder. |
| `run_plan` | `adl_rig check <plan>` first, then `adl_rig run <plan> --out <folder> --json [--loader <adl_load>]` | `pro.automation` | A load plan file the user chose. `--loader` only when `adl_load` is in the window's own folder. | For `check`, the exit code and its text (`valid`, or each error with its line and column), shown as plain text. For `run`, `started`, `step_began`, `step_ended`, `target_ended` and `finished`, with `run_folder`, the counts and `exit_code`. The window shows steps done of steps in all, each target's result, and one sentence at the end, and offers the run folder and "Keep the records", which is `keep_record` over the run's records. |

The Pro programs write `.adlplan`, `.adlbase`, `.adlrack` and `.adlrec` files. The window does not read or write any of them. They are Pro's own formats, and the window only passes a path.

**Roles not in the first version.** `adl_lab run <rack> --json` fits the same rules: its events are `started`, `target_started`, `target_stopped` and `finished`, with `target_id` and `at` on the targets' own events, and a run that the user stops ends with `interrupted` and exit code 0. It would belong in the mock target's window, and it is not proposed now. `adl_rig resume <run folder>` is a small addition to `run_plan` and is left to its package. `adl_media` has no program yet.

**Stopping.** Proposal. A Stop button asks the tool to stop, as Ctrl-C does at a terminal, and a second press ends the program at once. `adl_rig` and `adl_lab` stop their children when asked, write their summaries, and end with 1 (`adl_lab` with 0). On Linux and macOS the request is SIGTERM. On Windows the tools handle Ctrl-C and Ctrl-Break. A window has no console, and how it sends either to a child that is in a group of its own was not tried here: the package that builds it must try it on Windows before Stop is promised. It is platform code and a new function in `adl::platform`, so it is a public interface addition for the owner ([section 11](#11-decisions-for-the-owner)). A program that is ended by force leaves `adl_load` programs to end by themselves, and `adl_rig resume` can go on from the state it left, which says `running`.

**Closing the window** while a companion runs asks whether to stop it. It never leaves a companion running without saying so.

## 7. What the user is asked, and what is said

The user is never asked to trust a key, to approve a companion, to type a licence number, to sign in, or to allow a connection. There is nothing to approve. The words below are the window's own, translated like the rest of the window, with one exception: a sentence from a Pro tool is English, quoted, and marked as the tool's.

| Situation | What the window shows | Status |
| --- | --- | --- |
| No companion in any root (the Community edition, and a Pro licence with no package) | Nothing in the menus or the loader. In a list of Pro tools that the user opens, one sentence: "No Pro tools were found on this computer." and the two folders that were searched. | Proposal. The plan's first version shows nothing ([section 9](#9-what-the-two-editions-show-in-a-window)) |
| A companion found and checked | Its name, version and signer, its tools, and a line for each role it fills. The roles then appear where [section 9](#9-what-the-two-editions-show-in-a-window) puts them. | Proposal |
| A companion that fails a check | A line in the list, greyed, with the reason as a sentence, and the path. **Show details** gives the reason code and what was compared. There is no "run anyway". The tool can still be run at a command line, and the sentence says so. | Proposal |
| A companion newer than the window knows (`needs_newer_window`: a manifest format, a `seam`, a `critical` name, a signing key or an events number that this window does not have) | "These Pro tools need a newer version of ARINC Data Loader." The window names the version it is. It does not offer a download and does not open a page. | Proposal. It is what the licence screen says for a licence that needs a newer version |
| A companion older than the window can use | Does not arise: the window keeps every `seam` number it has ever known | Proposal |
| A file changed (`files_differ`) | "A file of these Pro tools was changed after it was signed: `<path>`. Copy the Pro package again." | Proposal |
| A signature that does not hold, or a key that was withdrawn | "The signature does not match their files." or "The key that signed these tools was withdrawn." | Proposal |
| A wrong system (`wrong_system`) | "These Pro tools are for `<system>`." | Proposal |
| The licence is missing, has ended, has no such feature, or is for another computer | The window starts the tool anyway. The tool stops with exit code 5 and one sentence. The window shows that sentence unchanged, and a **Show the licence** button. | Decided that the tool checks and the window does not refuse (6.1, 6.3). The button is a proposal |
| The licence is ending, in grace, or the clock looks wrong | One line in the activity log. The tool runs. | Decided for the tool (6.3, decision G) |
| No archive folder set for `keep_record` | A folder chooser, once. The choice is a setting. | Proposal |
| No key file set | The record is kept unsigned (as `.json`). A key file is optional, and a setting. The window says whether its copies are signed. | Proposal |
| `keep_record` after every operation | A setting, off. The window asks nothing. | Proposal |
| A plan that is not valid | The tool's own errors, with line and column, quoted. | Proposal for the display; the text is the tool's |
| `adl_load` not found (exit 4) | "The tool could not find `adl_load`." and where it looked. | Proposal |
| The user presses Stop | The tool is asked to stop. A second press ends it. | Proposal |

## 8. What is never done

All of this is decided by the plan or follows from it. It is repeated here because a later package must not drift from it.

- **No download.** The window never fetches, installs, updates or repairs a companion. The Pro package comes with the licence file (decision K).
- **No connection.** The seam opens no connection of any kind. Nothing checks for a newer companion, a revocation, a licence or a server.
- **No counting.** Nothing counts starts, users, computers, seats or runs. The window reads no hardware identity.
- **No refusing to run.** The window never declines to start a companion because of a licence, a term, a grace period, a clock, a seat or an edition. Where a Pro tool declines, it is the tool's own sentence, and the files, records and reports the user already has are untouched. The window itself declines to start a program in two cases only: the check of [5.3](#53-what-the-window-checks) fails, which is about the files, or the window has an operation of its own running that would share port 59 with the tool's loader ([section 6](#6-what-the-window-hands-over-and-what-it-reads-back)). Neither is about the customer.
- **No hiding and no greying by a licence.** A window looks the same with a licence and without one, except for the Licence screen. What it offers depends on whether a companion was found and checked.
- **No change to a licence or a companion.** The window writes no licence, no folder of a companion, no key and no file beside a companion.
- **No start the user did not ask for.** Searching and checking happen when the window starts. Starting a program happens when the user presses something, and `keep_record` after every operation only when the user has turned that on.
- **No secret in a request.** No key, no passphrase, no licence, no credential.
- **No claim.** The window says who signed a companion and what the tool reported. It does not say that a companion or a record is certified, qualified, compliant, approved or guaranteed, and it does not say "verified" of a companion.

Enforcement is by the terms of the licence, as for the Pro tools themselves (decision B). The seam adds none.

## 9. What the two editions show in a window

The plan's 6.5 is the first row. The others show what the seam would add.

| The computer has | The window shows | Status |
| --- | --- | --- |
| No licence, no Pro package (Community) | As today and, with licensing slice 5, the edition, the Licence screen and a row for each pack. No word about companions anywhere. | Decided (6.5) |
| A licence that includes `pro.*`, no Pro package | The Licence screen as above. In the list of Pro tools, which the user opens, "No Pro tools were found on this computer." | Proposal |
| A Pro package found and checked, with a Pro licence | The roles of [section 6](#6-what-the-window-hands-over-and-what-it-reads-back) in the loader: keep the receipt in the archive and compare with a baseline (after an operation, in the receipt), and run a plan (in the menu that starts operations). The list of Pro tools names the companion, its version and its signer. | Proposal |
| A Pro package found and checked, with no licence, or an ended one | **The same entries and the same list.** A use ends with the tool's sentence and the **Show the licence** button. | Proposal, from 6.1: nothing is locked, greyed or hidden by a licence |
| A Pro package that fails a check | The list says why. No entry appears. | Proposal |
| An Enterprise licence | As Pro. The data packs and the conformance kit are not companions of the window. | Proposal |

- **Where the list of Pro tools is.** A section of the Licence screen, which the plan's first version already has. Proposal.
- **Which windows.** The loader only. The generator (`adl_media` is not built) and the mock target (`adl_lab`) get no entry in the first version.
- **No advertisement.** A window with no companion shows no hint, no link and no price. A pointer to the product's page is the owner's choice and is not proposed ([section 11](#11-decisions-for-the-owner), question 9).
- **The record that the window hands over** is the record the window writes (C2). When licensing slice 5b is in, that record carries the edition and the licence number in its `licence` key. A companion reads it as text. It is not an entitlement, and the companion decides on its own licence.

## 10. What is left to later packages

Proposed names. All wait for the owner's answers in [section 11](#11-decisions-for-the-owner).

| Id | Where | What | Needs first | Size |
| --- | --- | --- | --- | --- |
| C8a | public | The manifest reader and the check of 5.3, in or beside `adl::licence`; the purpose `companion`; the test key; golden manifests; a fuzz harness with seeds; the owner's licence tool gains a command that signs a manifest. The strict JSON reader of the library is exposed in a header for it. | Licensing slices 2 to 4 merged; the answers to questions 1 to 4 | medium |
| C8b | public | Searching the roots (`ADL_SETTINGS_DIR` included), the checks at a start, starting a program, reading its lines and its exit code, and asking it to stop. Qt-free where it can be, over the platform layer. | C8a | medium |
| C8c | public | The window: a model for the Pro tools list, the roles in the loader, the settings, the text in the seven catalogues, the guide pages, the messages page, the screenshots and a changelog fragment. | C8b; C1b and C2 (the record writer); licensing slice 5 | large |
| C8d | private | The package step writes `companion.json`, the owner's signing step is in the releasing page, the package check's allow-list and bill of materials learn the two files, the guide says where to unpack. | C8a (the format is fixed) | small |
| C8e | private | `adl_records`: `audit --target <id>` and a `--json` event for `keep` that says where the record was kept (see section 12). | Nothing | small |

Every package here follows `AGENTS.md`: a labelled test for every behaviour, a fuzz harness for every parser of outside bytes (the manifest reader, the envelope reader and the line reader for a companion's events), no preprocessor, and the table of what to update.

## 11. Decisions for the owner

Each has my default. Questions 1 to 4 are additions to public interfaces, which `AGENTS.md` says to ask about. Nothing here is agreed.

1. **A new file format, `companion.json` and `companion.sig`,** with the key names of 5.2. Default: yes.
2. **A new key purpose, `companion`, and a key for it** (5.6). Default: yes, a key of its own.
3. **A new function in `adl::platform` to start a program in a group of its own and to ask it to stop** (6, "Stopping"). Needed for Stop on Windows. Default: yes. Without it the window can end a program and cannot ask it to stop.
4. **A header that exposes the licence library's strict JSON reader.** Default: yes, as the plan already proposes for C5 and C6.
5. **Whether the first Pro release has the seam at all.** The plan says "if the owner wants Pro to be visible in the windows" (6.5). Default: not in the first release. The Pro tools work from a command line and from the harness adapters without it.
6. **Official companions only** (5.4). Default: yes. A site's own key can follow.
7. **Keep VSS's root key and key list** (5.4). Default: no. Compiled-in keys.
8. **Two roots, and no machine root** (4). Default: yes. A setting that names one folder, so that a user need not copy the package, can follow.
9. **Whether a Community window with no Pro package points to Pro at all,** and with what words and link (9). Default: no pointer. The plan's first version shows only the Licence screen.
10. **`keep_record` after every operation** as a setting, off by default (7).
11. **Which windows get entries** (9). Default: the loader only.
12. **`audit_record` waits for `audit --target <id>`** in `adl_records` (section 12, point 2). Default: yes, ask P2 for it.

## 12. Found in the plan while reading

These are questions about the plan and the code that this page rests on. They are also in the report to the planning session.

1. **The receipt is not a file.** 6.5 says "keep every receipt in the records archive". The window's receipt and log are in memory only, and no file is written (`docs/user-guide/log-and-receipt.md`). The operation record exists only through C1b and C2, and the record writer is not on `develop` at this commit (`libs/loader_core` has the address, download folder and media set code only). C8c depends on them.
2. **A compare after Information would report every other target as missing.** `adl_records audit <baseline> --from-records <folder>` compares every target of the baseline, and one with no record in the folder is a `no_answer` finding with the reason `no_record`. The window has one record. There is no option to name one target. Proposed: `audit --target <id>` (C8e). The window cannot write a one-target baseline, because the baseline is Pro's own format.
3. **`adl_records keep` writes no events.** With or without `--json` its output is lines for a person. The window cannot learn the path that the record was kept at, except by reading text that is not a promise. Proposed: a `--json` event for `keep` and `keep-run` (C8e).
4. **The Pro package check is an allow-list.** The package unpacks to a folder named after the version and system, holds `README.txt`, `VERSION.json`, `LICENSE.txt`, `NOTICES.txt`, `BOM.cdx.json` and `doc/guide.html`, and refuses any file that is not on its list. `companion.json` and `companion.sig` have to join the list, and every file has to be in the manifest, because 5.3 refuses an unlisted file. `companion.sig` is made after the package is built and checked, so the order of the owner's steps changes (C8d). The bill of materials holds SHA-256 and the manifest SHA-512.
5. **`machine_config_directory()` is `/etc` on Linux.** It is a place for a licence and not for programs, which is why 4 has no machine root.
6. **The plan's table says C8 needs "P1 and P2 in use".** `adl_rig` and `adl_records` are on the private repository's `main`. Whether they are in use is not something this page can see.
7. **VSS's design is a trust model for plugins, and the plan says a companion is found "as a VSS plugin is".** It is, for the manifest and the signature. It is not for the root, the levels and the sandbox, and 5.4 says so. This is the one place where the page does less than the plan's sentence suggests.
8. **`adl_load` is not in the Pro package** (private plan, P7: not bundled). `adl_rig run` needs it beside itself or on the path, so a window that starts `adl_rig` passes `--loader` when `adl_load` is in the window's own folder, and otherwise the tool says exit code 4. Whether the window's packages carry `adl_load` is C3's release question.

## 13. What was not done or not checked

- No code was written, built or run. The Pro programs were read from their guides and format pages and from the tests that those pages cite. None was run.
- The behaviour of an idle window with respect to the loader's control port was not checked.
- VSS was read at `origin/develop` in the files named above. Its code was read for the signature header only. Its tests were not run.
- How a window with no console asks a child program to stop on Windows (Stop, in section 6) was not tried here.
- `QProcess`'s behaviour with a path outside the code page on Windows was not tried here. The Pro tools' own tests cover their side ("a package unpacked in a folder with characters outside the code page", private plan, P7).
- That a signed manifest of a few tens of megabytes of programs hashes in a fraction of a second is an estimate, not a measurement.
- The key names, reason codes and limits of sections 4 to 6 (the 16 folders, 64 files, 64 KiB, 2,000 lines) are proposals, chosen to be small, and have no tests.
- No customer was asked whether a window should start a Pro tool.

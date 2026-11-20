# Licensing: how Virtual Serial Studio does it, and how ARINC Data Loader will

This is a plan. It is not published (`plan/` is excluded from the site, `mkdocs.yml`). Nothing described here as "proposed" is built in this repository. It follows [the editions plan](editions.md), which sets out the commercial structure, and it adds the mechanism: how a program knows which edition it is, and what that changes.

Written 2026-11-19. The owner's decisions are marked **OWNER DECISION** and collected in [section 8](#8-decisions-for-the-owner).

## 1. Summary

**How Virtual Serial Studio (VSS) does it.** A licence is one small signed text file. The program checks it on the computer, against a public key built into the program. There is no activation, no server and no telemetry. The file names the licensee, the edition, the dates, a list of features and optionally a seat count and computer names. When it ends, the program goes back to Community after a grace period, and nothing the user made is lost. The check is a deterrent and not a lock, because the program is GPL software. What is sold is what a patched build cannot give: signed plugins, a signed driver and support. The owner issues licences from a private repository with a ledger and a gated signing job.

**What is already true in VSS.** The library, the owner's tool, the Licence page, the banners, the command-line command and a test suite are merged to `develop`. The check gates nothing yet; it only shows a state. No real licence has been issued, and the signing job has not run ([2.11](#211-what-is-built-and-what-is-not)).

**What ARINC Data Loader (ADL) will do.**

- Reuse the same design, with the same words for the same things.
- Write it as a library of its own, `adl_licence`, with no ADL-specific code, so that it can be moved out and shared with VSS ([section 6](#6-moving-it-out-and-sharing-it)).
- Read files through `adl::platform`, return `std::expected`, and take no dependency except one small vendored signature routine (the same one VSS uses).
- Show the edition in the three Qt programs through the shared `libs/ui`, and give every program one licence on a computer.
- Gate no program function in the first version. What ADL's paid editions give is support, supported versions and data packs, which are services and documents, not code ([3](#3-adl-compared-with-vss), [4.1](#41-what-a-licence-means-in-adl)).

**Decisions.** On 2026-11-19 the owner answered all twelve ([section 8](#8-decisions-for-the-owner)). What remains for the owner is not a decision but work only they can do: create the private ADL pro repository, make ADL's signing and standby keys, write the seat terms, and merge the docs pull requests. Every slice can start; slices 1 to 7 need no real key.

## 2. How Virtual Serial Studio does it

Read from `virtual-serial-studio` at `origin/develop` (9664a025, 2026-11-16), its design page `docs/licensing.md` and `docs/editions.md`, and from the private `virtual-serial-studio-pro` repository at `main` (7ffbece): its README, `ISSUING.md`, `REQUESTING.md`, `SETUP.md` and the shape of `licences/`. Nothing was built or run.

### 2.1 The principle

- Studio is GPL software, so anyone may build a copy with the check removed. The check is not the lock. It tells an honest customer where they stand and tells Studio what to offer.
- What is sold cannot be had by patching Studio: official plugins (which check the licence themselves), the Microsoft-signed driver, and support.
- A licence never gates anything in Community. Ports, setups, capture and files work the same with no licence, a valid one, an ended one or a broken one.
- Nothing is ever locked for good, and a paying customer is never punished to catch a cheat.

### 2.2 The file

Extension `.vsslic`. JSON in two layers.

```json
{ "format": 1, "purpose": "licence", "key_id": "lic-2026-a",
  "payload": "<base64url>", "signature": "<base64url>" }
```

- The payload travels as the exact bytes that were signed. A reader verifies first and parses second, so no parser reads unsigned data beyond the five envelope fields.
- Ed25519 (RFC 8032). What is signed is `"VSS-SIGNED-FILE-1" 0x00 purpose 0x00 payload`, so a signature made for one kind of file cannot pass as another (a licence, a revocation list, a plugin manifest).
- At most 64 KiB. Strict parsing: duplicate keys, bad dates and bad base64url are errors. Unknown fields are ignored, except a field named in `critical`, which makes an older program say "made for a newer version".
- Payload fields: `licence_id` (`VSS-<year>-<six digits>`, never reused), `kind` (trial, annual, enterprise), `edition` (pro, enterprise), `licensee` (name, optional contact, optional email), `issued_at`, `starts_at`, `expires_at`, `grace_days`, `seats` (0 means a site licence, not counted), `features`, `machines`, `updates_until`, `replaces`, `clock_reset`, `critical`.
- Features are dotted names, exact or a prefix ending `.*`: `plugins.official`, `plugins.<id>`, `driver.signed`, `support`.

### 2.3 Keys

- The trust list is a source file chosen by CMake (`trusted_keys_release.cpp`), not by the preprocessor. Each key has an id, one purpose (licence or revocation), a 32-byte public key and a window of signing dates.
- Two keys from the start: `lic-2026-a` signs and `lic-2026-b` is a standby whose private half has never been online. Both public halves ship in every build, so a leak of the first can be answered at once.
- A key that signs licences does not sign plugins. The plugin signer has its own key, purpose and revocation file.
- Tests use the RFC 8032 test vector 1 key, `test-rfc8032-1`. The code that loads the release list refuses any `test-` key, so a test key cannot verify a licence in a product. A build with an empty list accepts no licence and is Community.
- Ed25519 comes from vendored Monocypher (`third_party/monocypher`). The library also refuses non-canonical encodings itself, and its tests run the RFC 8032 and Project Wycheproof vectors.

### 2.4 States

One status comes out of a file, the trust list, the revocation list, the time and the computer's name. Every state has a reason code and one sentence for the user.

| State | When | Features |
| --- | --- | --- |
| `none` | No file | Off (Community) |
| `valid` | In the term | On |
| `expiring` | 30 days or fewer left (trial: 3) | On, with a notice |
| `grace` | Past the end, within `grace_days` | On, with a counting-down notice |
| `expired` | Past the grace period | Off (Community) |
| `not_yet_valid` | Before `starts_at` | Off |
| `clock_suspect` | The clock reads well before a time the program knows has passed | Off until the date is right |
| `invalid` | Bad signature, unknown or revoked key, withdrawn licence, other computer, newer format, unreadable | Off, with a reason |

An ended licence returns the program to Community. A plugin that is running when the grace period ends keeps running until stopped. A file that uses a licensed feature still opens and saves unchanged, with the feature shown as unavailable and the reason.

### 2.5 The clock check

A deterrent only. The program keeps the latest time it has seen in `licence-clock.json` and refuses to go back more than 48 hours behind the highest of that time, its own build time and the licence's `issued_at`. If the stored time is itself wrong, the owner sends a licence with a `clock_reset` token, which works once on a computer. The stored file is plain data and can be deleted, and the design accepts that.

### 2.6 Where files are

- Per user: `%LOCALAPPDATA%\Virtual Serial Studio\licences`, `$XDG_DATA_HOME/virtual-serial-studio/licences` (or `~/.local/share/...`), `~/Library/Application Support/Virtual Serial Studio/licences`. Import copies here.
- Per machine, read only: `%ProgramData%\...`, `/etc/virtual-serial-studio/licences`, `/Library/Application Support/...`. An administrator puts a site licence here.
- Up to 64 files from each, in name order. The best one wins: valid over grace over ended, then Enterprise over Pro, then the later end. Import never replaces a file and never deletes an older one. Removing a licence asks first.

### 2.7 Revocation

No network. Each release carries a list of withdrawn licence ids and key ids with a sequence number. The same list can be a signed `.vssrev` file in a licence folder, and the program keeps the highest sequence it has seen. It holds ids only, never names. A withdrawn licence shows as invalid with one sentence. This takes effect when the customer next updates, which the design accepts because a term is a year at most. Update checks do not carry revocations.

### 2.8 Seats, computers, the version window and trials

- **Seats** are counted by the licence terms, with the licensee named in the file. There is no hardware fingerprint, no MAC address and no count of installations. A leaked file names its source and can be withdrawn. In VSS one Pro seat is two computers (the one you work at and one you control or watch from it).
- **Computers**: a licence may list computer names (a trailing `*` matches a prefix). Another computer gets "invalid, for other computers". A control for honest sites, not a lock.
- **Version window**: an optional `updates_until`. Plugins released on or before that date keep working after the licence ends. Left off annual licences, offered in Enterprise agreements.
- **Trials** are licence files with `kind: trial`, 14 days, no grace, issued on request, one for each organisation. There is no self-starting trial inside the program, because a built-in one is reset by deleting a settings file.

### 2.9 What the user sees

- A **Licence** page (Help, Licence): edition, licensee, licence number, term, a status sentence in a polite live region, seats, computers, what it includes, other files found, Import and Remove. Drag and drop imports.
- A **banner** in the main window, one at a time, never a dialog and never blocking. Ending soon, in grace, ended once, clock wrong.
- An **edition chip** in the status bar.
- A **Support** page: what support the licence includes and the address to write to, with copy buttons. It opens no browser and no mail program.
- A **support bundle**: one zip the user reads before saving. It carries the licence number, edition, term and state, and not the licence file. Name and email are off by default.
- `vss_cli licence` (show) and `vss_cli licence import <file>`. The control gateway's status reports the edition. A library check that a plugin's licence covers it is on `develop`; the host that hands a plugin the licence file, and the reader a plugin may copy, are on the release candidate 6 and 7 integration branches and were not read in detail.
- Words: **licence** is the noun and **license** the verb. The editions are written Community, Pro and Enterprise. Never "activate", "register" or "unlock", because there is no activation and the text must not suggest one.

### 2.10 The owner's side

- A private repository holds `licences/customers/<id>.json`, `requests/<licence id>.json` (the payload fields), `issued/<licence id>.vsslic`, an append-only `ledger.jsonl`, and the revocation list.
- Python tools write a request with the next id and dates, link it to a customer, check all records against each other, list licences due within 45 days and write the note sent with a file.
- A pull request runs `vss_license issue --dry-run` on each request, with no key. After a merge, an `issue-licence` job waits at the `licence-signing` environment (the owner is the required reviewer), signs, verifies, commits the file and the ledger line, and prints the id and hash.
- The owner can sign on their own computer instead (`virtual-serial-studio-pro/tools/issue_local.py`), with the key file read by `vss_license` and not by the script.
- Keys: the signing key is an environment secret; the standby is offline only; the owner keeps a second encrypted copy on two offline media.
- `vss_license` (inspect, verify, issue, revocations, keygen) is public, built with the tools and never installed. A release check fails if it, a library of it or a key file is in a package.
- Records hold personal data. They stay in a private repository with no payment details, and a customer's record is removed on request once no live licence depends on it.

### 2.11 What is built, and what is not

Built and on `develop`: `virtual-serial-studio/libs/vss_license` (envelope, payload, states, clock, revocation, store), `virtual-serial-studio/apps/vss_license` (the owner's tool), the GUI backend and Licence page, banner and chip, the Support page, `vss_cli licence`, the control gateway reading the licence, fuzz targets and test vectors.

Not built, or not run:

- **Nothing is gated.** `check(feature)` is asked only by the Licence and Support pages. The plugin host that would run an official plugin is on the integration branches.
- **No licence has been issued.** In the local checkout of the private repository, `licences/issued/` is empty and `ledger.jsonl` has no lines. One request (`VSS-2026-000001`, an owner test) is written and not signed.
- **The signing job has not run.** It needs the environment, the secret and the variables of `SETUP.md`, which only the owner creates. An earlier accidental run created the environment without protection and signed nothing.
- The pairing check between the two computers of a seat, and plugins verifying the licence themselves, are decided and not built.

### 2.12 What there is to port

Library files: `error`, `license`, `signed_file`, `sign`, `store`, `strict_json`, `words`, `test_keys`, the per-OS `locations_*` and `computer_name_*`, the release key and revocation sources. Tests: `test_license`, `test_license_signature`, `test_license_store`, `test_license_tool`, `test_license_words`, `test_license_locations` and per-OS location and tool tests, the Wycheproof Ed25519 vectors, and a fuzz target with a corpus.

## 3. ADL compared with VSS

| | VSS | ADL | What it means for the port |
| --- | --- | --- | --- |
| Errors | `boost::system::result` | `std::expected` | Change the error type |
| JSON | Boost.JSON behind `strict_json` | No dependencies | Write a small strict parser for the flat payload, and fuzz it |
| Crypto | Vendored Monocypher | None. The preprocessor rule scans first-party code, and Monocypher is C that uses `#define` | Vendor it and decide how the scan treats it ([decision 7](#8-decisions-for-the-owner)) |
| OS code | CMake picks `src/platform/<os>/` files | `libs/platform` behind interfaces; no preprocessor conditionals anywhere | Put the folders and the computer name behind new platform interfaces |
| File access | `std::filesystem` in the store | `adl::platform::FileSystem`, with a fake host for tests | Write the store over `FileSystem`, so it is tested without a disk |
| Wall clock | The caller passes the time | `Clock` has an optional UTC wall-clock reading, "unsupported" where there is none | The library takes the time as a value, as VSS's does |
| Computer name | `computer_name()` per OS | `HostInfo::name` is the OS name only | New |
| Per-user and per-machine folders | Per OS | None. Qt's `QStandardPaths` is used once, for Documents | New |
| Type names | `snake_case` types | `PascalCase` types, `snake_case` functions | Write in ADL's names; rename at extraction ([6](#6-moving-it-out-and-sharing-it)) |
| GUI | Qt Widgets | Qt Quick, shared QML in `libs/ui` compiled into each program | Rewrite the screens in QML; keep the text and the states |
| Programs | A GUI and a command-line program (an agent and a control gateway are built from source and are not in the installers) | Three Qt programs and one command-line program | One licence for all four ([4.2](#42-the-programs)) |
| What is sold | Plugins, signed driver, support | Support, supported versions, an update policy, data packs ([editions plan](editions.md)) | Services and documents. A different feature list ([4.1](#41-what-a-licence-means-in-adl)) |
| Network | An update check | No connection except the ARINC 615A traffic to a target (TFTP and FIND). From a search of the programs' source: no HTTP client and no web address in `apps`. The shared About dialog opens the GPL's web page in the user's browser when a link is clicked | "Never contacts a server" is true for ADL, and stays so |
| Languages | Several | English plus `de es fr he ja yi zh_CN` | New strings need seven catalogues; the drafts are unreviewed |
| Interface rule | Public interfaces are the owner's | `AGENTS.md`: a new file format, program or option is said and asked, not slipped into another change | [Decision 5](#8-decisions-for-the-owner) |

## 4. The plan for ADL

### 4.1 What a licence means in ADL

The paid editions in [the editions plan](editions.md) are support, supported versions and an update policy (Pro), and data packs for certification use (Enterprise). All of that is delivered outside the programs, as a service and as documents. A patched program cannot take any of it. That fits the VSS stance exactly, so the first version does the same as VSS does today: it **shows** what the licence says and **gates nothing**.

- The programs show the edition, the licensee, the term and the state, and say what the licence includes.
- The library answers `check(feature)` so that a later change can gate something. The first features are `support` (Pro and Enterprise) and `data_packs` (Enterprise). A licence says which it carries; the edition field alone grants nothing.
- Community never degrades, and an ended licence returns the programs to Community with nothing lost.
- "Some other stuff" for Pro is not defined yet. Whatever it turns out to be goes in as a new feature name and does not change the file format. See [decision 2](#8-decisions-for-the-owner).

A licence file records what was bought. It does not change the GNU General Public License terms on the code, which stay as they are for every edition ([decision 3](#8-decisions-for-the-owner)).

Risk to note: test benches on closed networks sometimes have a real-time clock that resets. The clock check could show `clock_suspect` there. With nothing gated this changes only what the Licence page says, and the file stays valid.

### 4.2 The programs

One licence file on a computer serves all of them, because the programs share the licence folders and the same library.

| Program | Kind | What it does with the licence |
| --- | --- | --- |
| `adl_app` (the loader) | Qt | Shows the edition (About, status bar chip, Licence dialog, banner) |
| `adl_media_gen` | Qt | The same, from the same `libs/ui` source |
| `adl_mock_target` | Qt | The same |
| `adl_write_payloads` | Command line, no Qt | Nothing. Its options do not change |
| `adl_licence` (new) | Command line, no Qt | `show`, `import <file>`, `remove <name>`, `verify <file>`. For a computer with no display and for scripts. The counterpart of `vss_cli licence` |

### 4.3 Code

| Part | Where | Notes |
| --- | --- | --- |
| Licence core | Target `adl_licence`, alias `adl::licence`, headers `adl/licence/*.hpp` | Envelope, payload, states, `check`, revocation, clock state, words. No Qt, no `adl::` includes other than platform for the store. Unit tests (`unit`) |
| Strict JSON | Inside `adl_licence` | Flat payload only: strings, integers, arrays of strings, null. A fuzz harness |
| Ed25519 | `third_party/monocypher/` | Vendored, with its licence in `notices/` so About, Licences shows it, and a `REUSE.toml` entry |
| Store | Inside `adl_licence`, over `adl::platform::FileSystem` | Best of several files, import never replaces, remove only from the user folder. Tested on the fake host (`integration`) |
| Platform | `libs/platform/interfaces/adl/platform/` and one implementation per platform | New: `computer_name()`, `user_data_directory()`, `machine_config_directory()`. `unsupported` on an RTOS. No preprocessor; the platform fragment picks the file. Tried on macOS and Windows before it is queued, because it is platform code |
| Qt model | `libs/ui` | A `LicenceModel` that turns the library's answers into properties, compiled into each program as the rest of `libs/ui` is. The screens never read a file or decide a state |
| Screens | `libs/ui/qml` | Licence dialog, banner, status-bar chip, an "Edition" line in `AboutDialog.qml`, a Support section. Text from VSS's screen spec with the product words changed. `ui` tests, seven catalogues, new screenshots |
| Owner tool | A program under the tools folder, public and not installed | `inspect`, `verify`, `issue`, `revocations`, `keygen`, `--test`. It is public as VSS's is: it makes nothing that a program accepts without ADL's private key, which is in no repository. A package check fails the release if the tool, a signing library or a key file is in a package |
| User CLI | A program in the apps folder. The executable is `adl_licence`; its CMake target is `adl_licence_program`, as VSS names its own, so that `adl_licence` stays the library | Over the same store. `system` tests |
| Fuzz | Beside the library | The envelope and payload parser, and the revocation parser, with seeds from `tools/fuzz/make_seeds.py` |
| Test data | `tests/` | RFC 8032 and Wycheproof Ed25519 vectors, a table test of every state, the test key (RFC 8032 vector 1) in a target no product links |

Planned layout. None of it exists yet.

```text
libs/licence/                 adl_licence: envelope, payload, states, store, strict JSON
libs/licence/fuzz/            fuzz harnesses and seeds
third_party/monocypher/       vendored Ed25519, with its licence
tools/licence/                the owner tool (public, not installed)
tests/licence/                test data and test licences
apps/licence/                 adl_licence_program, installed as adl_licence
libs/platform/interfaces/adl/platform/locations.hpp   computer name and data folders
libs/ui/                      LicenceModel and the Licence, banner, chip and Support QML
```

**Where the licence files are in tests.** The programs already move their settings with `ADL_SETTINGS_DIR` so that a test never touches a real one. The licence folders and the clock file follow the same variable, so no new variable is needed and a test run never reads a real licence.

**Folders** (proposed, by the same pattern as VSS): per user `%LOCALAPPDATA%\ARINC Data Loader\licences`, `$XDG_DATA_HOME/arinc-data-loader/licences` (or `~/.local/share/...`), `~/Library/Application Support/ARINC Data Loader/licences`. Per machine `%ProgramData%\ARINC Data Loader\licences`, `/etc/arinc-data-loader/licences`, `/Library/Application Support/ARINC Data Loader/licences`.

### 4.4 The file, changed for two products

These changes make the format shareable. They are cheap now and expensive later ([section 6](#6-moving-it-out-and-sharing-it)).

- A signed `product` field in the payload (`adl`, `vss`), which the reader must match. A licence for one product is refused by the other with one plain sentence.
- A per-product file extension: `.adllic` and `.vsslic`. Separate extensions avoid opening a file with the wrong program.
- The signed-message prefix names the format and not a product (for example `ICL-SIGNED-FILE-1`), because a shared reader signs and checks it.
- Licence ids `ADL-<year>-<six digits>`.
- **Separate keys for each product.** A leak of one then cannot touch the other. A shared verifier takes the keys it trusts as an argument.
- The purposes stay as in VSS: `licence`, `revocations`. A data pack manifest could use the same envelope with its own purpose later. Not planned now.

### 4.5 Documentation

The words are in [section 7](#7-the-words). This section is what the pages have to cover and what constrains them. It comes from a read-only sweep of the pages, with line numbers as of 12:07 on 2026-11-19. Another session is editing the same pages (the guide rewrite), so the numbers will drift, and this plan does not edit any of them.

**The programs, and what to call them.** One licence serves every program on a computer. The pages have to name all of them, and today they do not: `faq.md:15` and `docs/README.md` (lines 5 and 17) name three, and `adl_write_payloads` is on no front page.

| Program | Display name in the program | Called in the guide | Guide pages |
| --- | --- | --- | --- |
| `adl_app` | ARINC Data Loader | "the loader" | The loader pages, Choose and check a media set, Find a target, Upload, Download, the log and receipt, Preferences, Keyboard |
| `adl_media_gen` | ARINC Media Set Generator | "the generator" | Make a media set |
| `adl_mock_target` | ARINC Mock Target | "the mock target" | Run the mock target |
| `adl_write_payloads` | none | "a command-line tool", "a command-line program" (both used) | Command line and exit codes |
| `adl_licence`, if the owner agrees ([decision 5](#8-decisions-for-the-owner)) | none | to be fixed with the page | Command line and exit codes, Your licence and edition |

The product is also called ARINC Data Loader, the same as the loader's display name. The licence belongs to the product, so licence text says "an ARINC Data Loader licence" and names each program, and never says "the loader's licence" for all four. The pages disagree about which Qt program is the second and which the third (`media-gen.md:3` and `roadmap.md:122`, `mock-target.md:3` and `roadmap.md:126`); that is for the docs session.

**Pages that change**, each in the slice that makes it true:

| Page | Change |
| --- | --- |
| New guide page, "Your licence and edition" | One licence for all the programs. Where each shows the edition. Importing, with the dialog and with `adl_licence`. What happens at the end of a term. That it works offline and sends nothing. Written in slices 5 and 6, not after, because of the check below |
| `user-guide/command-line.md` | `adl_licence` and its exit codes. It now says one program runs from a command line and the three windowed programs take one option (`--smoke-test`); both sentences change |
| Guide index, installing, quick reference, loader, generator, mock target, preferences | The programs list, and the new About line, status bar chip and Licence screen in each window. The About dialog's "Licences" button is the third-party notices (`preferences.md:63`), so the new screen is called "Licence", as in VSS, and the guide says which is which ([decision 11](#8-decisions-for-the-owner)) |
| Configuration reference, limits | The licence folders, the clock file, the 64 KiB file limit, the 64 files read from a folder, and that `ADL_SETTINGS_DIR` moves them too |
| `privacy.md` | Line 46 says "No check of the licence" and the page says the only option of the programs is `--smoke-test`. The first stops being true. The page has to say that a program reads a licence file on the computer, that the file holds a name and perhaps an email address, that a clock file keeps the latest time seen, and that nothing is sent. Its table of what is kept where needs rows |
| `licensing.md` (a developer page, third-party inventory) | Add Monocypher (BSD-2-Clause or CC0-1.0 in VSS's copy) to the inventory and the notices. Its title "Licensing" is also the name of the paid-editions page, `editions.md` ("Licensing and editions"), at a different address. Retitle it, for example "Third-party licences and notices". Its address does not change |
| `editions.md`, `support.md`, `assurance.md`, `faq.md`, `index.md`, `press.md` | "Pro and Site" becomes "Pro" ([decision 1](#8-decisions-for-the-owner)). They are front pages, so `AGENTS.md` lets them say only what the code, a test or a golden file shows: they stay "proposed" until slices 5 and 6 land, and then say what the programs do. See the next paragraph |
| `glossary.md` | Edition, licence file, licence number, licensee, seat, grace period, Community |
| `architecture.md`, `libraries.md`, `.github/ci/plan.json` | The new targets, the programs list ("three Qt applications", "The programs are ..."), and which paths need a build |
| `changelog.d/` | A fragment for each slice a user can see |

**The drafts are configured as published.** `editions.md` says "Do not publish it", and `tools/ci/check_site_pages.py` requires `licensing/`, `assurance/` and `support/` in the built site. The site is not deployed yet (`ci.md:212`), so nothing is public. Whether those pages go live before the decisions are made is the editions plan's decision 12.

**Checks that the new text has to pass.**

- `docs.messages` (`tools/ci/check_messages.py`, new on the guide-rewrite branch, not yet committed): every wording in the C++ code that the person sees, six characters or more between placeholders, must appear in the guide. State sentences in `LicenceModel` are such wording, so the guide page and the UI land together. Labels written in QML are not asked for. Every exit code of a program must be a row of the table in `command-line.md`. The check lists the programs it scans (`apps/write_payloads/main.cpp`, the three windowed programs, the smoke test), so slice 6 adds `adl_licence` to it.
- `docs.links` and `docs.paths`: every link, anchor and backticked repository path must resolve. A path that does not exist yet goes in a fenced block or needs an allow-list entry.
- `check_site_pages.py` for the front pages: no script, no style attribute, no `mailto:`, external links only to `github.com` and `www.gnu.org`.
- The same pages' rule that a front page says only what the code, a test or a golden file shows.

**Who writes it.** The pages above are the ADL documentation session's. Its three pull requests are open drafts, stacked: #78 (site shell and kit), #79 (front pages, on #78) and #80 (guide rewrite and the message check, on #79). Only the owner merges them. The guide page and the program pages are written with slices 5 and 6, on top of #80 once the owner has merged it; the state to go by is the pull request's, not a message. The vocabulary in section 7 can be used by that session now.

**Files to leave alone for now.** The docs session is about to change `AGENTS.md`, `docs/ci.md`, `docs/testing.md` and `docs/plan/editions.md`, in a branch stacked on #80, with an assurance data pack plan and two new checks. Nothing in slices 1 to 7 needs those files before then, and none of them is edited here. It also leaves "Pro and Site" as drafted until the owner answers decision 1.

## 5. Slices

Each is a draft pull request against `develop`, on a branch named by the working rules, with a changelog fragment where a user sees a change, and the checks in `AGENTS.md`. They are in order. Slice 1 depends only on decision 5, and slice 2 on decision 7. Both are answered.

| # | Branch | What | Public interface change | Tests |
| --- | --- | --- | --- | --- |
| 1 | `feature/licence-platform-locations` | `computer_name`, `user_data_directory` and `machine_config_directory`, in each platform. Merged as pull request #84 on 2026-11-19 | Adds to `adl::platform` | `unit`, `integration`; macOS and Windows CI legs |
| 2 | `feature/licence-core` | Vendored Monocypher with its notice fragment, `REUSE.toml` entry and a line in `docs/licensing.md`; envelope, payload, states, `check`, clock, revocation, words, strict JSON, fuzz harness and seeds, vectors | New library, not exported by the `adl` package | `unit`, fuzz |
| 3 | `feature/licence-store` | The store over `FileSystem`, import, remove, best of several, `ADL_SETTINGS_DIR` | None | `integration` |
| 4 | `feature/licence-owner-tool` | The owner tool, test-key issuing, a licence for each state, the package check | None (not shipped) | `system` |
| 5 | `feature/licence-ui` | `LicenceModel`, Licence dialog, banner, chip, About line, Support section, translations, screenshots, **and** the guide page, the program pages, `privacy.md` and the configuration rows, because `docs.messages` fails without them | New text in the three programs | `ui`, translation checks, `docs.messages` |
| 5b | `feature/licence-stamp` | The edition and licence number in the loader's receipt and activity log (the loader is the only program with a receipt), the log and receipt pages of the guide, and the tests that check the receipt text | Changes what the loader writes ([decision 2](#8-decisions-for-the-owner)) | `unit`, `ui`, `docs.messages` |
| 6 | `feature/licence-cli` | `adl_licence`, its rows in `command-line.md`, its entry in `check_messages.py` | New program, new file type | `system`, `docs.messages` |
| 7 | docs, slug licensing-pages (branch `docs/` prefix) | The front pages, the glossary, `architecture.md`, `libraries.md`, `.github/ci/plan.json`, the retitled developer licensing page | None | `docs.links`, `docs.paths`, `ci.site_checks` |
| 8 | In the shared repository | Extraction ([section 6](#6-moving-it-out-and-sharing-it)) | n/a | n/a |

Owner-only and not in any slice: generating ADL's own signing and standby keys, putting public halves in the trust list as the real list, creating the private ADL pro repository (modelled on `virtual-serial-studio-pro`), which holds the customer records, the ledger and the signing workflow, with its environment, secret and variables, and the company site entry. The slices use the test key only.

## 6. Moving it out and sharing it

The plan is to build it in ADL first and move it out after. Three things make the move cheap.

**Build it as the shared library from the first commit.** `adl_licence` takes no ADL name except the target prefix, and depends on `adl::platform` only through `FileSystem` and three small functions. The product-specific parts are listed here and are kept in files of their own.

| Shared | Product-specific |
| --- | --- |
| Envelope, payload (with `product`), states, `check`, clock, revocation, words | The trust list (keys) and the built-in revocation list |
| Store, over a file-system interface and injected folders | Folder names, extension, product id, licence id prefix |
| Strict JSON, Ed25519 wrapper, fuzz harness, vectors, test keys | The feature names and what each means |
| The owner tool (issue, verify, inspect, keygen), public | The screens, the text and the Support address function |
| The records and ledger format, the issuing workflow, the format page | The package check's file names |

**Public and private.** What is public: the verifier every program carries, the store, the test key and vectors, and the owner tool that signs and checks files. The owner tool is public in VSS because the security rests on the key and not on the code being hidden (its design page), and the owner adds that third parties need to be able to sign their own plugins. The second reason was not checked against the tool: VSS's release notes show its own plugins signed by a separate script (`virtual-serial-studio/tools/plugin_sign.py`), and the signed-file library is shared by both. Nothing the tool makes is accepted without the product's private key. ADL does the same. What is private: the signing keys (in no repository), and a pro repository with the customer records, the ledger, the request checks and the issuing workflow. ADL's is modelled on `virtual-serial-studio-pro`: its records layout, request checks, ledger, renewals list, issue workflow and `SETUP.md`. One limit to check first: VSS's local release notes say the GitHub plan does not allow required reviewers on a private repository, so its plugin-signing environment holds no key and signing is done on the owner's computer. The licence-signing environment in `SETUP.md` asks for the same reviewer. Signing locally with `issue_local.py` is the safe default until that is checked. Whether one private repository serves both products or each has its own is decision 4.

**Settle the format before VSS issues a licence.** The formats in the two products are still free to change: VSS has issued none. After the first real licence is signed, the envelope, its signed-message prefix and the payload names are fixed for everyone who holds one. The owner has agreed ([decision 9](#8-decisions-for-the-owner)) that the changes in [4.4](#44-the-file-changed-for-two-products) are made. They touch VSS, so they are written down as a plan for VSS's own repository and not carried out from here. The time limit is the first real VSS licence, so that plan is worth writing before slice 2 settles the ADL format.

**Where it lives.** A new repository ([decision 8](#8-decisions-for-the-owner)), pinned by tag the way `cmake/icl` is pinned to `icl-common-infra` and moved with a script. The other choice, a directory in `icl-common-infra`, was declined: that repository holds CMake modules, actions and documents and no C++ library, and a library has its own tests, fuzz corpus and releases. The owner creates the repository.

**Names.** ADL writes types in `PascalCase` and VSS in `snake_case`. The library is written in ADL's convention while it is in this repository, as the working rules say. The move includes one mechanical rename, to the shared repository's convention, and the code is the same afterwards.

**After the move.** ADL and VSS each keep: their key lists, their feature names, their screens and their packaging check. VSS drops its copy and takes the shared one, with the `product` field and the new prefix. No licence exists to migrate.

## 7. The words

The two products' pages should read as one family. This table sets the words. The first column is the concept, the second is what VSS's screens and pages say (`virtual-serial-studio/docs/gui-design/docs/screens/09-licence-and-support.md` and `virtual-serial-studio/docs/editions.md`), the third is what ADL's pages say today, and the last is the proposal for both. Wording in ADL's pages follows the owner's plain-language rule for release text: short sentences, no hyperbole, and no absolute that a test or the code does not back.

| Concept | VSS says | ADL says today | Use in both |
| --- | --- | --- | --- |
| The noun and the verb | **licence** is the noun, **license** the verb | Always **licence** for the noun (about 68 uses). **License** is in proper names (GNU General Public License, SIL Open Font License) and file names (`LICENSE`), and once as a verb (`licensing.md:32`). Same rule as VSS | Same. Identifiers in code use `licence` |
| The editions | Community, Pro, Enterprise, always capitalised; a trial is "Pro (trial)" | "Community", "Pro and Site", "Enterprise" (`editions.md:29-32`, `support.md:37-38`); the editions plan calls them working names | Community, Pro, Enterprise. No "Site" edition ([decision 1](#8-decisions-for-the-owner)) |
| A licence that covers many users | "Site licence" is the label for a licence with no seat count. It is "not a promise about what an Enterprise customer gets" | "Pro and Site", with a site left to the owner to define | Keep VSS's label for a licence with no seat count, and say what a seat is in each product's terms |
| What the licence is | "A licence is renewed by installing a new licence file" | No licence file exists yet. "Licence" already means the GPL text ("the licence text is in the file `LICENSE`", `editions.md:18`), so a bare "the licence" is ambiguous in these pages | "licence file", "licence number" (the id), "Licensed to" (the licensee) |
| After the end | "ended", not "expired", in the text a person reads; the program "is running as the Community edition" and "your ports, setups and files are unchanged" | Nothing. `privacy.md:46` says "No check of the licence", which stops being true ([4.5](#45-documentation)) | "ended". Say what is not lost, in the product's own nouns (a media set, a receipt, a log) |
| A withdrawn licence | "This licence was withdrawn. Contact support." | none | Same sentence |
| Offline | "checks a licence on this computer and never contacts a server" | none | Same sentence with the product's name. It is true for ADL ([3](#3-adl-compared-with-vss)) |
| Words never used for a licence | "activate", "register", "unlock" | No check exists yet. The docs session is adding one for certified, qualified, compliant, approved and guaranteed (a script called `check_claims.py`, with an exceptions file the owner approves). It will scan `README.md`, `SECURITY.md` and the docs outside the plan folder, so a licence page that uses one of those words needs an exception entry ([editions plan](editions.md)). Other words already in the pages, such as "registered" (`building.md`, `arinc615a-engine.md`) and "unlock" (`building.md:418`), mean other things | Never "activate" or "unlock", and never "register" for a licence, in the licence pages and screens. Do not add a site-wide ban: it would catch the uses that are right. The planned word check will need entries for "guaranteed" (`editions.md:21`, `support.md:37`, `user-guide/faq.md:79`, `SECURITY.md:49`) and "certified" (`verifying-releases.md:55`), and the new pages avoid all five words |
| What a licence includes | A row of features: Official plugins, Microsoft-signed driver, Support | The editions plan: support, supported versions, an update policy, data packs | The row in ADL is Support and Data packs. Supported versions and the update policy are terms, shown on the Support page |
| What Community gets | "supported by its users" | "no support promised; the maintainer is one person" | ADL's wording. It is the plainer and the more exact statement |
| State names in the programs and the command line | none, valid, expiring, grace, expired, not_yet_valid, clock_suspect, invalid | none | The same eight words, from the shared library, so a script reads them the same in both |

## 8. Decisions for the owner

All twelve were answered on 2026-11-19. Where the answer is the default the plan assumed, the default is shown as the answer.

1. **Edition names. Answered 2026-11-19: Pro and Enterprise only, no Site edition.** Community, Pro and Enterprise, as you described them. The drafts say "Pro and Site". Default: Pro only; a licence that covers a whole site is a Pro licence with no seat count (VSS shows it as "Site licence"). Needs: leave the word "Site" out of the pages.
2. **What Pro includes. Answered 2026-11-19: the programs show the edition, and the loader's receipt and activity log carry the edition and licence number.** Support, supported versions and an update policy are in the plan. "Some other stuff" is not. Default for the first version: nothing in a program is gated, and the licence shows what it includes. Possible additions, none assumed: the edition and licence number in the receipt and log, a notice when the running version is out of support, a notice when a newer release exists. Each is a change to what the programs write or show, and needs its own decision. Consequence of the answer: slice 5b. The receipt and the log are the loader's, and `privacy.md` and the log and receipt pages describe them as kept in memory only, so those pages, the tests and any golden text change with it. Whether the generator and the mock target have a log worth stamping was not checked.
3. **A licence file is a record, not a second code licence. Answered 2026-11-19: yes.** The GNU General Public License stays on all of the code in every edition. Whether to offer the code under another licence as well is the editions plan's decision 6, and is separate.
4. **Keys and records. Answered 2026-11-19: ADL gets a private pro repository, modelled on `virtual-serial-studio-pro`, for what must not be public.** That is the customer records, the ledger, the request checks and the signing workflow, and possibly the Enterprise data packs (the assurance plan decides that). The owner tool stays public, as in VSS ([section 6](#6-moving-it-out-and-sharing-it)). **Answered: one private repository for each product.** ADL's holds its own records and workflow, and the issuing scripts come from the shared component, so nothing is written twice. VSS's existing one stays as it is. ADL's signing and standby keys are made by the owner with the tool. The repository is the owner's to create. Nothing in slices 1 to 7 needs it or a real key.
5. **Additions to public interfaces** (`AGENTS.md` says to ask): a new file type `.adllic`; a new program `adl_licence`; new About, status bar and Licence text in the three programs; new functions in `adl::platform`; a clock file in the user's data folder. **Answered 2026-11-19: yes to all five.** Not included: a change to the exit codes or the options of an existing program. The receipt and log change is decision 2.
6. **What a seat is in ADL. Answered: the terms define it and the file shows the number.** VSS defines one seat as two computers. That does not carry over. The file carries a seat count and the screen shows "N seats", the terms say what a seat is, and nothing counts installations. A licence with no seat count shows "Site licence". The wording of the terms is the owner's, and the screen's text about a seat is written when they exist.
7. **How the preprocessor rule treats vendored C. Answered: vendor Monocypher.** Monocypher uses `#define`. It goes in `third_party/monocypher`, and `third_party/**` is added to `tools/preprocessor-exclude.txt`, so the count for first-party code stays at zero. The exclusion is a change to the rule's configuration and is made in slice 2 with the vendored files, in the same pull request, so a reviewer sees both. The other route, libsodium from vcpkg, was declined.
8. **Where the shared component lives. Answered: a new repository,** pinned by tag and moved with a script, with ADL's `adl_licence` as its starting point.
9. **Changing the shared format in VSS before it issues a licence** ([4.4](#44-the-file-changed-for-two-products)). **Answered: yes,** and plan it in VSS's own repository. The change is written down for VSS and not made from here.
10. **Trials. Answered: yes, same as VSS:** a licence file issued on request, 14 days, no grace, one for each organisation.
11. **The name of the new screen.** About already has a "Licences" button that opens the third-party notices (`preferences.md:63`). VSS has the same pair: an About tab "Licences" and a Help entry "Licence". **Answered: "Licence", as in VSS, and keep both.** The alternative, "Edition", was declined. The guide page says which is which: "Licences" is the third-party notices and "Licence" is your edition.
12. **The draft pages and the site. Answered: keep them as drafts and out of any deploy.** `editions.md` and `assurance.md` say "do not publish" and the site checks require them to exist, so they are built into the site. They keep their "proposed" wording until the editions, the support terms and the prices are decided. This is the editions plan's decision 12, repeated because the licence work now depends on it.

Answered after the twelve, on 2026-11-20, from the [Pro features plan](pro-features.md#10-decisions-for-the-owner):

- **The terms of the licence library (that plan's decision F): BSD-2-Clause as well as the GPL.** The library's own files say `GPL-3.0-or-later OR BSD-2-Clause` from slice 2 on, so that the closed Pro tools use the same code. Monocypher keeps its own terms.
- **Who drives the slices (that plan's decision M): the Pro planning session,** from slice 2's pull request on. It also merges a slice once its checks are green and its own build passed.

## 9. What was not done or not checked

- No code was written, built or run in either repository. VSS was read from source and documents. Its tests were not run here.
- The plugin host that hands a plugin the licence file, and the licence reader a plugin may copy, are on VSS's release candidate 6 and 7 integration branches and were not read beyond their names. The library check for plugins that is on `develop` was not read either.
- The private keys in the owner's key folder were not opened. Listing that folder showed the names of the files in it, and no file was read.
- The documentation findings in [4.5](#45-documentation) and [section 7](#7-the-words) come from a read-only sweep of the pages by a subagent, while another session was editing them. I re-checked the line numbers I cite from it against the files, and corrected two. The rest are as of 12:07 and may have moved.
- The docs checks named in 4.5 (`docs.messages`, the site checks) were read and not run. `docs.links` and `docs.paths` were run on this page and pass. (A first run also showed a broken link in `status.md`, from the guide rewrite. It was gone by the second run.)
- I did not check whether ADL's translations catalogues can take new strings without churn. The memory notes say the existing drafts are unreviewed.
- The price, the support channel and the support terms are not in this plan, and none is invented.

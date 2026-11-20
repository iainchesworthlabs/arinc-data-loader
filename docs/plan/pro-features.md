# What Pro and Enterprise cover, and how the paid features are built

This is a plan. It is not published (`plan/` is excluded from the site, `mkdocs.yml`). Nothing described here as "proposed" is built. It follows [the editions plan](editions.md), which sets out the commercial structure, and [the licensing plan](licensing.md), which is how a program knows its edition. This page adds what those two leave open: what a Pro licence and an Enterprise licence cover, which features a licence can turn on, how those features are built and delivered, and the work, cut into packages that an agent can be given one at a time ([section 8](#8-work-packages)).

Written 2026-11-19. The owner's decisions are collected in [section 10](#10-decisions-for-the-owner). No price, date or support term is invented here.

## 1. Summary

**The line between the editions.**

- **Community is the bench.** One person, one target, by hand, in a window. It is complete for that job and stays so.
- **Pro is the rig.** The same operations run unattended: many targets, from a plan, repeatably, with records kept, driven from a lab's own test harness.
- **Enterprise is the programme.** The whole site, the evidence about the tool for certification work, and work on the customer's own platform.

**What each licence covers.**

- A **Pro** licence is for a number of seats, for one year. It gives support and the Pro tools.
- An **Enterprise** licence is a site licence with no seat count, on negotiated terms. It gives everything in Pro, the data packs (what you called the qualification kit), a conformance kit, a version that stays supported for longer, and platform work.

**How a paid feature is turned on.** A paid feature is a separate program, built in the private repository and shipped in a separate Pro package. Each Pro program checks the licence itself. No licence switches anything in Community on or off. The public repository gains only the seams the Pro programs need, and those seams are free for anyone to use.

**The decision everything else follows from** ([decision A](#10-decisions-for-the-owner)), answered on 2026-11-19: a Pro program does not link the GPL libraries. It drives public commands. Virtual Serial Studio (VSS) does the same: its paid plugin links nothing from the public repository. The consequence is that a command that runs one operation on one target without a window is public and free, and Pro is what drives those commands across a rig.

**What can start now.** The owner agreed the public interface additions and the program names on 2026-11-19. C1 (the loader's logic without Qt) and C7 (a platform directory outside the repository) were started that day. C0 and C4 can start as well. The private work needs the private repository, which only the owner creates, and the licence library from the licensing plan's slices 2 and 3.

## 2. What this rests on

Read from this repository on 2026-11-19, on the branch that carries the documentation pull requests #78 to #83, and from VSS at `origin/develop`.

**What the product cannot do today.**

- No program runs an Information, Upload or Download without its window. The three Qt programs take one option, `--smoke-test` ([command line](../user-guide/command-line.md)).
- The loader's receipt and log are in memory only. They can be copied to the clipboard and are never written to a file ([the log and the receipt](../user-guide/log-and-receipt.md)).
- One target for each operation, and one job at a time. There is no queue, no saved job and no way to load one set to several targets.
- The generator has no description file and cannot be run again without the window. It offers one load in a set; the library can write more.
- The mock target's faults are fields in its window. They cannot be put in a sequence or saved. The engine can also refuse with 0x1002 and serve an initialization file of invalid structure, and the window has no control for either ([the mock target](../mock-target.md)).
- No program runs on the target side. The roadmap lists "a target-side application" as left to do ([roadmap](../roadmap.md)).
- A platform is a directory under `libs/platform`, chosen from a fixed list in `cmake/Platform.cmake`. A platform outside the repository cannot be selected.

**What is already a seam.**

- The development package exports five static libraries as the CMake package `adl`: `adl::arinc665`, `adl::arinc615a`, `adl::arinc615a_runner`, `adl::media_tree` and `adl::platform` (`cmake/AdlInstall.cmake`, [using the libraries](../libraries.md)). A program outside the repository can run an operation with them today.
- The logic the loader's window adds is in `apps/app/core` and is not exported. The part of it that has no Qt in it was moved into the library `libs/loader_core` (C1; for example `libs/loader_core/include/adl/loader_core/media_source.hpp`). The rest (`LoaderModel`, `EngineWorker`) is Qt.
- The mock target's engine is "plain C++23, no Qt, no sockets of its own" (`libs/mock_target_core/include/adl/mock/engine.hpp`, moved there by C4) and is not exported.

**The licence of the code.**

- All of it is GPL-3.0-or-later. Contributions are taken under the same licence with a sign-off, and there is no contributor licence agreement (`CONTRIBUTING.md`). `REUSE.toml` names the copyright holder as "Iain Chesworth Labs contributors".
- A closed program that links the `adl` libraries is a combined work under the GPL. Only someone who holds the copyright of all of the code can offer it on other terms.
- The owner has decided that a licence file is a record and that the GPL stays on all of the code in every edition ([licensing plan](licensing.md#8-decisions-for-the-owner), decision 3). Whether the code is also offered under another licence is still open ([editions plan](editions.md), decision 6).

**How VSS does it.**

- Its Community edition is complete for one job (the hobbyist and the reverse engineer). Pro and Enterprise are for the test lab, which uses the program as automated test equipment (`virtual-serial-studio/docs/editions.md`).
- The plugin interface, the control endpoint on the same computer, its Python client and the JUnit reports are public and free. The official plugins, the test harness adapters and remote control are private and paid.
- A paid plugin is a separate process. It links no GPL code: it reads the licence with a reader under a BSD licence, and the private plugin's own notes say "Nothing is copied from the public repository".
- The check in the GPL program is information and not a lock. What is sold is what a patched program cannot give.
- VSS has decided to take no outside contributions for now, so that it needs no contributor agreement.
- One point where its documents disagree: the design says a plugin checks the licence itself, and the notes of the one private plugin say it has no check of its own. ADL's Pro programs check for themselves from the first one ([6.3](#63-the-licence-check-in-a-pro-program)).

## 3. The line between the editions

| | Community | Pro | Enterprise |
| --- | --- | --- | --- |
| Who it is for | An engineer at a bench | A test lab, an integration rig, a production line | A programme or a company |
| The job | Make a media set, check it, load one target, see what happened | Load many targets unattended, the same way every time, and keep the record | Use the tool in certification work, on the customer's own platform, across the site |
| How it is used | By hand, in a window, or one command at a time | From a plan, a harness or a pipeline | As Pro, with evidence and agreements |

Five rules hold the line.

1. **Community never loses anything.** Everything the programs do today stays in Community. A window improvement stays in Community too ([section 7](#7-what-stays-in-community)).
2. **By hand is Community. Unattended at scale is Pro.** One operation on one target, in a window or from one command, is Community. Plans, many targets, retries, reports, records, harness adapters and scenario sets are Pro.
3. **What is sold cannot be had by patching.** A paid feature is code in the private repository, a document, or a service. It is never a switch in the GPL code.
4. **Formats and seams are public.** Anyone can write their own tool against them. Pro is the maintained and supported set.
5. **Reading what you made never needs a licence.** A record written with Pro can be read and verified with no licence, and an ended licence loses nothing.

## 4. What each licence covers

| | Community | Pro | Enterprise |
| --- | --- | --- | --- |
| Term | none | One year | Negotiated |
| Seats | not counted | A number of seats, shown in the file | Site licence: no seat count |
| Trial | n/a | 14 days, on request, as in VSS | By agreement |
| The four programs, under the GPL | yes | yes | yes |
| Support | none promised | Included. Channel and response are the editions plan's decision 4 | A support agreement |
| Supported versions | the latest release | The editions plan's decision 5 | As Pro, plus a version kept supported for as long as a data pack is relied on |
| Pro tools ([5.1](#51-rig-automation) to [5.4](#54-media-set-pipeline)) | no | yes | yes |
| Conformance kit ([5.5](#55-conformance-kit)) | no | no | yes |
| Data packs ([5.6](#56-data-packs)) | no | no | yes |
| Platform and target-side work ([5.7](#57-platform-and-target-side-work)) | no | no | By agreement |
| Tools keep running after the term, for versions released during it | n/a | no | yes (the `updates_until` field) |
| Named computers in the file | n/a | optional | optional |

**A seat.** One seat is one instance of the product in use ([decision B](#10-decisions-for-the-owner), answered 2026-11-19). The licence file shows the number of seats and the terms hold the customer to it. Nothing on a computer counts instances, and nothing will: the owner's view is that restricting use on customers' computers cannot be made to work, and that a customer who breaks the terms is dealt with under the terms.

**A site.** A site licence is a licence with no seat count, as in VSS. What a site is (one building, one programme, one company) is written in each agreement, and the file can carry computer name patterns such as `LAB-*` for a site that wants them.

**Why Enterprise keeps running after the term.** A project that relies on a data pack freezes the tool at the pack's version, often for years. A tool that stopped on a date would break that. The `updates_until` field of the licence file does this already: tools released on or before that date keep working after the licence ends.

**The feature names in a licence file.** The licence library answers `check(feature)`, and a name can end `.*`. The licensing plan already has `support` and `data_packs`. This plan adds:

| Name | Turns on | Carried by |
| --- | --- | --- |
| `pro.automation` | Rig automation | Pro, Enterprise, trial |
| `pro.records` | Load records and the configuration audit | Pro, Enterprise, trial |
| `pro.lab` | The target lab | Pro, Enterprise, trial |
| `pro.media` | The media set pipeline | Pro, Enterprise, trial |
| `enterprise.conformance` | The conformance kit | Enterprise |
| `enterprise.platform.<name>` | A platform pack, by platform | Enterprise, by agreement |

A Pro licence is written with `support` and `pro.*`. An Enterprise licence is written with `support`, `pro.*`, `enterprise.*` and `data_packs`. A trial carries `pro.*` only. Adding a name does not change the file format.

## 5. The paid features

Each is a pack: a set of programs that belong together. For each one this says what it does, who wants it, what has to be public for it to work, and what is not known.

### 5.1 Rig automation

Feature `pro.automation`. For a lab that loads the same rig every morning, or a line that loads every unit.

- **A load plan.** One file names the targets and what each one gets. A program, working name `adl_rig`, runs it: in order or several at once, with a rule for retries and time-outs, and it can carry on from where a failed run stopped.
- **Results a harness can read.** A summary for each target and for the run, as JUnit XML and as a page a person can read.
- **Harness adapters.** A pytest plugin and a Robot Framework library first. Bindings for .NET, LabVIEW and TestStand later, as VSS plans for its own.
- **Recipes.** Worked examples for a CI job, with the mock target standing in for the rig.

Public seam it needs: a command that runs one operation on one target and reports as JSON lines (C3). Private: the plan runner, the reports, the adapters.

### 5.2 Load records and the configuration audit

Feature `pro.records`. For anyone who has to answer "what was loaded on that unit, when, and from which media set".

- **Records kept.** Every operation's record is written to an archive folder under a naming rule, in place of a receipt that is gone when the window closes.
- **Records that show a change.** A record can be signed with a key the site holds, using the same signed-file envelope as the licence file with a purpose of its own. A changed record then fails its check. This shows that the record is as it was written. It does not show that the load was right.
- **The configuration audit.** A baseline file says which parts each target should hold. The audit runs Information against each target and reports the differences, and can list what a whole rig holds.
- **Export.** CSV and JSON, for a site's own configuration records.

Public seam it needs: the operation record as a documented file format (C2). Private: the archive, signing, the audit, the export. Checking a record needs no licence (rule 5).

### 5.3 Target lab

Feature `pro.lab`. For a team that writes a loader, or tests against targets it does not have yet.

- **Many simulated targets.** One file describes a rack of mock targets, each with its own identity, parts and address, and a program starts them all.
- **Scenario sets.** A maintained library of fault sequences: wait, refuse, drop a status file, corrupt a file, abort, go silent, each at a chosen moment, from a fixed seed so that a run can be repeated.
- **Record and replay.** A real target's side of a session is captured and played back as a simulated target.

Public seam it needs: the mock target without its window (C4) and a scenario mechanism in its engine (C5). Private: the rack runner, the scenario library, record and replay.

Not known: several mock targets on one computer. FIND is fixed to port 1001 and two sockets on one port are not reliable on every platform ([the mock target](../mock-target.md)), so a rack on one computer may need one address for each target.

### 5.4 Media set pipeline

Feature `pro.media`. For a team that makes media sets as part of a build.

- **Batches.** Many media sets from one table of part numbers and files.
- **Compare.** What differs between two media sets: parts, files, check values.
- **A manifest.** The contents of a set as CSV or JSON.
- **Site rules.** A site's own rules for part numbers and layout, applied to a set as findings.

Public seam it needs: the generator without its window, from a description file (C6). Private: the rest.

This is the weakest of the four. Most of its value is in the public seam. It is listed so that the decision to build it, or not, is made on purpose.

### 5.5 Conformance kit

Feature `enterprise.conformance`. For a supplier who writes a target's loading function, or a loader of their own, and wants it exercised before it meets real equipment.

- **Test a target.** A set of scripted sessions run against the customer's target: loss, duplication, reordering, WAIT, ABORT, silence, time-outs, wrong check values. The project's own tests already run these against a simulated target on the fake network. The kit runs them against a real one and writes a report.
- **Test a loader.** The same from the other side, with the scenario sets of the target lab.

What the report may say: what the target did, against this project's reading of the standard. It never says that a target meets the standard. Nothing here has been run against real equipment ([what has been checked](../status.md)).

Not known, and it decides the cost: a loader that misbehaves on purpose does not exist. The engine is written to behave. Under the choice of [6.2](#62-two-ways-a-pro-program-can-use-the-engine) the mechanism for that is public, and it is the hardest part of the kit. How much of it is built in public is to be decided before P6 is briefed.

### 5.6 Data packs

Feature `data_packs`. The Enterprise addition already planned in [the assurance data pack plan](assurance-data-pack.md). That plan stands and is not repeated here. It waits for its own decisions, and its largest part is written by people, not generated.

### 5.7 Platform and target-side work

Feature `enterprise.platform.<name>`. For a customer who wants the loading function inside their own equipment, on INTEGRITY-178B, VxWorks, QNX or another system.

What exists: the platform layer is shaped for these. FreeRTOS and Zephyr are built and tested on simulators in CI. The QNX and INTEGRITY code has never been compiled. VxWorks is a design ([platform abstraction](../platform-abstraction.md)). Those sources are public and stay public (rule 1).

What an agreement could add:

- **A port checked on the customer's system.** The conformance suite of the platform layer, run on their board and their toolchain, with the results. These toolchains are not free and cannot run in public CI, so this work is done with the customer.
- **A target-side program.** The roadmap's missing piece: the program that answers the loader on the target.
- **Other terms for the code.** A customer who ships equipment with GPL code inside it takes on the GPL's duties for that equipment. Most will not. So this pack is only worth offering if the code is also offered on other terms, which is the editions plan's decision 6 and needs a solicitor ([decision H](#10-decisions-for-the-owner)).

Public seam it needs: a platform directory outside the repository (C7), so that a port can be kept by the customer or in the private repository without a fork.

No work package for the ports is written here. Each needs a customer, a toolchain and decision H.

### 5.8 Later candidates

Not planned. Listed so that they are not lost. The statements about the standards are as understood and must be checked against the documents.

- **Loading over ARINC 429** (ARINC 615). The older loaders. It needs interface hardware from a card vendor, and a second protocol in the loader's window.
- **Signed loads and crates** (ARINC 835 and ARINC 827, as understood). Check a digital signature on a load, and read a crate. It needs the documents, which are not in the repository.
- **Remote control.** Start a plan on a rig from another computer, with a token. VSS has a design for this, and it could be shared.
- **Operator identity and approval.** Who ran a load, and a second person's approval before it starts.
- **A site policy file.** An administrator fixes preferences for every user of a computer. VSS has decided that its own policy file needs no licence.

### 5.9 What to build first

| Order | Pack | Why here |
| --- | --- | --- |
| 1 | Rig automation | The largest gap for a lab, and every other pack reuses its seam |
| 2 | Load records and the audit | The second thing a lab or a line asks for. It reuses the signed-file envelope |
| 3 | Target lab | Small once the mock target runs without its window |
| 4 | Conformance kit | The one Enterprise feature that is code. It depends on the target lab and on decision A |
| 5 | Media set pipeline | Low value on its own |
| With people | Data packs | Its own plan. It does not wait for any of the above |
| With a customer | Platform work | Needs decision H |

## 6. How a paid feature is turned on

### 6.1 Separate programs, not switches

A Pro feature is a program in a Pro package, installed beside the Community programs. The Community programs do the same with and without a licence. This is VSS's stance, and it is the only one that holds for GPL code: a switch in public source can be removed by anyone who builds it.

So the direct answer to "what does a Pro licence turn on" is: the Pro programs. Without a licence each one says so in one sentence and stops. Nothing in the three windows is locked, greyed or hidden by a licence.

### 6.2 Two ways a Pro program can use the engine

A Pro program has to run operations, and the engine is GPL. There are two ways.

| | Drive public commands (chosen) | Link the libraries |
| --- | --- | --- |
| How | The Pro program starts a public command as a child process and reads its JSON lines | The Pro program links the `adl` static libraries |
| What becomes public | The commands that run one operation, the mock target without a window, the generator without a window, the scenario mechanism | Nothing new |
| What is Pro | Plans, many targets, reports, adapters, records, audit, scenario sets, the kit's suites and reports | All of that, and the commands themselves |
| Legal step first | None | The owner must hold the copyright of all the code; outside contributions need an agreement or must stop; a solicitor reviews it |
| Same as VSS | Yes | No |
| Risk | A lab can write its own plan runner over the free commands | Anyone can publish a free command over the public libraries, which would leave the Pro one with little to sell |

The owner chose the first on 2026-11-19. It needs no legal step, so work can start. It matches VSS, so the two products make one promise. And it gives Community a real improvement, which is what brings people to the product.

The second way is still needed for one thing: equipment that carries the code ([5.7](#57-platform-and-target-side-work)). Keeping it possible costs little if it is done now: confirm that both commit identities on `develop` are the owner's, and decide what happens to outside contributions ([decision E](#10-decisions-for-the-owner)).

The work packages in [section 8](#8-work-packages) are written for that choice: C3 to C6 are public.

### 6.3 The licence check in a Pro program

- Each Pro program reads the licence itself, with the shared licence library, and asks for its own feature name. It does not ask a Community program.
- The library a closed program links has to be under terms that allow that. VSS's reader for plugins is under a BSD licence. The shared licence component needs the same answer before the first Pro program is built ([decision F](#10-decisions-for-the-owner)).
- No licence, or an ended one: the program prints one sentence that says why and how to see the licence, and exits with a code of its own. It never deletes or hides anything.
- In the grace period: it runs, and prints one line that says how many days are left.
- A run that has started finishes, even if the term ends during it.
- A program that only reads or verifies (a record, a manifest) asks for no licence.

The clock. The clock check can read `clock_suspect` on a bench whose clock resets. The owner decided on 2026-11-19 ([decision G](#10-decisions-for-the-owner)) that a Pro program which can tell that the clock is wrong says so in one line and carries on. It does not stop. Here too the terms are what hold a customer, not the program.

### 6.4 The repository and the package

The licensing plan already gives ADL a private repository for the customer records and the signing workflow. The Pro source lives in the same repository, as VSS's does. It uses Community at a pinned release and never forks it.

Planned layout. None of it exists.

```text
arinc-data-loader-pro/            private; created 2026-11-19 with its rules, plan and format design
  licences/                       records, requests, issued files, ledger (licensing plan)
  tools/                          issuing tools (licensing plan)
  programs/rig/                   adl_rig
  programs/records/               adl_records
  programs/lab/                   adl_lab
  programs/media/                 adl_media
  adapters/python/                the pytest plugin and the Robot Framework library
  suites/                         scenario sets and the conformance suites
  docs/                           the Pro guide, shipped in the package
  COMMUNITY_REF                   the Community release this builds and tests against
```

- **The package.** A separate download, signed and with a bill of materials as the Community releases are. A Community package never holds a Pro file, and the release check that already refuses the owner's tool in a package gets the Pro program names as well.
- **Delivery.** Sent by the owner with the licence file. There is no shop and no download area today.
- **The guide.** The Pro guide is in the private repository and in the package. The public site lists a pack once it is released, and until then says "proposed".

### 6.5 What the Community programs show

- The Licence screen lists what the licence includes, with a row for each pack. These are words in the licence library, in the licensing plan's slice 5.
- Nothing else in the first version.

Later, if the owner wants Pro to be visible in the windows, the way to do it is a companion seam (C8): a window can start an installed companion program and hand it what it has, such as the record of the operation that just ended. Three things it would allow: keep every receipt in the records archive, run a load plan from the loader, and compare a target with its baseline after Information. The seam is public and a companion is found by a signed manifest, as a VSS plugin is.

### 6.6 Plugins: not yet

A plugin framework is a large piece of work. VSS's has a manifest, signatures, trust levels, a policy file, a sandbox and a host, and it took several release candidates. ADL does not need one for the four Pro packs, because a pack is a program and not a part of a window.

A plugin is the right tool when something has to appear inside the loader's window: a second protocol, or a second kind of media. When that day comes, the plugin host should be the second component shared with VSS, after the licence library, and not a second design.

### 6.7 Platform packs

"Extra targets" in the sense of more operating systems are sources and results delivered under an agreement, not a switch. A port is compiled into the libraries, so the customer has its source in any case. The value is the checking on their system and the support. The seam (C7) lets a port live outside the repository.

## 7. What stays in Community

- Everything the four programs do today.
- Improvements to the windows: more than one load in a generated set, the generator's findings list, saving a receipt to a file by hand, the two mock target faults the window lacks.
- By the choice of 6.2: the commands that run one operation without a window, the mock target and the generator without their windows, and the scenario mechanism.
- Every file format: the operation record, the scenario file, the generator's description file. The load plan and the baseline file are Pro's own formats and are documented in the Pro guide.
- The licence check itself, the Licence screen and `adl_licence`.

## 8. Work packages

Each package is one pull request and can be given to one agent. Read [8.1](#81-rules-for-every-agent) with each brief.

### 8.1 Rules for every agent

- Read `AGENTS.md` and `CONTRIBUTING.md` first. They win over this page.
- Work in a worktree of your own on drive E, in a folder named after the branch under the worktree folder the owner uses there. Do not work in the shared checkout on drive C: another session changes its branch. Branch from `origin/develop`.
- Commit with `git commit -s`. No attribution lines in a commit or a pull request. Push, and open a draft pull request against `develop`, only if the person who started you said so.
- Build and test as `AGENTS.md` says: `linux-debug` and `linux-asan-ubsan`, and Windows. Non-Qt work can be configured with `-DADL_BUILD_APP=OFF`. One test, "a datagram arrives whole with the address it came from", fails in WSL on unchanged `develop`: report it and do not chase it.
- Run `tools/ci/precheck.py` before you report.
- The code rules are in `AGENTS.md`: no preprocessor, `std::expected`, the names, a labelled test for every behaviour, a fuzz harness for every function that parses bytes from outside, an SPDX line on every new file.
- A brief lists the additions to public interfaces that the owner has agreed for it. Anything else that changes the `adl` package, an option, a file format or what a program writes: stop and ask.
- Leave alone: `AGENTS.md`, `docs/ci.md`, `docs/testing.md`, `docs/plan/editions.md`, the licence library and its screens (another session is building them), and the documentation pull requests #78 to #83.
- A new program has rows in the guide's command-line page and an entry in the message check. Both exist only on the documentation branches until #80 is merged. If your base has them, update them. If not, say so in the pull request and do not create them.
- Words: "licence" is the noun. The editions are Community, Pro and Enterprise. Never "activate", "unlock" or "register" for a licence. Never say the tool is certified, qualified, compliant, approved or guaranteed.
- Report what you built, which tests you ran and their counts, and what you did not do.

### 8.2 The packages

| Id | Where | What | Needs first | Size | Can start |
| --- | --- | --- | --- | --- | --- |
| C0 | public | `CONTRIBUTING.md` says no outside contributions are accepted for now (decision E) | nothing | small | draft pull request #85, opened 2026-11-19 |
| C1 | public | The loader's logic without Qt, as a library. Split in three on 2026-11-19, after its inventory put it at about 2,300 changed lines: C1a the library and the plain moves, C1b the record and its recorder, C1c running an operation without Qt | nothing | large | C1a is draft pull request #89. C1b and C1c are built on branches of their own, cut from C1a's tip, and not pushed |
| C2 | public | The operation record as a file format | C1b | small | built on a branch cut from C1b's tip, not pushed |
| C3 | public | A command that runs one operation | C1, C2 | large | its base is ready: a local branch that merges C1b, C2 and C1c on top of C1a, built from nothing and passing 646 of 646 tests on Windows on 2026-11-19 |
| C4 | public | The mock target without its window. Split in three on 2026-11-19, after its inventory put it at 2,100 to 3,000 changed lines: C4a the library, C4b the program `adl_mock`, C4c a test for each fault | nothing | large | C4a is draft pull request #88. C4b is built and not pushed. A crash it found in the mock target's link when a port is in use goes on a branch of its own between the two |
| C5 | public | Scenarios in the mock target's engine | C4, the licence library's strict JSON reader | medium | after C4 |
| C6 | public | The generator from a description file | The strict JSON reader | medium | after licence slice 2 |
| C7 | public | A platform directory outside the repository | nothing | small | built and checked 2026-11-19; draft pull request #86. Not covered: macOS, and a platform whose image is built by a project of its own, as FreeRTOS and Zephyr are |
| C8 | public | The companion seam: a design, not code | P1 and P2 in use | small | later |
| C9 | public | A program's arguments as UTF-8, in the platform layer, used by `adl_write_payloads`. Found on 2026-11-19: on Windows a plain `main` gets its arguments in the code page, so a path with characters outside it is lost. `adl_load` and `adl_mock` take the function once it exists | nothing | small | started 2026-11-19; the owner agreed the addition to `adl::platform` that day |
| C10 | public | `adl_load serve`: one loader that runs operations on several targets at once from one port. Found on 2026-11-20 by running the Pro tools against the built commands: two loaders cannot share port 59 | C1, C3 | large | agreed by the owner 2026-11-20 (decision L); the contract is written and confirmed; started that day in the session that built C3, on two branches on top of C3's, not pushed |
| P0 | private | The Pro repository's build, the shared libraries, the licence check helper with a stand-in for the reader, stand-ins for the public commands | nothing; the real licence reader (licence slices 2 and 3, decision F) is wired in later and the package is refused until then | large | built on 2026-11-19 as two local branches, the second on the first: the build, strict JSON, starting a command and the common code; then the stand-ins, the licence helper, the interrupt helper, the package refusal and a workflow that runs only by hand. Merged into `main` on 2026-11-20 as pull requests 1 to 4, with the path and arguments fix and the common rules |
| P1 | private | `adl_rig`: load plans, results, JUnit | P0, and a stand-in for `adl_load` until C3 is released | large | started 2026-11-19 |
| P2 | private | `adl_records`: site keys, signing, the archive, then the baseline and the audit | P0, and the same stand-in | large | started 2026-11-19 |
| P3 | private | The pytest plugin and the Robot Framework library | P1 | medium | started 2026-11-20, on a branch cut from P1's |
| P4 | private | `adl_lab`: a rack of mock targets from one file. Scenario sets and record and replay are later work and wait for C5 | P0, and a stand-in for `adl_mock` until C4 is released | medium | started 2026-11-19 |
| P5 | private | `adl_media`: batches, compare, manifest, site rules | P0, C6 | medium | after C6 |
| P6 | private | The conformance kit | P4, and a decision on the misbehaving loader | large | last |
| P7 | private | The Pro package and the Pro guide | P1 and P2 | medium | after P2 |

Four formats are Pro's own: the load plan, the signed record, the baseline file and the rack file. A format is hard to change once a customer has files in it, so they were written down on 2026-11-19, before the programs, in the private repository's design folder. The contract of the public commands the Pro tools start, and the operation record of C2, is [headless-commands.md](headless-commands.md). Both are for the owner to review.

P1, P2 and P4 no longer wait for the public commands: P0 builds stand-ins for `adl_load` and `adl_mock` from that contract, and the tools are tried against the real commands when those exist.

### 8.3 Briefs for the public packages

**C1. The loader's logic without Qt.** Branch `feature/loader-core-library`.

- Goal: a program with no Qt can read and check a media set, and run Information, Upload and Download, by the same rules as the loader's window.
- Do: make a new static library, `adl_loader_core` with the alias `adl::loader_core`, beside the other libraries. Move into it the parts of `apps/app/core` that have no Qt in them or need none: the media set index and file source (`media_source`), the check that turns a set into findings, the making of an operation from the chosen loads, the naming of a download folder, and a plain record of an operation (target, operation, set, loads, outcome, status codes, times, log lines). `LoaderModel` and `EngineWorker` stay where they are and call the library.
- First step: list what in `apps/app/core` is free of Qt and what is not, and put the proposed cut in the pull request description before moving code. Move in small commits.
- Nothing a user sees changes. Every existing test passes, with only its includes changed.
- Agreed public addition (decision D): the library is exported by the `adl` package through `adl_install_library`.
- Also update: `docs/architecture.md`, `docs/libraries.md`, `.github/ci/plan.json` if paths change, a changelog fragment for the development package.
- Done when: the window's tests pass on Windows and Linux; the moved code has unit tests that build with `-DADL_BUILD_APP=OFF`; a consumer of the installed package can link the new target (the package's own consumer test, extended).

**C2. The operation record as a file.** Branch `feature/operation-record`.

- Goal: the record of C1 can be written to a file that another program can read.
- Do: a writer in `adl::loader_core` that turns a record into JSON with a `format` number, written by hand with no new dependency. A page section in `docs/file-reference.md` that gives every field. Golden files in the tests.
- Not in this package: a reader, a "save" control in the window, any change to the receipt on screen.
- Agreed public addition: the file format.
- Done when: the golden files match byte for byte on the three desktops, and the fields in the page are the fields the writer writes (a test compares them).

**C3. A command that runs one operation.** Branch `feature/headless-loader`. The program's name is decision C; `adl_load` is the working name.

- Goal: `find`, `information`, `upload` and `download` against one target, with no window and no Qt.
- Do: a program beside `adl_write_payloads`, over `adl::loader_core` and `adl::arinc615a_runner`. It takes the target's address and position, the media set folder, the loads to send or all of them, the files and folder for a download, the local address and the control port. With `--json` it prints one JSON object for each event on standard output. With `--record` it writes the record of C2. It reads the set and checks it before it sends anything, as the window does, and refuses a set that cannot be read.
- Exit codes: one for each of completed, the operation failed, wrong usage, the media set could not be read or has changed, the target did not answer. Fix the numbers in the brief's pull request and put them in the guide's table.
- Not in this package: more than one target, retries, a plan file, stopping on a signal (that needs platform code; say in the guide that stopping the program abandons the operation and the target times out).
- Tests: `system` label, against the mock target's engine on 127.0.0.1, the way the loopback test does.
- Agreed public additions: the program, its options, its output, its exit codes.
- Done when: the four operations pass over loopback on the three desktops, and every message and exit code is in the guide where the base has the guide's pages.

**C4. The mock target without its window.** Branch `feature/headless-mock-target`. Working name `adl_mock`.

- Goal: the mock target runs from a command, for a CI job or a rack.
- Do: move the engine and the link of `apps/mock_target/core` into a library that both programs use (not exported). Write a program that takes the identity, the parts, the folder of files it holds, the storage folder, the ports and every fault as options, including the two the window lacks (refuse with 0x1002, and an initialization file of invalid structure). It prints events as JSON lines with `--json`. It runs until it has served a given number of operations or for a given time.
- Not in this package: a configuration file, a sequence of faults (C5), more than one target in a process.
- Tests: `system`, with the loader's runner on loopback.
- Agreed public additions: the program, its options and output.
- Done when: every fault of the window can be set from the command line and a test shows the loader's reaction to each.

**C5. Scenarios in the mock target's engine.** Branch `feature/mock-scenarios`.

- Goal: a file says which fault happens when, so that a run can be repeated.
- Do: a scenario is an ordered list of steps. A step has a trigger (the nth request, the nth status file, a time since the operation began) and an action (any fault of C4, a delay). A seed fixes anything random. The engine applies it; the window and `adl_mock` both take a scenario file. Read the file with the licence library's strict JSON reader: do not write a second one. If that reader takes flat objects only, stop and ask before extending it.
- A fuzz harness for the scenario reader, with seeds from `tools/fuzz/make_seeds.py`.
- Agreed public additions: the file format, one option on `adl_mock`, one control in the window (with its text in the seven catalogues and the guide).
- Done when: three scenarios in the tests give the same events on every run and on the three desktops.

**C6. The generator from a description file.** Branch `feature/headless-generator`. Working name `adl_make_set`.

- Goal: a media set is made again from a file, with no window.
- Do: move what in `apps/media_gen/core` needs no Qt into a library. A description file gives the files, the part numbers, the members, the check value type and the batch file, and may give more than one load. A program reads it, writes the set, reads it back and verifies it, as the window does, and writes into place only after that. Same strict JSON reader as C5, and a fuzz harness.
- The output for a given description is the same bytes on every run and every desktop.
- Agreed public additions: the program, the description format.
- Done when: the sets it writes are read by the loader's own check in the tests, and two runs compare equal byte for byte.

**C7. A platform directory outside the repository.** Branch `feature/external-platform-dir`.

- Goal: a port can be built without adding a directory to the repository.
- Do: a CMake cache path (working name `ADL_PLATFORM_DIR`) that, with `ADL_TARGET_PLATFORM`, selects a platform fragment from another place. `cmake/Platform.cmake` uses it in place of its fixed path when it is set. Nothing changes when it is not set.
- Test: a CI step copies the Linux fragment to a folder outside the source tree and builds and runs the unit tests from there.
- Also update: `docs/building.md`, `docs/platform-abstraction.md`.
- Agreed public addition: the CMake option.
- Done when: the copied fragment builds and passes, and a build without the option is unchanged.

**C8. The companion seam.** A design page, not code, and not before P1 and P2 are in use. It answers: how a window finds a companion program, how the manifest is signed, what the window hands over, and what the user is asked. It should reuse VSS's manifest and signature and say what it leaves out.

**C10. One loader for several targets.** Branch `feature/loader-serve`, on top of the branch of C3.

- Goal: operations on several targets at once from one address and one port. Found on 2026-11-20 by running the Pro tools against the built commands: every `adl_load` listens on port 59, so the second one started at the same time ends with "address in use". A real target writes to 59, so giving each loader a port of its own only helps with simulated targets.
- Do, in the loader library: run several operations from one control socket, each with its own engine object, and give each datagram to the operation whose target sent it. One address has one operation at a time. An operation can be aborted from this side.
- Do, in the program: the command `adl_load serve` as [the contract, section 2.1](headless-commands.md#21-adl_load-serve-one-loader-several-targets) writes it: requests as lines of tab-separated arguments on standard input, events with an `id` on standard output, `request_refused`, `stopped`, and the exit codes.
- Test: unit tests of the request line reader with a fuzz harness; a system test with two and with eight simulated targets at once on loopback addresses; a `cancel`; a second request for an address that is in an operation; the end of the input while operations run; an operation that fails beside one that completes.
- Also update: the guide's command-line page, the messages page, `docs/architecture.md` for the library's new header, a changelog fragment.
- Agreed public addition (decision L): the command, its request lines, its events and its exit codes, and the new header of the loader library.
- The edition line: `serve` is a seam. It has no plan, no retries, no schedule and no report, and a person cannot use it without a program that drives it. Those stay in `adl_rig` and `adl_records`.
- What follows in the private repository, after the command exists: the stand-in for `adl_load` learns `serve`; `adl_rig` uses it when `run.parallel` is above 1, and `adl_records` when `--parallel` is; both fall back to one `adl_load` for each operation, with the rule against two loaders in one place, when the loader has no `serve`.
- Done when: eight simulated targets are loaded at once from one port on Windows and Linux, and a single `adl_load upload` is unchanged.

### 8.4 Briefs for the private packages

These are shorter, because each waits for something that does not exist yet, and its brief should be finished when it does. All are in the private repository and none links a GPL library (decision A).

**P0. The build, the check and the package.** A CMake project with the pinned Community release in one file. A helper that every Pro program calls first: read the licence, ask for a feature, and return "run", "run with this notice" or "stop with this sentence". The rules are those of [6.3](#63-the-licence-check-in-a-pro-program). Tests with the test key for every licence state. A script that makes the package and refuses to add a Community file to it. A check job that runs without any key.

**P1. `adl_rig`.** Reads a load plan. Starts the public command once for each target, in order or several at once. Applies the plan's retry and time-out rule. Writes a summary, JUnit XML and the records. Can carry on a failed run. Feature `pro.automation`. Its tests use the public mock target command as the rig.

**P2. `adl_records`.** Keeps records in an archive folder under a naming rule. Signs a record with a site key and verifies one (verifying asks for no licence). Reads a baseline file, runs Information through the public command, and reports what differs. Exports CSV and JSON. Feature `pro.records`.

**P3. Harness adapters.** A Python package with a pytest plugin and a Robot Framework library. Both call `adl_rig` and the public commands and hold no logic of their own, so the licence check stays in the compiled programs.

**P4. `adl_lab`.** Starts a rack of public mock targets from one file. Carries the scenario sets. Captures a target's side of a session and plays it back. Feature `pro.lab`.

**P5. `adl_media`.** Batches from a table through the public generator command, compare, manifest, site rules. Feature `pro.media`.

**P6. The conformance kit.** Not briefed until the misbehaving loader has an answer ([5.5](#55-conformance-kit)).

**P7. The package and the guide.** The Pro guide, the package for the three desktops, its signature and bill of materials, and the note that goes to a customer with it.

## 9. Order

1. **The owner answers the decisions.** A, C, D and E are answered, which unblocks the public work. B, F and G remain. F and G unblock P0.
2. **C1 and C7 start together.** They touch different files. C4 can start with them once A and C are answered: it touches the mock target only.
3. **C2, then C3.** In the meantime, one author writes the four Pro formats (plan, signed record, baseline, rack) and the owner agrees them.
4. **The owner creates the private repository.** Licence slices 2 and 3 are merged. P0.
5. **P1 and P2 together**, then P3 and P7. This is the first Pro release: rig automation and records.
6. **C5, C6, then P4 and P5.**
7. **P6**, and the data packs and platform work as their own plans and agreements allow.

Work that can collide with other sessions:

- The licence session is in the licence library, the shared QML and the platform paths. C1 to C7 stay out of those.
- The documentation session owns the guide. Every package that adds a program needs the guide's pages, which are on unmerged branches. Until #80 is merged, a package's guide changes wait.
- C1 changes many files in `apps/app/core`. Nothing else should change that folder while it is open, including the licensing plan's slice 5b (the edition in the receipt and log), which should follow C1 and write into the record of C1.

## 10. Decisions for the owner

A. **Whether a Pro program may link the GPL libraries. Answered 2026-11-19: no. A Pro program drives public commands, as in VSS.** The commands of C3 to C6 are public and free ([6.2](#62-two-ways-a-pro-program-can-use-the-engine)).

B. **What a seat is. Answered 2026-11-19: one seat is one instance of the product in use, held to by the terms.** No program counts instances or tries to restrict them on a computer ([section 4](#4-what-each-licence-covers)).

C. **The names of the new programs. Answered 2026-11-19: the short names.** Public: `adl_load`, `adl_mock`, `adl_make_set`. Pro: `adl_rig`, `adl_records`, `adl_lab`, `adl_media`.

D. **Additions to public interfaces** (`AGENTS.md` says to ask). **Answered 2026-11-19: yes to all.** A sixth library in the `adl` package (C1); the operation record format (C2); three new programs with their options, output and exit codes (C3, C4, C6); the scenario and description formats (C5, C6); one CMake option (C7). Anything beyond a brief is still asked.

E. **Outside contributions. Answered 2026-11-19: none are accepted for now, as VSS decided, and `CONTRIBUTING.md` is changed to say so (C0).** No contributor agreement is needed yet, and the rule can be opened later. The answer was given on the understanding that both commit identities on `develop` are the owner's. That keeps the second way of 6.2 possible.

F. **The terms of the shared licence library. Answered 2026-11-20: BSD-2-Clause as well as the GPL, as VSS does for its reader.** A closed Pro program uses it, so it cannot be GPL only. The library's own files carry both licences from slice 2 of the [licensing plan](licensing.md#5-slices) on, and the Pro repository takes the same code at a pinned commit until the shared repository exists ([licensing plan](licensing.md#6-moving-it-out-and-sharing-it)). There is one reader, not two.

G. **A Pro program on a computer whose clock is wrong. Answered 2026-11-19: if it knows the clock is wrong it says so, and it carries on.** Enforcement is by the terms ([6.3](#63-the-licence-check-in-a-pro-program)).

H. **Other terms for the code, for equipment that carries it.** This is the editions plan's decision 6, and it needs a solicitor. Platform and target-side work is only worth offering with it ([5.7](#57-platform-and-target-side-work)).

I. **Which packs are in the first Pro release.** Recommended: rig automation and load records ([5.9](#59-what-to-build-first)).

J. **What is available for the later candidates:** the ARINC documents beyond 665 and 615A, an ARINC 429 interface card, a real target, and an RTOS toolchain. None was assumed.

K. **Where a customer gets the Pro package.** Today: sent by the owner with the licence file.

L. **Operations on several targets at once from one address. Answered 2026-11-20: yes, a public command serves several targets from one port.** It is `adl_load serve` (C10). The owner confirmed the detail of the command, in [the contract](headless-commands.md#21-adl_load-serve-one-loader-several-targets), the same day: requests as lines of tab-separated arguments, events with an `id`, and up to 64 operations at once.

M. **Who merges, and who drives the licence slices. Answered 2026-11-20: the planning session does both.** It merges a pull request in either repository once its checks are green and its own build of that tip passed: by squash into `develop` in the public repository, as the working rules say, and in stack order into `main` in the private one. It starts and checks the remaining slices of the licensing plan. Releases, tags, repository settings, secrets, signing keys and the real licence keys stay the owner's.

Still open from the other plans and needed here: the support channel and response, and the supported versions ([editions plan](editions.md), decisions 4 and 5); the data pack decisions ([assurance data pack plan](assurance-data-pack.md), section 9).

## 11. What was not done or not checked

- No code was written and nothing was built or run for this plan.
- The facts in section 2 come from two read-only sweeps by subagents, of this repository and of VSS. I checked these myself: the files of `apps/app/core`, `apps/mock_target/core` and `apps/media_gen/core`; that `media_source.hpp` and the mock target's `engine.hpp` and `link.hpp` include no Qt; the runner's header; and which files and pull requests are on `develop`. The rest is as reported.
- How much of `apps/app/core` can move without Qt is not measured. C1's first step measures it. If little can move, C1 is larger than "medium".
- Whether the licence library's strict JSON reader can read a scenario or a description file is not known: the licensing plan describes it as reading a flat payload. C5 and C6 depend on the answer.
- The statements about ARINC 615, 827 and 835 in 5.8 are from memory and were not checked against the documents.
- No customer was asked what they would pay for. The order in 5.9 is a judgement from what the product lacks.
- The legal points (linking, other terms for the code, code written with an assistant's help, which VSS's own plan also raises) are for a solicitor. Nothing here is legal advice.
- The licence feature names in section 4 are not in the licensing plan yet. That page is being worked on by another session and was not edited.

# Assurance data packs: a plan

This is a plan for the owner to decide. It is not published (`plan/` is excluded from the site, `mkdocs.yml`). It works out what a data pack for ARINC Data Loader, used as a test tool in aerospace certification work, should contain, says what the repository can already supply and what needs people, and proposes a generator and a folder layout. **No generator is built, and none will be until the owner agrees.** It belongs with [the editions plan](editions.md): the data pack is the Enterprise edition's addition.

## What this plan is not

- It is not a claim that the tool is certified, qualified, compliant with DO-178C, or approved by any authority. It is none of those, and nothing here changes that.
- **A data pack supports the user's own tool qualification. It does not qualify the tool on its own.** The user's project owns the certification liaison, the plan that says where the tool is used, and the assessment of what could go wrong. The pack is evidence about the tool, for the project to use or not.
- It is not checked against the standards. DO-178C and DO-330 are not in the repository, and their text is not copied here. What follows is the structure of those documents as I understand it. **Every statement about them in sections 2 and 3 must be checked against the documents by the owner, or by the assessor of a project that will use a pack, before anything is built on it.**

## 1. Why a pack, and for whom

The customer is a project that uses ARINC Data Loader in work that is subject to DO-178C (software considerations in airborne systems) or a similar standard, for example to prepare, check or load a media set, or to test a target's loading behaviour. Such a project has to decide whether a tool it uses needs to be qualified, and, if it does, show the authority the evidence for the level it needs. The evidence is about the tool: what it is for, how it was made and checked, what it can get wrong. A pack is the project's way of buying that evidence, in a form a project can put in front of its authority.

The Community edition gives none of this ([Assurance](../assurance.md)).

## 2. What the standards ask for, as understood

Check every line of this section against DO-178C section 12.2 and DO-330.

- DO-178C says where tool qualification applies and points to DO-330 for how. A tool is considered by what it could do to the airborne software or to the verification of it, within the scope of its intended use in the project's plan.
- There are three criteria, in outline:
    1. The tool's output is part of the airborne software, so it could insert an error.
    2. The tool automates a verification process, so it could fail to detect an error, and its output is used to justify dropping or reducing some other verification or development process.
    3. The tool could fail to detect an error, within its intended use, and neither of the above applies.
- The qualification level depends on the criterion that applies and on the software level that the project assigns to the software the tool is used on. DO-330 names five tool qualification levels, TQL-1 (the most demanding) to TQL-5 (the least). Which cell of the table gives which level is to be read from the standard, not from this page. A tool used only on software of the lowest levels may need no qualification.
- DO-330 asks, at the level that applies, for a set of objectives and the data that show them met. The data items, in outline: a plan for the qualification, the tool's operational requirements, and (at the higher levels) its requirements, design and source; verification cases and procedures and their results; configuration management and quality assurance records; problem reports; and a summary of what was done.
- The project, not the tool's maker, proposes the qualification to its authority. The tool's maker can supply evidence, and in DO-330's terms may do some of the work, but the project's plan says what it relies on.

## 3. The questions that decide the level, and why the project answers them

Two questions decide whether a project needs to qualify ARINC Data Loader and to what level, and only the project can answer them, because they depend on how it uses the tool and on the software level of what the tool touches:

1. **Could the tool put an error into airborne software or its loads?** The media set generator writes the load package that goes towards the aircraft: the header file, the lists and the check values. A wrong header or a wrong check value in what it writes is an error in the package. The loader moves the files of a load to a target. If it altered a file in transit, that would be an error in what the target receives, though the target and later steps may detect it by their own checks. Whether either counts as part of the airborne software, for a given project, is the project's reading.
2. **Could it fail to detect an error, in the project's use of it?** The loader reads and checks a media set before it sends it (lengths, CRCs, check values, the standard's recommendations as findings). A project that relies on that check to show a load is correct, instead of checking by another route, relies on the tool to detect errors. The generator's read-back verification is similar. The mock target is a test aid and its output is not part of anything loaded.

[Safety of use](../user-guide/safety-of-use.md) already tells every user not to rely on the tool for these purposes unless their programme has assessed it. A pack is what a project would assess it with.

## 4. What the pack would contain

Each item says what it is, what exists today, and what is missing. "Today" is the repository at version 0.1.1.

| Item | What it is | Today | Missing |
| --- | --- | --- | --- |
| Intended use and operational environment | What the tool is for, the platforms and versions it runs on, the inputs it takes, the outputs it gives, and what is out of scope | The guide says what the programs do and the platforms they run on. `docs/status.md` says what has been tried | A statement written as an intended use, per program and per function, that a project can adopt or narrow |
| The analysis of whether it could insert or miss an error | Section 3, done properly: each function, each way it could go wrong, what would detect it, and what the evidence is | Section 3 of this page is a start. The libraries' own detection (read-back, CRC and check value verification) is described in the coverage pages | The analysis itself, reviewed. Which functions fall in which criterion is for the project |
| Tool operational requirements | What the tool shall do when used as intended, numbered, each testable | None. There are no requirement documents and no requirement identifiers | The requirements. Section 5 proposes a numbering and the topics. They must be written and agreed by people, not generated |
| A tool qualification plan | The plan for the qualification: what is claimed, the level, the process, who does what | None | The plan, for a level the project gives |
| Verification cases and procedures | The tests, how each is run, and what it shows, linked to the requirements | 556 test cases in three labelled tiers, golden files from an independent writer, a loopback test over real sockets, 16 fuzz harnesses, a window test suite, and coverage gated at 90 percent of lines and 80 percent of branches of the portable code | Links from each test to a requirement. Structural coverage to the depth a level asks for, where it asks for more. A test against real equipment: none exists |
| Verification results | What happened when the tests ran, for the version in the pack | CI logs and the release workflow's runs, kept by GitHub for as long as its settings allow | Results kept as a record for the pack's version, with the build that made them. Nothing is kept as a managed record today |
| Configuration management | What was built from what: the baseline, the build, the tools used, the change history | Git history, signed releases, hash-locked tool requirements, an SBOM in SPDX and CycloneDX, build attestations, a changelog assembled from fragments | A baseline index for the pack: the commit, the tag, the file hashes, the tool versions, as one record |
| Problem reporting | The problems found in the tool and what was done about each | GitHub issues, a bug report form, a security policy. Nothing classifies a problem by effect | A record of the open and closed problems for the pack's version, with a classification |
| Quality assurance | Evidence that the process was followed, by someone who is not the person who did the work | Branch protection and the merge queue require checks to pass. There is one maintainer | Independent review. One maintainer cannot give it alone |
| A tool accomplishment summary | What was done, what was found, what remains open, at the pack's version | None | The summary, signed by whoever the owner decides |

## 5. Requirements: a numbering to agree

Today there are no requirement identifiers. This plan does not write requirements, because requirements need decisions that are not mine. It proposes a numbering and the topics, and shows the gaps. The traceability plan ([traceability.md](traceability.md)) has the same scheme.

- **Standard-derived behaviour:** `R665-<section>` and `R615A-<section>`, using the standard's own section number, for example `R665-2.2.3.1.62`. The two coverage pages already have one row per section: 65 rows for ARINC 665-3 and 53 for ARINC 615A-3.
- **The programs:** `ADL-APP-nnn` (loader), `ADL-GEN-nnn` (generator), `ADL-MOCK-nnn` (mock target), `ADL-CLI-nnn` (`adl_write_payloads`), `ADL-PLAT-nnn` (the platform layer), `ADL-SEC-nnn` (what it does about untrusted input).
- **Tests:** the Catch2 test case name or the CTest name. A tag such as `[R665-2.2.3.1.62]` on a test case would let a check read the link both ways. That changes test code, so it waits for the owner.
- Topics that tool operational requirements would cover for the loader: reading and checking a media set; what the check refuses and what it reports; the operations and their time-outs; what is sent for an upload byte for byte; where files are written and what is never overwritten; abort. For the generator: what is written, in what layout, with which check values, and the read-back. The platform layer's behaviour and the limits of the programs are already in the guide and could be turned into requirements.

Gaps today (measured by `tools/ci/check_traceability.py`): 10 coverage rows say `n/a` for tests and one says `by construction`, no row has a requirement identifier, the link from a row to a test is by test file name only, and the application's behaviour as against the standards' has no requirement list.

## 6. What could be generated from the repository, and what needs people

| Item | From the repository | Needs people |
| --- | --- | --- |
| Tests, coverage, results | A list of tests (CTest and Catch2 can list them), the coverage report, the CI run's results for the pack's commit | Deciding that the results are acceptable |
| Traceability | Requirement to test and to documentation section, once identifiers exist | Writing the requirements and the links |
| Build and configuration records | The commit, the tag, file hashes, the tool and compiler versions (`tools/release/collect_build_info.py` already writes these), the SBOM, the attestations, the hash-locked requirements | Reviewing that the baseline is the one intended |
| Change history | `git log`, the changelog fragments, the pull requests | Judging which changes affect the tool's use |
| Problem reports | An export of the issues, with labels | Classifying each by effect, and closing the record |
| Intended use, failure analysis, requirements | Nothing: a generator can only lay out what people write | Writing and reviewing them |
| Plan and accomplishment summary | The template, with the generated facts filled in | Writing, review, independence, approval, signature |
| Certification liaison, the tool's place in the project's plan | Nothing | The user's project |

Roughly: the evidence about what exists and what ran can be assembled by a program. What it means, and whether it is enough, is a person's judgement.

## 7. A generator and a folder layout: proposed, not built

A generator would take a version (a tag) and write one folder, reproducibly. Proposed: a script, tools/assurance/make_pack.py (it does not exist), run on a clean checkout of the tag, with no network except to fetch the CI results of that commit through GitHub's interface; it writes no file outside the output folder.

```text
adl-pack-<version>/
  README.txt                       what the pack is and is not, the version, the commit, how it was made
  00-baseline/                     commit, tag, file hashes, tool and compiler versions, SBOM, attestations
  01-intended-use/                 written by people: intended use, operating environment
  02-failure-analysis/             written by people: could it insert or miss an error
  03-requirements/                 written by people: tool operational requirements, with identifiers
  04-plan/                         the tool qualification plan
  05-verification/
    cases/                         the list of tests, each linked to a requirement
    results/                       the CI results for the commit; coverage; fuzz run summaries
    traceability.csv               generated: requirement, test, documentation section, result
  06-configuration-management/     change history, problem reports, release records
  07-quality-assurance/            review records, independence statements (people)
  08-accomplishment-summary/       the summary, with the generated facts
  SHA256SUMS                       every file of the pack, and a signature by the release key
```

The folders marked "people" are written by people and kept in the repository (proposed: a folder docs/assurance/, which does not exist), so they are under the same change control as the code. The generator copies them in, checks that each exists and has been reviewed at this version, and fails when one has not. It does not write them.

## 8. What the user's project adds

The tool's place in the project's plan; the software level of what the tool is used on; the criterion that applies, and so the level; the decision whether to qualify at all; the liaison with the authority; the project's own verification that does not rely on the tool; and the acceptance of the pack as evidence. A pack never replaces any of them.

## 9. Decisions for the owner

1. Whether to build the generator, and when. This plan stops here until then.
2. Who writes and who reviews the people-written items, and whether independence is offered ([editions.md](editions.md), decision 9).
3. The first requirements: which topics, and whether to start with the loader or the generator.
4. Whether to tag tests with requirement identifiers (a change to test code).
5. How evidence is retained and for how long ([editions.md](editions.md), decision 8).
6. Who checks sections 2 and 3 against the standards, and when.
7. Whether a pack is priced and delivered as a document set, as a repository, or as a service.

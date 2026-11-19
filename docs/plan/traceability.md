# Traceability: what is linked, what is not, and a numbering to agree

This is a plan for the owner to decide. It is not published (`plan/` is excluded from the site, `mkdocs.yml`). It says what links a requirement, a test and a page of documentation today, shows the gaps, and proposes a numbering. It does not write requirements: a requirement is a decision about what the tool shall do, and those decisions are not mine to make. [The data pack plan](assurance-data-pack.md) says why a project that uses the tool in certification work would want the links.

## What exists

| Link | State today |
| --- | --- |
| Standard's section to code to test | The coverage pages for [ARINC 665-3](../arinc665-coverage.md) and [ARINC 615A-3](../arinc615a-coverage.md) have one row for each section of the standard that the code handles: 65 and 53 rows. Each row names where the code does it and the test files that hold it down |
| Test to test case | 74 of the rows name individual test cases, by the name of the Catch2 test case (a long name may be cut short with three dots) |
| Programs' behaviour to requirement | None. There are no requirements for the three programs, and no requirement identifiers anywhere |
| Test to requirement | None. Nothing in a test says which requirement it holds down |
| Requirement to documentation | None |

`tools/ci/check_traceability.py` (the CTest test `docs.traceability`) holds the first two links to the tests: each test file a row names is a test source file of the repository, each test case a row names is in that file, and each row has a test or says why not. Its own test is `ci.check_traceability`. When it was first run it found four test names that the coverage page had cut short and one row that said "every golden and layout test" without naming one; the first are now understood, and the second names the tests.

## The gaps

Measured on 19 November 2026, at version 0.1.1:

| Gap | Count or state |
| --- | --- |
| Sections in the coverage tables | 118 (65 for ARINC 665-3, 53 for ARINC 615A-3) |
| Rows linked to a test file | 104 |
| Rows that say "as" another row | 3 |
| Rows with no test: `n/a` | 10 |
| Rows with no test: `by construction` | 1 |
| Rows with a requirement identifier | 0 |
| Test cases (Catch2) | about 556 |
| Test cases tagged with a requirement | 0 |
| Requirements for the three programs | 0 |

Run `python3 tools/ci/check_traceability.py --report` to list the rows with no test. Most of them are sections of the standard that the code does not implement because they are about something else (the electronic distribution of media, the list of manufacturers' codes) and are `n/a` for a reason the row gives. They are gaps in what is *tested*, not necessarily in what is *built*.

## A numbering to agree

- **From the standards:** `R665-<section>` and `R615A-<section>`, the identifier being the standard's own section number, for example `R665-2.2.3.1.62`. The coverage tables would gain an identifier column. The section number is stable as long as the edition is.
- **For the programs:** `ADL-APP-nnn` (loader), `ADL-GEN-nnn` (generator), `ADL-MOCK-nnn` (mock target), `ADL-CLI-nnn` (`adl_write_payloads`), `ADL-PLAT-nnn` (platform layer), `ADL-SEC-nnn` (untrusted input). A requirement would be one sentence, testable, in a requirements document under `docs/`, and written in "shall" language there and nowhere else.
- **Tests:** the Catch2 test case name, or the CTest name. A tag such as `[R665-2.2.3.1.62]` on a test case would let a check read the link in both directions: from a requirement to the tests that carry its tag, and from a test to its requirement. That changes test code, so it waits for the owner.
- **Documentation:** the section anchor of the page.

## What would be built, if the owner agrees

1. A requirements document for the loader, then for the generator, with identifiers. The topics are listed in the data pack plan.
2. An identifier column in the two coverage tables, and the check extended to hold it.
3. Tags on the test cases that hold a requirement down, and a check that every requirement has a test, and that every tag names a requirement.
4. A generated traceability matrix (requirement, test, page, result), which the data pack would carry.

None of it is started. Steps 1 and 3 change what the project promises and what its tests say, which is why they wait.

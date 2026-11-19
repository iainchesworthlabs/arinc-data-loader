# Editions: a proposal for the owner to decide

This is a plan. It is not published (`plan/` is excluded from the site, `mkdocs.yml`). It writes down the commercial structure the owner described, says what each edition would include, and lists what is still open. Nothing here is a fact about a product that exists, and nothing in it is a price.

The published pages [Licensing and editions](../editions.md) and [Assurance](../assurance.md) are drafted from this page. Each open decision below is an `OWNER DECISION` comment in those pages. The plan for the Enterprise data packs is a separate page, added in a later pull request.

## The owner's intent, as given

| Edition | What it is |
| --- | --- |
| Community | Free to use under the licence in the repository. No assurances, no support, "as is". |
| Pro and Site | Support: a response channel and terms, supported versions, an update policy. |
| Enterprise | Everything above, plus data packs for certification use, and for use as a test tool in aerospace certification work. |

## What each edition would include

| | Community | Pro and Site | Enterprise |
| --- | --- | --- | --- |
| The programs and libraries, under GPL-3.0-or-later | yes | yes | yes |
| Warranty | none (the About dialog says so) | OWNER DECISION | OWNER DECISION |
| Support | none promised; the maintainer is one person | OWNER DECISION: channel, hours, response times | as Pro and Site |
| Supported versions | the latest release only, until 1.0 (`SECURITY.md`) | OWNER DECISION | as Pro and Site, plus the versions a data pack covers |
| Update policy | none | OWNER DECISION | as Pro and Site |
| Data packs for certification use | no | no | yes (the data pack plan) |
| Price | none | OWNER DECISION | OWNER DECISION |

"Pro" and "Site" are listed together because that is how the intent was given. Whether they are one edition or two, and what a "site" is (one building, one organisation, one programme), is decision 2.

## Facts the decisions depend on

- The code is GPL-3.0-or-later (`LICENSE`). Anyone may use, change and share it under that licence. A paid edition cannot take that away from the Community edition, and cannot stop anyone redistributing the GPL code.
- `REUSE.toml` names the copyright holder as "Iain Chesworth Labs contributors". `git shortlog` on `develop` shows 255 commits from two author identities and 6 from Dependabot. `CONTRIBUTING.md` takes contributions under GPL-3.0-or-later with a sign-off, and there is no contributor licence agreement. If both identities are the owner, the owner can offer the code under another licence as well. A contribution from anyone else could not be offered that way without their agreement.
- The programs link Qt 6.12 dynamically under the LGPL, version 3, and one module that the deployment places is offered under the GPL, version 3. They bundle the Archivo and Noto Sans Hebrew typefaces under the SIL Open Font License 1.1. A build offered on other terms still has to meet those licences ([licensing.md](../licensing.md)).
- The standards the code implements are the publisher's documents. They are not in the repository and a data pack cannot contain their text ([licensing.md](../licensing.md#the-standards)).
- Today nothing has been checked against real equipment ([status.md](../status.md)). Today there are no requirements documents, no requirement identifiers, and no record of reviews beyond what GitHub keeps for pull requests. The data pack plan says what is missing.
- The company site's rules for a product page: honest status, no testimonials, no price unless true, contact by a role address and not a personal one.

## Open decisions

1. **Names.** Community, Pro, Site, Enterprise are working names.
2. **Pro and Site.** One edition or two. If two, what differs (the number of people, the number of machines, who may use the support).
3. **Prices and billing.** None are set, and none are invented. Where a price is shown, how it is paid, and whether a price is shown at all.
4. **Support.** The channel (private GitHub issues, a role email address, another), hours and time zone, response targets, languages, what is in scope (defects, use, integration) and what is not, how a severity is decided. There is no support email today, and the company site says its contact address is not set up yet.
5. **Supported versions and updates.** How many releases back are supported, how long, how a security fix is delivered, how an end of support is announced, and how a Qt update is handled.
6. **Licence texts.**
    - Whether every edition has only the GPL, so that the paid editions are support and data packs, or whether the code is also offered under another licence (see the facts above; take legal advice).
    - The licence of a data pack. It is a separate work and can have its own terms.
    - The warranty and liability wording of the paid editions, and any consumer-law points. Take legal advice.
7. **Who may redistribute.** The GPL lets anyone redistribute the programs. Whether a customer may pass a data pack to their own subcontractors or authority, and whether its contents are confidential.
8. **Evidence retention.** How long the project keeps the records a pack is made from (build logs, test results, review records, problem reports), where, and for how long a pack version can be made again. Today that is whatever GitHub's settings give, and it is not a managed record.
9. **Independence and approvals.** Who reviews and approves the content of a pack. A programme that applies DO-178C or DO-330 may need those who verify to be independent of those who develop. One maintainer cannot give that alone.
10. **Contact.** A role address, not a personal one.
11. **The product name.** "ARINC" is the name of the standards' publisher. Whether the name stays, and what the pages say about the relationship.
12. **Deploy.** Which branch publishes the site (`main` is recommended, so a page describes a released version), and whether the Licensing and Assurance pages are published before the decisions above are made.

## What the published pages may and may not say

- They say what the Community edition is and what it does not give, and what a data pack is for, in plain words.
- They never say that the tool is "certified", "qualified", "compliant with DO-178C" or "approved" by any authority, unless the owner supplies the evidence. "DO-178C" and "DO-330" appear only to name the standards a user's own programme applies.
- They give no price, no date, no customer, no number of users and no quote.
- A check for those words, added in a later pull request, fails a page that uses one of the words "certified", "qualified", "compliant", "approved" or "guaranteed" without an entry in the owner-approved exception list.

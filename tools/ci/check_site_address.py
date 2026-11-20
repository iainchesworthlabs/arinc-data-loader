#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Fail when the documentation site's address is written down in more than one way.

    python3 tools/ci/check_site_address.py [--root .]

`site_url` in mkdocs.yml is the one place that holds the address, and it is fixed: https://iainchesworthlabs.com/adl/ (the
company site serves this program under /adl/ and no other path). The pages of docs/ write `{{ site_url }}` where they need it and a
hook (tools/docs/site_address_hook.py) fills it in at build time. This check fails when

- `site_url` is not that address;
- a page of docs/ other than docs/ci.md (which describes the address) writes the host out, or writes the old address, the GitHub Pages
  site of release 0.1.1;
- README.md, which cannot read mkdocs.yml, names the old address, or links to the documentation at another address than `site_url`.

A move to another address is then: change `site_url` and FIXED_ADDRESS in tools/ci/site_address.py, fix what this check names, and
change the deploy job's prefix (owner: the company site's apps.ts; docs/ci.md, "The site address").
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import site_address  # noqa: E402

OLD_HOST = "iainchesworthlabs.github.io"
# The page that describes the decision itself and the history of the old address may name hosts.
PAGES_THAT_MAY_NAME_THE_HOST = ("docs/ci.md",)


def check(root: Path) -> list[str]:
    problems: list[str] = []
    site = site_address.site_url(root)
    host = site_address.host(root)
    if site != site_address.FIXED_ADDRESS:
        problems.append(f"mkdocs.yml: site_url is {site!r}; the address of this site is {site_address.FIXED_ADDRESS!r}")
    for page in sorted((root / "docs").rglob("*.md")):
        rel = page.relative_to(root).as_posix()
        if rel in PAGES_THAT_MAY_NAME_THE_HOST:
            continue
        text = page.read_text(encoding="utf-8")
        if host in text:
            problems.append(f"{rel}: writes the site's host out; write {{{{ site_url }}}} so one edit moves the site")
        if OLD_HOST in text:
            problems.append(f"{rel}: names the old address ({OLD_HOST}); the site is at {site}")
    readme = root / "README.md"
    if readme.is_file():
        text = readme.read_text(encoding="utf-8")
        if OLD_HOST in text:
            problems.append(f"README.md: names the old address ({OLD_HOST}); the site is at {site}")
        for url in re.findall(rf"https://{re.escape(host)}[^\s\"')>]*", text):
            if not url.startswith(site):
                problems.append(f"README.md: {url} is not under {site}")
    return problems


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--root", type=Path, default=site_address.ROOT)
    args = parser.parse_args(argv)
    try:
        problems = check(args.root)
    except (OSError, ValueError) as error:
        print(f"check_site_address: cannot check: {error}", file=sys.stderr)
        return 2
    for problem in problems:
        print(f"::error::{problem}")
    if problems:
        return 1
    print("Site address: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())

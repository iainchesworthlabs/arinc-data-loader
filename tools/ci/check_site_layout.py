#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check the built site against tools/docs/site_layout.json: no page of the first site is lost, and each old address leads somewhere.

    python3 tools/ci/check_site_layout.py [--site site] [--root .]

Without --site it builds the site with `mkdocs build --strict` into a temporary folder (it needs the packages of
.github/requirements/docs.txt) and checks that. With --site it checks a folder that is already built.

site_layout.json lists every page of the first documentation site (release 0.1.1, on GitHub Pages), the file that serves it now and its
address on https://iainchesworthlabs.com/adl/ (`null`: the page is published on GitHub only). A page the first site did not have, but
that is not at the address its file name gives, has `"old": null`. This check fails when

- a page that has an address is not in the built site, or its canonical address is not the one in the table;
- a page that has no address (GitHub only) is in the built site, or its file is not in docs/;
- a built page sits at an old address that the table gives to another page (so two pages would answer to one address);
- the stubs for the old addresses (tools/docs/make_old_address_stubs.py) are missing a page, or one of them points to a page that
  is not in the site or to a file that is not in the repository.

The same table is applied while the site is built (tools/docs/site_layout_hook.py).
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(ROOT / "tools" / "docs"))
import make_old_address_stubs  # noqa: E402
import site_address  # noqa: E402
import site_tools  # noqa: E402


def check(site: Path, root: Path) -> list[str]:
    problems: list[str] = []
    site_url = site_address.site_url(root)
    repo_url = site_address.repo_url(root)
    entries = json.loads((root / "tools" / "docs" / "site_layout.json").read_text(encoding="utf-8"))["pages"]
    addresses = {e["new"] for e in entries if e["new"] is not None}
    for entry in entries:
        old, src, new = entry["old"], entry["src"], entry["new"]
        if not (root / "docs" / src).is_file():
            problems.append(f"site_layout.json: docs/{src} is not in docs/")
        if old is None and new is None:
            problems.append(f"site_layout.json: docs/{src} has no old address and no new one")
            continue
        if new is None:
            if old is not None and (site / old / "index.html").is_file() and old not in addresses:
                problems.append(f"{old}: is published, but site_layout.json says it is on GitHub only")
            continue
        built = site / new / "index.html"
        if not built.is_file():
            problems.append(f"{new}: is not in the built site (site_layout.json gives it to docs/{src})")
            continue
        canonical = site_tools.parse(built).canonical
        if canonical != site_url + new:
            problems.append(f"{new}: its canonical address is {canonical!r}, not {site_url + new!r}")
    for old in (e["old"] for e in entries if e["old"] is not None):
        if old and old not in addresses and (site / old / "index.html").is_file():
            problems.append(f"{old}: a page of the built site is at an old address that site_layout.json gives to another page")
    # The stubs.
    with tempfile.TemporaryDirectory(prefix="adl-stubs-") as tmp:
        out = Path(tmp)
        make_old_address_stubs.write(out, root)
        for old, target in make_old_address_stubs.targets(root).items():
            stub = out / old / "index.html"
            if not stub.is_file():
                problems.append(f"{old or '(home)'}: no stub page was written")
                continue
            if target.startswith(site_url):
                rel = target[len(site_url):]
                if not (site / rel / "index.html").is_file():
                    problems.append(f"{old or '(home)'}: its stub points to {target}, which is not in the built site")
            elif target.startswith(repo_url + "/blob/main/"):
                rel = target[len(repo_url + "/blob/main/"):]
                if not (root / rel).is_file():
                    problems.append(f"{old or '(home)'}: its stub points to {target}, and {rel} is not in the repository")
            else:
                problems.append(f"{old or '(home)'}: its stub points to {target}, which is neither the site nor the repository")
            if not re.search(r"http-equiv=\"refresh\"", stub.read_text(encoding="utf-8")):
                problems.append(f"{old or '(home)'}: its stub has no refresh")
    return problems


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--site", type=Path, help="a site that is already built")
    args = parser.parse_args(argv)
    tmp = None
    site = args.site
    if site is None:
        if not site_tools.material_installed():
            print("check_site_layout: mkdocs-material is not installed (pip install --require-hashes -r .github/requirements/docs.txt): cannot build the site", file=sys.stderr)
            return 2
        tmp = tempfile.mkdtemp(prefix="adl-site-")
        site = Path(tmp)
        if site_tools.build(args.root, site) != 0:
            shutil.rmtree(tmp, ignore_errors=True)
            return 1
    try:
        problems = check(site, args.root)
    finally:
        if tmp:
            shutil.rmtree(tmp, ignore_errors=True)
    for problem in problems:
        print(f"::error::{problem}")
    if problems:
        return 1
    print("Site layout: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())

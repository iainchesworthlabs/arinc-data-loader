#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Write a small page at every address of the first documentation site, pointing to where the page is now.

    python3 tools/docs/make_old_address_stubs.py --out <folder>

Release 0.1.1's documentation was published on GitHub Pages (https://iainchesworthlabs.github.io/arinc-data-loader/). The site
is now https://iainchesworthlabs.com/adl/ (mkdocs.yml, `site_url`). tools/docs/site_layout.json lists the old addresses and says, for
each, where its page went: an address on the new site, or the file on GitHub when the page is published there only. For each old
address this writes <folder>/<address>index.html: a page with a link to the new address and a meta refresh to it, no script. The
folder is what the Pages job publishes (.github/workflows/docs.yml), so an old link still arrives. A file that is not a page (the
pictures, the public key) is not forwarded.

The folder also gets a 404.html that says the site moved, for an address that was never a page.
"""

from __future__ import annotations

import argparse
import html
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "ci"))
import site_address  # noqa: E402

LAYOUT = Path("tools/docs/site_layout.json")  # relative to the repository root

PAGE = """<!doctype html>
<html lang="en-AU">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{title}</title>
<link rel="canonical" href="{target}">
<meta http-equiv="refresh" content="0; url={target}">
</head>
<body>
<h1>{heading}</h1>
<p>The documentation of ARINC Data Loader has moved to <a href="{site}">{site}</a>.</p>
<p>{where} <a href="{target}">{target}</a></p>
</body>
</html>
"""


def targets(root: Path = ROOT) -> dict[str, str]:
    """Old address (with a trailing slash, empty for the home page) -> the address to send the reader to."""
    site = site_address.site_url(root)
    repo = site_address.repo_url(root)
    out: dict[str, str] = {}
    for entry in json.loads((root / LAYOUT).read_text(encoding="utf-8"))["pages"]:
        if entry["new"] is None:
            out[entry["old"]] = f"{repo}/blob/main/docs/{entry['src']}"
        else:
            out[entry["old"]] = site + entry["new"]
    return out


def render(target: str, site: str, *, moved: bool) -> str:
    return PAGE.format(
        title=html.escape("This page has moved: ARINC Data Loader" if moved else "This site has moved: ARINC Data Loader"),
        heading=html.escape("This page has moved" if moved else "This site has moved"),
        site=html.escape(site, quote=True),
        target=html.escape(target, quote=True),
        where=html.escape("This page is now at" if moved else "Start from the home page:"),
    )


def write(out: Path, root: Path = ROOT) -> int:
    site = site_address.site_url(root)
    pages = targets(root)
    for old, target in pages.items():
        folder = out / old
        folder.mkdir(parents=True, exist_ok=True)
        (folder / "index.html").write_text(render(target, site, moved=True), encoding="utf-8", newline="\n")
    (out / "404.html").write_text(render(site, site, moved=False), encoding="utf-8", newline="\n")
    return len(pages)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--root", type=Path, default=ROOT)
    args = parser.parse_args(argv)
    count = write(args.out, args.root)
    print(f"Wrote {count} pages that point to the new address, in {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

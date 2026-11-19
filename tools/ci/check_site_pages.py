#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check the built documentation site: the front pages are plain HTML, load nothing from other sites, and link to files that exist.

    python3 tools/ci/check_site_pages.py [--site site] [--root .]

Without --site it builds the site with `mkdocs build --strict` into a temporary folder (it needs the packages of
.github/requirements/docs.txt) and checks that. With --site it checks a folder that is already built.

Every page:
- has a <title>, exactly one <h1> (the first heading of the main content), headings that do not skip a level, and a canonical address;
- has an alternative text on every picture;
- links to a file that exists, with an anchor that exists in it. Links that start with "/" are the company shell's
  (/, /support/, /assets/v1/...) and are not followed, but none of them may start with /adl/ (the pages must work under a preview
  address, so the pages of this site use relative links). An absolute link is allowed to GitHub and to gnu.org only on a front page.

The front pages (tools/docs/theme/marketing.html), which run under a preview that blocks every script, also have

- lang="en-AU", a description, and a title and description of their own (no other page has the same);
- no <script> except a JSON-LD data block, no <style> and no style= attribute;
- a width and a height on every picture;
- nothing loaded from another host: no src, srcset, stylesheet link or CSS url() or @import that starts with http(s):// or //;
- no mailto: link (there is no email address for support).

And the pages that the company site's contract requires, or that other pages and the old addresses lead to, exist: see FIXED.
"""

from __future__ import annotations

import argparse
import re
import shutil
import sys
import tempfile
from pathlib import Path
from urllib.parse import unquote, urlparse

sys.path.insert(0, str(Path(__file__).resolve().parent))
import site_tools  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
FIXED = ["index.html", "privacy/index.html", "support/index.html", "guide/index.html", "developers/index.html",
         "how-to-use/index.html", "features/index.html", "faq/index.html", "whats-new/index.html", "licensing/index.html",
         "assurance/index.html", "accessibility/index.html", "press/index.html", "theme.css", "icon.svg"]
ALLOWED_HOSTS = {"github.com", "www.gnu.org"}


def check_headings(where: str, levels: list[int]) -> list[str]:
    problems: list[str] = []
    if levels and levels[0] != 1:
        problems.append(f"{where}: the first heading is an h{levels[0]}, not the h1")
    for before, after in zip(levels, levels[1:]):
        if after > before + 1:
            problems.append(f"{where}: a heading goes from h{before} to h{after}; headings do not skip a level")
            break
    return problems


def check_site(site: Path, root: Path) -> list[str]:
    problems: list[str] = []
    for name in FIXED:
        if not (site / name).is_file():
            problems.append(f"{name}: is not in the built site (the company's contract requires it, or other pages lead to it)")
    pages = {p: site_tools.parse(p) for p in sorted(site.rglob("*.html"))}
    titles: dict[str, str] = {}
    descriptions: dict[str, str] = {}
    for path, page in pages.items():
        rel = path.relative_to(site).as_posix()
        if rel == "404.html" or rel == "search/index.html":
            continue
        front = not page.material
        where = rel
        if not page.title.strip():
            problems.append(f"{where}: no <title>")
        if page.h1 != 1:
            problems.append(f"{where}: has {page.h1} <h1>, not one")
        if not page.canonical:
            problems.append(f"{where}: no canonical address")
        problems += check_headings(where, page.headings)
        for image in page.images:
            if "alt" not in image:
                problems.append(f"{where}: a picture ({image.get('src', '?')}) has no alt attribute")
        if front:
            if page.lang != "en-AU":
                problems.append(f'{where}: <html> needs lang="en-AU"')
            if not page.description.strip():
                problems.append(f"{where}: no description")
            elif page.description in descriptions:
                problems.append(f"{where}: has the same description as {descriptions[page.description]}")
            else:
                descriptions[page.description] = where
            if page.title in titles:
                problems.append(f"{where}: has the same title as {titles[page.title]}")
            else:
                titles[page.title] = where
            for kind in page.scripts:
                if kind != "application/ld+json":
                    problems.append(f"{where}: has a <script>; the front pages are script-free (a preview blocks scripts)")
            if page.styles or page.style_attrs:
                problems.append(f"{where}: has <style> or a style= attribute; put it in docs/assets/adl.css")
            for image in page.images:
                if not image.get("width") or not image.get("height"):
                    problems.append(f"{where}: a picture ({image.get('src', '?')}) has no width and height")
        for tag, attribute, value in page.refs:
            value = value.strip()
            if not value or value.startswith(("data:", "javascript:")):
                continue
            if value.startswith("mailto:"):
                problems.append(f"{where}: {value}: there is no email address for support; use the GitHub issues")
                continue
            parts = urlparse(value)
            if parts.scheme in ("http", "https") or value.startswith("//"):
                if not front:
                    continue  # the guide's own links (Material's credit in the footer, the repository)
                if parts.scheme not in ("http", "https") or parts.netloc not in ALLOWED_HOSTS:
                    problems.append(f"{where}: {value}: not a link to a host this site may name ({', '.join(sorted(ALLOWED_HOSTS))})")
                elif attribute == "srcset" or (tag != "a" and tag != "link"):
                    problems.append(f"{where}: {value}: loads something from another host")
                elif tag == "link" and "icon" not in value:
                    problems.append(f"{where}: {value}: a <link> to another host")
                continue
            if value.startswith("/"):
                if value.startswith("/adl/"):
                    problems.append(f"{where}: {value}: write a relative link, so the page works under a preview address")
                continue
            if parts.scheme:
                problems.append(f"{where}: {value}: unknown link type")
                continue
            target = site_tools.target_file(site, path, value)
            if target is None or not target.exists():
                problems.append(f"{where}: {value}: no such file in the built site")
                continue
            if parts.fragment and target.suffix == ".html":
                other = pages.get(target) or site_tools.parse(target)
                if unquote(parts.fragment) not in other.ids:
                    problems.append(f"{where}: {value}: no anchor #{parts.fragment} in {target.relative_to(site).as_posix()}")
    for name in ("theme.css", "assets/adl.css", "assets/guide.css", "assets/icl.css"):
        css = site / name
        if css.is_file():
            text = re.sub(r"/\*.*?\*/", "", css.read_text(encoding="utf-8"), flags=re.S)
            if re.search(r"@import|url\(\s*['\"]?(https?:)?//", text):
                problems.append(f"{name}: loads something with @import or an address; the site loads nothing from other sites")
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
            print("check_site_pages: mkdocs-material is not installed (pip install --require-hashes -r .github/requirements/docs.txt): cannot build the site", file=sys.stderr)
            return 2
        tmp = tempfile.mkdtemp(prefix="adl-site-")
        site = Path(tmp)
        if site_tools.build(args.root, site) != 0:
            shutil.rmtree(tmp, ignore_errors=True)
            return 1
    try:
        problems = check_site(site, args.root)
    finally:
        if tmp:
            shutil.rmtree(tmp, ignore_errors=True)
    for problem in problems:
        print(f"::error::{problem}")
    if problems:
        return 1
    print("Site pages: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())

# SPDX-License-Identifier: GPL-3.0-or-later
"""What the site checks share: building the site, and reading a built page."""

from __future__ import annotations

import os
import subprocess
import sys
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urlparse


def material_installed() -> bool:
    return subprocess.run([sys.executable, "-c", "import mkdocs, material"], capture_output=True, check=False).returncode == 0


def build(root: Path, out: Path) -> int:
    """`mkdocs build --strict` into `out`. Prints MkDocs' own output when it fails."""
    env = dict(os.environ, NO_MKDOCS_2_WARNING="true", PYTHONDONTWRITEBYTECODE="1")
    result = subprocess.run([sys.executable, "-m", "mkdocs", "build", "--strict", "--quiet", "--site-dir", str(out)],
                            cwd=root, capture_output=True, text=True, check=False, env=env)
    if result.returncode != 0:
        sys.stderr.write(result.stdout + result.stderr)
    return result.returncode


class Page(HTMLParser):
    """The parts of a built page the checks look at."""

    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self.title = ""
        self._in_title = False
        self.h1 = 0
        self.lang = ""
        self.canonical = ""
        self.ids: set[str] = set()
        self.refs: list[tuple[str, str, str]] = []  # (tag, attribute, value)
        self.scripts: list[str] = []  # the type of each <script>
        self.styles = 0
        self.style_attrs = 0
        self.material = False
        self.description = ""
        self.headings: list[int] = []  # the levels of the headings of the main content, in order
        self.images: list[dict[str, str]] = []  # the attributes of each <img>
        self._main = 0  # > 0 inside <main> (the front pages) or the article of a Material page
        self._stack: list[bool] = []

    def handle_starttag(self, tag, attrs):  # noqa: ANN001, ANN201
        a = {k: (v or "") for k, v in attrs}
        if tag in ("main", "article") and (tag == "main" or "md-typeset" in a.get("class", "").split()):
            self._main += 1
            self._stack.append(tag)
        elif tag in ("main", "article"):
            self._stack.append("")
        if self._main and len(tag) == 2 and tag[0] == "h" and tag[1] in "123456":
            self.headings.append(int(tag[1]))
        if tag == "img":
            self.images.append(a)
        if tag == "meta" and a.get("name") == "description":
            self.description = a.get("content", "")
        if tag == "html":
            self.lang = a.get("lang", "")
        if tag == "title":
            self._in_title = True
        if tag == "h1":
            self.h1 += 1
        if tag == "script":
            self.scripts.append(a.get("type", ""))
        if tag == "style":
            self.styles += 1
        if "style" in a:
            self.style_attrs += 1
        if "id" in a:
            self.ids.add(a["id"])
        if tag == "a" and a.get("name"):
            self.ids.add(a["name"])
        if "class" in a and "md-container" in a["class"].split():
            self.material = True
        if tag == "link" and a.get("rel") == "canonical":
            self.canonical = a.get("href", "")
            return
        for attribute in ("href", "src"):
            if attribute in a:
                self.refs.append((tag, attribute, a[attribute]))
        if "srcset" in a:
            for part in a["srcset"].split(","):
                if part.strip():
                    self.refs.append((tag, "srcset", part.split()[0]))

    def handle_endtag(self, tag):  # noqa: ANN001, ANN201
        if tag == "title":
            self._in_title = False
        if tag in ("main", "article") and self._stack:
            if self._stack.pop():
                self._main -= 1

    def handle_data(self, data):  # noqa: ANN001, ANN201
        if self._in_title:
            self.title += data


def parse(path: Path) -> Page:
    page = Page()
    page.feed(path.read_text(encoding="utf-8"))
    return page


def target_file(site: Path, page_file: Path, link: str) -> Path | None:
    """The file a relative link names, or None when it leaves the site."""
    path = unquote(urlparse(link).path)
    page_file = page_file.resolve()
    target = (page_file.parent / path).resolve() if path else page_file
    try:
        target.relative_to(site.resolve())
    except ValueError:
        return None
    if target.is_dir():
        target = target / "index.html"
    return target

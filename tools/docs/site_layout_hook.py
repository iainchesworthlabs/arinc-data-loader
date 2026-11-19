# SPDX-License-Identifier: GPL-3.0-or-later
"""MkDocs hook: where each page of docs/ is on the site.

The files of docs/ stay where they are, because the code, the tests, CONTRIBUTING.md and the other pages cite them by path. The site
has its own layout: the user's guide is /adl/guide/ (the folder user-guide/ in the repository) and the pages for programmers are
under /adl/developers/. A page of docs/user-guide/ that the table does not name (a new page of the guide) goes under guide/ with its own name.
tools/docs/site_layout.json says, for each page of the first site (release 0.1.1), the file that now
serves it and the address it has now (and, with "old": null, the address of a new page that is not where its file name puts it). This hook applies that table to the files MkDocs found, and refuses to build when the
table and the rest of the configuration disagree, so a page cannot be lost quietly:

- a file in the table that is not in docs/;
- a page in the table that has an address ("new") but is excluded from the site (`exclude_docs` in mkdocs.yml), or one that has none
  (`null`: published on GitHub only) and is not excluded.

tools/ci/check_site_layout.py checks the built site against the same table. docs/ci.md, "The site layout".
"""

from __future__ import annotations

import json
from pathlib import Path

LAYOUT = Path(__file__).resolve().parent / "site_layout.json"


def load() -> list[dict]:
    return json.loads(LAYOUT.read_text(encoding="utf-8"))["pages"]


GUIDE_FOLDER = "user-guide/"


def guide_destination(src_uri: str) -> str | None:
    """The address of a page in docs/user-guide/ that the table does not name: the same name under guide/."""
    if not (src_uri.startswith(GUIDE_FOLDER) and src_uri.endswith(".md")):
        return None
    stem = src_uri[len(GUIDE_FOLDER):-len(".md")]
    return "guide/" + ("" if stem == "index" else stem + "/") + "index.html"


def on_files(files, config):  # noqa: ANN001, ANN201 (MkDocs passes its own types)
    problems: list[str] = []
    named = {entry["src"] for entry in load()}
    for file in files:
        if file.src_uri not in named and (destination := guide_destination(file.src_uri)):
            file.dest_uri = destination
            for cached in ("url", "abs_dest_path"):
                file.__dict__.pop(cached, None)
    for entry in load():
        src, new = entry["src"], entry["new"]
        file = files.get_file_from_path(src)
        if file is None:
            problems.append(f"site_layout.json: {src} is not in docs/")
            continue
        if new is None:
            if not file.inclusion.is_excluded():
                problems.append(f"site_layout.json: {src} has no address on the site (null) but mkdocs.yml does not exclude it")
            continue
        if file.inclusion.is_excluded():
            problems.append(f"site_layout.json: {src} has the address {new!r} but mkdocs.yml excludes it (exclude_docs)")
            continue
        file.dest_uri = new + "index.html"
        # MkDocs worked these out from the old dest_uri and keeps them until they are asked for again.
        for cached in ("url", "abs_dest_path"):
            file.__dict__.pop(cached, None)
    if problems:
        raise ValueError("\n".join(problems))
    return files

#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Self-test of the documentation site's checks (CTest: ci.site_checks).

Covers tools/ci/check_site_kit.py, check_site_address.py, check_site_layout.py and check_site_pages.py, tools/docs/make_old_address_stubs.py and
tools/docs/site_layout_hook.py: each is shown to pass on a good tree and to fail, with a reason, on a bad one. It needs neither MkDocs
nor a built site (the layout check is given a small site made here), and the palette tests are skipped without Node.
"""

import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(REPO / "tools" / "docs"))

import check_site_address  # noqa: E402
import check_site_kit  # noqa: E402
import check_site_layout  # noqa: E402
import check_site_pages  # noqa: E402
import make_old_address_stubs  # noqa: E402
import site_layout_hook  # noqa: E402
import site_version_hook  # noqa: E402

KIT = check_site_kit.KIT


def copy_kit(root: Path) -> None:
    shutil.copytree(REPO / KIT, root / KIT)
    (root / "docs").mkdir(exist_ok=True)
    shutil.copyfile(REPO / "docs" / "theme.css", root / "docs" / "theme.css")


class Kit(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        copy_kit(self.root)

    def tearDown(self) -> None:
        self.tmp.cleanup()

    def test_the_committed_copy_is_good(self) -> None:
        self.assertEqual(check_site_kit.check_sums(REPO), [])

    def test_an_edited_file_is_found(self) -> None:
        css = self.root / KIT / "icl.css"
        css.write_text(css.read_text(encoding="utf-8") + "\nbody { color: red; }\n", encoding="utf-8")
        problems = check_site_kit.check_sums(self.root)
        self.assertEqual(len(problems), 1)
        self.assertIn("icl.css", problems[0])
        self.assertIn("edited", problems[0])

    def test_line_endings_are_not_compared(self) -> None:
        css = self.root / KIT / "icl.css"
        css.write_bytes(css.read_bytes().replace(b"\r\n", b"\n").replace(b"\n", b"\r\n"))
        self.assertEqual(check_site_kit.check_sums(self.root), [])

    def test_a_missing_and_a_surplus_file_are_found(self) -> None:
        (self.root / KIT / "kit" / "theme.css").unlink()
        (self.root / KIT / "kit" / "extra.css").write_text("a {}\n", encoding="utf-8")
        text = "\n".join(check_site_kit.check_sums(self.root))
        self.assertIn("kit/theme.css is listed in SHA256SUMS.txt and is missing", text)
        self.assertIn("kit/extra.css is not in the kit's SHA256SUMS.txt", text)

    @unittest.skipUnless(shutil.which("node"), "the company's palette checker needs Node")
    def test_the_palette_passes_and_a_bad_one_fails(self) -> None:
        self.assertEqual(check_site_kit.check_palette(self.root), [])
        theme = self.root / "docs" / "theme.css"
        text = theme.read_text(encoding="utf-8")
        theme.write_text(text.replace("--app-link:          light-dark(#1f4a72,", "--app-link:          light-dark(#9db4cc,"), encoding="utf-8")
        problems = check_site_kit.check_palette(self.root)
        self.assertEqual(len(problems), 1)
        self.assertIn("R3", problems[0])

    @unittest.skipUnless(shutil.which("node"), "the company's palette checker needs Node")
    def test_another_slug_or_property_fails(self) -> None:
        theme = self.root / "docs" / "theme.css"
        good = theme.read_text(encoding="utf-8")
        theme.write_text(good.replace('[data-app="adl"]', '[data-app="field-test"]'), encoding="utf-8")
        self.assertIn("does not match folder", "".join(check_site_kit.check_palette(self.root)))
        theme.write_text(good.replace("}", "  --app-extra: light-dark(#000000, #ffffff);\n}"), encoding="utf-8")
        self.assertIn("not allowed", "".join(check_site_kit.check_palette(self.root)))


MKDOCS = "site_name: x\nsite_url: https://iainchesworthlabs.com/adl/\nrepo_url: https://github.com/example/repo\n"


class Address(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        (self.root / "docs").mkdir()
        (self.root / "mkdocs.yml").write_text(MKDOCS, encoding="utf-8")
        (self.root / "docs" / "a.md").write_text("A page. Its address is {{ site_url }}.\n", encoding="utf-8")
        (self.root / "README.md").write_text("The documentation is at https://iainchesworthlabs.com/adl/guide/.\n", encoding="utf-8")

    def tearDown(self) -> None:
        self.tmp.cleanup()

    def test_the_repository_is_good(self) -> None:
        self.assertEqual(check_site_address.check(REPO), [])

    def test_a_good_tree_passes(self) -> None:
        self.assertEqual(check_site_address.check(self.root), [])

    def test_another_address_is_refused(self) -> None:
        (self.root / "mkdocs.yml").write_text(MKDOCS.replace("/adl/", "/arinc-data-loader/"), encoding="utf-8")
        self.assertIn("the address of this site is", "".join(check_site_address.check(self.root)))

    def test_a_page_that_writes_the_host_or_the_old_address_is_refused(self) -> None:
        (self.root / "docs" / "a.md").write_text("See https://iainchesworthlabs.com/adl/guide/ and https://iainchesworthlabs.github.io/x/.\n", encoding="utf-8")
        text = "\n".join(check_site_address.check(self.root))
        self.assertIn("writes the site's host out", text)
        self.assertIn("names the old address", text)

    def test_ci_md_may_name_the_hosts(self) -> None:
        (self.root / "docs" / "ci.md").write_text("The site is at https://iainchesworthlabs.com/adl/; it was at iainchesworthlabs.github.io.\n", encoding="utf-8")
        self.assertEqual(check_site_address.check(self.root), [])

    def test_the_readme_may_not_name_the_old_address_or_another_path(self) -> None:
        (self.root / "README.md").write_text("Old: https://iainchesworthlabs.github.io/arinc-data-loader/. Other: https://iainchesworthlabs.com/field-test/.\n", encoding="utf-8")
        text = "\n".join(check_site_address.check(self.root))
        self.assertIn("README.md: names the old address", text)
        self.assertIn("is not under", text)


LAYOUT = {
    "pages": [
        {"old": "", "src": "index.md", "new": ""},
        {"old": "user-guide/upload/", "src": "user-guide/upload.md", "new": "guide/upload/"},
        {"old": "ci/", "src": "ci.md", "new": None},
    ]
}


def page(canonical: str) -> str:
    return f'<!doctype html><html lang="en-AU"><head><title>t</title><link rel="canonical" href="{canonical}"></head><body><h1>t</h1></body></html>'


class Layout(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name) / "repo"
        self.site = Path(self.tmp.name) / "site"
        (self.root / "docs" / "user-guide").mkdir(parents=True)
        (self.root / "tools" / "docs").mkdir(parents=True)
        (self.root / "mkdocs.yml").write_text(MKDOCS, encoding="utf-8")
        for name in ("index.md", "user-guide/upload.md", "ci.md"):
            (self.root / "docs" / name).write_text("# t\n", encoding="utf-8")
        (self.root / "tools" / "docs" / "site_layout.json").write_text(json.dumps(LAYOUT), encoding="utf-8")
        (self.site / "guide" / "upload").mkdir(parents=True)
        (self.site / "index.html").write_text(page("https://iainchesworthlabs.com/adl/"), encoding="utf-8")
        (self.site / "guide" / "upload" / "index.html").write_text(page("https://iainchesworthlabs.com/adl/guide/upload/"), encoding="utf-8")

    def tearDown(self) -> None:
        self.tmp.cleanup()

    def test_a_good_site_passes(self) -> None:
        self.assertEqual(check_site_layout.check(self.site, self.root), [])

    def test_a_lost_page_is_found(self) -> None:
        (self.site / "guide" / "upload" / "index.html").unlink()
        text = "\n".join(check_site_layout.check(self.site, self.root))
        self.assertIn("guide/upload/: is not in the built site", text)
        self.assertIn("which is not in the built site", text)  # and its stub points nowhere

    def test_a_wrong_canonical_address_is_found(self) -> None:
        (self.site / "guide" / "upload" / "index.html").write_text(page("https://iainchesworthlabs.com/adl/upload/"), encoding="utf-8")
        self.assertIn("its canonical address is", "\n".join(check_site_layout.check(self.site, self.root)))

    def test_a_page_published_that_should_be_on_github_only_is_found(self) -> None:
        (self.site / "ci").mkdir()
        (self.site / "ci" / "index.html").write_text(page("https://iainchesworthlabs.com/adl/ci/"), encoding="utf-8")
        self.assertIn("ci/: is published, but site_layout.json says it is on GitHub only", "\n".join(check_site_layout.check(self.site, self.root)))

    def test_a_page_left_at_an_old_address_is_found(self) -> None:
        (self.site / "user-guide" / "upload").mkdir(parents=True)
        (self.site / "user-guide" / "upload" / "index.html").write_text(page("https://iainchesworthlabs.com/adl/user-guide/upload/"), encoding="utf-8")
        self.assertIn("at an old address", "\n".join(check_site_layout.check(self.site, self.root)))

    def test_a_page_left_under_user_guide_is_found(self) -> None:
        (self.site / "user-guide" / "limits").mkdir(parents=True)
        (self.site / "user-guide" / "limits" / "index.html").write_text(page("https://iainchesworthlabs.com/adl/user-guide/limits/"), encoding="utf-8")
        self.assertIn("still under user-guide/", "\n".join(check_site_layout.check(self.site, self.root)))

    def test_a_missing_source_file_is_found(self) -> None:
        (self.root / "docs" / "ci.md").unlink()
        self.assertIn("docs/ci.md", "\n".join(check_site_layout.check(self.site, self.root)))

    def test_a_new_page_at_a_chosen_address_is_checked_and_gets_no_stub(self) -> None:
        layout = dict(LAYOUT, pages=LAYOUT["pages"] + [{"old": None, "src": "editions.md", "new": "licensing/"}])
        (self.root / "tools" / "docs" / "site_layout.json").write_text(json.dumps(layout), encoding="utf-8")
        (self.root / "docs" / "editions.md").write_text("# t\n", encoding="utf-8")
        self.assertIn("licensing/: is not in the built site", "\n".join(check_site_layout.check(self.site, self.root)))
        (self.site / "licensing").mkdir()
        (self.site / "licensing" / "index.html").write_text(page("https://iainchesworthlabs.com/adl/licensing/"), encoding="utf-8")
        self.assertEqual(check_site_layout.check(self.site, self.root), [])
        self.assertEqual(sorted(make_old_address_stubs.targets(self.root)), ["", "ci/", "user-guide/upload/"])


class Stubs(unittest.TestCase):
    def test_every_old_address_of_the_repository_gets_a_stub_that_points_on(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            out = Path(tmp)
            count = make_old_address_stubs.write(out, REPO)
            pages = json.loads((REPO / "tools" / "docs" / "site_layout.json").read_text(encoding="utf-8"))["pages"]
            self.assertEqual(count, len([p for p in pages if p["old"] is not None]))
            home = (out / "index.html").read_text(encoding="utf-8")
            self.assertIn('http-equiv="refresh" content="0; url=https://iainchesworthlabs.com/adl/"', home)
            guide = (out / "user-guide" / "upload" / "index.html").read_text(encoding="utf-8")
            self.assertIn("https://iainchesworthlabs.com/adl/guide/upload/", guide)
            github = (out / "ci" / "index.html").read_text(encoding="utf-8")
            self.assertIn("https://github.com/iainchesworthlabs/arinc-data-loader/blob/main/docs/ci.md", github)
            self.assertNotIn("<script", home + guide + github)
            self.assertTrue((out / "404.html").is_file())


class Fake:
    """Just enough of MkDocs' File and Files for the layout hook."""

    class Inclusion:
        def __init__(self, excluded: bool) -> None:
            self.excluded = excluded

        def is_excluded(self) -> bool:
            return self.excluded

    def __init__(self, src: str, dest: str, excluded: bool = False) -> None:
        self.src_uri = src
        self.dest_uri = dest
        self.inclusion = Fake.Inclusion(excluded)


class FakeFiles:
    def __init__(self, *files: Fake) -> None:
        self.by_src = {f.src_uri: f for f in files}

    def get_file_from_path(self, path: str):  # noqa: ANN201
        return self.by_src.get(path)

    def __iter__(self):  # noqa: ANN204
        return iter(self.by_src.values())


class LayoutHook(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.original = site_layout_hook.LAYOUT
        site_layout_hook.LAYOUT = Path(self.tmp.name) / "site_layout.json"
        site_layout_hook.LAYOUT.write_text(json.dumps(LAYOUT), encoding="utf-8")

    def tearDown(self) -> None:
        site_layout_hook.LAYOUT = self.original
        self.tmp.cleanup()

    def test_a_page_gets_its_new_address(self) -> None:
        upload = Fake("user-guide/upload.md", "user-guide/upload/index.html")
        files = FakeFiles(Fake("index.md", "index.html"), upload, Fake("ci.md", "ci/index.html", excluded=True))
        site_layout_hook.on_files(files, {})
        self.assertEqual(upload.dest_uri, "guide/upload/index.html")

    def test_a_file_that_is_not_there_stops_the_build(self) -> None:
        files = FakeFiles(Fake("index.md", "index.html"), Fake("ci.md", "ci/index.html", excluded=True))
        with self.assertRaises(ValueError) as caught:
            site_layout_hook.on_files(files, {})
        self.assertIn("user-guide/upload.md is not in docs/", str(caught.exception))

    def test_a_new_page_of_the_guide_goes_under_guide(self) -> None:
        limits = Fake("user-guide/limits.md", "user-guide/limits/index.html")
        index = Fake("user-guide/index.md", "user-guide/index/index.html")
        files = FakeFiles(Fake("index.md", "index.html"), Fake("user-guide/upload.md", "user-guide/upload/index.html"), Fake("ci.md", "ci/index.html", excluded=True), limits, index)
        site_layout_hook.on_files(files, {})
        self.assertEqual(limits.dest_uri, "guide/limits/index.html")
        self.assertEqual(index.dest_uri, "guide/index.html")
        self.assertEqual(site_layout_hook.guide_destination("faq.md"), None)

    def test_excluding_a_page_that_has_an_address_and_publishing_one_that_has_none_stop_the_build(self) -> None:
        files = FakeFiles(Fake("index.md", "index.html"), Fake("user-guide/upload.md", "x", excluded=True), Fake("ci.md", "ci/index.html"))
        with self.assertRaises(ValueError) as caught:
            site_layout_hook.on_files(files, {})
        self.assertIn("excludes it", str(caught.exception))
        self.assertIn("does not exclude it", str(caught.exception))


def front(title: str, body: str = "<h1>t</h1>", head: str = "", description: str | None = None, lang: str = "en-AU") -> str:
    description = f"About {title}." if description is None else description
    return (f'<!doctype html><html lang="{lang}" data-app="adl"><head><meta charset="utf-8"><title>{title}</title>'
            f'<meta name="description" content="{description}"><link rel="canonical" href="https://iainchesworthlabs.com/adl/{title}/">{head}'
            f'</head><body><main id="main">{body}</main></body></html>')


class Pages(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.site = Path(self.tmp.name)
        for name in check_site_pages.FIXED:
            path = self.site / name
            path.parent.mkdir(parents=True, exist_ok=True)
            if name.endswith(".html"):
                path.write_text(front(name.replace("/index.html", "") or "home"), encoding="utf-8")
            else:
                path.write_text("a {}\n" if name.endswith(".css") else "<svg/>\n", encoding="utf-8")

    def tearDown(self) -> None:
        self.tmp.cleanup()

    def problems(self) -> str:
        return "\n".join(check_site_pages.check_site(self.site, REPO))

    def put(self, name: str, html: str) -> None:
        (self.site / name).write_text(html, encoding="utf-8")

    def test_a_good_site_passes(self) -> None:
        self.assertEqual(self.problems(), "")

    def test_a_missing_required_page_is_found(self) -> None:
        (self.site / "privacy" / "index.html").unlink()
        self.assertIn("privacy/index.html: is not in the built site", self.problems())

    def test_a_script_a_style_and_an_outside_load_on_a_front_page_are_found(self) -> None:
        self.put("faq/index.html", front("faq", head='<script src="x.js"></script><style>a{}</style><link rel="stylesheet" href="https://example.com/x.css">'))
        text = self.problems()
        self.assertIn("has a <script>", text)
        self.assertIn("has <style> or a style= attribute", text)
        self.assertIn("not a link to a host this site may name", text)

    def test_json_ld_is_allowed_and_inline_style_is_not(self) -> None:
        self.put("faq/index.html", front("faq", head='<script type="application/ld+json">{}</script>'))
        self.assertEqual(self.problems(), "")
        self.put("faq/index.html", front("faq", body='<h1 style="color:red">t</h1>'))
        self.assertIn("style=", self.problems())

    def test_the_company_shell_may_link_to_adl_and_the_page_may_not(self) -> None:
        shell = '<header class="icl-header icl-header--next"><a href="/adl/">ARINC Data Loader</a></header><main id="main">'
        self.put("faq/index.html", front("faq").replace('<main id="main">', shell))
        self.assertEqual(self.problems(), "")
        self.put("faq/index.html", front("faq", body='<h1>t</h1><a href="/adl/guide/">guide</a>'))
        self.assertIn("/adl/guide/: write a relative link", self.problems())

    def test_a_mailto_link_is_found(self) -> None:
        self.put("faq/index.html", front("faq", body='<h1>t</h1><a href="mailto:someone@example.com">mail</a>'))
        self.assertIn("the owner's address is private", self.problems())

    def test_a_link_to_a_missing_file_or_anchor_is_found(self) -> None:
        self.put("faq/index.html", front("faq", body='<h1>t</h1><a href="../nothing/">x</a><a href="../support/#missing">y</a>'))
        text = self.problems()
        self.assertIn("../nothing/: no such file in the built site", text)
        self.assertIn("no anchor #missing", text)

    def test_a_link_that_works_is_not_flagged_and_an_adl_path_is(self) -> None:
        self.put("faq/index.html", front("faq", body='<h1>t</h1><a href="../support/">x</a><a href="/support/">company</a>'))
        self.assertEqual(self.problems(), "")
        self.put("faq/index.html", front("faq", body='<h1>t</h1><a href="/adl/support/">x</a>'))
        self.assertIn("write a relative link", self.problems())

    def test_a_picture_needs_alt_and_a_front_page_picture_needs_a_size(self) -> None:
        self.put("faq/index.html", front("faq", body='<h1>t</h1><img src="../icon.svg" width="1" height="1">'))
        self.assertIn("has no alt attribute", self.problems())
        self.put("faq/index.html", front("faq", body='<h1>t</h1><img src="../icon.svg" alt="x">'))
        self.assertIn("has no width and height", self.problems())

    def test_headings_may_not_skip_a_level_and_start_with_h1(self) -> None:
        self.put("faq/index.html", front("faq", body="<h1>t</h1><h2>a</h2><h4>b</h4>"))
        self.assertIn("goes from h2 to h4", self.problems())
        self.assertEqual(check_site_pages.check_headings("x", [2, 3]), ["x: the first heading is an h2, not the h1"])

    def test_two_front_pages_may_not_share_a_title_or_a_description(self) -> None:
        self.put("faq/index.html", front("same", description="One description."))
        self.put("press/index.html", front("same", description="One description."))
        text = self.problems()
        self.assertIn("has the same title as", text)
        self.assertIn("has the same description as", text)

    def test_a_front_page_needs_its_language_and_one_h1(self) -> None:
        self.put("faq/index.html", front("faq", body="<h1>a</h1><h1>b</h1>", lang="en"))
        text = self.problems()
        self.assertIn('needs lang="en-AU"', text)
        self.assertIn("has 2 <h1>", text)

    def test_a_stylesheet_that_loads_from_another_host_is_found(self) -> None:
        (self.site / "theme.css").write_text("@import url(https://example.com/x.css);\n", encoding="utf-8")
        self.assertIn("loads something with @import or an address", self.problems())


SUPPORT_FORM = (lambda text: text[text.index("<form"):text.index("</form>") + len("</form>")])(
    (REPO / "docs" / "support.md").read_text(encoding="utf-8"))


class ContactForm(unittest.TestCase):
    """The form of docs/support.md, and what the check says about changes to it."""

    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.site = Path(self.tmp.name)
        for name in check_site_pages.FIXED:
            path = self.site / name
            path.parent.mkdir(parents=True, exist_ok=True)
            if name.endswith(".html"):
                path.write_text(front(name.replace("/index.html", "") or "home"), encoding="utf-8")
            else:
                path.write_text("a {}\n" if name.endswith(".css") else "<svg/>\n", encoding="utf-8")

    def tearDown(self) -> None:
        self.tmp.cleanup()

    def problems(self, form: str = SUPPORT_FORM, name: str = "support/index.html") -> str:
        (self.site / name).write_text(front("support", body=f"<h1>t</h1>{form}"), encoding="utf-8")
        return "\n".join(check_site_pages.check_site(self.site, REPO))

    def changed(self, old: str, new: str) -> str:
        self.assertIn(old, SUPPORT_FORM, "the test expects this text in the form of docs/support.md")
        return self.problems(SUPPORT_FORM.replace(old, new, 1))

    def test_the_form_of_the_support_page_passes(self) -> None:
        self.assertEqual(self.problems(), "")

    def test_a_guide_page_may_have_a_form_of_its_own(self) -> None:
        material = '<!doctype html><html lang="en"><head><title>g</title><link rel="canonical" href="https://iainchesworthlabs.com/adl/g/"></head>' \
                   '<body><div class="md-container"><form class="md-search__form" action="x"><input name="q"></form><article class="md-typeset"><h1>g</h1></article></div></body></html>'
        (self.site / "guide" / "index.html").write_text(material, encoding="utf-8")
        self.assertEqual(check_site_pages.check_site(self.site, REPO), [])

    def test_it_posts_to_the_one_endpoint_and_nowhere_else(self) -> None:
        self.assertIn("it may post only to /api/contact", self.changed('action="/api/contact"', 'action="https://example.com/api/contact"'))
        self.assertIn("it may post only to /api/contact", self.changed('action="/api/contact"', 'action="/api/other"'))
        self.assertIn("it may post only to /api/contact", self.changed('action="/api/contact"', 'action="/adl/api/contact"'))
        self.assertIn('method="post"', self.changed('method="post"', 'method="get"'))
        self.assertIn("enctype must be the default", self.changed('method="post"', 'method="post" enctype="multipart/form-data"'))

    def test_a_second_form_is_refused(self) -> None:
        self.assertIn("has 2 forms", self.problems(SUPPORT_FORM + '<form action="/x"><input name="q"></form>'))

    def test_the_fields_are_the_fields_of_the_endpoint(self) -> None:
        self.assertIn("the form's fields are", self.changed('name="organisation"', 'name="company"'))
        self.assertIn("the form's fields are", self.problems(SUPPORT_FORM.replace("</form>", '<input name="phone"></form>')))
        self.assertIn('names a field twice', self.problems(SUPPORT_FORM.replace("</form>", '<input type="hidden" name="name" value="x"></form>')))
        self.assertIn('"source" must be a hidden field with the value "adl"', self.changed('value="adl"', 'value="other"'))

    def test_every_field_has_a_label_and_the_endpoints_limits(self) -> None:
        self.assertIn('the field "name" has no <label for>', self.changed('<label for="cf-name">', '<label for="somewhere-else">'))
        self.assertIn('the field "name" is required', self.changed('autocomplete="name" maxlength="100" required', 'autocomplete="name" maxlength="100"'))
        self.assertIn('the field "message" needs maxlength="4000"', self.changed('maxlength="4000"', 'maxlength="9000"'))
        self.assertIn('the field "email" needs type="email"', self.changed('id="cf-email" name="email" type="email"', 'id="cf-email" name="email" type="text"'))
        self.assertIn('the field "message" must be a <textarea>', self.changed('<textarea id="cf-message" name="message"', '<input id="cf-message" name="message" type="text"').replace("</textarea>", ""))

    def test_the_topics_are_the_topics_of_the_endpoint(self) -> None:
        self.assertIn('the field "topic" must be a <select>', self.changed('<option value="enterprise">', '<option value="enterprise-plus">'))
        self.assertIn('the field "topic" must be a <select>', self.changed('<option value="platform-work">A port to your platform</option>\n', ""))

    def test_the_honeypot_must_be_hidden_and_out_of_the_tab_order(self) -> None:
        self.assertIn("the honeypot", self.changed(' aria-hidden="true"', ""))
        self.assertIn("the honeypot", self.changed('tabindex="-1"', ""))
        self.assertIn("the honeypot", self.changed('type="text" tabindex="-1" autocomplete="off"', 'type="text" tabindex="-1"'))
        self.assertIn("the honeypot", self.changed('tabindex="-1" autocomplete="off"', 'tabindex="-1" autocomplete="off" required'))

    def test_consent_and_the_privacy_link_and_a_submit_button(self) -> None:
        self.assertIn('the field "consent" must be a checkbox with the value "yes"', self.changed('type="checkbox" value="yes"', 'type="checkbox" value="on"'))
        self.assertIn("must link to /privacy/#contact-form", self.problems(SUPPORT_FORM.replace("/privacy/#contact-form", "/privacy/")))
        self.assertIn("has no submit button", self.problems(SUPPORT_FORM.replace('<button class="icl-button" type="submit">', '<button class="icl-button" type="button">')))

    def test_a_script_on_the_form_page_is_still_refused(self) -> None:
        self.assertIn("has a <script>", self.problems(SUPPORT_FORM + "<script>x()</script>"))


class Version(unittest.TestCase):
    def test_the_version_is_read_from_the_project_command(self) -> None:
        text = "cmake_minimum_required(VERSION 3.28)\nproject(ArincDataLoader\n    VERSION 1.22.3\n    LANGUAGES CXX)\n"
        self.assertEqual(site_version_hook.read_version(text), "1.22.3")

    def test_the_repository_has_one(self) -> None:
        text = (REPO / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertRegex(site_version_hook.read_version(text), r"^\d+\.\d+\.\d+$")

    def test_a_missing_version_stops_the_build(self) -> None:
        with self.assertRaises(ValueError):
            site_version_hook.read_version("project(Other VERSION 1.0.0)\n")


if __name__ == "__main__":
    unittest.main()

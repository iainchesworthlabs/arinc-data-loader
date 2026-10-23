"""Writes the release's software bill of materials, as SPDX 2.3 and as CycloneDX 1.6 (JSON).

    python make_sbom.py --version 0.1.0 --tag v0.1.0 --commit <sha> --packages <dir> --build-info <dir>
                        --out <dir> [--created 2026-01-01T00:00:00Z]

<packages> holds the package archives of every leg (zip, tar.gz, dmg); <build-info> holds the
build-info-<leg>.json files and qt-sbom/ directory tools/release/collect_build_info.py wrote. The
output is <out>/arinc-data-loader-<version>.spdx.json and <out>/arinc-data-loader-<version>.cdx.json,
with the Qt documents the SPDX one refers to copied beside them.

What it describes, and where each fact comes from:

  - the release and its packages: name, size-independent SHA-256 of each archive, read from the files;
  - the first-party programs and libraries, under GPL-3.0-or-later, at the commit that was built;
  - Qt: the version the kit reported, used under the LGPL-3.0, and for each Qt module that ships a
    runtime library in a package, the kit's own SPDX document, referenced from the SPDX document by
    namespace and SHA-1 as SPDX 2.3 asks (the documents are published beside this one);
    - the Archivo and Noto Sans Hebrew typefaces (SIL OFL 1.1), embedded in every program;
  - the runtime files Qt's deployment tool placed in a package (Mesa's software OpenGL, Microsoft's
    Direct3D compiler and Visual C++ runtime installer, ICU), found by name in the archive's file list;
  - the vcpkg packages installed during the build (Catch2): test-only, so not in any package, and said
    to be a test dependency;
  - the build tools (compiler, CMake, Ninja) of each leg, as build tools.

A fact the build did not report is NOASSERTION, never a guess. Standard library only; the documents are
checked against the formal schemas by tools/release/validate_sbom.py in the workflow.
"""

import argparse
import hashlib
import json
import re
import shutil
import sys
import uuid
from datetime import datetime, timezone
from pathlib import Path

REPO_URL = "https://github.com/iainchesworthlabs/arinc-data-loader"
FONT_URL = "https://github.com/Omnibus-Type/Archivo"
HEBREW_FONT_URL = "https://github.com/notofonts/hebrew"
NOASSERTION = "NOASSERTION"
PROGRAMS = [
    ("adl_app", "The ARINC 615A data loader application"),
    ("adl_media_gen", "The ARINC 665 media set generator application"),
    ("adl_mock_target", "The mock ARINC 615A target application"),
    ("adl_write_payloads", "A command-line tool that writes sample ARINC 665 media sets"),
]
LIBRARIES = [
    ("adl_arinc665", "The ARINC 665 media set, load and check-value library"),
    ("adl_arinc615a", "The ARINC 615A codec and data loader state machines"),
    ("adl_arinc615a_runner", "The runner that drives the 615A engine over adl::platform sockets"),
    ("adl_media_tree", "A media set as a directory tree on disk"),
    ("adl_platform", "The platform interfaces and this host's implementation of them"),
]

# Qt libraries that belong to modules other than qtbase, by the name after "Qt6".
QT_MODULE_PREFIXES = [
    ("qtdeclarative", ("Qml", "Quick", "LabsQml", "LabsFolder")),
    ("qtsvg", ("Svg",)),
    ("qtshadertools", ("ShaderTools",)),
    ("qttools", ("Designer", "Help", "UiTools")),
]
QT_LIBRARY = re.compile(r"(?:^|/)(?:lib)?Qt6([A-Za-z0-9]+?)d?(?:\.dll|\.so(?:\.[0-9.]+)?|\.[0-9.]+\.dylib|\.dylib)$|"
                        r"(?:^|/)Qt([A-Za-z0-9]+)\.framework/")
RUNTIME_FILES = [
    # (file name pattern, id, name, license, homepage, description)
    (re.compile(r"(?:^|/)opengl32sw\.dll$"), "Mesa-llvmpipe", "opengl32sw (Mesa llvmpipe)", "MIT",
     "https://www.mesa3d.org/", "Software OpenGL rasteriser Qt's kit provides for machines without OpenGL"),
    (re.compile(r"(?:^|/)d3dcompiler_47\.dll$"), "D3DCompiler", "d3dcompiler_47", NOASSERTION,
     "https://learn.microsoft.com/windows/win32/direct3dtools/d3dcompiler", "Microsoft Direct3D shader compiler, redistributed under the Windows SDK terms"),
    (re.compile(r"(?:^|/)vc_redist\.x64\.exe$"), "VCRedist", "Microsoft Visual C++ Redistributable", NOASSERTION,
     "https://learn.microsoft.com/cpp/windows/latest-supported-vc-redist", "The C++ runtime installer carried so the programs can run on a machine without it"),
    (re.compile(r"(?:^|/)libicu(?:data|i18n|uc|io|tu)\.so\.[0-9.]+$"), "ICU", "ICU", "ICU",
     "https://icu.unicode.org/", "International Components for Unicode, carried with Qt's Linux kit"),
]


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def sha1(path):
    digest = hashlib.sha1()  # nosec: SPDX 2.3 asks for SHA-1 to identify an external document
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def spdx_id(text):
    return "SPDXRef-" + re.sub(r"[^A-Za-z0-9.-]+", "-", text).strip("-")


def qt_module(library):
    for module, prefixes in QT_MODULE_PREFIXES:
        if library.startswith(prefixes):
            return module
    return "qtbase"


def qt_modules_in(files):
    modules = set()
    for name in files:
        match = QT_LIBRARY.search(name)
        if match:
            modules.add(qt_module(match.group(1) or match.group(2)))
    return modules


def read_infos(directory):
    infos = {}
    for path in sorted(Path(directory).glob("build-info-*.json")):
        info = json.loads(path.read_text(encoding="utf-8"))
        infos[info["leg"]] = info
    if not infos:
        raise SystemExit(f"no build-info-*.json in {directory}")
    return infos


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--version", required=True)
    parser.add_argument("--tag", default="")
    parser.add_argument("--commit", required=True)
    parser.add_argument("--packages", required=True)
    parser.add_argument("--build-info", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--created", default=datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"))
    args = parser.parse_args()

    infos = read_infos(args.build_info)
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    version, commit = args.version, args.commit
    download = f"git+{REPO_URL}.git@{commit}"
    namespace = "https://github.com/iainchesworthlabs/arinc-data-loader/sbom/" + str(
        uuid.uuid5(uuid.NAMESPACE_URL, f"{REPO_URL}/{args.tag or version}/{commit}"))

    # --- the archives --------------------------------------------------------
    archives = []
    for leg, info in infos.items():
        for name, files in info["archives"].items():
            path = Path(args.packages) / name
            if not path.is_file():
                raise SystemExit(f"{name} is in build-info-{leg}.json but not in {args.packages}")
            archives.append({"leg": leg, "name": name, "files": files, "sha256": sha256(path),
                             "runtime": "-dev-" not in name})
    archives.sort(key=lambda a: a["name"])

    # --- SPDX ------------------------------------------------------------------
    packages = []
    relationships = []
    external_refs = {}

    def add_relationship(left, kind, right, comment=None):
        item = {"spdxElementId": left, "relationshipType": kind, "relatedSpdxElement": right}
        if comment:
            item["comment"] = comment
        relationships.append(item)

    def purl(kind, name, ver):
        return {"referenceCategory": "PACKAGE-MANAGER", "referenceType": "purl",
                "referenceLocator": f"pkg:{kind}/{name}@{ver}"}

    root = "SPDXRef-Release"
    packages.append({
        "SPDXID": root, "name": "arinc-data-loader", "versionInfo": version,
        "supplier": "Organization: iainchesworthlabs", "downloadLocation": download,
        "filesAnalyzed": False, "homepage": REPO_URL, "licenseConcluded": "GPL-3.0-or-later",
        "licenseDeclared": "GPL-3.0-or-later", "copyrightText": NOASSERTION,
        "primaryPackagePurpose": "APPLICATION",
        "description": "ARINC 615A and 665 data loader: libraries, three Qt applications and a command-line tool",
        "sourceInfo": f"Built from commit {commit}" + (f" (tag {args.tag})" if args.tag else ""),
        "externalRefs": [purl("github", "iainchesworthlabs/arinc-data-loader", version)],
    })

    program_ids, library_ids = {}, {}
    for name, description in PROGRAMS:
        ident = spdx_id(name)
        program_ids[name] = ident
        packages.append({"SPDXID": ident, "name": name, "versionInfo": version,
                         "supplier": "Organization: iainchesworthlabs", "downloadLocation": download,
                         "filesAnalyzed": False, "licenseConcluded": "GPL-3.0-or-later",
                         "licenseDeclared": "GPL-3.0-or-later", "copyrightText": NOASSERTION,
                         "primaryPackagePurpose": "APPLICATION", "description": description})
        add_relationship(root, "CONTAINS", ident)
    for name, description in LIBRARIES:
        ident = spdx_id(name)
        library_ids[name] = ident
        packages.append({"SPDXID": ident, "name": name, "versionInfo": version,
                         "supplier": "Organization: iainchesworthlabs", "downloadLocation": download,
                         "filesAnalyzed": False, "licenseConcluded": "GPL-3.0-or-later",
                         "licenseDeclared": "GPL-3.0-or-later", "copyrightText": NOASSERTION,
                         "primaryPackagePurpose": "LIBRARY", "description": description})
        add_relationship(root, "CONTAINS", ident)

    # Qt, once; the version is the one every leg reported.
    qt_versions = sorted({info["qt_version"] for info in infos.values()})
    qt_version = qt_versions[0] if len(qt_versions) == 1 else NOASSERTION
    qt_id = "SPDXRef-Qt"
    packages.append({
        "SPDXID": qt_id, "name": "Qt", "versionInfo": qt_version, "supplier": "Organization: The Qt Company",
        "downloadLocation": "https://download.qt.io/official_releases/qt/", "filesAnalyzed": False,
        "homepage": "https://www.qt.io/", "licenseConcluded": NOASSERTION, "licenseDeclared": "LGPL-3.0-only",
        "licenseComments": "Used under the GNU LGPL version 3, as shared libraries deployed beside the programs. "
                           "The Qt SPDX documents referenced from this one give each module's full license expression.",
        "copyrightText": NOASSERTION, "primaryPackagePurpose": "FRAMEWORK",
        "description": "The Qt libraries (shared) the three applications use",
        "externalRefs": [purl("generic", "qt", qt_version)],
    })

    font_id = "SPDXRef-Archivo"
    packages.append({
        "SPDXID": font_id, "name": "Archivo", "versionInfo": NOASSERTION,
        "supplier": "Organization: The Archivo Project Authors", "downloadLocation": FONT_URL,
        "filesAnalyzed": False, "homepage": FONT_URL, "licenseConcluded": "OFL-1.1", "licenseDeclared": "OFL-1.1",
        "copyrightText": "Copyright 2020 The Archivo Project Authors", "primaryPackagePurpose": "FILE",
        "description": "The typeface embedded, unmodified, in every program (Regular, Medium, SemiBold, ExtraBold)",
    })
        # The typeface of the Hebrew and Yiddish windows (libs/ui/fonts, notices/fragments/fonts.txt): four static
    # instances of the weights the design uses, cut from the variable font the Noto Project publishes.
    hebrew_font_id = "SPDXRef-NotoSansHebrew"
    packages.append({
        "SPDXID": hebrew_font_id, "name": "Noto Sans Hebrew", "versionInfo": NOASSERTION,
        "supplier": "Organization: The Noto Project Authors", "downloadLocation": HEBREW_FONT_URL,
        "filesAnalyzed": False, "homepage": HEBREW_FONT_URL, "licenseConcluded": "OFL-1.1",
        "licenseDeclared": "OFL-1.1", "copyrightText": "Copyright 2022 The Noto Project Authors",
        "primaryPackagePurpose": "FILE",
        "description": "The typeface of the Hebrew and Yiddish windows, embedded in every program: four static "
                       "instances (Regular, Medium, SemiBold, ExtraBold) cut from the variable font the Noto "
                       "Project publishes; the letters are not otherwise changed",
    })
    for name in ("adl_app", "adl_media_gen", "adl_mock_target"):
        add_relationship(program_ids[name], "DYNAMIC_LINK", qt_id)
        add_relationship(program_ids[name], "CONTAINS", font_id)
        add_relationship(program_ids[name], "CONTAINS", hebrew_font_id)
    # How the first-party pieces build on each other (the libraries are static, so a program that uses
    # one carries it).
    add_relationship(library_ids["adl_arinc615a"], "DEPENDS_ON", library_ids["adl_arinc665"])
    add_relationship(library_ids["adl_media_tree"], "DEPENDS_ON", library_ids["adl_arinc665"])
    add_relationship(library_ids["adl_arinc615a_runner"], "DEPENDS_ON", library_ids["adl_arinc615a"])
    add_relationship(library_ids["adl_arinc615a_runner"], "DEPENDS_ON", library_ids["adl_platform"])
    for library in ("adl_arinc665", "adl_media_tree"):
        add_relationship(program_ids["adl_write_payloads"], "STATIC_LINK", library_ids[library])

    # The kit's own SPDX documents, referenced by namespace and SHA-1, for the Qt modules whose
    # libraries a package holds.
    qt_docs_dir = Path(args.build_info) / "qt-sbom"
    seen_modules = {}
    for archive in archives:
        if not archive["runtime"]:
            continue
        for module in sorted(qt_modules_in(archive["files"])):
            document = qt_docs_dir / f"{archive['leg']}-{module}-{qt_version}.spdx.json"
            if not document.is_file():
                continue
            ref = f"DocumentRef-{archive['leg']}-{module}"
            if ref in external_refs:
                continue
            content = json.loads(document.read_text(encoding="utf-8"))
            external_refs[ref] = {
                "externalDocumentId": ref, "spdxDocument": content["documentNamespace"],
                "checksum": {"algorithm": "SHA1", "checksumValue": sha1(document)}}
            described = content.get("documentDescribes") or [
                r["relatedSpdxElement"] for r in content.get("relationships", [])
                if r["spdxElementId"] == "SPDXRef-DOCUMENT" and r["relationshipType"] == "DESCRIBES"]
            for target in described:
                add_relationship(qt_id, "CONTAINS", f"{ref}:{target}",
                                 f"{module} for {archive['leg']}: {document.name}")
            seen_modules[document.name] = ref
            # Published beside this document, so the reference can be followed.
            shutil.copyfile(document, out / document.name)
            sibling = document.with_name(document.name.replace(".spdx.json", ".cdx.json"))
            if sibling.is_file():
                shutil.copyfile(sibling, out / sibling.name)

    # The packages, and what each holds.
    archive_ids = {}
    runtime_files_seen = {}
    for archive in archives:
        ident = spdx_id("Package-" + archive["name"])
        archive_ids[archive["name"]] = ident
        packages.append({
            "SPDXID": ident, "name": archive["name"], "versionInfo": version, "packageFileName": archive["name"],
            "supplier": "Organization: iainchesworthlabs", "downloadLocation": NOASSERTION, "filesAnalyzed": False,
            "checksums": [{"algorithm": "SHA256", "checksumValue": archive["sha256"]}],
            "licenseConcluded": NOASSERTION, "licenseDeclared": NOASSERTION,
            "licenseComments": "A package of GPL-3.0-or-later programs with the third-party components listed in NOTICES.txt inside it.",
            "copyrightText": NOASSERTION, "primaryPackagePurpose": "ARCHIVE",
            "comment": f"Built on the {archive['leg']} leg.",
        })
        add_relationship(root, "CONTAINS", ident)
        if archive["runtime"]:
            for name in program_ids:
                add_relationship(ident, "CONTAINS", program_ids[name])
            add_relationship(ident, "CONTAINS", qt_id)
            add_relationship(ident, "CONTAINS", font_id)
            add_relationship(ident, "CONTAINS", hebrew_font_id)
            for pattern, key, label, license_, homepage, description in RUNTIME_FILES:
                if any(pattern.search(f) for f in archive["files"]):
                    component = spdx_id("Runtime-" + key)
                    if component not in runtime_files_seen:
                        runtime_files_seen[component] = True
                        packages.append({
                            "SPDXID": component, "name": label, "versionInfo": NOASSERTION,
                            "supplier": NOASSERTION, "downloadLocation": NOASSERTION, "filesAnalyzed": False,
                            "homepage": homepage, "licenseConcluded": NOASSERTION, "licenseDeclared": license_,
                            "copyrightText": NOASSERTION, "primaryPackagePurpose": "LIBRARY", "description": description})
                    add_relationship(ident, "CONTAINS", component)
        else:
            for name in library_ids:
                add_relationship(ident, "CONTAINS", library_ids[name])

    # What the build used, and was not shipped.
    cdx_tools = {}
    for leg, info in infos.items():
        tools = [("compiler-" + info["compiler"]["id"], info["compiler"]["id"] + " C++ compiler", info["compiler"]["version"]),
                 ("cmake", "CMake", info["cmake"]), ("ninja", "Ninja", info["ninja"])]
        for key, label, ver in tools:
            ident = spdx_id(f"BuildTool-{leg}-{key}")
            packages.append({
                "SPDXID": ident, "name": label, "versionInfo": ver, "supplier": NOASSERTION,
                "downloadLocation": NOASSERTION, "filesAnalyzed": False, "licenseConcluded": NOASSERTION,
                "licenseDeclared": NOASSERTION, "copyrightText": NOASSERTION, "primaryPackagePurpose": "APPLICATION",
                "comment": f"Used on the {leg} leg to build {version}; not part of any package."})
            add_relationship(ident, "BUILD_TOOL_OF", root)
            cdx_tools[(label, ver)] = None
        for port in info["vcpkg"]:
            ident = spdx_id(f"Vcpkg-{leg}-{port['name']}")
            packages.append({
                "SPDXID": ident, "name": port["name"], "versionInfo": port["version"] or NOASSERTION,
                "supplier": NOASSERTION, "downloadLocation": port["download"] or NOASSERTION, "filesAnalyzed": False,
                "homepage": port["homepage"] or NOASSERTION, "licenseConcluded": port["license"] or NOASSERTION,
                "licenseDeclared": NOASSERTION, "copyrightText": NOASSERTION, "primaryPackagePurpose": "LIBRARY",
                "comment": f"Installed by vcpkg on the {leg} leg for the tests; it is in no package."})
            # vcpkg's own helper ports (vcpkg-cmake, ...) drive the build of a port; the others are
            # libraries the tests link.
            kind = "BUILD_TOOL_OF" if port["name"].startswith("vcpkg-") else "TEST_DEPENDENCY_OF"
            add_relationship(ident, kind, root)

    spdx = {
        "spdxVersion": "SPDX-2.3", "dataLicense": "CC0-1.0", "SPDXID": "SPDXRef-DOCUMENT",
        "name": f"arinc-data-loader-{version}", "documentNamespace": namespace,
        "creationInfo": {"created": args.created,
                         "creators": ["Tool: arinc-data-loader/tools/release/make_sbom.py",
                                      "Organization: iainchesworthlabs"]},
        "documentDescribes": [root],
        "packages": packages,
        "relationships": relationships,
    }
    if external_refs:
        spdx["externalDocumentRefs"] = list(external_refs.values())
    spdx_path = out / f"arinc-data-loader-{version}.spdx.json"
    spdx_path.write_text(json.dumps(spdx, indent=2) + "\n", encoding="utf-8", newline="\n")

    # --- CycloneDX ---------------------------------------------------------------
    def component(ref, kind, name, ver, **extra):
        item = {"bom-ref": ref, "type": kind, "name": name}
        if ver:
            item["version"] = ver
        item.update(extra)
        return item

    licence = lambda expression: [{"expression": expression}]  # noqa: E731
    components = []
    dependencies = {}

    def depends(ref, *targets):
        dependencies.setdefault(ref, [])
        for target in targets:
            if target not in dependencies[ref]:
                dependencies[ref].append(target)

    root_ref = "release"
    for name, description in PROGRAMS:
        components.append(component(name, "application", name, version, description=description,
                                    licenses=licence("GPL-3.0-or-later")))
        depends(root_ref, name)
    for name, description in LIBRARIES:
        components.append(component(name, "library", name, version, description=description,
                                    licenses=licence("GPL-3.0-or-later")))
        depends(root_ref, name)
    components.append(component("qt", "framework", "Qt", qt_version, supplier={"name": "The Qt Company"},
                                purl=f"pkg:generic/qt@{qt_version}", licenses=licence("LGPL-3.0-only"),
                                externalReferences=[{"type": "website", "url": "https://www.qt.io/"}],
                                description="The Qt libraries (shared) the three applications use; Qt's own CycloneDX "
                                            "documents per module are published beside this one"))
    components.append(component("archivo", "data", "Archivo", None, licenses=licence("OFL-1.1"),
                                externalReferences=[{"type": "website", "url": FONT_URL}],
                                description="The typeface embedded, unmodified, in every program"))
    components.append(component("noto-sans-hebrew", "data", "Noto Sans Hebrew", None, licenses=licence("OFL-1.1"),
                                externalReferences=[{"type": "website", "url": HEBREW_FONT_URL}],
                                description="The typeface of the Hebrew and Yiddish windows, embedded in every "
                                            "program: four static instances cut from the variable font the Noto "
                                            "Project publishes"))
    for name in ("adl_app", "adl_media_gen", "adl_mock_target"):
        depends(name, "qt", "archivo", "noto-sans-hebrew")
    depends("adl_arinc615a", "adl_arinc665")
    depends("adl_media_tree", "adl_arinc665")
    depends("adl_arinc615a_runner", "adl_arinc615a", "adl_platform")
    depends("adl_write_payloads", "adl_arinc665", "adl_media_tree")
    for archive in archives:
        ref = "package-" + archive["name"]
        components.append(component(ref, "file", archive["name"], version,
                                    hashes=[{"alg": "SHA-256", "content": archive["sha256"]}],
                                    description=f"Built on the {archive['leg']} leg"))
        depends(root_ref, ref)
        if archive["runtime"]:
            depends(ref, *program_ids.keys(), "qt", "archivo", "noto-sans-hebrew")
            for pattern, key, label, license_, homepage, description in RUNTIME_FILES:
                if any(pattern.search(f) for f in archive["files"]):
                    cref = "runtime-" + key
                    if not any(c["bom-ref"] == cref for c in components):
                        item = component(cref, "library", label, None, description=description,
                                         externalReferences=[{"type": "website", "url": homepage}])
                        if license_ != NOASSERTION:
                            item["licenses"] = [{"license": {"id": license_}}]
                        components.append(item)
                    depends(ref, cref)
        else:
            depends(ref, *library_ids.keys())
    for leg, info in infos.items():
        for port in info["vcpkg"]:
            ref = f"vcpkg-{leg}-{port['name']}"
            components.append(component(ref, "library", port["name"], port["version"], scope="excluded",
                                        description=f"Installed by vcpkg on the {leg} leg for the tests only; "
                                                    f"in no package"))
    tool_components = [component(f"tool-{n}-{v}".replace(" ", "-"), "application", n, v) for (n, v) in sorted(cdx_tools)]

    cdx = {
        "bomFormat": "CycloneDX", "specVersion": "1.6",
        "serialNumber": "urn:uuid:" + str(uuid.uuid5(uuid.NAMESPACE_URL, namespace + "/cdx")), "version": 1,
        "metadata": {
            "timestamp": args.created,
            "tools": {"components": [component("make-sbom", "application", "make_sbom.py", None,
                                               author="iainchesworthlabs", group="arinc-data-loader")]},
            "component": component(root_ref, "application", "arinc-data-loader", version, licenses=licence("GPL-3.0-or-later"),
                                   purl=f"pkg:github/iainchesworthlabs/arinc-data-loader@{version}",
                                   externalReferences=[{"type": "vcs", "url": f"{REPO_URL}.git"},
                                                       {"type": "website", "url": REPO_URL}]),
            "properties": [{"name": "git:commit", "value": commit}] + ([{"name": "git:tag", "value": args.tag}] if args.tag else []),
        },
        "components": components + tool_components,
        "dependencies": [{"ref": ref, "dependsOn": targets} for ref, targets in dependencies.items()],
    }
    cdx_path = out / f"arinc-data-loader-{version}.cdx.json"
    cdx_path.write_text(json.dumps(cdx, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"{spdx_path.name}: {len(packages)} package(s), {len(relationships)} relationship(s), "
          f"{len(external_refs)} Qt document reference(s)")
    print(f"{cdx_path.name}: {len(cdx['components'])} component(s)")


if __name__ == "__main__":
    sys.exit(main())

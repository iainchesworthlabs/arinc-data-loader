# SPDX-License-Identifier: GPL-3.0-or-later
"""Checks the release's SBOM documents against their formal specifications.

    python validate_sbom.py <file.spdx.json | file.cdx.json>...

An SPDX 2.3 document goes through spdx-tools' full validator (the element, relationship, licence and
external-reference rules of the specification), a CycloneDX 1.6 document through the JSON schema that
cyclonedx-python-lib ships and a check that every dependency names a component that exists. Needs the
packages in .github/requirements/sbom.txt (installed with --require-hashes). Exit 0 = all valid.
"""

import json
import sys
from pathlib import Path


def check_spdx(path):
    from spdx_tools.spdx.parser.parse_anything import parse_file
    from spdx_tools.spdx.validation.document_validator import validate_full_spdx_document

    document = parse_file(str(path))
    return [f"{message.validation_message} ({message.context})" for message in validate_full_spdx_document(document)]


def check_cyclonedx(path):
    from cyclonedx.schema import SchemaVersion
    from cyclonedx.validation.json import JsonStrictValidator

    text = path.read_text(encoding="utf-8")
    error = JsonStrictValidator(SchemaVersion.V1_6).validate_str(text)
    problems = [str(error)] if error else []
    document = json.loads(text)
    known = {c["bom-ref"] for c in document.get("components", []) if "bom-ref" in c}
    known.add(document["metadata"]["component"]["bom-ref"])
    for dependency in document.get("dependencies", []):
        for ref in [dependency["ref"], *dependency.get("dependsOn", [])]:
            if ref not in known:
                problems.append(f"dependency names {ref!r}, which is not a component")
    return problems


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    failed = False
    for name in sys.argv[1:]:
        path = Path(name)
        if path.name.endswith(".spdx.json"):
            problems = check_spdx(path)
        elif path.name.endswith(".cdx.json"):
            problems = check_cyclonedx(path)
        else:
            raise SystemExit(f"{name}: neither .spdx.json nor .cdx.json")
        if problems:
            failed = True
            print(f"{name}: {len(problems)} problem(s)")
            for problem in problems:
                print(f"  {problem}")
        else:
            print(f"{name}: valid")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())

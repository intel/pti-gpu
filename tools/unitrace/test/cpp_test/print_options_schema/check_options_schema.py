# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

# Validate `unitrace --print-options-schema`: the output must be well-formed JSON
# carrying the schema the VS Code extension consumes. This is a no-GPU test --
# --print-options-schema prints and exits before any Level Zero initialization.

import json
import subprocess
import sys


def fail(msg):
    print(f"[ERROR] {msg}", file=sys.stderr)
    sys.exit(1)


def main():
    if len(sys.argv) != 2:
        fail(f"usage: {sys.argv[0]} <path-to-unitrace>")
    unitrace = sys.argv[1]

    proc = subprocess.run(
        [unitrace, "--print-options-schema"],
        capture_output=True, text=True)
    if proc.returncode != 0:
        fail(f"unitrace --print-options-schema exited {proc.returncode}\n{proc.stderr}")

    try:
        schema = json.loads(proc.stdout)
    except json.JSONDecodeError as e:
        fail(f"output is not valid JSON: {e}\n--- output ---\n{proc.stdout}")

    # Required top-level keys.
    for key in ("schemaVersion", "unitraceVersion", "build", "groups", "flags"):
        if key not in schema:
            fail(f"missing top-level key: {key!r}")

    if not isinstance(schema["flags"], list) or not schema["flags"]:
        fail("'flags' must be a non-empty array")

    if not isinstance(schema["build"], dict):
        fail("'build' must be an object")

    # Groups carry the form's section labels; collect their ids so each flag can
    # be checked against them.
    if not isinstance(schema["groups"], list) or not schema["groups"]:
        fail("'groups' must be a non-empty array")
    group_ids = set()
    for group in schema["groups"]:
        for field in ("id", "label"):
            if field not in group:
                fail(f"group {group.get('id', '?')!r} missing field {field!r}")
        group_ids.add(group["id"])

    # Every flag must carry the mechanical fields the extension relies on.
    valid_types = {"bool", "int", "string", "enum", "path", "action"}
    names = set()
    for flag in schema["flags"]:
        for field in ("name", "type", "group", "help"):
            if field not in flag:
                fail(f"flag {flag.get('name', '?')!r} missing field {field!r}")
        if flag["type"] not in valid_types:
            fail(f"flag {flag['name']!r} has unknown type {flag['type']!r}")
        if flag["group"] not in group_ids:
            fail(f"flag {flag['name']!r} references group {flag['group']!r} "
                 f"with no label in 'groups'")
        if flag["name"] in names:
            fail(f"duplicate flag name: {flag['name']!r}")
        names.add(flag["name"])

    # Relation fields (conflictsWith/requires) must reference real flags.
    for flag in schema["flags"]:
        for rel in ("conflictsWith", "requires"):
            for target in flag.get(rel, []):
                if target not in names:
                    fail(f"flag {flag['name']!r} {rel} references unknown flag {target!r}")

    # --help is always present; spot-check that a known flag survived the dump.
    if "--help" not in names:
        fail("expected --help in the flag list")

    # SchemaOmit()ted flags stay in --help/dispatch but must NOT leak into the schema.
    for omitted in ("--pause", "--resume", "--stop"):
        if omitted in names:
            fail(f"{omitted!r} is SchemaOmit()ted and must not appear in the schema")

    print(f"[OK] --print-options-schema produced {len(schema['flags'])} valid flags "
          f"in {len(group_ids)} groups")


if __name__ == "__main__":
    main()

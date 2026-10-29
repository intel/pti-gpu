# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================
#
# Fuzzes scripts/summary/categorize.py build_tree_from_ini() (ConfigParser and
# the search-tree builder), then looks names up in the tree with
# summary.categorize() as summary.py does. Input: name \0 INI, or a plain INI
# (the seed corpus).

import configparser
import sys
import atheris
import fuzz_common

fuzz_common.add_script_dir("summary")
with fuzz_common.instrument():
    import categorize
    import summary

EXPECTED = (configparser.Error, UnicodeDecodeError)


def TestOneInput(data):
    """Atheris entry point: data is a category schema INI, optionally preceded by a
    name to look up and a NUL (name \\0 INI); without a NUL it is all INI, so plain
    INI files serve as seeds.

    Builds the search tree with categorize.build_tree_from_ini(), with and without
    remove_tag_spaces, and looks the name up in each with summary.categorize().
    configparser errors and undecodable bytes are a clean rejection; any other
    exception is a finding.
    """
    head, sep, ini = data.partition(b"\0")
    if not sep:
        head, ini = b"", data
    name = head.decode("utf-8", "replace")
    with fuzz_common.TempFile(ini, ".ini") as path:
        for remove_spaces in (False, True):
            try:
                tree = categorize.build_tree_from_ini(path, remove_spaces)
            except EXPECTED:
                return
            summary.categorize(tree, name)


if __name__ == "__main__":
    atheris.Setup(sys.argv, TestOneInput)
    atheris.Fuzz()

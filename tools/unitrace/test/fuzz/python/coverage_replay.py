# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================
#
# Replays a corpus through one harness's TestOneInput, for coverage.py:
#   UNITRACE_FUZZ_REPLAY=1 python3 -m coverage run coverage_replay.py <target> <dir>...
# An input that raises is reported and counted, not fatal.

import importlib
import os
import sys

os.environ["UNITRACE_FUZZ_REPLAY"] = "1"
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))


def main():
    """Replays inputs through one harness for coverage.py.

    Arguments: the target name (fuzz_<target>.py is imported) and the directories
    whose files are the inputs. Each input is passed to TestOneInput once; one that
    raises is reported and counted, not fatal, so the whole corpus is measured.
    """
    target, dirs = sys.argv[1], sys.argv[2:]
    harness = importlib.import_module("fuzz_" + target)
    runs = failures = 0
    for d in dirs:
        for name in sorted(os.listdir(d)):
            path = os.path.join(d, name)
            if not os.path.isfile(path):
                continue
            with open(path, "rb") as f:
                data = f.read()
            runs += 1
            try:
                harness.TestOneInput(data)
            except Exception as ex:
                failures += 1
                print(f"[REPLAY] {path}: {type(ex).__name__}: {ex}", file=sys.stderr)
    print(f"[REPLAY] {target}: {runs} inputs, {failures} raised")


if __name__ == "__main__":
    main()

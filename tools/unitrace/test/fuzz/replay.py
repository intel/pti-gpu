#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

# Replays one fuzz target's seed corpus (corpus/<target>/) and crash inputs
# (regression/<target>/), as tests.txt runs it:
#   replay.py <target> <dir holding the C++ fuzz_* binaries>
# C++ targets are the libFuzzer binaries of the fuzz build; Python targets the
# harnesses in python/. Exit status: 0 passed, 1 failed, 77 skipped (the target
# cannot run here: off Linux, no clang with libFuzzer, or no atheris; the last
# line printed says why).

import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SKIPPED = 77


def run(cmd):
    """Runs cmd and forwards its output, decoded with replacement: a harness echoes
    parts of its input (e.g. a rejected device list holding 0xff), and the test
    runner reads this script's output as UTF-8 text."""
    result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    sys.stdout.write(result.stdout.decode("utf-8", "replace"))
    sys.stdout.flush()
    return result


def inputs(target):
    """The input directories of target that exist: corpus/<target> and
    regression/<target>.
    """
    dirs = [os.path.join(HERE, d, target) for d in ("corpus", "regression")]
    return [d for d in dirs if os.path.isdir(d)]


def replay_cpp(target, bin_dir):
    """Replays the inputs of a C++ target through its libFuzzer binary (-runs=0).

    target: the name, e.g. "elf64"; bin_dir: the directory holding fuzz_<target>.
    Returns 0 if every input runs cleanly, 1 if one fails or the binary is missing
    although it could be built, SKIPPED if it is missing because the fuzz build
    cannot run here (fuzz_tests.skip_reason()).
    """
    exe = os.path.join(bin_dir, "fuzz_" + target)
    if not os.path.isfile(exe):
        sys.path.insert(0, os.path.dirname(HERE))
        import fuzz_tests
        reason = fuzz_tests.skip_reason()
        if reason:
            print(reason)
            return SKIPPED
        print(f"[ERROR] fuzz_{target} is not built at {exe}")
        return 1
    cmd = [exe, "-runs=0", "-timeout=10", "-rss_limit_mb=2048", "-malloc_limit_mb=1024"] + inputs(target)
    print("Command: " + " ".join(cmd))
    result = run(cmd)
    if result.returncode != 0:
        print(f"[ERROR] fuzz_{target} failed on an input (exit status {result.returncode})")
        return 1
    return 0


def replay_python(target, harness):
    """Replays the inputs of a Python target through its Atheris harness.

    target: the name, e.g. "summary"; harness: the path of fuzz_<target>.py. Each
    input file is run once. Returns 0 if all run cleanly, 1 if one fails or there are
    no inputs, SKIPPED if atheris (which the harnesses import) is not installed.
    """
    try:
        import atheris  # noqa: F401  (the harnesses import it)
    except ImportError:
        print("atheris not installed")
        return SKIPPED
    files = [os.path.join(d, f) for d in inputs(target) for f in sorted(os.listdir(d))
             if os.path.isfile(os.path.join(d, f))]
    if not files:
        print(f"[ERROR] fuzz_{target} has no inputs")
        return 1
    # A harness given files runs each once (atheris/libFuzzer -runs semantics).
    cmd = [sys.executable, harness] + files
    print("Command: " + " ".join(cmd[:2]) + f" <{len(files)} inputs>")
    result = run(cmd)
    if result.returncode != 0:
        print(f"[ERROR] fuzz_{target} failed on an input (exit status {result.returncode})")
        return 1
    return 0


def main():
    """replay.py <target> <fuzz binaries dir>: replays one target; returns its exit
    status (0 passed, 1 failed, SKIPPED). Off Linux every target is SKIPPED.
    """
    if len(sys.argv) != 3:
        print("usage: replay.py <target> <fuzz binaries dir>")
        return 1
    target, bin_dir = sys.argv[1], sys.argv[2]
    if sys.platform != "linux":
        print(f"not supported on {sys.platform}")
        return SKIPPED
    harness = os.path.join(HERE, "python", f"fuzz_{target}.py")
    if os.path.isfile(harness):
        return replay_python(target, harness)
    if os.path.isfile(os.path.join(HERE, "cpp", f"fuzz_{target}.cc")):
        return replay_cpp(target, bin_dir)
    print(f"[ERROR] no fuzz target '{target}'")
    return 1


if __name__ == "__main__":
    sys.exit(main())

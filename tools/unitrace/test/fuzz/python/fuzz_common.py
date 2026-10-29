# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================
#
# Shared helpers for the Atheris harnesses. A harness lists the exceptions that
# are a clean rejection of malformed input; any other exception propagates and
# Atheris records it as a crash with its reproducer.

import atexit
import os
import shutil
import sys
import tempfile

SCRIPTS = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "scripts"))


def add_script_dir(*parts):
    """Makes the scripts in tools/unitrace/scripts/<parts> importable.

    parts: the path below scripts/, e.g. "metrics". The directory is put first on
    sys.path, once, so a harness imports the script unitrace ships.
    """
    path = os.path.join(SCRIPTS, *parts)
    if path not in sys.path:
        sys.path.insert(0, path)


_private_dir = None


def _private_tmp():
    """A directory only this user can enter (mkdtemp: mode 0700, unpredictable name),
    created once per process and removed at exit. Keeps TempFile's fixed names out
    of the shared temporary directory, where another user could plant a symlink."""
    global _private_dir
    if _private_dir is None:
        _private_dir = tempfile.mkdtemp(prefix="unitrace_fuzz_")
        atexit.register(shutil.rmtree, _private_dir, True)
    return _private_dir


class TempFile:
    """A file holding data, at one path per process and suffix.

    Used as `with TempFile(data, ".csv") as path:`; the file and path + ".out" (where
    a harness lets a script write its output) are removed on exit. They live in a
    private directory (_private_tmp()), not the shared temporary one. The path is
    reused because the scripts resolve it with pathlib, which interns every distinct
    path for the life of the process: a fresh temporary name per input grows memory
    without bound over millions of inputs.
    """

    def __init__(self, data, suffix=""):
        self.data = data
        self.path = os.path.join(_private_tmp(), f"input{suffix}")

    def __enter__(self):
        with open(self.path, "wb") as f:
            f.write(self.data)
        return self.path

    def __exit__(self, *exc):
        for p in (self.path, self.path + ".out"):
            try:
                os.unlink(p)
            except OSError:
                pass
        return False


def instrument():
    """The context to import the fuzzed scripts in.

    Returns atheris.instrument_imports(), so Atheris gets coverage feedback from the
    scripts, or a no-op context when UNITRACE_FUZZ_REPLAY is set (coverage_replay.py
    under coverage.py, where Atheris' instrumentation would get in the way).
    """
    if os.environ.get("UNITRACE_FUZZ_REPLAY"):
        import contextlib
        return contextlib.nullcontext()
    import atheris
    return atheris.instrument_imports()

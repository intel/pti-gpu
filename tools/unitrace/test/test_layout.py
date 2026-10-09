#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

# Where the test harness finds test binaries and writes its output.
#
# The workloads are built incrementally in the source tree
# (test/cpp_test/build) and installed into the unitrace build directory, so a
# clean unitrace build does not force a rebuild of the tests:
#
#   <unitrace build>/test/bin/<workload>/       one directory per workload: its
#                                               binary and data files (.spv kernels)
#   <unitrace build>/test/output/<group>/logs/     per-scenario logs
#   <unitrace build>/test/output/<group>/results/<test>/  unitrace output files,
#                                               one directory per tests.txt test
#
# <group> is cpp_test or python_test. test_unitrace.py --output-dir replaces
# <unitrace build>/test/output with a directory of the caller's choice, e.g. a
# short one on Windows where unitrace output paths must stay under MAX_PATH.

import os
import platform

_HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_UNITRACE_BUILD_DIR = os.path.normpath(os.path.join(_HERE, "..", "build"))


def exe(name):
    """Executable file name for the current platform."""
    return name + ".exe" if platform.system() == "Windows" else name


def unitrace_build_dir(build_dir=None):
    return os.path.abspath(build_dir) if build_dir else DEFAULT_UNITRACE_BUILD_DIR


def default_build_dir_error(build_dir=None):
    """Error message when no build directory was given and the default
    tools/unitrace/build holds no unitrace (e.g. unitrace was built from the
    repository top level). Installing there would create a stray build/ that
    contains only test/; None when the build directory is usable."""
    if build_dir or os.path.exists(os.path.join(DEFAULT_UNITRACE_BUILD_DIR, exe("unitrace"))):
        return None
    return (f"[ERROR] No unitrace found in the default build directory "
            f"{DEFAULT_UNITRACE_BUILD_DIR}. If unitrace was built elsewhere (e.g. "
            f"the repository top-level build), pass --unitrace-build-dir <that "
            f"build directory>; otherwise build tools/unitrace first.")


def test_root(build_dir=None):
    """Install prefix of the test workloads."""
    return os.path.join(unitrace_build_dir(build_dir), "test")


def bin_dir(build_dir=None):
    return os.path.join(test_root(build_dir), "bin")


def workload_exe(test_bin_dir, name):
    """Installed executable of workload <name>. Each workload installs into its
    own bin/<name>/ so data files with the same name (e.g. two tests shipping a
    gemm.spv) cannot overwrite each other."""
    return os.path.join(test_bin_dir, name, exe(name))


def remove_flat_bin_files(build_dir=None):
    """Delete files left directly in bin/ by the old flat layout. bin/<name>
    used to be a binary and is now a directory, so a stale file there makes
    cmake --install fail; the new layout keeps only directories in bin/."""
    root = bin_dir(build_dir)
    if not os.path.isdir(root):
        return
    for entry in os.listdir(root):
        path = os.path.join(root, entry)
        if os.path.isfile(path) or os.path.islink(path):
            os.remove(path)


def output_dir(build_dir=None, group="cpp_test", root=None):
    """Output directory of a test group, under root when given."""
    base = os.path.abspath(root) if root else os.path.join(test_root(build_dir), "output")
    return os.path.join(base, group)


# Windows paths longer than this fail in unitrace, which is not long-path aware
# (MAX_PATH is 260 including the terminating NUL; directories get 248).
WINDOWS_MAX_PATH = 259


def windows_path_too_long(path):
    """True when path cannot be created on Windows without long-path support."""
    return platform.system() == "Windows" and len(path) > WINDOWS_MAX_PATH


def logs_dir(group_output_dir):
    return os.path.join(group_output_dir, "logs")


def results_dir(group_output_dir, test=None):
    """Results root, or the results directory of one tests.txt test."""
    root = os.path.join(group_output_dir, "results")
    return os.path.join(root, test) if test else root


def metric_scratch_dir(group_output_dir, test=None):
    """Where metric scenarios write before run_test.py moves the files to results."""
    root = os.path.join(group_output_dir, "metric_scratch")
    return os.path.join(root, test) if test else root

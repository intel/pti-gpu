#!/usr/bin/env python3
# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================

"""Build sysmon and check its output in each mode.

Self-contained, like sysmon itself: it needs only Python 3, CMake, a C++
compiler and the Level Zero headers and loader (see ../README.md).

Usage:
  python3 tools/sysmon/test/test_sysmon.py [--build-dir DIR] [--build-type Release|Debug]
                                           [--mode=none|-p|-l|-d ...]

By default sysmon is built in tools/sysmon/build and all modes are checked.
Exit code: 0 when all checked modes pass, 1 otherwise.
"""

import argparse
import os
import re
import subprocess
import sys

SYSMON_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MODES = ["none", "-p", "-l", "-d"]

# Default mode and -p print, for every GPU:
#   GPU <id>: <name>    PCI Bus: ...
#   ...
#   EU Count: <n>    Threads Per EU: <n>    EU SIMD Width: <n>    Total Memory(MB): ...
# The EU values must be positive numbers; "0" or "unknown" means sysmon could
# not read them (GSD-13969: "EU SIMD Width: 0" on BMG).
EU_TOPOLOGY = re.compile(r"EU Count: (\S+)\s+Threads Per EU: (\S+)\s+EU SIMD Width: (\S+)")


def run_process(command, cwd):
    process = subprocess.run(command, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                             encoding="latin-1", check=False)
    return process.returncode, process.stdout, process.stderr


def build(build_dir, build_type):
    """Configure and build sysmon. Returns None on success, the error output otherwise."""
    os.makedirs(build_dir, exist_ok=True)
    steps = [
        ["cmake", "-DCMAKE_BUILD_TYPE=" + build_type, SYSMON_DIR],
        ["cmake", "--build", build_dir, "--config", build_type],
    ]
    for command in steps:
        returncode, stdout, stderr = run_process(command, build_dir)
        if returncode != 0:
            return " ".join(command) + "\n" + stdout + stderr
    return None


def find_executable(build_dir, build_type):
    for candidate in (os.path.join(build_dir, "sysmon"),
                      os.path.join(build_dir, build_type, "sysmon")):
        if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
            return candidate
    return None


def check_short_info(output):
    total_devices = 0
    topology = []
    for line in output.split("\n"):
        if re.match(r"GPU \d+: ", line):
            total_devices += 1
        match = EU_TOPOLOGY.search(line)
        if match:
            topology.append(match.groups())
    if total_devices < 1:
        return "no 'GPU <id>:' line found"
    if len(topology) != total_devices:
        return f"{total_devices} GPUs but {len(topology)} EU topology lines"
    for values in topology:
        for value in values:
            if not value.isdigit() or int(value) < 1:
                return f"EU topology value '{value}' is not a positive number: {values}"
    return None


def check_list(output):
    lines = output.split("\n")
    if not any("Driver" in line for line in lines):
        return "no 'Driver' line found"
    if not any("Device" in line for line in lines):
        return "no 'Device' line found"
    return None


def check_details(output):
    total_eu_count = 0
    for line in output.split("\n"):
        if line.startswith("Total EU Count"):
            total_eu_count += int(line.split(",")[1].strip())
    if total_eu_count < 1:
        return "'Total EU Count' is missing or not positive"
    return None


CHECKS = {"none": check_short_info, "-p": check_short_info, "-l": check_list, "-d": check_details}


def run_mode(executable, mode):
    """Run sysmon in one mode. Returns None on success, a failure description otherwise."""
    command = [executable] if mode == "none" else [executable, mode]
    returncode, stdout, stderr = run_process(command, os.path.dirname(executable))
    if returncode != 0:
        return f"exit code {returncode}\n{stdout}{stderr}"
    if stderr:
        return "unexpected stderr output:\n" + stderr
    if not stdout:
        return "stdout is empty"
    problem = CHECKS[mode](stdout)
    if problem:
        return problem + "\n" + stdout
    return None


def parse_arguments():
    parser = argparse.ArgumentParser(description="Build sysmon and check its output.")
    parser.add_argument("--build-dir", default=os.path.join(SYSMON_DIR, "build"),
                        help="build directory (default: tools/sysmon/build)")
    parser.add_argument("--build-type", default="Release", choices=["Release", "Debug"],
                        help="CMake build type (default: Release)")
    parser.add_argument("--mode", action="append", choices=MODES,
                        help="sysmon mode to check, may be repeated (default: all); "
                             "'none' runs sysmon without options (the default mode, same as -p); "
                             "write options as "
                             "--mode=-l (with '=')")
    return parser.parse_args()


def main():
    args = parse_arguments()
    build_dir = os.path.abspath(args.build_dir)

    sys.stdout.write("Building sysmon (" + args.build_type + ") in " + build_dir + "...")
    sys.stdout.flush()
    log = build(build_dir, args.build_type)
    executable = None if log else find_executable(build_dir, args.build_type)
    if log or executable is None:
        print("FAILED")
        print(log or "sysmon executable not found in " + build_dir)
        return 1
    print("done")

    passed = 0
    failed = 0
    for mode in args.mode or MODES:
        label = "sysmon" if mode == "none" else "sysmon " + mode
        sys.stdout.write("Running " + label + "...")
        sys.stdout.flush()
        problem = run_mode(executable, mode)
        if problem:
            print("FAILED")
            print(problem)
            failed += 1
        else:
            print("PASSED")
            passed += 1

    print("PASSED: " + str(passed) + " / FAILED: " + str(failed))
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())

# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================
"""Smoke test for the pc_sampling_profiler sample.

Runs the launcher on the bundled GEMM workload and checks that a stall report with
samples was produced. Exits with 77 (CTest SKIP_RETURN_CODE) when the platform does
not offer EU stall sampling, for example when the perf stream is restricted.
"""

import os
import re
import subprocess
import sys
import tempfile

SKIP_EXIT_CODE = 77
TIMEOUT_S = 300

ENVIRONMENT_FAILURES = {
    "PTI_ERROR_PC_SAMPLING_UNSUPPORTED": "no supported GPU/driver or sampling permission",
    "PTI_ERROR_PC_SAMPLING_CONFIGURATION_FAIL": "no supported sampling device",
}


def environment_skip_reason(output):
    for line in output.splitlines():
        match = re.search(
            r"Failed to (?:enable|configure|start) PC sampling(?: collection)?: "
            r"(PTI_ERROR_PC_SAMPLING_[A-Z_]+)(?: \(\d+\))?",
            line,
        )
        if match and match.group(1) in ENVIRONMENT_FAILURES:
            return f"{match.group(1)}: {ENVIRONMENT_FAILURES[match.group(1)]}"
    return None


def run_check(launcher, workload, module=None, expected_output=None, kernel_name=None):
    with tempfile.TemporaryDirectory() as work_dir:
        output_path = os.path.join(work_dir, "stalls.txt")
        command = [launcher, "--output", output_path, "--", workload]
        if module is None:
            command += ["--size", "2048", "--repeat", "3"]
        else:
            command.append(module)
        completed = subprocess.run(
            command,
            capture_output=True,
            text=True,
            timeout=TIMEOUT_S,
            check=False,
        )
        sys.stdout.write(completed.stdout)
        sys.stderr.write(completed.stderr)
        if module is not None and "no Level Zero GPU found" in completed.stderr:
            return SKIP_EXIT_CODE
        if expected_output is None:
            expected_output = (
                "Level Zero GEMM: 8 launches, valid output"
                if module is not None
                else "GEMM results are CORRECT"
            )
        if completed.returncode != 0 or expected_output not in completed.stdout:
            if completed.returncode == SKIP_EXIT_CODE:
                return SKIP_EXIT_CODE
            print(f"FAILED: workload exited with {completed.returncode}")
            return 1
        if not os.path.exists(output_path):
            print("FAILED: workload produced no profiler report")
            return 1
        with open(output_path, "r", encoding="utf-8", errors="replace") as report_file:
            report = report_file.read()
        skip_reason = environment_skip_reason(
            completed.stdout + completed.stderr + report
        )
        if skip_reason:
            print(f"PC sampling unavailable on this system ({skip_reason}), skipping")
            return SKIP_EXIT_CODE
    if kernel_name is None and module is not None:
        kernel_name = "]: GEMM"
    return check_report(report, kernel_name)


def main():
    if len(sys.argv) == 4 and sys.argv[1] == "--sycl":
        return run_check(sys.argv[2], sys.argv[3], kernel_name="GEMMKernel")
    if len(sys.argv) == 5 and sys.argv[1] == "--pytorch":
        return run_check(
            sys.argv[2],
            sys.argv[3],
            sys.argv[4],
            expected_output="PyTorch XPU training:",
            kernel_name="",
        )
    if len(sys.argv) == 4:
        return run_check(*sys.argv[1:])
    print(
        f"Usage: {sys.argv[0]} <pc_sampling_profiler> <workload> <module.spv>\n"
        f"       {sys.argv[0]} --sycl <pc_sampling_profiler> <workload>\n"
        f"       {sys.argv[0]} --pytorch <pc_sampling_profiler> <python> <script>"
    )
    return 1


def check_report(report, kernel_name):
    expected_entries = (
        "=== Initializing Profiling Environment ===",
        "PC Sampling collection started successfully",
        "PC Sampling collection stopped successfully",
        "PC Sampling disabled and resources freed successfully",
        "=== PC Sampling Stall Data ===",
        "Stall reasons (",
        "Kernels with samples:",
        "Per-instruction stall samples:",
    )
    if kernel_name:
        expected_entries += (kernel_name,)
    for expected in expected_entries:
        if expected not in report:
            print(f"FAILED: '{expected}' missing from the report")
            return 1

    total_samples = 0
    for line in report.splitlines():
        if "Total samples collected" in line:
            total_samples += int(line.split(":")[1].strip())
    if total_samples == 0:
        print("FAILED: report contains no samples")
        return 1

    if not kernel_name and not re.search(
        r"^\s*Kernel \[\d+/\d+\]:\s+.+$", report, re.MULTILINE
    ):
        print("FAILED: report contains no sampled kernels")
        return 1

    print(f"PASSED: {total_samples} samples collected")
    return 0


if __name__ == "__main__":
    sys.exit(main())

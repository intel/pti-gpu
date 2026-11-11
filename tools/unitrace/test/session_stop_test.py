# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

# Stop a named session with `unitrace --stop` while the profiled application
# still has GPU work in flight; the application must survive and report the
# collected data. The stop is flushed by the watcher thread outside any tracing
# callback, where the collector's own API calls used to be traced and re-lock
# its mutex on the same thread. Timing dependent, so repeated.
#
# usage: session_stop_test.py <unitrace> <bin-dir> [--iterations N]
#                             [--stop-after SECONDS] [--repeat-count N]

import argparse
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import test_layout  # noqa: E402

# Seconds a run may take to finish after --stop.
RUN_TIMEOUT = 600

# What a non-empty -d report contains.
REPORT_HEADER = "=== Device Timing Summary ==="
REPORT_KERNEL = '"GEMM"'

# Exit status for an inconclusive run; the harness reports Skipped.
SKIPPED_RETURN_CODE = 77

# The failure signature this test guards against.
CRASH_MARKERS = ("terminate called", "Resource deadlock avoided", "Aborted")

# The application ended before --stop: the GPU is too fast for --repeat-count.
INCONCLUSIVE = "finished before --stop was sent"


def fail(msg):
    print(f"[ERROR] {msg}", file=sys.stderr)
    sys.exit(1)


def run_once(unitrace, workload, args, session, stop_after):
    """Runs one profiled workload, stops the session after stop_after seconds
    and returns (output, error); error is None on success and INCONCLUSIVE if
    the application finished before --stop was sent."""
    command = [unitrace, "--session", session, "-d", workload] + args
    print(f"[INFO] {' '.join(command)}")
    proc = subprocess.Popen(command, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True)
    try:
        output, _ = proc.communicate(timeout=stop_after)
        return output, INCONCLUSIVE
    except subprocess.TimeoutExpired:
        pass

    stop = subprocess.run([unitrace, "--stop", session], capture_output=True, text=True)
    if stop.returncode != 0:
        proc.kill()
        proc.communicate()
        return stop.stdout + stop.stderr, f"--stop exited ({stop.returncode})"

    try:
        output, _ = proc.communicate(timeout=RUN_TIMEOUT)
    except subprocess.TimeoutExpired:
        proc.kill()
        output, _ = proc.communicate()
        return output, f"did not exit within {RUN_TIMEOUT}s after --stop"

    if proc.returncode != 0:
        return output, f"exited ({proc.returncode}) after --stop"
    for marker in CRASH_MARKERS:
        if marker in output:
            return output, f"output contains {marker!r}"
    if REPORT_HEADER not in output:
        return output, f"report missing {REPORT_HEADER!r}"
    if REPORT_KERNEL not in output:
        return output, f"report has no {REPORT_KERNEL} row"
    return output, None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("unitrace")
    parser.add_argument("bin_dir")
    parser.add_argument("--iterations", type=int, default=20,
                        help="how many stop-while-running rounds to run (default: 20)")
    parser.add_argument("--stop-after", type=float, default=1.5,
                        help="seconds between launching the app and --stop (default: 1.5)")
    parser.add_argument("--repeat-count", type=int, default=1500,
                        help="ze_gemm repeat count; must outlast --stop-after (default: 1500)")
    opts = parser.parse_args()

    workload = test_layout.workload_exe(opts.bin_dir, "ze_gemm")
    if not os.path.exists(workload):
        fail(f"workload not found: {workload}")
    if not os.path.exists(opts.unitrace):
        fail(f"unitrace not found: {opts.unitrace}")

    args = ["1024", str(opts.repeat_count)]
    failures = []
    for i in range(opts.iterations):
        # Session names must be alphanumeric; unique per process and round.
        session = f"sstop{os.getpid()}r{i}"
        start = time.monotonic()
        output, error = run_once(opts.unitrace, workload, args, session, opts.stop_after)
        elapsed = time.monotonic() - start
        if error is INCONCLUSIVE:
            print(f"[WARNING] iteration {i}: {error} ({elapsed:.1f}s); "
                  f"raise --repeat-count or lower --stop-after for this GPU")
            sys.exit(SKIPPED_RETURN_CODE)
        if error:
            failures.append((i, error))
            print(f"[ERROR] iteration {i}: {error} ({elapsed:.1f}s)\n--- output ---\n{output}",
                  file=sys.stderr)
        else:
            print(f"[INFO] iteration {i}: ok ({elapsed:.1f}s)")

    if failures:
        fail(f"{len(failures)}/{opts.iterations} iterations failed: "
             + "; ".join(f"#{i}: {e}" for i, e in failures))
    print(f"[INFO] {opts.iterations}/{opts.iterations} stop-while-running iterations passed")


if __name__ == "__main__":
    main()

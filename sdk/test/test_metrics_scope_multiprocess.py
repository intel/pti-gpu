#!/usr/bin/env python3
# ============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# ============================================================
#
# Multi-process Metrics Scope test driver. Spawns two instances of the
# metrics_scope sample concurrently, both running in auto-detect mode
# (--devices=auto) but pinning the workload to different GPUs (--workload=0
# and --workload=1). This exercises the auto-mode lazy profiler construction
# path under genuine cross-process concurrency. Asserts that:
#   1. both processes exited successfully,
#   2. each captured exactly --repeat kernel records (one per GEMM repetition)
#      and both workers produced the same record count,
#   3. the wall-clock collection windows reported by the two processes overlap
#      (i.e. they were collecting metrics at the same time).
#
# Skip (CTest SKIP_RETURN_CODE = 77) when the host has fewer than 2 L0 GPUs.

import argparse
import re
import subprocess
import sys
from typing import List, Optional, Tuple

PTI_MP_PREFIX = "[PTI_MP] "
GPU_COUNT_KEY = "gpu_count"
WINDOW_START_KEY = "collection_window_start_ns"
WINDOW_STOP_KEY = "collection_window_stop_ns"
# 77 is the GNU Autotools / CTest convention for "test skipped".
SKIP_EXIT = 77


def parse_pti_mp(stdout: str) -> dict:
    out = {}
    for line in stdout.splitlines():
        if not line.startswith(PTI_MP_PREFIX):
            continue
        kv = line[len(PTI_MP_PREFIX) :]
        if "=" not in kv:
            continue
        key, val = kv.split("=", 1)
        out[key.strip()] = val.strip()
    return out


def count_collection_buffers(stdout: str) -> int:
    m = re.search(r"Number of collection buffers used:\s*(\d+)", stdout)
    return int(m.group(1)) if m else 0


_KERNEL_HDR = re.compile(r"^\s*Kernel\s+(\d+):\s*$")
_FIELD = re.compile(r"^\s+([^:]+?):\s*(.+?)\s*$")


def parse_kernel_records(stdout: str) -> List[dict]:
    # Walk the sample's "Kernel N:" / "      <field>: <value>" blocks. A
    # blank line ends a record. We keep just ID + Kernel Name + metric values.
    records: List[dict] = []
    cur: Optional[dict] = None
    for line in stdout.splitlines():
        m = _KERNEL_HDR.match(line)
        if m:
            if cur is not None:
                records.append(cur)
            cur = {"index": int(m.group(1)), "fields": {}}
            continue
        if cur is None:
            continue
        if line.strip() == "":
            records.append(cur)
            cur = None
            continue
        fm = _FIELD.match(line)
        if fm:
            cur["fields"][fm.group(1)] = fm.group(2)
    if cur is not None:
        records.append(cur)
    return records


def short_kernel_name(name: str) -> str:
    # Sample logs the full mangled SYCL kernel name; the trailing "::__GEMM_*"
    # is the useful bit.
    last = name.rsplit("::", 1)[-1]
    return last if last else name


def probe_gpu_count(binary: str) -> int:
    proc = subprocess.run(
        [
            binary,
            "--devices",
            "auto",
            "--workload",
            "auto",
            "--size",
            "32",
            "--repeat",
            "1",
        ],
        capture_output=True,
        text=True,
        timeout=120,
    )
    info = parse_pti_mp(proc.stdout)
    if GPU_COUNT_KEY not in info:
        print(
            f"[driver] probe could not parse {GPU_COUNT_KEY} from sample output. "
            f"rc={proc.returncode}",
            file=sys.stderr,
        )
        print("---- probe stdout ----", file=sys.stderr)
        print(proc.stdout, file=sys.stderr)
        print("---- probe stderr ----", file=sys.stderr)
        print(proc.stderr, file=sys.stderr)
        return 0
    try:
        return int(info[GPU_COUNT_KEY])
    except ValueError:
        return 0


def spawn_worker(
    binary: str, workload_index: int, size: int, repeat: int
) -> subprocess.Popen:
    cmd = [
        binary,
        "--devices",
        "auto",
        "--workload",
        str(workload_index),
        "--size",
        str(size),
        "--repeat",
        str(repeat),
    ]
    return subprocess.Popen(
        cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
    )


def windows_overlap(
    a_start: int, a_stop: int, b_start: int, b_stop: int
) -> Tuple[bool, int]:
    overlap_start = max(a_start, b_start)
    overlap_stop = min(a_stop, b_stop)
    return overlap_start < overlap_stop, max(0, overlap_stop - overlap_start)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--sample-binary", required=True, help="Path to the metrics_scope executable"
    )
    parser.add_argument(
        "--size",
        type=int,
        default=2048,
        help="Matrix size; larger keeps the collection window open longer",
    )
    parser.add_argument(
        "--repeat",
        type=int,
        default=4,
        help="Per-device GEMM repetitions; larger widens the collection window",
    )
    parser.add_argument(
        "--timeout",
        type=int,
        default=60,
        help="Per-process wall-clock timeout in seconds; on expiry the "
        "worker is killed and the test fails",
    )
    args = parser.parse_args()

    gpu_count = probe_gpu_count(args.sample_binary)
    if gpu_count < 2:
        print(f"[driver] need >= 2 GPUs, found {gpu_count}; skipping", file=sys.stderr)
        return SKIP_EXIT

    procs = [
        spawn_worker(
            args.sample_binary, workload_index=0, size=args.size, repeat=args.repeat
        ),
        spawn_worker(
            args.sample_binary, workload_index=1, size=args.size, repeat=args.repeat
        ),
    ]
    workload_indices = [0, 1]

    outs, errs, rcs = [], [], []
    failed = False
    for w_id, p in enumerate(procs):
        try:
            stdout, stderr = p.communicate(timeout=args.timeout)
        except subprocess.TimeoutExpired:
            p.kill()
            stdout, stderr = p.communicate()
            print(
                f"[driver] worker {w_id} timed out after {args.timeout}s",
                file=sys.stderr,
            )
            failed = True
        outs.append(stdout)
        errs.append(stderr)
        rcs.append(p.returncode)

    info = [parse_pti_mp(s) for s in outs]
    buffers = [count_collection_buffers(s) for s in outs]
    records = [parse_kernel_records(s) for s in outs]

    starts: Optional[List[int]] = None
    stops: Optional[List[int]] = None
    try:
        starts = [int(info[w][WINDOW_START_KEY]) for w in range(2)]
        stops = [int(info[w][WINDOW_STOP_KEY]) for w in range(2)]
    except (KeyError, ValueError) as e:
        print(
            f"[driver] failed to parse collection window markers: {e}", file=sys.stderr
        )
        failed = True

    for w_id in range(2):
        wl_handles = [
            v for k, v in info[w_id].items() if k.startswith("workload_device_handle")
        ]
        wl_handle_str = ", ".join(wl_handles) if wl_handles else "?"
        print(f"\n[driver] -- worker {w_id} ---------------------------------------")
        print(f"[driver]   exit_code        = {rcs[w_id]}")
        print(f"[driver]   profiled device  = auto")
        print(
            f"[driver]   workload device  = {workload_indices[w_id]} (handle {wl_handle_str})"
        )
        if starts is not None and stops is not None:
            duration_ms = (stops[w_id] - starts[w_id]) / 1e6
            print(
                f"[driver]   window           = {starts[w_id]} -> {stops[w_id]} "
                f"({duration_ms:.1f} ms)"
            )
        print(f"[driver]   buffers          = {buffers[w_id]}")
        print(f"[driver]   records          = {len(records[w_id])}")
        for rec in records[w_id]:
            f = rec["fields"]
            kid = f.get("ID", "?")
            kname = short_kernel_name(f.get("Kernel Name", ""))
            metrics = {k: v for k, v in f.items() if k not in {"ID", "Kernel Name"}}
            metric_str = "  ".join(f"{k}={v}" for k, v in metrics.items())
            print(
                f"[driver]     kernel {rec['index']}  ID={kid}  {kname}  {metric_str}"
            )

    for w_id, rc in enumerate(rcs):
        if rc != 0:
            print(f"\n[driver] worker {w_id} exited with {rc}", file=sys.stderr)
            failed = True
    expected_records = args.repeat
    for w_id, n in enumerate(records):
        if len(n) != expected_records:
            print(
                f"[driver] worker {w_id} collected {len(n)} records, expected "
                f"{expected_records}",
                file=sys.stderr,
            )
            failed = True
    if len(records[0]) != len(records[1]):
        print(
            f"[driver] worker record counts differ: {len(records[0])} vs "
            f"{len(records[1])}",
            file=sys.stderr,
        )
        failed = True

    if starts is not None and stops is not None:
        overlap_ok, overlap_ns = windows_overlap(
            starts[0], stops[0], starts[1], stops[1]
        )
        overlap_ms = overlap_ns / 1_000_000.0
        print(
            f"\n[driver] overlap = {overlap_ms:.1f} ms"
            + ("" if overlap_ok else "  [WORKERS DID NOT RUN IN PARALLEL]")
        )
        if not overlap_ok:
            failed = True

    if failed:
        for w_id in range(2):
            print(
                f"\n========== worker {w_id} stdout (rc={rcs[w_id]}) ==========",
                file=sys.stderr,
            )
            print(outs[w_id], file=sys.stderr)
            if errs[w_id].strip():
                print(f"========== worker {w_id} stderr ==========", file=sys.stderr)
                print(errs[w_id], file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())

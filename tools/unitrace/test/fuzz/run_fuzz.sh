#!/bin/bash
# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================
#
# Runs fuzzing campaigns for unitrace (SDL505).
#
# Usage:
#   ./run_fuzz.sh [-t seconds] [-s seed] [-o out_dir] [-b build_dir] [-m mb]
#              [target...]
#
#   -t  duration per target in seconds (default 3600)
#   -s  libFuzzer/Atheris random seed (default 1), recorded for reproduction
#   -o  output directory (default results/campaign); a target directory that already
#       exists (an earlier or concurrent run) is refused, so a run's artifacts, log
#       and run.json stay together
#   -b  directory holding the C++ fuzz_* binaries (default ../../build/test/fuzz/cpp)
#   -m  RSS limit in MB (default 2048). Atheris' instrumentation grows the Python
#       process by ~1.7 KB per input, so hour-long Python runs may need 4096.
#   target  a target name, as in fuzz_<name> under cpp/ and python/ (e.g. elf64,
#           kmd_trace); default: all, run in parallel
#
# Exits non-zero if any campaign failed (a crash, a missing binary, or its run.json
# could not be written).
#
# Each target gets <out>/<target>/ with:
#   corpus/     the evolving corpus (starts from corpus/<target>/ and regression/<target>/)
#   artifacts/  crash-*, oom-*, timeout-* reproducers
#   fuzz.log    full fuzzer output, including coverage and final statistics
#   run.json    command line, seed, duration, start/end time, exit status

set -u
FUZZ_DIR="$(cd "$(dirname "$0")" && pwd)"
DURATION=3600
SEED=1
OUT="$FUZZ_DIR/results/campaign"
BUILD_DIR="$FUZZ_DIR/../../build/test/fuzz/cpp"
RSS_MB=2048

while getopts "t:s:o:b:m:" opt; do
  case $opt in
    t) DURATION=$OPTARG ;;
    s) SEED=$OPTARG ;;
    o) OUT=$OPTARG ;;
    b) BUILD_DIR=$OPTARG ;;
    m) RSS_MB=$OPTARG ;;
    *) sed -n '10,23p' "$0" >&2; exit 2 ;;
  esac
done
shift $((OPTIND - 1))

# The targets are the harnesses in cpp/ and python/.
CPP_TARGETS=()
for f in "$FUZZ_DIR"/cpp/fuzz_*.cc; do t="${f##*/fuzz_}"; CPP_TARGETS+=("${t%.cc}"); done
PY_TARGETS=()
for f in "$FUZZ_DIR"/python/fuzz_*.py; do
  t="${f##*/fuzz_}"; t="${t%.py}"; [[ $t == common ]] || PY_TARGETS+=("$t")
done
TARGETS=("$@")
[[ ${#TARGETS[@]} -eq 0 ]] && TARGETS=("${CPP_TARGETS[@]}" "${PY_TARGETS[@]}")

# Succeeds if target $1 is a C++ (libFuzzer) target, fails for a Python (Atheris)
# one.
is_cpp() { [[ " ${CPP_TARGETS[*]} " == *" $1 "* ]]; }

# Runs one campaign and returns its fuzzer's exit status: fuzzes target $1 for
# $DURATION seconds with seed $SEED, from a corpus seeded with corpus/<target> and
# regression/<target>. Writes
# $OUT/<target>/: the evolved corpus/, reproducers in artifacts/ (crash-*, oom-*,
# timeout-*), the fuzzer output in fuzz.log and run.json (command, seed, duration,
# start and end time, exit status).
run_target() {
  local t=$1 dir="$OUT/$1" cmd
  # A directory holds one run: its artifacts, log and run.json describe the same run
  # (make_evidence.py reads them together). mkdir reserves it atomically, so an
  # earlier run, a target given twice, or another campaign writing to the same -o
  # directory is refused instead of overwriting it.
  mkdir -p "$OUT"
  if ! mkdir "$dir" 2>/dev/null; then
    echo "[ERROR] $dir already exists (an earlier or concurrent run); use another -o directory"
    return 1
  fi
  mkdir -p "$dir/corpus" "$dir/artifacts"
  for seeds in "$FUZZ_DIR/corpus/$t" "$FUZZ_DIR/regression/$t"; do
    [[ -d "$seeds" ]] && cp -n "$seeds"/* "$dir/corpus/" 2>/dev/null
  done
  local flags=(-max_total_time="$DURATION" -seed="$SEED" -timeout=10 -rss_limit_mb="$RSS_MB"
               -malloc_limit_mb=1024 -print_final_stats=1 -close_fd_mask=3
               -artifact_prefix="$dir/artifacts/")
  if is_cpp "$t"; then
    cmd=("$BUILD_DIR/fuzz_$t" "${flags[@]}" "$dir/corpus")
  else
    cmd=(python3 "$FUZZ_DIR/python/fuzz_$t.py" "${flags[@]}" "$dir/corpus")
  fi
  local start; start=$(date -Iseconds)
  "${cmd[@]}" > "$dir/fuzz.log" 2>&1
  local status=$?
  python3 - "$dir/run.json" "$t" "$SEED" "$DURATION" "$start" "$(date -Iseconds)" "$status" "${cmd[@]}" <<'PY'
import json, sys
path, target, seed, duration, start, end, status, *cmd = sys.argv[1:]
json.dump({"target": target, "seed": int(seed), "duration_s": int(duration), "start": start,
           "end": end, "exit_status": int(status), "command": cmd}, open(path, "w"), indent=2)
PY
  # Without its run.json the run is no evidence, whatever the fuzzer returned.
  if [[ $? -ne 0 ]]; then
    echo "[ERROR] $t: cannot write $dir/run.json"
    return 1
  fi
  echo "[DONE] $t exit=$status"
  return $status
}

mkdir -p "$OUT"
pids=()
for t in "${TARGETS[@]}"; do
  run_target "$t" &
  pids+=($!)
done
# A campaign that crashed, or whose binary is missing, makes the script fail.
failed=0
for pid in "${pids[@]}"; do
  wait "$pid" || failed=1
done
exit $failed

#!/bin/bash
# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================
#
# Measures the code coverage a campaign's corpora reach in the fuzzed product code.
#
# Usage: ./coverage_report.sh [campaign_dir] [out_dir]
#   campaign_dir  output of run_fuzz.sh: <dir>/<target>/corpus (default results/campaign);
#                 a target the campaign did not run is measured on corpus/<target>
#   out_dir       default results/coverage
#
# C++: each harness is rebuilt with clang source-based coverage and its corpus and
#      regression inputs replayed (-runs=0); llvm-cov reports the product sources
#      (src/), not the harnesses. Each binary carries an empty stub for every header
#      function it does not call, which a profile or report merged across binaries
#      mixes with the real records ("functions have mismatched data"), so each target
#      is measured on its own and a line, branch or function counts as covered if any
#      target covers it.
# Python: coverage.py (lines and branches) over coverage_replay.py, restricted to the
#      fuzzed scripts; lines and branches are reported separately, their mix as
#      total_percent.
# Writes cpp_report.txt, cpp_coverage.json, lcov/, html_cpp/<target>/,
# python_report.txt, python_coverage.json, html_python/ and coverage_summary.json.

set -eu
FUZZ_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$FUZZ_DIR/../.." && pwd)"
SRC="$ROOT/src"
SCRIPTS="$ROOT/scripts"
CAMPAIGN="$(cd "${1:-$FUZZ_DIR/results/campaign}" && pwd)"
OUT="${2:-$FUZZ_DIR/results/coverage}"
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
BIN="$OUT/bin"
PROF="$OUT/profraw"
rm -rf "$BIN" "$PROF" "$OUT/lcov" "$OUT/html_cpp" "$OUT/html_python" "$OUT"/.coverage*
mkdir -p "$BIN" "$PROF" "$OUT/lcov" "$OUT/html_cpp"

LLVM_PROFDATA=$(command -v llvm-profdata || command -v llvm-profdata-18)
LLVM_COV=$(command -v llvm-cov || command -v llvm-cov-18)

# The targets are the harnesses in cpp/ and python/.
CPP_TARGETS=()
for f in "$FUZZ_DIR"/cpp/fuzz_*.cc; do t="${f##*/fuzz_}"; CPP_TARGETS+=("${t%.cc}"); done
PY_TARGETS=()
for f in "$FUZZ_DIR"/python/fuzz_*.py; do
  t="${f##*/fuzz_}"; t="${t%.py}"; [[ $t == common ]] || PY_TARGETS+=("$t")
done
# Files that are parsers throughout: measured whole.
CPP_SOURCES=("$SRC/utils/elf64_reader.h" "$SRC/utils/kernel_extractor.h"
             "$SRC/utils/source_resolver.h" "$SRC/utils/cli_options.h")
# Files that hold parsers next to Level Zero code: only the parser functions are
# measured (the rest needs a GPU and is out of scope).
CPP_MIXED_SOURCES=("$SRC/levelzero/ze_metrics.h" "$SRC/levelzero/ze_collector.h" "$SRC/utils/utils_ze.h"
                   "$SRC/utils/utils_host.h" "$SRC/chromelogger.h" "$SRC/unitrace.cc")
CPP_PARSER_FUNCTIONS="SplitFilterLine|ParseFilterList|ReadMetricQueryRecord|ReadMetricSegment|\
ReadKernelPropsRecord|ReadKernelTimeRecord|CalculatedSamplesFit|UnwrapTimestamp|ParseDeviceList|\
ParseRankList|ParseDeviceIndex|DevicesStringToSet|JsonEscape|JsonString|Utf8SequenceLength|IsDigits|\
ToInt|ToUint32|ParseChromeEventBufferSize|UnquoteName|JsonNameField|IsDecimal|ParseKmdLine|ReadKmdEvent|\
KmdEventToJson|StringifyJsonArray"

# The input directories of target $1, one per line: its campaign corpus, or the
# checked-in corpus/<target> when the campaign did not run it (a campaign on selected
# targets), and regression/<target> if there is one. Only existing directories.
inputs() {
  local dirs=()
  if [[ -d "$CAMPAIGN/$1/corpus" ]]; then
    dirs+=("$CAMPAIGN/$1/corpus")
  elif [[ -d "$FUZZ_DIR/corpus/$1" ]]; then
    dirs+=("$FUZZ_DIR/corpus/$1")
  fi
  [[ -d "$FUZZ_DIR/regression/$1" ]] && dirs+=("$FUZZ_DIR/regression/$1")
  printf '%s\n' "${dirs[@]}"
}

# The harnesses include the unitrace sources, so they build with unitrace.
cmake -S "$ROOT" -B "$BIN/build" -DUNITRACE_BUILD_FUZZ=ON -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -DUNITRACE_FUZZ_SANITIZERS=fuzzer \
  -DUNITRACE_FUZZ_EXTRA_FLAGS="-O0 -fprofile-instr-generate -fcoverage-mapping" -DBUILD_WITH_OMP=0 -DBUILD_WITH_PERFETTO=OFF ${UNITRACE_CMAKE_ARGS:-} \
  > "$OUT/build.log" 2>&1
cmake --build "$BIN/build" --target unitrace_fuzz >> "$OUT/build.log" 2>&1

for t in "${CPP_TARGETS[@]}"; do
  cp "$BIN/build/test/fuzz/cpp/fuzz_$t" "$BIN/fuzz_$t"
  mapfile -t dirs < <(inputs "$t")
  LLVM_PROFILE_FILE="$PROF/$t.profraw" "$BIN/fuzz_$t" -runs=0 -close_fd_mask=3 \
    -rss_limit_mb=2048 -malloc_limit_mb=1024 "${dirs[@]}" > "$OUT/replay_$t.log" 2>&1
  "$LLVM_PROFDATA" merge -sparse "$PROF/$t.profraw" -o "$OUT/lcov/$t.profdata"
  "$LLVM_COV" export -format=lcov "$BIN/fuzz_$t" -instr-profile="$OUT/lcov/$t.profdata" \
    "${CPP_SOURCES[@]}" "${CPP_MIXED_SOURCES[@]}" > "$OUT/lcov/$t.info"
  # Per-function line ranges, to keep only the parser functions of the mixed files.
  "$LLVM_COV" export -format=text -skip-expansions "$BIN/fuzz_$t" -instr-profile="$OUT/lcov/$t.profdata" \
    "${CPP_MIXED_SOURCES[@]}" > "$OUT/lcov/$t.json"
  "$LLVM_COV" show "$BIN/fuzz_$t" -instr-profile="$OUT/lcov/$t.profdata" -format=html \
    -output-dir="$OUT/html_cpp/$t" "${CPP_SOURCES[@]}" "${CPP_MIXED_SOURCES[@]}"
done

PARSER_FUNCTIONS="$CPP_PARSER_FUNCTIONS" MIXED_SOURCES="${CPP_MIXED_SOURCES[*]}" \
python3 - "$OUT" "$ROOT" "$("$LLVM_COV" --version | grep -o 'version [0-9.]*')" "${CPP_TARGETS[@]}" <<'PY'
import collections, json, os, re, sys
out, root, version, targets = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4:]
parser_re = re.compile(os.environ["PARSER_FUNCTIONS"])
mixed = {os.path.relpath(p, root) for p in os.environ["MIXED_SOURCES"].split()}
# In the mixed files only the lines inside a parser function count.
allowed = collections.defaultdict(set)     # file -> lines of parser functions
for t in targets:
    for fn in json.load(open(os.path.join(out, "lcov", t + ".json")))["data"][0]["functions"]:
        if not parser_re.search(fn["name"]):
            continue
        for r in fn["regions"]:
            f = os.path.relpath(fn["filenames"][r[5]], root)
            if f in mixed:
                allowed[f].update(range(r[0], r[2] + 1))
def keep(f, ln):
    return f not in mixed or ln in allowed[f]
lines = collections.defaultdict(dict)      # file -> line -> max count over targets
branches = collections.defaultdict(dict)   # file -> (line, block, branch) -> max taken
functions = collections.defaultdict(dict)  # file -> mangled name -> max count
for t in targets:
    f = None
    for rec in open(os.path.join(out, "lcov", t + ".info")):
        key, _, val = rec.strip().partition(":")
        if key == "SF":
            f = os.path.relpath(val, root)
        elif key == "DA":
            ln, cnt = val.split(",")[:2]
            if not keep(f, int(ln)):
                continue
            lines[f][int(ln)] = max(lines[f].get(int(ln), 0), int(cnt))
        elif key == "BRDA":
            ln, blk, br, taken = val.split(",")
            if not keep(f, int(ln)):
                continue
            k = (int(ln), blk, br)
            branches[f][k] = max(branches[f].get(k, 0), 0 if taken == "-" else int(taken))
        elif key == "FNDA":
            cnt, name = val.split(",", 1)
            if f in mixed and not parser_re.search(name):
                continue
            functions[f][name] = max(functions[f].get(name, 0), int(cnt))
pct = lambda h, n: round(100.0 * h / n, 2) if n else 100.0
hit = lambda d: [sum(1 for c in d.values() if c > 0), len(d)]
files = {f: {"lines": hit(lines[f]), "branches": hit(branches[f]), "functions": hit(functions[f])}
         for f in sorted(lines)}
tot = {k: [sum(v[k][0] for v in files.values()), sum(v[k][1] for v in files.values())]
       for k in ("lines", "branches", "functions")}
json.dump({"tool": f"llvm-cov {version}", "method": "union over targets of per-target llvm-cov LCOV exports",
           "files": files, "totals": tot}, open(os.path.join(out, "cpp_coverage.json"), "w"), indent=2)
with open(os.path.join(out, "cpp_report.txt"), "w") as o:
    o.write(f"C++ product code coverage (llvm-cov {version}): union over the {len(targets)} targets of each\n"
            "target's own profile, covered hits / instrumented total.\n\n")
    o.write(f"{'File':34} {'Lines':>20} {'Branches':>20} {'Functions':>20}\n")
    for f, v in list(files.items()) + [("TOTAL", tot)]:
        cell = lambda k: f"{v[k][0]}/{v[k][1]} {pct(*v[k]):6.2f}%"
        o.write(f"{f:34} {cell('lines'):>20} {cell('branches'):>20} {cell('functions'):>20}\n")
PY

PY_SOURCES="$SCRIPTS/metrics/analyzeperfmetrics.py,$SCRIPTS/metrics/perfdashboard.py,$SCRIPTS/tracemerge/mergetrace.py,$SCRIPTS/summary/summary.py,$SCRIPTS/summary/categorize.py,$SCRIPTS/roofline/roofline_libs.py"
for t in "${PY_TARGETS[@]}"; do
  mapfile -t dirs < <(inputs "$t")
  (cd "$OUT" && UNITRACE_FUZZ_REPLAY=1 COVERAGE_FILE="$OUT/.coverage.$t" \
     python3 -m coverage run --branch --include="$PY_SOURCES" \
     "$FUZZ_DIR/python/coverage_replay.py" "$t" "${dirs[@]}" > "$OUT/replay_$t.log" 2>&1)
done
(cd "$OUT" && python3 -m coverage combine --keep .coverage.* > /dev/null &&
 python3 -m coverage report > python_report.txt &&
 python3 -m coverage json -o python_coverage.json > /dev/null &&
 python3 -m coverage html -d html_python > /dev/null)

python3 - "$OUT" "$ROOT" <<'PY'
import json, os, sys
out, root = sys.argv[1], sys.argv[2]
cpp = json.load(open(os.path.join(out, "cpp_coverage.json")))
py = json.load(open(os.path.join(out, "python_coverage.json")))
pct = lambda h, n: round(100.0 * h / n, 2) if n else 100.0
summary = {
    "cpp": {
        "tool": cpp["tool"],
        "method": cpp["method"],
        "files": {f: {"lines_percent": pct(*v["lines"]), "branches_percent": pct(*v["branches"]),
                      "functions_percent": pct(*v["functions"])} for f, v in cpp["files"].items()},
        "total_lines_percent": pct(*cpp["totals"]["lines"]),
        "total_branches_percent": pct(*cpp["totals"]["branches"]),
        "total_functions_percent": pct(*cpp["totals"]["functions"]),
    },
    "python": {
        "tool": "coverage.py " + py["meta"]["version"] + " (lines and branches)",
        # percent_covered mixes statements and branches; lines and branches are
        # reported on their own, the mix as total_percent. A relative filename is
        # relative to out, where coverage json ran; join keeps an absolute one.
        "files": {os.path.relpath(os.path.join(out, k), root): {
                      "lines_percent": pct(v["summary"]["covered_lines"], v["summary"]["num_statements"]),
                      "branches_percent": pct(v["summary"].get("covered_branches", 0), v["summary"].get("num_branches", 0))}
                  for k, v in py["files"].items()},
        "total_lines_percent": pct(py["totals"]["covered_lines"], py["totals"]["num_statements"]),
        "total_branches_percent": pct(py["totals"].get("covered_branches", 0), py["totals"].get("num_branches", 0)),
        "total_percent": round(py["totals"]["percent_covered"], 2),
    },
}
json.dump(summary, open(os.path.join(out, "coverage_summary.json"), "w"), indent=2)
print(json.dumps(summary, indent=2))
PY
rm -rf "$BIN" "$PROF"

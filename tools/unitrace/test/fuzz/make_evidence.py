# ==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# ==============================================================
#
# Builds the SDL505 evidence report from the outputs of run_fuzz.sh,
# coverage_report.sh and the fuzz tests of test_unitrace.py, and from
# findings.json.
#
# Usage: python3 make_evidence.py [--campaign NAME=DIR]... [--coverage DIR]
#                                 [--regression FILE] [--out DIR] [--notes JSON]
# Writes <out>/SDL505_fuzzing_report.json, .txt and .html (text only, no images).

import argparse
import datetime
import glob
import html
import json
import os
import platform
import re
import subprocess
import sys

FUZZ_DIR = os.path.dirname(os.path.abspath(__file__))

INTERFACES = {
    "elf64": ("Module native binary (ELF64/ZeBin) returned by the GPU driver (zeModuleGetNativeBinary): "
              "section table, IntelGT compatibility notes, kernel ISA, DWARF .debug_line and relocations",
              "src/utils/elf64_reader.h, kernel_extractor.h, source_resolver.h", "C++", "libFuzzer"),
    "metric_query": ("Intermediate metric query file .metrics.<pid>.q read back from UNITRACE_DataDir",
                     "src/levelzero/ze_metrics.h ReadMetricQueryRecord", "C++", "libFuzzer"),
    "stall_sampling": ("Intermediate metric sampling file (.t) read back from UNITRACE_DataDir",
                       "src/levelzero/ze_metrics.h ReadMetricSegment", "C++", "libFuzzer"),
    "kernel_props": ("Kernel properties (.kprops) and kernel times (.ktime) files read back from UNITRACE_DataDir",
                     "src/levelzero/ze_metrics.h ReadKernelPropsRecord, ReadKernelTimeRecord",
                     "C++", "libFuzzer"),
    "cli_args": ("Command line: the table-driven option parser and its JSON schema escaping",
                 "src/utils/cli_options.h ParseArgs, Lookup, JsonEscape", "C++", "libFuzzer"),
    "kernel_filter": ("--include-kernels / --exclude-kernels strings and their -file contents",
                      "src/levelzero/ze_collector.h ParseFilterList", "C++", "libFuzzer"),
    "sample_lists": ("--devices-to-sample, --ranks-to-sample and the MPI launcher environment "
                     "(PMI_SIZE, PMIX_SIZE, OMPI_*_SIZE, PMI_RANK, PMIX_RANK)",
                     "src/utils/utils_ze.h ParseDeviceList, ParseRankList; utils_host.h ToInt",
                     "C++", "libFuzzer"),
    "kmd_trace": ("Raw OS/KMD trace produced by the user's bpftrace script (--chrome-kmd-logging, runs as root)",
                  "src/unitrace.cc ReadKmdEvent, KmdEventToJson", "C++", "libFuzzer"),
    "analyzeperfmetrics": ("Metric CSV files (query, time-based, stall) given to analyzeperfmetrics.py, "
                           "uniview.py and perfdashboard.py",
                           "scripts/metrics/analyzeperfmetrics.py", "Python", "Atheris"),
    "mergetrace": ("Chrome JSON and Perfetto traces given to mergetrace.py / uniview.py",
                   "scripts/tracemerge/mergetrace.py", "Python", "Atheris"),
    "summary": ("Tar archive of unitrace summaries given to summary.py",
                "scripts/summary/summary.py read_summary_tar", "Python", "Atheris"),
    "categorize": ("Category schema INI given to categorize.py, and lookups in the resulting tree",
                   "scripts/summary/categorize.py build_tree_from_ini, summary.categorize", "Python", "Atheris"),
    "roofline_csv": ("Device config CSV and metric query CSV given to roofline.py",
                     "scripts/roofline/roofline_libs.py Platform, file_to_json", "Python", "Atheris"),
    "trace_json": ("Names written into the Chrome trace and the --result-dir config log: kernel names (module "
                   "binary), ITT task names, argument keys and strings (application), device names (driver), "
                   "the command line",
                   "src/utils/utils_host.h JsonString, src/chromelogger.h JsonNameField, "
                   "src/unitrace.cc StringifyJsonArray", "C++", "libFuzzer"),
    "env_numbers": ("UNITRACE_DeviceId, PTI_DEVICE_ID, PTI_SUB_DEVICE_ID, --chrome-event-buffer-size, and the "
                    "devices to sample as the metric profiler reads them",
                    "src/utils/utils_ze.h ParseDeviceIndex, DevicesStringToSet; src/utils/utils_host.h "
                    "ToUint32, ParseChromeEventBufferSize", "C++", "libFuzzer"),
    "metric_samples": ("Counts returned by zetMetricGroupCalculateMultipleMetricValuesExp and the sampling "
                       "timestamps, in place of a Level Zero mock",
                       "src/levelzero/ze_metrics.h CalculatedSamplesFit, UnwrapTimestamp",
                       "C++", "libFuzzer"),
    "metrics_server": ("Requests to the localhost metrics server and the -f options file of analyzeperfmetrics.py",
                       "scripts/metrics/analyzeperfmetrics.py ParseMetricsRequest, ReadOptionsFromFile",
                       "Python", "Atheris"),
    "perfdashboard_upload": ("CSV uploaded to the counter dashboard",
                             "scripts/metrics/perfdashboard.py upload_csv_input1, index_kernels", "Python",
                             "Atheris"),
}

OUT_OF_SCOPE = [
    "Level Zero / OpenCL / XPTI / OMPT / ITT tracing callbacks: the values they receive (handles, timestamps, "
    "sizes) come from the vendor runtime in the same process and are not parsed; the one complex structure a "
    "runtime returns, the module native binary, is fuzzed (elf64), and so are the names written to the trace "
    "(trace_json). The ITT name copies and zeKernelGetName were reviewed and fixed (FUZZ-024, FUZZ-025).",
    "zetMetricGroupCalculateMultipleMetricValuesExp: the raw metric blobs are decoded by the Level Zero driver, "
    "not by unitrace; unitrace's framing of them (.q/.t) is fuzzed, and so are its checks on the counts the "
    "driver returns (metric_samples).",
    "PDF plotting (matplotlib) and the HTTP plumbing of the analyzeperfmetrics.py and uniview.py servers "
    "(http.server; they bind to 127.0.0.1): the request parsing unitrace adds is fuzzed (metrics_server). "
    "uniview.py serves the trace file's directory to ui.perfetto.dev for a single request.",
    "The bpftrace script given to --chrome-kmd-logging: executed by bpftrace as root by design; its output is fuzzed. "
    "The application's arguments are passed to bpftrace -c joined by spaces (reviewed): bpftrace splits the "
    "string again, so an argument containing whitespace does not reach the application unchanged. The command "
    "runs with the privileges the user gave unitrace; no change made.",
    "The removal of the temporary data directory: created by mkdtemp, not taken from input; reviewed (FUZZ-026).",
]


LIMITATIONS = [
    "No Intel GPU with a Level Zero runtime was available on the fuzzing host, so unitrace was not run "
    "end-to-end on a GPU workload after the changes. The changed parsers are exercised by the harnesses and "
    "the regression suite, the product (unitrace, libunitrace_tool.so) was built from the changed sources, "
    "the existing Python tests (test_dashboard_*, test_uniview_routing: 38 tests) pass, and FUZZ-005, "
    "FUZZ-018 and FUZZ-019 were verified on the built binaries (product_binary_verification.txt).",
    "FUZZ-024 (ITT names), FUZZ-025 (zeKernelGetName) and FUZZ-026 (data directory clean-up) are in code "
    "that runs only with a GPU or an instrumented application; their fixes are built and reviewed, not "
    "fuzzed or run. FUZZ-023's checks are fuzzed (metric_samples), its call sites only built.",
    "The .q metric query reader's call site (ze_metrics.h ComputeMetricsQueried, FUZZ-001/FUZZ-002) is "
    "compiled only for Windows, which was not built here: the parser it calls (ReadMetricQueryRecord) is built and "
    "fuzzed, but the call site and the FUZZ-002 bounds check are covered by review only. A Windows build is "
    "needed before release.",
    "Campaigns ran 1 hour per target and round. Edge coverage stopped growing well before the end of each "
    "run (see 'edges @exec') except for elf64 in gaps and gaps_seed8, where the DWARF-4/5 seeds opened new "
    "code: two more 1 h runs from their merged corpus (elf64_ext, elf64_ext_seed9) reached no new edge after "
    "their first 65,536 executions; and analyzeperfmetrics, which still gains an edge now and then (267, 268 and 270 "
    "edges in main, seed8 and round4). Longer runs, e.g. nightly in CI, remain recommended.",
]


def run(cmd):
    """Run a command and return its standard output.

    cmd: the argument list for subprocess.run (no shell), with a 30 s timeout.
    Returns stdout stripped of surrounding whitespace, or "" if the command cannot
    be started, times out or raises any other exception. Exit status is ignored.
    """
    try:
        return subprocess.run(cmd, capture_output=True, text=True, timeout=30).stdout.strip()
    except Exception:
        return ""


def environment():
    """Describe the fuzzing host and tools for the report.

    Returns a dict: host OS, CPU model and count, the first line of
    clang++ --version, the sanitizers used, Python and Atheris versions, and the
    git commit and branch of FUZZ_DIR. A value that cannot be found is "" (see run).
    """
    atheris_version = run([sys.executable, "-c",
                           "import importlib.metadata as m; print(m.version('atheris'))"])
    return {
        "host_os": platform.platform(),
        "cpu": run(["sh", "-c", "grep -m1 'model name' /proc/cpuinfo | cut -d: -f2"]),
        "cpu_count": os.cpu_count(),
        "clang": run(["clang++", "--version"]).splitlines()[0] if run(["clang++", "--version"]) else "",
        "libfuzzer_sanitizers": "AddressSanitizer + UndefinedBehaviorSanitizer (-fno-sanitize-recover=undefined)",
        "python": platform.python_version(),
        "atheris": atheris_version,
        "git_commit": run(["git", "-C", FUZZ_DIR, "rev-parse", "HEAD"]),
        "git_branch": run(["git", "-C", FUZZ_DIR, "rev-parse", "--abbrev-ref", "HEAD"]),
    }


STAT = re.compile(r"^stat::(\w+):\s+(\d+)", re.M)
PROGRESS = re.compile(r"^#(\d+)\s+(NEW|REDUCE|pulse|INITED|DONE|RELOAD)\s+cov: (\d+) ft: (\d+) corp: (\d+)/", re.M)
CRASH = re.compile(r"(ERROR: libFuzzer: [^\n]+|=== Uncaught Python exception: ===\n[^\n]*\n?[^\n]*)")


def campaign(dir_):
    """Summarize one run_fuzz.sh output directory.

    dir_: the campaign directory; each subdirectory with a run.json is one target.
    Returns {target: dict} with seed, duration, timing, command, executions,
    exec/s, peak RSS, coverage edges and features, corpus size, the executions at
    which coverage last grew, artifact names and up to three crash reports, parsed
    from run.json, fuzz.log and artifacts/. Subdirectories without run.json are
    skipped; a missing fuzz.log or a malformed run.json raises (OSError, KeyError,
    json.JSONDecodeError).
    """
    out = {}
    for tdir in sorted(glob.glob(os.path.join(dir_, "*"))):
        meta_path = os.path.join(tdir, "run.json")
        if not os.path.isfile(meta_path):
            continue
        meta = json.load(open(meta_path))
        log = open(os.path.join(tdir, "fuzz.log"), errors="replace").read()
        stats = {k: int(v) for k, v in STAT.findall(log)}
        progress = PROGRESS.findall(log)
        new = [p for p in progress if p[1] == "NEW"]
        edges_grew, max_edges = None, -1
        for p in progress:
            if int(p[2]) > max_edges:
                edges_grew, max_edges = int(p[0]), int(p[2])
        last = progress[-1] if progress else None
        artifacts = sorted(os.path.basename(a) for a in glob.glob(os.path.join(tdir, "artifacts", "*")))
        out[meta["target"]] = {
            "seed": meta["seed"],
            "duration_s": meta["duration_s"],
            "elapsed_s": int((datetime.datetime.fromisoformat(meta["end"]) -
                              datetime.datetime.fromisoformat(meta["start"])).total_seconds()),
            "start": meta["start"],
            "end": meta["end"],
            "exit_status": meta["exit_status"],
            "command": " ".join(meta["command"]),
            "executions": stats.get("number_of_executed_units", int(last[0]) if last else 0),
            "average_exec_per_sec": stats.get("average_exec_per_sec"),
            "peak_rss_mb": stats.get("peak_rss_mb"),
            "coverage_edges": int(last[2]) if last else None,
            "coverage_features": int(last[3]) if last else None,
            "corpus_size": len(os.listdir(os.path.join(tdir, "corpus")))
                           if os.path.isdir(os.path.join(tdir, "corpus")) else None,
            "last_new_coverage_at_execution": int(new[-1][0]) if new else None,
            "edges_last_grew_at_execution": edges_grew,
            "crashes": artifacts,
            "crash_reports": CRASH.findall(log)[:3],
        }
    return out


# Campaigns run on the fixed code; the others ran before a fix and found the FUZZ-* issues.
FINAL_CAMPAIGNS = ["main", "seed8", "round4", "gaps", "gaps_seed8", "elf64_ext", "elf64_ext_seed9", "rebased"]
# libFuzzer artifact prefix -> what it counts as in the summary; a timeout is a hang.
ARTIFACT_KINDS = {"crash": "crashes", "oom": "out-of-memory", "timeout": "hangs"}


def artifact_summary(campaigns):
    """Count the libFuzzer artifacts of every run by kind.

    campaigns: {campaign name: campaign() result}.
    Returns a dict: the FINAL_CAMPAIGNS present, their run count and artifact
    counts, each out-of-memory artifact of them as (campaign, target, artifact),
    and the run count and artifact counts over all campaigns. Kinds come
    from the artifact name prefix via ARTIFACT_KINDS (crash, out-of-memory, hangs
    for timeouts); an unknown prefix counts as a crash.
    """
    def count(names):
        n = {k: 0 for k in ARTIFACT_KINDS.values()}
        for name in names:
            for c in campaigns.get(name, {}).values():
                for a in c["crashes"]:
                    n[ARTIFACT_KINDS.get(a.split("-", 1)[0], "crashes")] += 1
        return n
    final = [c for c in FINAL_CAMPAIGNS if c in campaigns]
    oom = [(name, target, a) for name in final for target, c in campaigns[name].items()
           for a in c["crashes"] if a.startswith("oom-")]
    return {
        "final_campaigns": final,
        "final_oom": oom,
        "runs_final": sum(len(campaigns[c]) for c in final),
        "final": count(final),
        "all": count(campaigns),
        "runs_all": sum(len(r) for r in campaigns.values()),
    }


CRASH_START = re.compile(r"ERROR: libFuzzer|ERROR: AddressSanitizer|runtime error:|Uncaught Python exception|ALARM:|\[FUZZ\]")
KEEP_HEAD = KEEP_TAIL = 150
LOG_LIMIT = 1 << 20


def copy_log(src, dst):
    """Copy a fuzzer log into the evidence package.

    src: the log path. dst: the destination path.
    A log of at most LOG_LIMIT bytes is copied byte for byte. A larger one is
    written trimmed: the first KEEP_HEAD lines, a note with the original size, the
    60 lines from each crash report start (CRASH_START), and the last KEEP_TAIL
    lines. Undecodable bytes are replaced. Returns None.
    """
    if os.path.getsize(src) <= LOG_LIMIT:
        with open(src, "rb") as i, open(dst, "wb") as o:
            o.write(i.read())
        return
    head, tail, crash, total, after = [], [], [], 0, 0
    with open(src, errors="replace") as f:
        for line in f:
            total += 1
            if total <= KEEP_HEAD:
                head.append(line)
                continue
            if CRASH_START.search(line):
                after = 60
            if after > 0:
                crash.append(line)
                after -= 1
            tail.append(line)
            if len(tail) > KEEP_TAIL:
                tail.pop(0)
    with open(dst, "w") as o:
        o.writelines(head)
        o.write(f"\n[... log trimmed for the evidence package: {total} lines, {os.path.getsize(src)} bytes "
                f"in the original; the first {KEEP_HEAD}, every crash report and the last {KEEP_TAIL} are kept ...]\n\n")
        if crash:
            o.write("[crash reports]\n")
            o.writelines(crash)
            o.write("\n[last lines]\n")
        o.writelines(tail)


def package_logs(campaigns, out):
    """Copy every campaign's run metadata and logs into the evidence directory.

    campaigns: a list of [name, dir] pairs. out: the evidence directory.
    For each target with a run.json writes out/campaigns/<name>/<target>/ with
    run.json, the fuzz*.log files (via copy_log) and artifacts.txt listing the
    artifact names, or "none". Returns None.
    """
    for name, d in campaigns:
        for meta in sorted(glob.glob(os.path.join(d, "*", "run.json"))):
            tdir = os.path.dirname(meta)
            dst = os.path.join(out, "campaigns", name, os.path.basename(tdir))
            os.makedirs(dst, exist_ok=True)
            with open(meta) as i, open(os.path.join(dst, "run.json"), "w") as o:
                o.write(i.read())
            for log in glob.glob(os.path.join(tdir, "fuzz*.log")):
                copy_log(log, os.path.join(dst, os.path.basename(log)))
            arts = sorted(os.listdir(os.path.join(tdir, "artifacts"))) if os.path.isdir(
                os.path.join(tdir, "artifacts")) else []
            with open(os.path.join(dst, "artifacts.txt"), "w") as o:
                o.write("".join(a + "\n" for a in arts) or "none\n")


def text_report(r):
    """Render the evidence report as plain text.

    r: the report dict built by main.
    Returns the text: interfaces fuzzed, tools and environment, campaign tables,
    hangs and crashes, coverage, findings, regression suite, product binary
    verification, limitations and reproduction steps.
    Raises KeyError if r lacks a field it prints.
    """
    L = []
    w = L.append
    w("SDL505 - Perform Fuzz Testing: Evidence Report")
    w("=" * 78)
    w(f"Project:   {r['project']}")
    w(f"Task:      {r['task']}")
    w(f"Generated: {r['generated']}")
    w(f"Branch:    {r['environment']['git_branch']} @ {r['environment']['git_commit']}")
    w("")
    w("1. INTERFACES FUZZED")
    w("-" * 78)
    for t, i in r["interfaces"].items():
        w(f"  {t} [{i['language']}, {i['fuzzer']}]")
        w(f"      input: {i['interface']}")
        w(f"      code:  {i['code']}")
    w("  Out of scope:")
    for o in r["out_of_scope"]:
        w(f"    - {o}")
    w("")
    w("2. TOOLS AND ENVIRONMENT")
    w("-" * 78)
    for k, v in r["environment"].items():
        w(f"  {k:22} {v}")
    w("")
    w("3. CAMPAIGNS")
    w("-" * 78)
    w("  Limits: 10 s per input, 1 GB per allocation, 2 GB RSS (4 GB for round4, see FUZZ-016).")
    w("  'edges @exec' is the execution at which the run last reached a new edge, 'last new @exec'")
    w("  the one at which it last kept an input for new coverage (edges or finer-grained features):")
    w("  well below 'executions' means coverage plateaued; 0 means the run started from an earlier")
    w("  run's corpus and found no new coverage at all. 'corpus' '-': the run used the seed directory.")
    w("")
    for name, runs in r["campaigns"].items():
        w(f"  3.{list(r['campaigns']).index(name) + 1} {name}")
        if r["campaign_notes"].get(name):
            w(f"      {r['campaign_notes'][name]}")
        w(f"  {'target':20} {'seed':>4} {'ran(s)':>7} {'executions':>13} {'exec/s':>7} {'edges':>6} "
          f"{'corpus':>6} {'edges @exec':>13} {'last new @exec':>14} crashes")
        for t, c in runs.items():
            w(f"  {t:20} {c['seed']:>4} {c['elapsed_s']:>7} {c['executions']:>13,} "
              f"{c['average_exec_per_sec'] or 0:>7} {c['coverage_edges'] or 0:>6} {c['corpus_size'] if c['corpus_size'] is not None else '-':>6} "
              f"{c.get('edges_last_grew_at_execution') or 0:>13,} "
              f"{c['last_new_coverage_at_execution'] or 0:>14,} {len(c['crashes'])}")
            for a in c["crashes"]:
                w(f"      crash artifact: {a}")
        w("")
    h = r["hangs_and_crashes"]
    fmt = lambda n: ", ".join(f"{k} {v}" for k, v in n.items())
    w(f"  HANGS AND CRASHES (libFuzzer artifacts; a timeout counts as a hang)")
    w(f"    Final code, {h['runs_final']} runs ({', '.join(h['final_campaigns'])}): {fmt(h['final'])}")
    for campaign_name, target, artifact in h.get("final_oom", []):
        # Only the artifact FUZZ-016 investigated is explained as its false positive;
        # any other out-of-memory is reported as a finding to triage.
        if (campaign_name, target) == ("seed8", "analyzeperfmetrics") and "round4" in h["final_campaigns"]:
            w(f"      out-of-memory: {target} in {campaign_name} ({artifact}), a false positive (FUZZ-016):")
            w("      the Atheris process grew past the 2 GB RSS limit; round4 reran it at 4 GB for the full")
            w("      hour with none.")
        else:
            w(f"      out-of-memory: {target} in {campaign_name} ({artifact}): not triaged - add it to the")
            w("      findings register with its disposition.")
    w(f"    All campaigns, {h['runs_all']} runs, including those that found FUZZ-*: {fmt(h['all'])}")
    w("")
    cov = r["coverage"]
    w("4. CODE COVERAGE (corpora replayed on coverage-instrumented builds)")
    w("-" * 78)
    if cov:
        w(f"  C++ product code (llvm-cov):  lines {cov['cpp']['total_lines_percent']}%  "
          f"functions {cov['cpp']['total_functions_percent']}%  branches {cov['cpp']['total_branches_percent']}%")
        w(f"  {r['coverage_scope']}")
        for f, v in cov["cpp"]["files"].items():
            w(f"    {f:48} lines {v['lines_percent']:6}%  functions {v['functions_percent']:6}%  "
              f"branches {v['branches_percent']:6}%")
        py = cov["python"]
        if "total_lines_percent" in py:
            w(f"  Python scripts (coverage.py): lines {py['total_lines_percent']}%  "
              f"branches {py['total_branches_percent']}%")
        else:  # a coverage report from before lines and branches were split
            w(f"  Python scripts (coverage.py): {py['total_percent']}% of lines and branches")
        for f, v in py["files"].items():
            branches = f"  branches {v['branches_percent']:6}%" if "branches_percent" in v else ""
            w(f"    {f:48} lines {v['lines_percent']:6}%{branches}")
        w(f"  Target: {r['coverage_target']}")
    w("")
    w("5. FINDINGS AND DISPOSITION")
    w("-" * 78)
    for f in r["findings"]:
        w(f"  {f['id']}  [{f['severity']}] [{f['disposition']}]  {f['title']}")
        w(f"      found by:   {f['found_by']}")
        w(f"      location:   {f['location']}")
        w(f"      root cause: {f['root_cause']}")
        w(f"      impact:     {f['impact']}")
        w(f"      fix:        {f['fix']}")
        if f["regression_tests"]:
            w(f"      regression: {', '.join(f['regression_tests'])}")
        if f.get("note"):
            w(f"      note:       {f['note']}")
    s = r["findings_summary"]
    w(f"  Total {s['total']}: " + ", ".join(f"{k} {v}" for k, v in s["by_disposition"].items()))
    w("")
    w("6. REGRESSION SUITE")
    w("-" * 78)
    w("  " + r["regression"].replace("\n", "\n  "))
    w("")
    w("7. PRODUCT BINARY VERIFICATION")
    w("-" * 78)
    w("  " + r["product_binary_verification"].strip().replace("\n", "\n  "))
    w("")
    w("8. LIMITATIONS")
    w("-" * 78)
    for l in r["limitations"]:
        w(f"  - {l}")
    w("")
    w("9. REPRODUCTION")
    w("-" * 78)
    for line in r["reproduction"]:
        w("  " + line)
    return "\n".join(L) + "\n"


def html_report(r, text):
    """Wrap the text report in a minimal HTML page.

    r: the report dict (unused). text: the text from text_report.
    Returns an HTML document with the HTML-escaped text in a <pre> block.
    """
    return ("<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\"><title>SDL505 Fuzzing Evidence - unitrace"
            "</title></head><body><pre>\n" + html.escape(text) + "</pre></body></html>\n")


def main():
    """Build the SDL505 fuzzing evidence report.

    Options: --campaign NAME=DIR (repeatable; default main=results/campaign),
    --coverage, --regression, --out, --notes (JSON of campaign descriptions).
    Reads findings.json, the campaigns, coverage_summary.json, the regression
    output and product_binary_verification.txt (missing optional files become
    None or "not run"), copies the logs (package_logs) and writes
    SDL505_fuzzing_report.json, .txt and .html to --out, printing the text.
    """
    ap = argparse.ArgumentParser()
    ap.add_argument("--campaign", action="append", default=None,
                    help="NAME=DIR of a run_fuzz.sh output; repeatable")
    ap.add_argument("--coverage", default=os.path.join(FUZZ_DIR, "results", "coverage"))
    ap.add_argument("--regression", default=os.path.join(FUZZ_DIR, "results", "regression.txt"))
    ap.add_argument("--out", default=os.path.join(FUZZ_DIR, "evidence"))
    ap.add_argument("--notes", help="JSON object: campaign name -> description")
    args = ap.parse_args()
    campaigns = args.campaign or [f"main=" + os.path.join(FUZZ_DIR, "results", "campaign")]
    notes = {}
    if args.notes and os.path.isfile(args.notes):
        notes = json.load(open(args.notes))
    verification = os.path.join(args.out, "product_binary_verification.txt")

    findings = json.load(open(os.path.join(FUZZ_DIR, "findings.json")))
    by_disp = {}
    for f in findings["findings"]:
        by_disp[f["disposition"]] = by_disp.get(f["disposition"], 0) + 1
    cov_path = os.path.join(args.coverage, "coverage_summary.json")
    runs = {name: campaign(d) for name, d in (c.split("=", 1) for c in campaigns)}
    report = {
        "project": findings["project"],
        "task": findings["task"],
        "generated": datetime.datetime.now().astimezone().isoformat(timespec="seconds"),
        "environment": environment(),
        "interfaces": {t: {"interface": i[0], "code": i[1], "language": i[2], "fuzzer": i[3]}
                       for t, i in INTERFACES.items()},
        "out_of_scope": OUT_OF_SCOPE,
        "campaign_notes": notes,
        "limitations": LIMITATIONS,
        "product_binary_verification": open(verification).read() if os.path.isfile(verification)
                                       else "not run",
        "campaigns": runs,
        "hangs_and_crashes": artifact_summary(runs),
        "coverage": json.load(open(cov_path)) if os.path.isfile(cov_path) else None,
        "coverage_target": "SDL505 aspirational target 30% code coverage",
        "coverage_scope": ("Scope: the files the harnesses reach (listed below), not the whole of unitrace; the collectors and tracing callbacks that only pass runtime values through are out of scope (section 1)."),
        "severity_scale": findings["severity_scale"],
        "findings": findings["findings"],
        "findings_summary": {"total": len(findings["findings"]), "by_disposition": by_disp},
        "regression": open(args.regression).read().strip() if os.path.isfile(args.regression) else "not run",
        "reproduction": [
            "cmake -S tools/unitrace -B build-fuzz -DUNITRACE_BUILD_FUZZ=ON -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang && cmake --build build-fuzz --target unitrace_fuzz",
            "pip install atheris   # plus tools/unitrace/requirements.txt",
            "cd tools/unitrace/test/fuzz",
            "./run_fuzz.sh -t 3600 -s <seed> -b ../../../../build-fuzz/test/fuzz/cpp [-m 4096] [target...]   # seeds: section 3",
            "./coverage_report.sh results/campaign results/coverage",
            "(cd .. && python3 test_unitrace.py --test-name 'fuzz_*') > results/regression.txt",
            "python3 make_evidence.py --campaign main=results/campaign",
            "Replay one input: ../../../../build-fuzz/test/fuzz/cpp/fuzz_<target> <file>  |  python3 python/fuzz_<target>.py <file>",
        ],
    }
    os.makedirs(args.out, exist_ok=True)
    package_logs([c.split("=", 1) for c in campaigns], args.out)
    base = os.path.join(args.out, "SDL505_fuzzing_report")
    json.dump(report, open(base + ".json", "w"), indent=2)
    text = text_report(report)
    open(base + ".txt", "w").write(text)
    open(base + ".html", "w").write(html_report(report, text))
    print(text)


if __name__ == "__main__":
    main()

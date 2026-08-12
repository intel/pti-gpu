#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

# Timeline trace-format test entry point (CI: Unitrace-trace-format): verifies the
# json and protobuf timeline outputs describe the same work. For each scenario
# (apps x chrome flags, Level Zero + OpenCL, device- vs call-logging) it drives
# unitrace TWICE on the same app (json and protobuf) and checks equivalence, flow
# direction, smaller protobuf size, and a structural cross-check via Perfetto's
# own traceconv (downloaded to the build tree if not cached).
#
# unitrace emits ONE format per run, so the two runs differ in kernel timing;
# anything timing-dependent (timestamps, track names, device lane packing) is not
# comparable, so equivalence is on the slice-NAME MULTISET. The protobuf is parsed
# with protoc-generated bindings (see trace_compare.py). --app/--scenario run a
# subset. Fails hard on any mismatch or if protobuf support is unavailable (CI
# builds with BUILD_WITH_PERFETTO=ON).

import argparse
import glob
import os
import platform
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "trace_utils"))
import trace_compare
import perfetto_vs_json_e2e

# Built executables carry a .exe suffix on Windows
_EXE = ".exe" if platform.system() == "Windows" else ""
DEFAULT_UNITRACE = os.path.normpath(os.path.join(HERE, "..", "build", "unitrace" + _EXE))
APP_DIR = os.path.join(HERE, "cpp_test", "build")


# Build the cpp_test apps (cmake + ninja), the same way test_unitrace.py does, so a
def build_test_apps(targets):
  os.makedirs(APP_DIR, exist_ok=True)
  print("[INFO] building cpp_test apps in {}".format(APP_DIR))
  steps = (["cmake", "..", "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release"],
           ["ninja", "-k", "0"] + sorted(set(targets)))
  for step in steps:
    try:
      rc = subprocess.run(step, cwd=APP_DIR).returncode
    except FileNotFoundError:
      rc = "{!r} not found".format(step[0])
    if rc != 0:
      print("[WARN] cpp_test build step {} failed ({}); continuing with the apps "
            "that did build. Ensure oneAPI (icx/IntelSYCL) and cmake/ninja are on "
            "PATH to build every SYCL/OpenCL scenario.".format(step[0], rc))
      # ninja -k 0 still builds the targets it can, so keep going even on rc!=0.
      if step[0] != "ninja":
        return

# The scenario matrix. Each entry drives both formats and compares them. Flags
# are chosen to cover distinct emitter paths:
#   - call+kernel logging exercises the host API slices + the H2D flow records
#     (the flow-arrow path that differs most between formats);
#   - device logging alone exercises only the device (gpu_op) slices, no host
#     flows;
#   - cl_gemm drives the OpenCL collector/emitter; ze_/dpc_ drive Level Zero.
SCENARIOS = [
    {"name": "ze_call_kernel",  "app": "ze_gemm/ze_gemm",
     "flags": ["--chrome-call-logging", "--chrome-kernel-logging"]},
    {"name": "ze_device",       "app": "ze_gemm/ze_gemm",
     "flags": ["--chrome-device-logging"]},
    {"name": "cl_call_kernel",  "app": "cl_gemm/cl_gemm",
     "flags": ["--chrome-call-logging", "--chrome-kernel-logging", "--opencl"]},
    {"name": "dpc_call_kernel", "app": "dpc_gemm/dpc_gemm",
     "flags": ["--chrome-call-logging", "--chrome-kernel-logging"]},
    {"name": "cl_device",       "app": "cl_gemm/cl_gemm",
     "flags": ["--chrome-device-logging", "--opencl"]},
]


def run_unitrace(unitrace, fmt, chrome_flags, app, app_args, out_dir):
  os.makedirs(out_dir, exist_ok=True)
  cmd = [unitrace] + list(chrome_flags)
  if fmt == "protobuf":
    cmd += ["--output-format", "protobuf"]
  cmd += ["--output-dir-path", out_dir, app] + app_args
  r = subprocess.run(cmd, capture_output=True, text=True)
  if r.returncode != 0:
    print("[ERROR] unitrace ({}) failed rc={}\n{}".format(fmt, r.returncode, r.stderr))
    sys.exit(1)
  # With --output-dir-path the trace is named <app>.<pid>.<ext> (the
  # chrome_trace.<ext> name is only used under --result-dir). Each format runs
  # into its own dir, so there is exactly one matching file to discover.
  ext = "pftrace" if fmt == "protobuf" else "json"
  matches = glob.glob(os.path.join(out_dir, "*." + ext))
  if not matches:
    print("[ERROR] no *.{} trace produced in {} (stderr:\n{})".format(ext, out_dir, r.stderr))
    sys.exit(1)
  if len(matches) > 1:
    print("[ERROR] expected one *.{} trace in {}, found {}".format(ext, out_dir, matches))
    sys.exit(1)
  return matches[0]


# Smallest protobuf advantage we require on file size. Protobuf (varint ints, no
# quoting/whitespace) is well under half the JSON size in practice; 25% smaller is
# a conservative floor that still catches a regression to a bloated encoding.
_MIN_SIZE_SAVING = 0.25


def check_size(name, json_bytes, pb_bytes):
  size_saving = 1.0 - (pb_bytes / json_bytes) if json_bytes else 0.0
  if size_saving < _MIN_SIZE_SAVING:
    return ["protobuf trace not enough smaller than json: {}B vs {}B "
            "({:.0%} < required {:.0%})".format(
                pb_bytes, json_bytes, size_saving, _MIN_SIZE_SAVING)]
  return []


# Run one scenario (one app + flag set) in both formats and compare. Returns a
# list of error strings (empty == passed); does not exit, so the matrix driver
# can run every scenario and report all failures together.
def compare_scenario(unitrace, name, chrome_flags, app, app_args):
  # Host<->device flow arrows exist only when host API calls are logged
  # alongside kernels; device-only logging emits kernel flow_ids with no host
  # endpoint to pair with, so flow validation is gated on call+kernel logging.
  check_flows = ("--chrome-call-logging" in chrome_flags
                 and "--chrome-kernel-logging" in chrome_flags)

  errors = []
  with tempfile.TemporaryDirectory() as tmp:
    json_path = run_unitrace(unitrace, "json", chrome_flags, app, app_args, os.path.join(tmp, "json"))
    pb_path = run_unitrace(unitrace, "protobuf", chrome_flags, app, app_args, os.path.join(tmp, "pb"))
    json_bytes = os.path.getsize(json_path)
    pb_bytes = os.path.getsize(pb_path)
    j = trace_compare.fingerprint_json(json_path)
    p = trace_compare.fingerprint_protobuf(pb_path)
    n_flows, flow_errors = (trace_compare.validate_protobuf_flows(pb_path) if check_flows else (0, []))
    # An empty json fingerprint means the timeline is truncated (trace_compare
    # reports the details) or empty. There is nothing to compare against, and
    # traceconv would parse the same unparsable file, so record the failure and
    # skip the cross-check instead of letting it raise: this scenario fails and the
    # matrix driver still runs the rest.
    json_ok = j["num_tracks"] != 0 and sum(j["slice_names"].values()) != 0
    if not json_ok:
      errors.append("[{}] json trace has no tracks/slices (truncated or empty).".format(name))
    # Second engine: the protobuf trace also round-trips through Perfetto's own
    # traceconv and must match the json structure (must run before tempdir cleanup).
    elif not perfetto_vs_json_e2e.compare_traces(pb_path, json_path):
      errors.append("[{}] traceconv structural compare failed".format(name))

  # The protobuf trace must independently be well-formed.
  if p["num_tracks"] == 0 or sum(p["slice_names"].values()) == 0:
    errors.append("[{}] protobuf trace has no tracks/slices (malformed or empty).".format(name))

  # The compact binary format must be meaningfully smaller than the text JSON.
  errors += check_size(name, json_bytes, pb_bytes)

  # Equivalence = identical slice-name multiset (same operations, same counts),
  # ignoring the event APIs (see trace_compare.EXCLUDED_SLICE_NAMES).
  diff = [(n, j["slice_names"].get(n, 0), p["slice_names"].get(n, 0))
          for n in (set(j["slice_names"]) | set(p["slice_names"]))
          if n not in trace_compare.EXCLUDED_SLICE_NAMES
          and j["slice_names"].get(n, 0) != p["slice_names"].get(n, 0)]
  if diff:
    msg = ["[{}] slice-name multiset differs between json and protobuf (name, json, proto):".format(name)]
    for d in sorted(diff)[:30]:
      msg.append("    {!r} {} {}".format(*d))
    errors.append("\n".join(msg))

  # Flow arrows must be well-formed (begin -> step -> terminate-while-active).
  for fe in flow_errors:
    errors.append("[{}] {}".format(name, fe))
  if check_flows and n_flows == 0 and not flow_errors:
    errors.append("[{}] no host<->device flows found (expected with "
                  "call+kernel logging).".format(name))
  return errors


def main():
  ap = argparse.ArgumentParser(description="Compare unitrace json vs protobuf timeline output (same app, two runs).")
  ap.add_argument("--unitrace", default=DEFAULT_UNITRACE, help="path to built unitrace")
  ap.add_argument("--app", default=None, help="single test app to trace (overrides the scenario matrix)")
  ap.add_argument("--scenario", default=None, help="run only the matrix scenario with this name")
  ap.add_argument("--app-args", nargs=argparse.REMAINDER, default=[], help="args for the app (after --)")
  ap.add_argument("--no-build", action="store_true",
                  help="do not build the cpp_test apps; use whatever is already built")
  args = ap.parse_args()

  if not os.path.exists(args.unitrace):
    print("[ERROR] unitrace not found at {} (build it first).".format(args.unitrace))
    sys.exit(1)
  app_args = args.app_args[1:] if args.app_args and args.app_args[0] == "--" else args.app_args

  # --app: single ad-hoc run with the default call+kernel flags. Otherwise run
  # the scenario matrix (optionally filtered by --scenario).
  if args.app:
    scenarios = [{"name": "custom", "app": args.app,
                  "flags": ["--chrome-call-logging", "--chrome-kernel-logging"], "abs_app": args.app}]
  else:
    scenarios = []
    for s in SCENARIOS:
      if args.scenario and s["name"] != args.scenario:
        continue
      s = dict(s)
      s["abs_app"] = os.path.join(APP_DIR, s["app"].replace("/", os.sep) + _EXE)
      scenarios.append(s)
    if not scenarios:
      print("[ERROR] no scenario matched --scenario {!r}".format(args.scenario))
      sys.exit(1)

  # Build the apps the selected scenarios need up front (like test_unitrace.py), so
  # nothing is skipped just because it was not pre-built. The ninja target is the app
  # basename (the part after the last '/' in the scenario's "app"). Skip only for
  # --app (an arbitrary path) or when explicitly disabled.
  if not args.app and not args.no_build:
    build_test_apps([s["app"].rsplit("/", 1)[-1] for s in scenarios])

  RED = "\033[31m"
  RESET = "\033[0m"
  print("Start testing...")
  ran = 0
  failed = 0
  for s in scenarios:
    label = s["name"] + " " + " ".join(s["flags"])
    if not os.path.exists(s["abs_app"]):
      print("{} : {} : Skipped (app not built)".format(ran + 1, label))
      continue
    ran += 1
    errors = compare_scenario(args.unitrace, s["name"], s["flags"], s["abs_app"], app_args)
    if errors:
      failed += 1
      print("{}{} : {} : Failed{}".format(RED, ran, label, RESET))
      for e in errors:
        print("    " + e.replace("\n", "\n    "))
    else:
      print("{} : {} : Passed".format(ran, label))

  if ran == 0:
    print("[ERROR] no scenario apps were built; nothing compared.")
    sys.exit(1)
  print("{}/{} scenarios passed.".format(ran - failed, ran))
  sys.exit(1 if failed else 0)


if __name__ == "__main__":
  main()

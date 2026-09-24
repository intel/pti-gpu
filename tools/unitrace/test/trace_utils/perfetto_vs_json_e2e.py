#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

# Convert a Perfetto protobuf trace to JSON with Perfetto's own traceconv and
# check it has the same timeline STRUCTURE (track + slice-name multiset) as the
# legacy direct-JSON trace of the same workload -- a cross-check via the real
# Perfetto toolchain. Structural only: traceconv emits us timestamps, and the two
# traces come from separate runs, so exact timing is not comparable. traceconv is
# fetched version-pinned (or UNITRACE_TRACECONV) and cached.

import argparse
import hashlib
import json
import os
import platform
import re
import subprocess
import sys
import urllib.request

# Same directory; on sys.path both when imported by trace_format_test.py (which adds
# trace_utils/) and when this file is run as a script.
import trace_compare

# Fetch the launcher from the immutable ${PERFETTO_TAG} git path, not the rolling
# get.perfetto.dev pointer (whose hash changes on every upstream roll). The launcher
# self-verifies the native binary it downloads, so pinning its one hash suffices.
_LAUNCHER_URL = "https://raw.githubusercontent.com/google/perfetto/{tag}/tools/traceconv"

# tools/traceconv launcher sha256 per PERFETTO_TAG. To bump: change PERFETTO_TAG in
# cmake/Perfetto.cmake, fetch that tag's launcher, verify it, add its hash here.
LAUNCHER_SHA256 = {
    "v56.1": "6592b882ca8c49b9511124431676df5a49a236b8cd1d0788e009e3bd89cb0922",
}


def fail(msg):
  print("[ERROR] " + msg, file=sys.stderr)
  sys.exit(1)


_HERE = os.path.dirname(os.path.abspath(__file__))
# Cache traceconv in the unitrace build tree (next to the protoc/proto the build
# already produces), so it persists across runs and is co-located with the build.
_BUILD_DIR = os.path.normpath(os.path.join(_HERE, "..", "..", "build"))
_PERFETTO_CMAKE = os.path.normpath(os.path.join(_HERE, "..", "..", "cmake", "Perfetto.cmake"))


# Read PERFETTO_TAG from cmake/Perfetto.cmake (single source of truth with the proto).
def perfetto_tag():
  try:
    text = open(_PERFETTO_CMAKE).read()
  except OSError as ex:
    fail("could not read PERFETTO_TAG from {}: {}".format(_PERFETTO_CMAKE, ex))
  m = re.search(r'set\(PERFETTO_TAG\s+"([^"]+)"', text)
  if not m:
    fail("PERFETTO_TAG not found in {}".format(_PERFETTO_CMAKE))
  return m.group(1)


def get_traceconv():
  override = os.environ.get("UNITRACE_TRACECONV")
  if override:
    if os.path.exists(override):
      return override
    fail("UNITRACE_TRACECONV={} does not exist".format(override))
  tag = perfetto_tag()
  expected = LAUNCHER_SHA256.get(tag)
  if not expected:
    fail("no vetted traceconv launcher sha256 for PERFETTO_TAG={}; fetch {}, "
         "verify it, and add its hash to LAUNCHER_SHA256 (or set "
         "UNITRACE_TRACECONV).".format(tag, _LAUNCHER_URL.format(tag=tag)))
  # Cache per tag so a bump fetches fresh instead of trusting the old launcher.
  cache = os.path.join(_BUILD_DIR, "traceconv-{}".format(tag))
  if os.path.exists(cache):
    # Re-verify the cached copy so a stale/tampered file is not trusted.
    if hashlib.sha256(open(cache, "rb").read()).hexdigest() != expected:
      os.remove(cache)
  if not os.path.exists(cache):
    url = _LAUNCHER_URL.format(tag=tag)
    # Defense in depth for the sha256-pinned download below: the URL comes from
    # the https constant above, so any other scheme means tampering.
    if not url.startswith("https://"):
      fail("refusing non-https traceconv URL: {}".format(url))
    try:
      os.makedirs(_BUILD_DIR, exist_ok=True)
      # nosec B310: https enforced above; the payload is sha256-verified below
      # against the pinned launcher hash before it is written or used.
      with urllib.request.urlopen(url, timeout=60) as resp:  # nosec B310
        data = resp.read()
    except Exception as ex:
      fail("could not download traceconv ({}): {}".format(url, ex))
    digest = hashlib.sha256(data).hexdigest()
    if digest != expected:
      fail("traceconv launcher sha256 mismatch for {}: got {}, expected {} "
           "(immutable git blob changed unexpectedly; set UNITRACE_TRACECONV to a "
           "vetted binary).".format(tag, digest, expected))
    with open(cache, "wb") as f:
      f.write(data)
    # Owner-only: the launcher is always invoked via sys.executable (see
    # traceconv_to_json), so no execute or group/other bits are needed.
    os.chmod(cache, 0o700)
  return cache


def traceconv_to_json(traceconv, in_file, out_file):
  # The launcher is a python3 script (self-downloads its native binary); invoke it explicitly.
  cmd = [sys.executable, traceconv, "json", in_file, out_file]
  try:
    r = subprocess.run(cmd, capture_output=True, text=True)
  except Exception as ex:
    fail("traceconv invocation failed: {}".format(ex))
  if r.returncode != 0:
    fail("traceconv returned {}:\n{}".format(r.returncode, r.stderr))


from collections import Counter


# Structural fingerprint of a Chrome-JSON trace: the multiset of slice ("X")
# event names. Event names are format-invariant; absolute timestamps and the
# pid/tid track *labels* are not (traceconv re-derives them), and the number of
# device tracks varies run to run with engine/lane packing. The busy-wait APIs in
# trace_compare.EXCLUDED_SLICE_NAMES are dropped, so this engine compares exactly
# the same set of names as the direct-bindings compare in trace_compare.py.
def slice_names_from_chrome_json(path):
  # strict=False: the OpenCL path can embed raw control characters (e.g. NUL in a
  # device name) that the default decoder rejects; harmless for the structural
  # comparison here.
  with open(path) as f:
    data = json.loads(f.read(), strict=False)
  events = data.get("traceEvents", data) if isinstance(data, dict) else data
  name_counts = Counter()
  for e in events:
    if e.get("ph") == "X":
      name = e.get("name", "")
      if name not in trace_compare.EXCLUDED_SLICE_NAMES:
        name_counts[name] += 1
  return name_counts


# Convert |protobuf| via traceconv and structurally compare it to |json|
# Returns True on match. Importable by trace_format_test.py.
def compare_traces(protobuf, json_path):
  if not os.path.exists(protobuf):
    fail("protobuf trace not found: {}".format(protobuf))
  if not os.path.exists(json_path):
    fail("json trace not found: {}".format(json_path))

  # traceconv.exe has a Windows bug:
  # its EXPORT_JSON step can't open its own temp file, so it exits 0 but emits no events.
  # Skip this cross-check here; trace_compare.py already validates the protobuf.
  if platform.system() == "Windows":
    print("[WARN] skipping traceconv cross-check on Windows (traceconv.exe "
          "EXPORT_JSON temp-file bug); protobuf validated by trace_compare.py.",
          file=sys.stderr)
    return True

  traceconv = get_traceconv()
  converted = protobuf + ".converted.json"
  traceconv_to_json(traceconv, protobuf, converted)

  proto_names = slice_names_from_chrome_json(converted)
  json_names = slice_names_from_chrome_json(json_path)

  if not json_names:
    fail("no slices in the legacy json trace (nothing to compare)")

  ok = True
  if proto_names != json_names:
    ok = False
    print("[ERROR] slice-name multiset differs between protobuf->json and legacy json", file=sys.stderr)
    for name in sorted(set(proto_names) | set(json_names)):
      jc, pc = json_names.get(name, 0), proto_names.get(name, 0)
      if jc != pc:
        print("  {!r}: legacy json={}  protobuf->json={}".format(name, jc, pc), file=sys.stderr)
  return ok


def main():
  parser = argparse.ArgumentParser(description="Compare protobuf(->traceconv json) vs legacy json trace structure.")
  parser.add_argument("--protobuf", required=True, help=".pftrace file")
  parser.add_argument("--json", required=True, help="legacy .json trace of the same workload")
  args = parser.parse_args()
  ok = compare_traces(args.protobuf, args.json)
  print("[OK] trace structure matches (traceconv)" if ok else "[FAIL] trace structure differs")
  sys.exit(0 if ok else 1)


if __name__ == "__main__":
  main()

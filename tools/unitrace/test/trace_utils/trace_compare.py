#==============================================================
# Copyright (C) Intel Corporation
#
# SPDX-License-Identifier: MIT
# =============================================================

# The single library for parsing unitrace timeline traces (legacy Chrome JSON and
# Perfetto protobuf). Everything that reads a trace lives here so callers don't
# duplicate parsing; consumers are the per-test overlap checker
# (cpp_test/ze_counterbased_events/check_outfiles.py) and the trace-format-test
# orchestrator (trace_format_test.py).
#
# Two kinds of readers:
#   - timing stats for the overlap guard: parse_timeline_stats[_perfetto] -> calc_stats
#   - format-equivalence fingerprints / flow validation: fingerprint_json,
#     fingerprint_protobuf, validate_protobuf_flows
# Protobuf parsing uses pure protobuf bindings (no perfetto trace_processor / no
# traceconv -- those would lose the int64-ns precision the format exists to keep);
# the bindings are generated on demand by protoc from the build's perfetto_trace.proto.

import importlib
import os
import platform
import subprocess
import sys
import json
from collections import Counter

# tools/unitrace, three levels up (test/trace_utils/ -> test/ -> tools/unitrace/).
_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
_UNITRACE_DIR = os.path.normpath(os.path.join(_THIS_DIR, "..", ".."))
_BUILD_DIR = os.path.join(_UNITRACE_DIR, "build")


class Range:
  def __init__(self, start, dur):
    self.start = start
    self.end = start + dur


def calc_stats(name, rangevec):
  min_gap = 0
  max_gap = 0
  idle = 0
  total = 0
  busy = 0
  if (len(rangevec) > 0):
    v = sorted(rangevec, key=lambda r: r.start)
    first_ts = v[0].start
    last_ts = v[len(v) - 1].end
    total = last_ts - first_ts
    busy = v[0].end - v[0].start
    for i in range(1, len(v)):
      busy += v[i].end - v[i].start
      gap = v[i].start - v[i-1].end
      if (i == 1):
        min_gap = gap
        max_gap = gap
      elif (gap < min_gap):
        min_gap = gap
      elif (gap > max_gap):
        max_gap = gap
      if (gap > 0):
        idle += gap
  return {"name": name,
          "total": total,
          "busy": busy,
          "idle": idle,
          "min_gap": min_gap,
          "max_gap": max_gap,
          "overlaps": busy - (total - idle)
         }


def parse_timeline_stats(filename):
  thread_names = {}
  threads = {}

  with open(filename, 'r') as fp:
    try:
      # strict=False: tolerate raw control chars (e.g. NUL in OpenCL device names).
      data = json.loads(fp.read(), strict=False)
    except Exception as ex:
      print("File parse errors")
      exit(-1)
    if 'traceEvents' in data:
      for e in data['traceEvents']:
        if (e["ph"] == "M"):
          if (e["name"] == "thread_name"):
            pid_k = "{}_{}".format(e["pid"], e["tid"])
            thread_names[pid_k] = e["args"]["name"]
          elif (e["name"] == "process_name"):
            pid_k = "{}_{}".format(e["pid"], e["pid"])
            thread_names[pid_k] = e["args"]["name"]
        elif (e["ph"] == "X" and e["cat"] == "gpu_op" and "pid" in e and "tid" in e):
          pid_k = "{}_{}".format(e["pid"], e["tid"])
          if (pid_k not in threads):
            threads[pid_k] = []
          r = Range(e["ts"], e["dur"])
          threads[pid_k].append(r)
    stats = []
    for k in threads:
      if (len(threads[k]) > 0):
        stats.append(calc_stats(thread_names.get(k, k), threads[k]))
    return stats


def _import_perfetto_trace_pb2():
  # Generate Python bindings on demand from the perfetto_trace.proto and protoc
  # that the unitrace build produced. Resolve both from the build tree (env
  # overrides UNITRACE_PERFETTO_PROTO / UNITRACE_PROTOC take precedence). There
  # is no PATH fallback: protobuf support is a build requirement, so a missing
  # proto/protoc is a hard error rather than a silent skip.
  proto = os.environ.get("UNITRACE_PERFETTO_PROTO") or \
      os.path.join(_BUILD_DIR, "perfetto", "perfetto_trace.proto")
  protoc_exe = "protoc.exe" if platform.system() == "Windows" else "protoc"
  protoc = os.environ.get("UNITRACE_PROTOC") or \
      os.path.join(_BUILD_DIR, "protobuf-install", "bin", protoc_exe)
  gen_dir = os.path.join(_THIS_DIR, "_perfetto_pb2")
  module = os.path.join(gen_dir, "perfetto_trace_pb2.py")

  if not os.path.exists(module):
    if not os.path.exists(proto):
      print("[ERROR] perfetto_trace.proto not found at {} (build unitrace with "
            "BUILD_WITH_PERFETTO=ON, or set UNITRACE_PERFETTO_PROTO).".format(proto))
      exit(-1)
    if not os.path.exists(protoc):
      print("[ERROR] protoc not found at {} (build unitrace with "
            "BUILD_WITH_PERFETTO=ON, or set UNITRACE_PROTOC).".format(protoc))
      exit(-1)
    os.makedirs(gen_dir, exist_ok=True)
    try:
      subprocess.run([protoc, "--python_out=" + gen_dir,
                      "--proto_path=" + os.path.dirname(proto), proto], check=True)
    except subprocess.CalledProcessError:
      print("[ERROR] failed to run protoc to generate perfetto_trace_pb2.")
      exit(-1)

  if gen_dir not in sys.path:
    sys.path.insert(0, gen_dir)
  try:
    return importlib.import_module("perfetto_trace_pb2")
  except ImportError:
    print("[ERROR] could not import perfetto_trace_pb2 (the 'protobuf' pip "
          "package is required).")
    exit(-1)


def parse_timeline_stats_perfetto(filename):
  # Parse a Perfetto protobuf trace directly (no trace_processor dependency).
  # The file is a sequence of single-packet Trace messages concatenated; protobuf
  # message concatenation is itself a valid Trace, so one parse yields all packets.
  # We pair TYPE_SLICE_BEGIN/END per track with a stack (Perfetto's nesting model)
  # and compute the same per-track stats as the JSON path, in int ns.
  pb = _import_perfetto_trace_pb2()
  TE = pb.TrackEvent
  with open(filename, "rb") as fp:
    trace = pb.Trace()
    trace.ParseFromString(fp.read())

  track_names = {}    # uuid -> name (from TrackDescriptor)
  open_slices = {}    # uuid -> stack of open begin timestamps
  tracks = {}         # uuid -> list[Range]
  for packet in trace.packet:
    if packet.HasField("track_descriptor"):
      d = packet.track_descriptor
      name = d.name
      if not name and d.HasField("thread"):
        name = d.thread.thread_name
      if not name and d.HasField("process"):
        name = d.process.process_name
      track_names[d.uuid] = name or str(d.uuid)
    elif packet.HasField("track_event"):
      ev = packet.track_event
      uuid = ev.track_uuid
      if ev.type == TE.TYPE_SLICE_BEGIN:
        open_slices.setdefault(uuid, []).append(packet.timestamp)
      elif ev.type == TE.TYPE_SLICE_END:
        stack = open_slices.get(uuid)
        if stack:
          start = stack.pop()
          tracks.setdefault(uuid, []).append(Range(start, packet.timestamp - start))

  stats = []
  for uuid in tracks:
    if len(tracks[uuid]) > 0:
      stats.append(calc_stats(track_names.get(uuid, str(uuid)), tracks[uuid]))
  return stats


# ---- format-equivalence fingerprints --------------------------------------
# A fingerprint is the run- and format-invariant signal used to assert json and
# protobuf describe the same work: the slice-NAME MULTISET (count per operation
# name). Track identity / lane packing / timestamps are NOT compared -- they
# differ across the two separate runs and across the formats by design.

def _summarize(tracks):
  slice_names = Counter()
  for slices in tracks.values():
    for s in slices:
      slice_names[s] += 1
  return {"num_tracks": len(tracks), "slice_names": slice_names}


def fingerprint_json(path):
  # strict=False: the OpenCL path can embed raw control characters in kernel
  # names/args, which the default (strict) decoder rejects; harmless here since we
  # only look at slice names.
  with open(path) as f:
    data = json.loads(f.read(), strict=False)
  tracks = {}
  for e in data.get("traceEvents", []):
    if e.get("ph") == "X" and e.get("cat") in ("gpu_op", "cpu_op"):
      k = "{}_{}".format(e.get("pid"), e.get("tid"))
      tracks.setdefault(k, []).append(e.get("name", ""))
  return _summarize(tracks)


def fingerprint_protobuf(path):
  pb = _import_perfetto_trace_pb2()
  TE = pb.TrackEvent
  trace = pb.Trace()
  with open(path, "rb") as f:
    trace.ParseFromString(f.read())
  tracks = {}
  for p in trace.packet:
    if p.HasField("track_event"):
      ev = p.track_event
      if ev.type == TE.TYPE_SLICE_BEGIN:
        tracks.setdefault(ev.track_uuid, []).append(ev.name)
  return _summarize(tracks)


# ---- protobuf flow-arrow validation ---------------------------------------
# unitrace emits only H2D flows: the same flow_id on a host SUBMIT slice (cpu_op)
# and the device kernel slice (gpu_op). Perfetto infers direction from slice
# begin-timestamp (earlier = source), so a correct H2D flow has its EARLIEST slice
# be the host submit (arrow host->device) and touches >= 2 slices (else no arrow).
# D2H (kernel->wait) flows are intentionally not emitted (they would render
# backwards; see UNITRACEI-107), so terminating_flow_ids must be absent.
# Returns (n_flows, errors).
def validate_protobuf_flows(path):
  pb = _import_perfetto_trace_pb2()
  TE = pb.TrackEvent
  trace = pb.Trace()
  with open(path, "rb") as f:
    trace.ParseFromString(f.read())

  host_tracks = set()
  touches = {}          # fid -> list[(ts, is_host)]
  begins = []           # (ts, track_uuid, [flow_ids]) to classify after tracks known
  has_terminating = False
  for p in trace.packet:
    if not p.HasField("track_event"):
      continue
    ev = p.track_event
    if ev.type != TE.TYPE_SLICE_BEGIN:
      continue
    if "cpu_op" in ev.categories:
      host_tracks.add(ev.track_uuid)
    if ev.flow_ids:
      begins.append((p.timestamp, ev.track_uuid, list(ev.flow_ids)))
    if ev.terminating_flow_ids:
      has_terminating = True

  for ts, tu, fids in begins:
    is_host = tu in host_tracks
    for fid in fids:
      touches.setdefault(fid, []).append((ts, is_host))

  errors = []
  if has_terminating:
    errors.append("unexpected terminating_flow_ids present (unitrace emits H2D flow_ids only)")

  n_flows = 0
  orphan = []
  bad_dir = []
  for fid, ts_list in touches.items():
    if len(ts_list) < 2:
      orphan.append(fid)        # a flow id on only one slice draws no arrow
      continue
    n_flows += 1
    # The earliest slice carrying the id must be the host submit (host -> device).
    if not min(ts_list, key=lambda x: x[0])[1]:
      bad_dir.append(fid)
  if orphan:
    errors.append("flow ids on only one slice (no arrow drawn): {}".format(sorted(orphan)[:10]))
  if bad_dir:
    errors.append("H2D flow whose earliest slice is NOT the host submit "
                  "(arrow would point device->host): {}".format(sorted(bad_dir)[:10]))
  return n_flows, errors

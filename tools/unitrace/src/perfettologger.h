//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_UNITRACE_PERFETTO_LOGGER_H_
#define PTI_TOOLS_UNITRACE_PERFETTO_LOGGER_H_

// Perfetto protobuf trace emitter (compiled only when BUILD_WITH_PERFETTO=1).
//
// Builds TracePacket messages directly and appends one single-packet Trace per
// event to a binary Logger; concatenated single-packet Traces are a valid trace,
// so merging per-rank files is byte concatenation.

#include <atomic>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "logger.h"
#include "perfetto_trace.pb.h"

namespace perfetto_emit {

// The binary Logger this emitter appends to. Set once by the ChromeLogger ctor
inline std::shared_ptr<Logger>& EmitLogger() {
  static std::shared_ptr<Logger> logger;
  return logger;
}

// Serializes writes to EmitLogger() (see EmitTrace) so concurrent writer threads
// don't interleave packets in the binary stream.
inline std::recursive_mutex& EmitLock() {
  static std::recursive_mutex lock;
  return lock;
}

// Perfetto BuiltinClock ids. REALTIME is globally meaningful (NTP) for cross-rank
// sync; MONOTONIC_RAW is the fallback under UNITRACE_SystemTime=1. The
// ClockSnapshot must relate the event clock to BOOTTIME, else the trace processor
// drops every event with clock_sync_failure_no_path.
constexpr uint32_t kClockRealtime    = 1;  // BUILTIN_CLOCK_REALTIME
constexpr uint32_t kClockMonotonicRaw = 5;  // BUILTIN_CLOCK_MONOTONIC_RAW
constexpr uint32_t kClockBoottime    = 6;  // BUILTIN_CLOCK_BOOTTIME

// The clock domain every timestamp in this process resolves to
inline uint32_t& ClockId() {
  static uint32_t clock_id = kClockRealtime;
  return clock_id;
}

// Deterministic, stable 64-bit track UUID from integer key parts (FNV-1a); keyed
// on the per-rank-unique device pid/tid so tracks stay distinct across ranks.
inline uint64_t MakeUuid(uint64_t a, uint64_t b = 0, uint64_t c = 0) {
  uint64_t h = 1469598103934665603ull;  // FNV offset basis
  const uint64_t prime = 1099511628211ull;
  const uint64_t parts[3] = {a, b, c};
  for (uint64_t p : parts) {
    for (int i = 0; i < 8; i++) {
      h ^= (p >> (i * 8)) & 0xff;
      h *= prime;
    }
  }
  // Avoid 0 (Perfetto treats uuid 0 as "unset").
  return h ? h : 1;
}

// Per-writer-thread packet sequence id, so each thread's slice begin/end pairs
// stack correctly per track.
inline uint32_t NextSequenceId() {
  static std::atomic<uint32_t> next{1};
  return next.fetch_add(1, std::memory_order_relaxed);
}

// One shared sequence id for TrackDescriptors and the ClockSnapshot, which are
// sequence-independent (global) in the trace processor.
inline uint32_t DescriptorSeqId() {
  static uint32_t id = NextSequenceId();
  return id;
}

// TODO(perf): reuse a per-thread Trace/string to avoid the per-event allocation. It
//   must NOT be a lazily-initialized thread_local object: first-touch TLS init runs
//   under the Windows loader lock, deadlocking with logger_lock_.

// Serialize one single-packet Trace and append it to the binary logger.
inline void EmitTrace(const perfetto::protos::Trace& trace) {
  std::string out;
  if (!trace.SerializeToString(&out)) {
    std::cerr << "[WARNING] failed to serialize a Perfetto trace packet; "
                 "dropping one event from the timeline." << std::endl;
    return;
  }
  std::lock_guard<std::recursive_mutex> lock(EmitLock());
  auto& logger = EmitLogger();
  if (logger) {
    logger->Log(out.data(), out.size());
  }
}

// Common packet scaffolding: set timestamp + clock domain + sequence id.
inline perfetto::protos::TracePacket* NewPacket(perfetto::protos::Trace& trace,
                                                uint64_t ts_ns, uint32_t seq_id) {
  auto* packet = trace.add_packet();
  packet->set_timestamp(ts_ns);
  packet->set_timestamp_clock_id(ClockId());
  packet->set_trusted_packet_sequence_id(seq_id);
  return packet;
}

// Emit a ClockSnapshot relating the event clock to BOOTTIME (the trace
// processor's base); without this pair every event is dropped with
// clock_sync_failure_no_path.
inline void EmitClockSnapshot(uint64_t event_ts, uint64_t boottime_ts, uint32_t seq_id) {
  perfetto::protos::Trace trace;
  auto* packet = trace.add_packet();
  packet->set_trusted_packet_sequence_id(seq_id);
  auto* snap = packet->mutable_clock_snapshot();
  auto* ev_clock = snap->add_clocks();
  ev_clock->set_clock_id(ClockId());
  ev_clock->set_timestamp(event_ts);
  if (ClockId() != kClockBoottime) {
    auto* boot_clock = snap->add_clocks();
    boot_clock->set_clock_id(kClockBoottime);
    boot_clock->set_timestamp(boottime_ts);
  }
  EmitTrace(trace);
}

// Emit a process TrackDescriptor (embeds ProcessDescriptor).
// uuid identifies the track.
inline void EmitProcessTrack(uint64_t uuid, int32_t pid, const std::string& name,
                             uint64_t ts_ns, uint32_t seq_id) {
  perfetto::protos::Trace trace;
  auto* packet = NewPacket(trace, ts_ns, seq_id);
  auto* desc = packet->mutable_track_descriptor();
  desc->set_uuid(uuid);
  desc->set_name(name);
  auto* proc = desc->mutable_process();
  proc->set_pid(pid & 0x7FFFFFFF);
  proc->set_process_name(name);
  EmitTrace(trace);
}

// Emit a thread TrackDescriptor (embeds ThreadDescriptor). parent_uuid links the
// thread track to its process track.
inline void EmitThreadTrack(uint64_t uuid, uint64_t parent_uuid, int32_t pid,
                            int32_t tid, const std::string& name, uint64_t ts_ns,
                            uint32_t seq_id) {
  perfetto::protos::Trace trace;
  auto* packet = NewPacket(trace, ts_ns, seq_id);
  auto* desc = packet->mutable_track_descriptor();
  desc->set_uuid(uuid);
  desc->set_parent_uuid(parent_uuid);
  desc->set_name(name);
  auto* thread = desc->mutable_thread();
  thread->set_pid(pid & 0x7FFFFFFF);
  thread->set_tid(tid & 0x7FFFFFFF);
  thread->set_thread_name(name);
  EmitTrace(trace);
}

// A key/value debug annotation to attach to a slice. Only one value field is
// used per annotation (mirrors the proto's value oneof).
struct Annotation {
  enum Kind { kString, kInt, kUint } kind = kString;
  std::string name;
  std::string str_value;
  int64_t int_value = 0;
  uint64_t uint_value = 0;

  static Annotation Str(const std::string& n, const std::string& v) {
    Annotation a; a.kind = kString; a.name = n; a.str_value = v; return a;
  }
  static Annotation Int(const std::string& n, int64_t v) {
    Annotation a; a.kind = kInt; a.name = n; a.int_value = v; return a;
  }
  static Annotation Uint(const std::string& n, uint64_t v) {
    Annotation a; a.kind = kUint; a.name = n; a.uint_value = v; return a;
  }
};

inline void ApplyAnnotations(perfetto::protos::TrackEvent* ev,
                             const std::vector<Annotation>& annotations) {
  for (const auto& a : annotations) {
    auto* da = ev->add_debug_annotations();
    da->set_name(a.name);
    switch (a.kind) {
      case Annotation::kString: da->set_string_value(a.str_value); break;
      case Annotation::kInt:    da->set_int_value(a.int_value); break;
      case Annotation::kUint:   da->set_uint_value(a.uint_value); break;
    }
  }
}


// Optional fields shared by slice-begin / instant events;
struct SliceOptions {
  std::string name;
  std::string category;                 // -> TrackEvent.categories
  std::vector<Annotation> annotations;  // -> debug_annotations
  std::vector<uint64_t> flow_ids;       // -> TrackEvent.flow_ids (flow arrows)
};

// Emit one TrackEvent of |type| on |track_uuid|. The single builder behind
// EmitSliceBegin/End/Instant.
inline void EmitTrackEvent(uint32_t seq_id, uint64_t track_uuid, uint64_t ts_ns,
                           perfetto::protos::TrackEvent::Type type,
                           const SliceOptions& opts) {
  perfetto::protos::Trace trace;
  auto* packet = NewPacket(trace, ts_ns, seq_id);
  auto* ev = packet->mutable_track_event();
  ev->set_type(type);
  ev->set_track_uuid(track_uuid);
  if (!opts.name.empty()) {
    ev->set_name(opts.name);
  }
  if (!opts.category.empty()) {
    ev->add_categories(opts.category);
  }
  for (uint64_t fid : opts.flow_ids) {
    ev->add_flow_ids(fid);
  }
  ApplyAnnotations(ev, opts.annotations);
  EmitTrace(trace);
}

// Begin of a duration slice on track_uuid.
inline void EmitSliceBegin(uint32_t seq_id, uint64_t track_uuid, uint64_t ts_ns,
                           const SliceOptions& opts) {
  EmitTrackEvent(seq_id, track_uuid, ts_ns,
                 perfetto::protos::TrackEvent::TYPE_SLICE_BEGIN, opts);
}

// End of a duration slice on track_uuid. The begin/end pair is matched by
// track + order, so the end carries no name/category.
inline void EmitSliceEnd(uint32_t seq_id, uint64_t track_uuid, uint64_t ts_ns) {
  perfetto::protos::Trace trace;
  auto* packet = NewPacket(trace, ts_ns, seq_id);
  auto* ev = packet->mutable_track_event();
  ev->set_type(perfetto::protos::TrackEvent::TYPE_SLICE_END);
  ev->set_track_uuid(track_uuid);
  EmitTrace(trace);
}

// A zero-duration mark on track_uuid.
inline void EmitInstant(uint32_t seq_id, uint64_t track_uuid, uint64_t ts_ns,
                        const SliceOptions& opts) {
  EmitTrackEvent(seq_id, track_uuid, ts_ns,
                 perfetto::protos::TrackEvent::TYPE_INSTANT, opts);
}

}  // namespace perfetto_emit

#endif  // PTI_TOOLS_UNITRACE_PERFETTO_LOGGER_H_

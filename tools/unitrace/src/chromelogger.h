//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_UNITRACE_CHROME_LOGGER_H_
#define PTI_TOOLS_UNITRACE_CHROME_LOGGER_H_

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>
#include <thread>
#include <tuple>
#include <set>
#include <vector>
#include "unitimer.h"
#include "unikernel.h"
#include "unievent.h"
#include "unimemory.h"
#include "logger_factory.h"
#include "utils_host.h"

#include "common_header.gen"

#if BUILD_WITH_PERFETTO
#include "perfettologger.h"
#endif /* BUILD_WITH_PERFETTO */

static constexpr uint32_t num_device_timestamps_cahced_ = 1024;

#ifdef _WIN32
#define strdup _strdup
#else /* _WIN32 */
#define strdup strdup
#endif /* _WIN32 */

static std::string EncodeURI(const std::string &input) {
  std::ostringstream encoded;
  encoded.fill('0');
  encoded << std::hex;

  for (auto c : input) {
    // accepted characters
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded << c;
      continue;
    }

    // percent-encoded otherwise
    encoded << std::uppercase;
    encoded << '%' << std::setw(2) << int(c);
    encoded << std::nouppercase;
  }

  return encoded.str();
}

static std::string rank = (utils::GetEnv("PMI_RANK").empty()) ? utils::GetEnv("PMIX_RANK") : utils::GetEnv("PMI_RANK");
#ifdef _WIN32
static uint32_t mpi_rank = rank.empty() ? _getpid() : std::atoi(rank.c_str());  // use pid as a dummy rank id
#else /* _WIN32 */
static uint32_t mpi_rank = rank.empty() ? getpid() : std::atoi(rank.c_str());  // use pid as a dummy rank id
#endif /* _WIN32 */

static std::string pmi_hostname = GetHostName();

std::string GetZeKernelCommandName(uint64_t id, ze_group_count_t& group_count, size_t size, bool detailed);
ze_pci_ext_properties_t *GetZeDevicePciPropertiesAndId(ze_device_handle_t device, int32_t *parent_device_id, int32_t *device_id, int32_t *subdevice_id);
std::string GetClKernelCommandName(uint64_t id);
std::string GetZeDeviceName(ze_device_handle_t device);
std::string GetZeEngineName(ze_device_handle_t device, uint32_t ordinal);
#if BUILD_WITH_OMP
extern const char *GetOmptEventName(uint32_t);
#endif /* BUILD_WITH_OMP */

static std::shared_ptr<Logger> logger_ = nullptr;

std::recursive_mutex logger_lock_; //lock to synchronize file write

static bool device_logging_no_thread_ = (utils::GetEnv("UNITRACE_ChromeNoThreadOnDevice") == "1") ? true : false;
static bool device_logging_no_engine_ = (utils::GetEnv("UNITRACE_ChromeNoEngineOnDevice") == "1") ? true : false;

static std::mutex device_pid_tid_map_lock_;

struct ZeDevicePidKey {
  ze_pci_address_ext_t pci_addr_;
  int32_t parent_device_id_;
  int32_t device_id_;
  int32_t subdevice_id_;
  int32_t host_pid_;
};

struct ZeDeviceTidKey {
  ze_pci_address_ext_t pci_addr_;
  int32_t parent_device_id_;
  int32_t device_id_;
  int32_t subdevice_id_;
  uint32_t engine_ordinal_;
  uint32_t engine_index_;
  int32_t host_pid_;
  int32_t host_tid_;
  uint32_t track_id_;
};

struct ZeDevicePidKeyCompare {
  bool operator()(const ZeDevicePidKey& lhs, const ZeDevicePidKey& rhs) const {
    return (memcmp((char *)(&lhs), (char *)(&rhs), sizeof(ZeDevicePidKey)) < 0);
  }
};
static std::map<ZeDevicePidKey, std::tuple<uint32_t, uint64_t>, ZeDevicePidKeyCompare> device_pid_map_;
struct ZeDeviceTidKeyCompare {
  bool operator()(const ZeDeviceTidKey& lhs, const ZeDeviceTidKey& rhs) const {
    return (memcmp((char *)(&lhs), (char *)(&rhs), sizeof(ZeDeviceTidKey)) < 0);
  }
};

static std::map<ZeDeviceTidKey, std::tuple<uint32_t, uint32_t, uint64_t>, ZeDeviceTidKeyCompare> device_tid_map_;

static uint32_t next_device_pid_ = (uint32_t)(~0) - (mpi_rank << 5);  // each rank uses no more than 32 devices
static uint32_t next_device_tid_ = (uint32_t)(~0) - (mpi_rank << 5);  // the first device thread is the "main" thread which has the same id as the device process id

// Emit a device process/thread metadata record in the active format (Perfetto
// track vs JSON "M").
static void EmitDeviceProcessMetadata(uint32_t device_pid, const std::string& name,
                                      double start_time) {
#if BUILD_WITH_PERFETTO
  if (UseProtobufOutput()) {
    // Called once per device pid (the device_pid_map_ miss branch), so emit the
    // descriptor directly -- no MarkTrackEmitted dedup needed.
    perfetto_emit::EmitProcessTrack(perfetto_emit::MakeUuid(device_pid),
                                    device_pid, name,
                                    UniTimer::GetEpochTime(UniTimer::GetHostTimestamp()),
                                    perfetto_emit::DescriptorSeqId());
    return;
  }
#endif /* BUILD_WITH_PERFETTO */
  logger_->Log(",\n{\"ph\": \"M\", \"name\": \"process_name\", \"pid\": " + std::to_string(device_pid) +
               ", \"ts\": " + std::to_string(start_time) + ", \"args\": {\"name\": \"" + name + "\"}}");
  logger_->Flush();
}

static void EmitDeviceThreadMetadata(uint32_t device_pid, uint32_t device_tid,
                                     const std::string& name, double start_time) {
#if BUILD_WITH_PERFETTO
  if (UseProtobufOutput()) {
    // Called once per device tid (the device_tid_map_ miss branch), so emit the
    // descriptor directly -- no MarkTrackEmitted dedup needed.
    perfetto_emit::EmitThreadTrack(perfetto_emit::MakeUuid(device_pid, device_tid),
                                   perfetto_emit::MakeUuid(device_pid),
                                   device_pid, device_tid, name,
                                   UniTimer::GetEpochTime(UniTimer::GetHostTimestamp()),
                                   perfetto_emit::DescriptorSeqId());
    return;
  }
#endif /* BUILD_WITH_PERFETTO */
  logger_->Log(",\n{\"ph\": \"M\", \"name\": \"thread_name\", \"pid\": " + std::to_string(device_pid) + ", \"tid\": " +
               std::to_string(device_tid) + ", \"ts\": " + std::to_string(start_time) + ", \"args\": {\"name\": \"" +
               name + "\"}}");
  logger_->Flush();
}

static std::tuple<uint32_t, uint32_t> GetDevicePidTid(ze_device_handle_t device, uint32_t engine_ordinal, uint32_t engine_index, int host_pid, int host_tid, uint32_t track_id) {
  if (device_logging_no_thread_) {
    // map all threads to the process
    host_tid = host_pid;
  }

  if (device_logging_no_engine_) {
    // ignore engine ordinal and index
    engine_ordinal = (uint32_t)(-1);
    engine_index = (uint32_t)(-1);
  }

  uint32_t device_pid;
  uint32_t device_tid;
  const std::lock_guard<std::mutex> lock(device_pid_tid_map_lock_);

  ze_pci_ext_properties_t *props;
  int32_t device_id;
  int32_t parent_device_id;
  int32_t subdevice_id;
  props = GetZeDevicePciPropertiesAndId(device, &parent_device_id, &device_id, &subdevice_id);
  PTI_ASSERT(props != nullptr);

  ZeDeviceTidKey tid_key;

  memset(&tid_key, 0, sizeof(ZeDeviceTidKey));
  tid_key.pci_addr_ = props->address;
  tid_key.parent_device_id_ = parent_device_id;
  tid_key.device_id_ = device_id;
  tid_key.subdevice_id_ = subdevice_id;
  tid_key.engine_ordinal_ = engine_ordinal;
  tid_key.engine_index_ = engine_index;
  tid_key.host_pid_ = host_pid;
  tid_key.host_tid_ = host_tid;
  tid_key.track_id_ = track_id;

  auto it = device_tid_map_.find(tid_key);
  if (it != device_tid_map_.cend()) {
    device_pid = std::get<0>(it->second);
    device_tid = std::get<1>(it->second);
  }
  else {
    ZeDevicePidKey pid_key;

    memset(&pid_key, 0, sizeof(ZeDevicePidKey));
    pid_key.pci_addr_ = props->address;
    pid_key.parent_device_id_ = parent_device_id;
    pid_key.device_id_ = device_id;
    pid_key.subdevice_id_ = subdevice_id;
    pid_key.host_pid_ = host_pid;

    auto it2 = device_pid_map_.find(pid_key);
    if (it2 != device_pid_map_.cend()) {
      device_pid = std::get<0>(it2->second);
    }
    else {
      device_pid = next_device_pid_--;
      auto start_time = UniTimer::GetEpochTimeInUs(UniTimer::GetHostTimestamp());
      device_pid_map_.insert({pid_key, std::make_tuple(device_pid, start_time)});

      std::lock_guard<std::recursive_mutex> lock(logger_lock_);

      // Build the device process display name (shared by both output formats).
      std::string device_proc_name;
      if (rank.empty()) {
        device_proc_name = "DEVICE<" + pmi_hostname + ">";
      }
      else {
        device_proc_name = "RANK " + std::to_string(mpi_rank) + " DEVICE<" + pmi_hostname + ">";
      }

      std::string device_name = GetZeDeviceName(device);
      if (device_name.size() > 0) {
        device_proc_name += "[" + device_name + "] ";
      }

      char str2[128];
      snprintf(str2, sizeof(str2), "%x", pid_key.pci_addr_.domain);
      device_proc_name += std::string(str2) + ":";
      snprintf(str2, sizeof(str2), "%x", pid_key.pci_addr_.bus);
      device_proc_name += std::string(str2) + ":";
      snprintf(str2, sizeof(str2), "%x", pid_key.pci_addr_.device);
      device_proc_name += std::string(str2) + ":";
      snprintf(str2, sizeof(str2), "%x", pid_key.pci_addr_.function);
      device_proc_name += std::string(str2);

      if (pid_key.parent_device_id_ >= 0) {
        device_proc_name += " #" + std::to_string(pid_key.parent_device_id_) + "." + std::to_string(pid_key.subdevice_id_);
      }
      else {
        device_proc_name += " #" + std::to_string(pid_key.device_id_);
      }

      EmitDeviceProcessMetadata(device_pid, device_proc_name, start_time);
    }

    device_tid = next_device_tid_--;
    auto start_time = UniTimer::GetEpochTimeInUs(UniTimer::GetHostTimestamp());
    device_tid_map_.insert({tid_key, std::make_tuple(device_pid, device_tid, start_time)});

    std::lock_guard<std::recursive_mutex> lock(logger_lock_);

    // Build the device thread display name (shared by both output formats).
    std::string device_thread_name;
    if (device_logging_no_thread_) {
      if (device_logging_no_engine_) {
        device_thread_name = "L0";
      } else {
        device_thread_name = GetZeEngineName(device, tid_key.engine_ordinal_);
        device_thread_name += "<" + std::to_string(tid_key.engine_ordinal_) + "," + std::to_string(tid_key.engine_index_) + ">";
      }
    } else {
      if (device_logging_no_engine_) {
        device_thread_name = "Thread " + std::to_string(tid_key.host_tid_) + " L0";
      } else {
        device_thread_name = "Thread " + std::to_string(tid_key.host_tid_);
        device_thread_name += " " + GetZeEngineName(device, tid_key.engine_ordinal_);
        device_thread_name += "<" + std::to_string(tid_key.engine_ordinal_) + "," + std::to_string(tid_key.engine_index_) + ">";
      }
    }

    EmitDeviceThreadMetadata(device_pid, device_tid, device_thread_name, start_time);
  }

  return std::tuple<uint32_t, uint32_t>(device_pid, device_tid);
}

#if BUILD_WITH_OPENCL

struct ClDevicePidKey {
  cl_device_pci_bus_info_khr pci_addr_;
  cl_device_id device_;
  int32_t host_pid_;
};

struct ClDeviceTidKey {
  cl_device_pci_bus_info_khr pci_addr_;
  cl_device_id device_;
  cl_command_queue queue_;
  int32_t host_pid_;
  int32_t host_tid_;
  uint32_t track_id_;
};

std::string GetClDeviceName(cl_device_id device);

struct ClDevicePidKeyCompare {
  bool operator()(const ClDevicePidKey& lhs, const ClDevicePidKey& rhs) const {
    return (memcmp((char *)(&lhs), (char *)(&rhs), sizeof(ClDevicePidKey)) < 0);
  }
};
static std::map<ClDevicePidKey, std::tuple<uint32_t, uint64_t>, ClDevicePidKeyCompare> cl_device_pid_map_;
struct ClDeviceTidKeyCompare {
  bool operator()(const ClDeviceTidKey& lhs, const ClDeviceTidKey& rhs) const {
    return (memcmp((char *)(&lhs), (char *)(&rhs), sizeof(ClDeviceTidKey)) < 0);
  }
};

static std::map<ClDeviceTidKey, std::tuple<uint32_t, uint32_t, uint64_t>, ClDeviceTidKeyCompare> cl_device_tid_map_;

static std::tuple<uint32_t, uint32_t> ClGetDevicePidTid(cl_device_pci_bus_info_khr& pci, cl_device_id device, cl_command_queue queue, int host_pid, int host_tid, uint32_t track_id) {
  if (device_logging_no_thread_) {
    // map all threads to the process
    host_tid = host_pid;
  }

  if (device_logging_no_engine_) {
    // ignore engine ordinal and index
    queue = (cl_command_queue)(-1);
  }

  uint32_t device_pid;
  uint32_t device_tid;
  const std::lock_guard<std::mutex> lock(device_pid_tid_map_lock_);

  ClDeviceTidKey tid_key;

  memset(&tid_key, 0, sizeof(ClDeviceTidKey));
  tid_key.pci_addr_ = pci;
  tid_key.device_ = device;
  tid_key.queue_ = queue;
  tid_key.host_pid_ = host_pid;
  tid_key.host_tid_ = host_tid;
  tid_key.track_id_ = track_id;

  auto it = cl_device_tid_map_.find(tid_key);
  if (it != cl_device_tid_map_.cend()) {
    device_pid = std::get<0>(it->second);
    device_tid = std::get<1>(it->second);
  }
  else {
    ClDevicePidKey pid_key;

    memset(&pid_key, 0, sizeof(ClDevicePidKey));
    pid_key.pci_addr_ = pci;
    pid_key.device_ = device;
    pid_key.host_pid_ = host_pid;
    auto it2 = cl_device_pid_map_.find(pid_key);
    if (it2 != cl_device_pid_map_.cend()) {
      device_pid = std::get<0>(it2->second);
    }
    else {
      device_pid = next_device_pid_--;
      auto start_time = UniTimer::GetEpochTimeInUs(UniTimer::GetHostTimestamp());
      cl_device_pid_map_.insert({pid_key, std::make_tuple(device_pid, start_time)});

      std::lock_guard<std::recursive_mutex> lock(logger_lock_);

      // Build the device process display name (shared by both output formats).
      std::string device_proc_name;
      if (rank.empty()) {
        device_proc_name = "DEVICE<" + pmi_hostname + ">";
      }
      else {
        device_proc_name = "RANK " + std::to_string(mpi_rank) + " DEVICE<" + pmi_hostname + ">";
      }

      std::string device_name = GetClDeviceName(device);
      if (device_name.size() > 0) {
        device_proc_name += "[" + device_name + "] ";
      }

      char str2[128];
      snprintf(str2, sizeof(str2), "%x", pid_key.pci_addr_.pci_domain);
      device_proc_name += std::string(str2) + ":";
      snprintf(str2, sizeof(str2), "%x", pid_key.pci_addr_.pci_bus);
      device_proc_name += std::string(str2) + ":";
      snprintf(str2, sizeof(str2), "%x", pid_key.pci_addr_.pci_device);
      device_proc_name += std::string(str2) + ":";
      snprintf(str2, sizeof(str2), "%x", pid_key.pci_addr_.pci_function);
      device_proc_name += std::string(str2);

      EmitDeviceProcessMetadata(device_pid, device_proc_name, start_time);
    }
    device_tid = next_device_tid_--;
    auto start_time = UniTimer::GetEpochTimeInUs(UniTimer::GetHostTimestamp());
    cl_device_tid_map_.insert({tid_key, std::make_tuple(device_pid, device_tid, start_time)});

    std::lock_guard<std::recursive_mutex> lock(logger_lock_);

    // Build the device thread display name (shared by both output formats).
    std::string device_thread_name;
    if (device_logging_no_thread_) {
      if (device_logging_no_engine_) {
        device_thread_name = "CL";
      }
      else {
        char str2[128];
        snprintf(str2, sizeof(str2), "%p", tid_key.queue_);
        device_thread_name = "CL Queue<" + std::string(str2) + ">";
      }
    }
    else {
      if (device_logging_no_engine_) {
        device_thread_name = "Thread " + std::to_string(tid_key.host_tid_) + " CL";
      }
      else {
        char str2[128];
        snprintf(str2, sizeof(str2), "%p", tid_key.queue_);
        device_thread_name = "Thread " + std::to_string(tid_key.host_tid_) + " CL Queue<" + std::string(str2) + ">";
      }
    }

    EmitDeviceThreadMetadata(device_pid, device_tid, device_thread_name, start_time);
  }

  return std::tuple<uint32_t, uint32_t>(device_pid, device_tid);
}
#endif /* BUILD_WITH_OPENCL */

#if BUILD_WITH_ITT
static std::string ConvertDataToString(IttArgs* args) {
  std::string strData = "";
  void* dataPtr = args->isIndirectData ? args->data[0] : args->data;
  if (args->count) {
    switch (args->type) {
      case __itt_metadata_u64: {
        const uint64_t* uint64Ptr = reinterpret_cast<const uint64_t*>(dataPtr);
        for (size_t i=0; i < args->count; i++) {
          if (i) {
            strData += ",";
          }
          strData += std::to_string(*(uint64Ptr + i));
        }
        break;
      }
      case __itt_metadata_s64: {
        const int64_t* int64Ptr = reinterpret_cast<const int64_t*>(dataPtr);
        for (size_t i=0; i < args->count; i++) {
          if (i) {
            strData += ",";
          }
          strData += std::to_string(*(int64Ptr + i));
        }
        break;
      }
      case __itt_metadata_u32: {
        const uint32_t* uint32Ptr = reinterpret_cast<const uint32_t*>(dataPtr);
        for (size_t i=0; i < args->count; i++) {
          if (i) {
            strData += ",";
          }
          strData += std::to_string(*(uint32Ptr + i));
        }
        break;
      }
      case __itt_metadata_s32: {
        const int32_t* int32Ptr = reinterpret_cast<const int32_t*>(dataPtr);
        for (size_t i=0; i < args->count; i++) {
          if (i) {
            strData += ",";
          }
          strData += std::to_string(*(int32Ptr + i));
        }
        break;
      }
      case __itt_metadata_u16: {
        const uint16_t* uint16Ptr = reinterpret_cast<const uint16_t*>(dataPtr);
        for (size_t i=0; i < args->count; i++) {
          if (i) {
            strData += ",";
          }
          strData += std::to_string(*(uint16Ptr + i));
        }
        break;
      }
      case __itt_metadata_s16: {
        const int16_t* uint16Ptr = reinterpret_cast<const int16_t*>(dataPtr);
        for (size_t i=0; i < args->count; i++) {
          if (i) {
            strData += ",";
          }
          strData += std::to_string(*(uint16Ptr + i));
        }
        break;
      }
      case __itt_metadata_float: {
        const float* floatPtr = reinterpret_cast<const float*>(dataPtr);
        for (size_t i=0; i < args->count; i++) {
          if (i) {
            strData += ",";
          }
          strData += std::to_string(*(floatPtr + i));
        }
        break;
      }
      case __itt_metadata_double: {
        const double* doublePtr = reinterpret_cast<const double*>(dataPtr);
        for (size_t i=0; i < args->count; i++) {
          if (i) {
            strData += ",";
          }
          strData += std::to_string(*(doublePtr + i));
        }
        break;
      }
      default: {  // default is string
        const char* stringPtr = reinterpret_cast<const char*>(dataPtr);
        strData += "\"";
        strData += std::string(stringPtr, args->count);
        strData += "\"";
        break;
      }
    }
  }
  return strData;
}
#else /* BUILD_WITH_ITT */
static std::string ConvertDataToString(IttArgs* args) {
  return "";
}
#endif /* BUILD_WITH_ITT */

#if BUILD_WITH_OMP
static void OmpArgsToString(const OmpArgs &args, std::string &o) {
  switch (args.ompt.type) {
  case ompt_callback_implicit_task:
    o += "\"actual_parallelism\": ";
    o += std::to_string(args.ompt.record.implicit_task.actual_parallelism);
    o += ", \"index\": ";
    o += std::to_string(args.ompt.record.implicit_task.index);
    break;
  case ompt_callback_dispatch:
    if (args.ompt.record.dispatch.kind == ompt_dispatch_iteration) {
      o += "\"iteration\": ";
      o += std::to_string(args.ompt.record.dispatch.instance.value);
    } else if (args.ompt.record.dispatch.kind == ompt_dispatch_section) {
      o += "\"section\": ";
      o += std::to_string(args.ompt.record.dispatch.instance.value);
    } else {
      o += "\"chunk_start\": " + std::to_string(args.data[0]);
      o += ", \"chunk_iterations\": " + std::to_string(args.data[1]);
    }
    break;
  case ompt_callback_task_create:
    o += "\"new_task_id\": ";
    o += std::to_string(args.ompt.record.task_create.new_task_id);
    break;
  case ompt_callback_task_dependence:
    o += "\"src_task_id\": ";
    o += std::to_string(args.ompt.record.task_dependence.src_task_id);
    o += ", \"sink_task_id\": ";
    o += std::to_string(args.ompt.record.task_dependence.sink_task_id);
    break;
  default:
    ;
  }
}
#endif /* BUILD_WITH_OMP */

// comparator for std::pair<start_time, end_time> of device timestamps
struct DeviceTimestampComparator {
  bool operator()(const std::pair<uint64_t, uint64_t>& a, const std::pair<uint64_t, uint64_t>& b) const {
    // sort by end_time ascending, then start_time ascending
    if (a.second != b.second) {
      return a.second < b.second; // primary sort
    }
    return a.first< b.first;   // secondary sort
  }
};

// overlap but not completely nested events go to different tracks
uint32_t GetDeviceEventTrack(std::vector<std::set<std::pair<uint64_t, uint64_t>, DeviceTimestampComparator>>& timestamps, uint64_t start_time, uint64_t end_time) {
  bool track_found = false;
  uint32_t track = 0;
  for (uint32_t i = 0; i < timestamps.size(); i++) {
    auto& rdt = timestamps[i];
    bool overlap = false;
    for (auto it = rdt.rbegin(); it != rdt.rend(); ++it) {
      auto s = std::get<0>(*it);
      auto t = std::get<1>(*it);
      if (start_time > t) {
        // ok to put in this track
        break;
      }
      else {
        if (((start_time < s) && (end_time > s) && (end_time < t)) || ((start_time  > s) && (start_time < t) && (end_time > t))) {
          // overlap but not nested
          // check the next track
          overlap = true;
          break;
        }
      }
    }
    if (!overlap) {
      track_found = true;
      track = i;
      rdt.erase(rdt.begin());
      rdt.insert({start_time, end_time});
      break;
    }
  }

  if (!track_found) {
    // create a new track
    track = timestamps.size();
    timestamps.push_back(std::set<std::pair<uint64_t, uint64_t>, DeviceTimestampComparator>());
    auto& rdt = timestamps[track];
    // populate the recent device timestamps with dummies
    for (int i = 0; i < num_device_timestamps_cahced_; i++) {
      rdt.insert({0, i});
    }
    rdt.erase(rdt.begin());
    rdt.insert({start_time, end_time});
  }

  return track;
}

class TraceBuffer;
std::set<TraceBuffer *> *trace_buffers_ = nullptr;

#define BUFFER_SLICE_SIZE_DEFAULT  (0x1 << 20)

#if BUILD_WITH_PERFETTO
// H2D flow ids (from EVENT_FLOW_SOURCE records) awaiting the next host slice.
// Per host-thread buffer: the callback pushes a call's flow records right before
// its EVENT_COMPLETE on one thread, so they stay contiguous.
struct PendingFlows {
  std::vector<uint64_t> source;  // -> flow_ids on the submit slice
};

// Host process track uuid, scoped by mpi_rank so concatenated per-rank traces
// with repeating pids don't merge (thread tracks use this as parent_uuid).
inline uint64_t HostProcessTrackUuid(uint32_t pid) {
  constexpr uint64_t kHostProcMarker = 0x50524f43;  // "PROC"
  return perfetto_emit::MakeUuid(pid, mpi_rank, kHostProcMarker);
}

// Emit a HostEventRecord as Perfetto TrackEvent(s) on its host thread track.
// Shared by TraceBuffer and ClTraceBuffer; reproduces the JSON host output
// (cpu_op category, "id" arg, MPI/ITT args) and frees name_/ITT data like
// StringifyHostEvent. Flow ids accumulate in |pending| and ride on the next slice.
// |host_track_emitted| is the buffer's once-per-thread flag: the host thread track
// uuid is constant for a buffer (pid/tid fixed), so the descriptor is emitted on
// the first host event only, avoiding a global-locked MarkTrackEmitted probe per
// event.
inline void PerfettoEmitHostEvent(HostEventRecord& rec, uint32_t pid, uint32_t tid,
                                  uint32_t seq_id, PendingFlows& pending,
                                  bool& host_track_emitted) {
  // "HOST" marker keeps host uuids off device uuids; +mpi_rank scopes per rank.
  constexpr uint64_t kHostTrackMarker = 0x484f5354;
  uint64_t track_uuid = perfetto_emit::MakeUuid(pid, tid, kHostTrackMarker + mpi_rank);
  if (!host_track_emitted) {
    perfetto_emit::EmitThreadTrack(track_uuid, HostProcessTrackUuid(pid),
                                   pid, tid, "Thread " + std::to_string(tid),
                                   UniTimer::GetEpochTime(rec.start_time_),
                                   perfetto_emit::DescriptorSeqId());
    host_track_emitted = true;
  }
  uint64_t ts_ns = UniTimer::GetEpochTime(rec.start_time_);

  // Flow records emit no slice of their own. Only H2D (submit) ids are kept; they
  // ride as flow_ids on the EVENT_COMPLETE that follows, and the kernel carries
  // the same id -> Perfetto draws submit -> kernel. D2H (sink) ids are dropped:
  // the host wait begins before the device work it waits on, so a shared flow_id
  // would render backwards (wait -> kernel). The fix is GpuCorrelation (see
  // UNITRACEI-107); until then the submit -> kernel arrow shows the association.
  if (rec.type_ == EVENT_FLOW_SOURCE || rec.type_ == EVENT_FLOW_SINK) {
    if (rec.type_ == EVENT_FLOW_SOURCE) {
      pending.source.push_back(rec.id_);
    }
    if (rec.name_ != nullptr) { free(rec.name_); rec.name_ = nullptr; }
    return;
  }

  perfetto_emit::SliceOptions opts;
  opts.category = "cpu_op";
  if (rec.name_ != nullptr) {
    opts.name = rec.name_;
    if (opts.name.size() >= 2 && opts.name.front() == '"' && opts.name.back() == '"') {
      opts.name = opts.name.substr(1, opts.name.size() - 2);
    }
  } else if ((rec.api_id_ != XptiTracingId) && (rec.api_id_ != IttTracingId)) {
    opts.name = get_symbol(rec.api_id_);
  }
  if (rec.name_ != nullptr) { free(rec.name_); rec.name_ = nullptr; }

  // Build args (mirror the JSON args), replicating ITT free side effects.
  bool has_args = false;
  if (rec.api_type_ == API_TYPE_MPI) {
    const MpiArgs& args = rec.mpi_args_;
    if (args.src_size != 0) {
      opts.annotations.push_back(perfetto_emit::Annotation::Uint("ssize", args.src_size));
      if (args.is_tagged) {
        opts.annotations.push_back(perfetto_emit::Annotation::Int("src", args.src_location));
        opts.annotations.push_back(perfetto_emit::Annotation::Int("stag", args.src_tag));
      }
      has_args = true;
    }
    if (args.dst_size != 0) {
      opts.annotations.push_back(perfetto_emit::Annotation::Uint("dsize", args.dst_size));
      if (args.is_tagged) {
        opts.annotations.push_back(perfetto_emit::Annotation::Int("dst", args.dst_location));
        opts.annotations.push_back(perfetto_emit::Annotation::Int("dtag", args.dst_tag));
      }
      has_args = true;
    }
    if (args.mpi_counter >= 0) {
      opts.annotations.push_back(perfetto_emit::Annotation::Int("mpi_counter", args.mpi_counter));
      has_args = true;
    }
  } else if (rec.api_type_ == API_TYPE_ITT) {
    opts.annotations.push_back(perfetto_emit::Annotation::Str(rec.itt_args_.key, ConvertDataToString(&rec.itt_args_)));
    if (rec.itt_args_.isIndirectData) {
      free(rec.itt_args_.data[0]);
    }
    IttArgs* args = rec.itt_args_.next;
    while (args != nullptr) {
      opts.annotations.push_back(perfetto_emit::Annotation::Str(args->key, ConvertDataToString(args)));
      IttArgs* toFree = args;
      args = args->next;
      free(toFree);
    }
    rec.itt_args_.count = 0;
    rec.api_type_ = API_TYPE_NONE;
    has_args = true;
  }
  // The JSON path emits a top-level "id" only when there are no args.
  if (!has_args) {
    opts.annotations.push_back(perfetto_emit::Annotation::Uint("id", rec.id_));
  }

  if (rec.type_ == EVENT_COMPLETE) {
    // Attach the H2D flow ids accumulated from this call's FLOW_SOURCE records.
    if (!pending.source.empty()) {
      opts.flow_ids = std::move(pending.source);
      pending.source.clear();
    }
    perfetto_emit::EmitSliceBegin(seq_id, track_uuid, ts_ns, opts);
    perfetto_emit::EmitSliceEnd(seq_id, track_uuid, UniTimer::GetEpochTime(rec.end_time_));
  } else if (rec.type_ == EVENT_DURATION_START) {
    perfetto_emit::EmitSliceBegin(seq_id, track_uuid, ts_ns, opts);
  } else if (rec.type_ == EVENT_DURATION_END) {
    perfetto_emit::EmitSliceEnd(seq_id, track_uuid, ts_ns);
  } else if (rec.type_ == EVENT_MARK) {
    perfetto_emit::EmitInstant(seq_id, track_uuid, ts_ns, opts);
  }
}

// Emit one device kernel command as a Perfetto slice (begin+end) on its device
// thread track. The Level Zero and OpenCL device emitters differ only in how
// pid/tid/kname are resolved, so they resolve those and call this with the
// common fields. Carries the gpu_op category, the kernel id arg, optional
// metrics arg, and (non-implicit-scaling) the kid as a flow_id -- the device end
// of the H2D submit -> kernel arrow (the host submit slice carries the same id).
// Shared device-slice emit for both ZE and CL (the wrappers resolve the backend).
inline void PerfettoEmitDeviceSlice(uint32_t seq_id, uint32_t pid, uint32_t tid,
                                    const std::string& kname, uint64_t kid,
                                    bool implicit_scaling, uint32_t tile,
                                    bool metrics_enabled, uint64_t start_time,
                                    uint64_t end_time) {
  perfetto_emit::SliceOptions opts;
  opts.category = "gpu_op";
  if (implicit_scaling) {
    opts.name = "Tile #" + std::to_string(tile) + ": " + kname;
  } else {
    opts.name = kname;
    // The JSON path emits a dep flow tied to kid for non-scaled commands.
    opts.flow_ids.push_back(kid);
  }
  opts.annotations.push_back(perfetto_emit::Annotation::Str("id", std::to_string(kid)));
  if (metrics_enabled) {
    opts.annotations.push_back(perfetto_emit::Annotation::Str(
        "metrics", "http://localhost:8000/" + EncodeURI(kname) + "/" + std::to_string(kid)));
  }

  uint64_t track_uuid = perfetto_emit::MakeUuid(pid, tid);
  perfetto_emit::EmitSliceBegin(seq_id, track_uuid, UniTimer::GetEpochTime(start_time), opts);
  perfetto_emit::EmitSliceEnd(seq_id, track_uuid, UniTimer::GetEpochTime(end_time));
}
#endif /* BUILD_WITH_PERFETTO */

// TODO(refactor): TraceBuffer (Level Zero) and ClTraceBuffer (OpenCL) differ only
// in the record type and device-id resolution; a TraceBuffer templated on the
// record + a device-id trait would fold both into one. Deferred to keep this
// change scoped to the Perfetto output.
class TraceBuffer {
  public:
    TraceBuffer() : flush_immediately_(false) {
      std::string szstr = utils::GetEnv("UNITRACE_ChromeEventBufferSize");
      if (szstr.empty() || (szstr == "-1")) {
        buffer_capacity_ = -1;
        slice_capacity_ = BUFFER_SLICE_SIZE_DEFAULT;
      }
      else {
        buffer_capacity_ = std::stoi(szstr);
        if (buffer_capacity_ == 0) {
          buffer_capacity_ = 1;  // at least one event slot
          flush_immediately_ = true;
        }
        slice_capacity_ = buffer_capacity_;
      }
      ZeKernelCommandExecutionRecord *der = (ZeKernelCommandExecutionRecord *)(malloc(sizeof(ZeKernelCommandExecutionRecord) * slice_capacity_));
      UniMemory::ExitIfOutOfMemory((void *)(der));

      device_event_buffer_.push_back(der);

      HostEventRecord *her = (HostEventRecord *)(malloc(sizeof(HostEventRecord) * slice_capacity_));
      UniMemory::ExitIfOutOfMemory((void *)(her));

      host_event_buffer_.push_back(her);
      tid_= utils::GetTid();
      pid_= utils::GetPid();
#if BUILD_WITH_PERFETTO
      // One packet sequence per writer thread; slices on the same track from the
      // same sequence stack correctly. Per-rank files are already separate.
      seq_id_ = perfetto_emit::NextSequenceId();
#endif /* BUILD_WITH_PERFETTO */

      current_device_event_buffer_slice_ = 0;
      current_host_event_buffer_slice_ = 0;
      next_device_event_index_ = 0;
      next_host_event_index_ = 0;
      device_event_buffer_flushed_ = false;
      host_event_buffer_flushed_ = false;
      finalized_.store(false, std::memory_order_release);
      if ((utils::GetEnv("UNITRACE_MetricQuery") == "1") || (utils::GetEnv("UNITRACE_KernelMetrics") == "1")) {
        metrics_enabled_ = true;
      }
      else {
        metrics_enabled_ = false;
      }

      std::lock_guard<std::recursive_mutex> lock(logger_lock_);  // use this lock to protect trace_buffers_

      if (trace_buffers_ == NULL) {
        trace_buffers_ = new std::set<TraceBuffer *>;
        UniMemory::ExitIfOutOfMemory((void *)(trace_buffers_));
      }
      trace_buffers_->insert(this);
    }

    ~TraceBuffer() {
      std::lock_guard<std::recursive_mutex> lock(logger_lock_);
      if (!finalized_.exchange(true)) {
        // finalize if not finalized
        if (!device_event_buffer_flushed_) {
          for (int i = 0; i < current_device_event_buffer_slice_; i++) {
            for (int j = 0; j < slice_capacity_; j++) {
              FlushDeviceEvent(device_event_buffer_[i][j]);
            }
          }
          for (int j = 0; j < next_device_event_index_; j++) {
            FlushDeviceEvent(device_event_buffer_[current_device_event_buffer_slice_][j]);
          }
          device_event_buffer_flushed_ = true;
        }
        if (!host_event_buffer_flushed_) {
          for (int i = 0; i < current_host_event_buffer_slice_; i++) {
            for (int j = 0; j < slice_capacity_; j++) {
              FlushHostEvent(host_event_buffer_[i][j]);
            }
          }
          for (int j = 0; j < next_host_event_index_; j++) {
            FlushHostEvent(host_event_buffer_[current_host_event_buffer_slice_][j]);
          }
          host_event_buffer_flushed_ = true;
        }
        for (auto& slice : device_event_buffer_) {
          free(slice);
        }
        device_event_buffer_.clear();

        for (auto& slice : host_event_buffer_) {
          free(slice);
        }
        host_event_buffer_.clear();
        trace_buffers_->erase(this);
      }
    }

    TraceBuffer(const TraceBuffer& that) = delete;
    TraceBuffer& operator=(const TraceBuffer& that) = delete;

    // device event timestampes cached are <device, engine_ordinal, engine_index> specific
    std::vector<std::set<std::pair<uint64_t, uint64_t>, DeviceTimestampComparator>>&
    GetRecentDeviceTimestamps(ze_device_handle_t device, uint32_t engine_ordinal, uint32_t engine_index) {
      auto it = recent_device_timestamps_.find({device, engine_ordinal, engine_index});
      if (it == recent_device_timestamps_.end()) {
        auto rdt = std::vector<std::set<std::pair<uint64_t, uint64_t>, DeviceTimestampComparator>>();
        rdt.push_back(std::set<std::pair<uint64_t, uint64_t>, DeviceTimestampComparator>());
        auto& ts = rdt[0];
        // populate the recent device timestamps with dummies
        for (int i = 0; i < num_device_timestamps_cahced_; i++) {
          ts.insert({0, i});
        }
        recent_device_timestamps_.insert({{device, engine_ordinal, engine_index}, std::move(rdt)});

        it = recent_device_timestamps_.find({device, engine_ordinal, engine_index});
      }

      return it->second;
    }

    ZeKernelCommandExecutionRecord *GetDeviceEvent(void) {
      if (next_device_event_index_ ==  slice_capacity_) {
        if (buffer_capacity_ == -1) {
          ZeKernelCommandExecutionRecord *der = (ZeKernelCommandExecutionRecord *)(malloc(sizeof(ZeKernelCommandExecutionRecord) * slice_capacity_));
          UniMemory::ExitIfOutOfMemory((void *)(der));

          device_event_buffer_.push_back(der);
          current_device_event_buffer_slice_++;
          next_device_event_index_ = 0;
        }
        else {
          FlushDeviceBuffer();
        }
      }
      return &(device_event_buffer_[current_device_event_buffer_slice_][next_device_event_index_]);
    }

    HostEventRecord *GetHostEvent(void) {
      if (next_host_event_index_ ==  slice_capacity_) {
        if (buffer_capacity_ == -1) {
          HostEventRecord *her = (HostEventRecord *)(malloc(sizeof(HostEventRecord) * slice_capacity_));
          UniMemory::ExitIfOutOfMemory((void *)(her));
          host_event_buffer_.push_back(her);
          current_host_event_buffer_slice_++;
          next_host_event_index_ = 0;
        }
        else {
          FlushHostBuffer();
        }
      }
      return &(host_event_buffer_[current_host_event_buffer_slice_][next_host_event_index_]);
    }

    void BufferHostEvent(void) {
      if (flush_immediately_) {
        std::lock_guard<std::recursive_mutex> lock(logger_lock_);
        FlushHostEvent(host_event_buffer_[current_host_event_buffer_slice_][next_host_event_index_]);
        // in case that flush_immediately_ is true, only one slice and one event slot, so set the flushed flag to true
        host_event_buffer_flushed_ = true;
      }
      else {
        next_host_event_index_++;
        host_event_buffer_flushed_ = false;
      }
    }

    void BufferDeviceEvent(void) {
      if (flush_immediately_) {
        std::lock_guard<std::recursive_mutex> lock(logger_lock_);
        FlushDeviceEvent(device_event_buffer_[current_device_event_buffer_slice_][next_device_event_index_]);
        // in case that flush_immediately_ is true, only one slice and one event slot, so set the flushed flag to true
        device_event_buffer_flushed_ = true;
      }
      else {
        next_device_event_index_++;
        device_event_buffer_flushed_ = false;
      }
    }

    uint32_t GetTid() { return tid_; }
    uint32_t GetPid() { return pid_; }


    std::string StringifyDeviceEvent(ZeKernelCommandExecutionRecord& rec) {
      auto& rdt = GetRecentDeviceTimestamps(rec.device_, rec.engine_ordinal_, rec.engine_index_);
      uint32_t track = GetDeviceEventTrack(rdt, rec.start_time_, rec.end_time_);
      auto [pid, tid] = GetDevicePidTid(rec.device_, rec.engine_ordinal_, rec.engine_index_, pid_, rec.tid_, track);
      std::string kname = GetZeKernelCommandName(rec.kernel_command_id_, rec.group_count_, rec.mem_size_);
      std::string str = ",\n{";

      str += "\"ph\": \"X\"";
      str += ", \"tid\": " + std::to_string(tid);
      str += ", \"pid\": " + std::to_string(pid);

      if (rec.implicit_scaling_) {
        str += ", \"name\": \"Tile #" + std::to_string(rec.tile_) + ": ";
        if (!kname.empty()) {
          if (kname[0] == '\"') {
            str += kname.substr(1, kname.size() - 2);
          } else {
            str += kname;
          }
        }
        str += "\"";
      } else {
        if (!kname.empty()) {
          if (kname[0] == '\"') {
            // name is already quoted
            str += ", \"name\": " + kname;
          } else {
            str += ", \"name\": \"" + kname + "\"";
          }
        }
      }

      str += ", \"cat\": \"gpu_op\"";
      str += ", \"ts\": " + std::to_string(UniTimer::GetEpochTimeInUs(rec.start_time_));
      str += ", \"dur\": " + std::to_string(UniTimer::GetTimeInUs(rec.end_time_ - rec.start_time_));
      str += ", \"args\": {\"id\": \"" + std::to_string(rec.kid_) + "\"";
      if (metrics_enabled_) {
        // viewing the metrics on the same local host, no need to use https
        str += ", \"metrics\": \"http://localhost:8000/" + EncodeURI(kname) + "/" + std::to_string(rec.kid_) + "\"";
      }
      str += "}}";

      if (!rec.implicit_scaling_) {
        str += ",\n{";
        str += "\"ph\": \"t\"";
        str += ", \"tid\": " + std::to_string(tid);
        str += ", \"pid\": " + std::to_string(pid);
        str += ", \"name\": \"dep\"";
        str += ", \"cat\": \"Flow_H2D_" + std::to_string(rec.kid_) + "_" + std::to_string(mpi_rank) + "\"";
        str += ", \"ts\": " + std::to_string(UniTimer::GetEpochTimeInUs(rec.start_time_));
        str += ", \"id\": " + std::to_string(rec.kid_);
        str += "},\n";

        str += "{";
        str += "\"ph\": \"s\"";
        str += ", \"tid\": " + std::to_string(tid);
        str += ", \"pid\": " + std::to_string(pid);
        str += ", \"name\": \"dep\"";
        str += ", \"cat\": \"Flow_D2H_" + std::to_string(rec.kid_) + "_" + std::to_string(mpi_rank) + "\"";
        str += ", \"ts\": " + std::to_string(UniTimer::GetEpochTimeInUs(rec.start_time_));
        str += ", \"id\": " + std::to_string(rec.kid_);
        str += "}";
      }

      return str;
    }

    std::string StringifyHostEvent(HostEventRecord& rec) {
      std::string str = ",\n{";  // header

      if (rec.type_ == EVENT_COMPLETE) {
        str += "\"ph\": \"X\"";
      } else if (rec.type_ == EVENT_DURATION_START) {
        str += "\"ph\": \"B\"";
      } else if (rec.type_ == EVENT_DURATION_END) {
        str += "\"ph\": \"E\"";
      } else if (rec.type_ == EVENT_FLOW_SOURCE) {
        str += "\"ph\": \"s\"";
      } else if (rec.type_ == EVENT_FLOW_SINK) {
        str += "\"ph\": \"t\"";
      } else if (rec.type_ == EVENT_MARK) {
        str += "\"ph\": \"R\"";
      } else {
        // should never get here
      }

      str += ", \"tid\": " + std::to_string(tid_);
      str += ", \"pid\": " + std::to_string(pid_);

      if (rec.type_ == EVENT_FLOW_SOURCE) {
        str += ", \"name\": \"dep\"";
        str += ", \"cat\": \"Flow_H2D_" + std::to_string(rec.id_) + "_" + std::to_string(mpi_rank) + "\"";
      } else if (rec.type_ == EVENT_FLOW_SINK) {
        str += ", \"name\": \"dep\"";
        str += ", \"cat\": \"Flow_D2H_" + std::to_string(rec.id_) + "_" + std::to_string(mpi_rank) + "\"";
      } else {
        if (rec.name_ != nullptr) {
          if (rec.name_[0] == '\"') {
            // name is already quoted
            str += ", \"name\": " + std::string(rec.name_);
          } else {
            str += ", \"name\": \"" + std::string(rec.name_) + "\"";
          }
#if BUILD_WITH_OMP
        } else if (rec.api_id_ == OmpTracingId) {
          str += ", \"name\": \"" + std::string(GetOmptEventName(rec.id_)) + "\"";
#endif /* BUILD_WITH_OMP */
        } else {
          if ((rec.api_id_ != XptiTracingId) && (rec.api_id_ != IttTracingId)) {
            str += ", \"name\": \"" + get_symbol(rec.api_id_) + "\"";
          }
        }
        str += ", \"cat\": \"cpu_op\"";
      }

      // free rec.name_. It is not needed any more
      if (rec.name_ != nullptr) {
        free(rec.name_);
        rec.name_ = nullptr;
      }

      // It is always present
      str += ", \"ts\": " + std::to_string(UniTimer::GetEpochTimeInUs(rec.start_time_));

      if (rec.type_ == EVENT_COMPLETE) {
        str += ", \"dur\": " + std::to_string(UniTimer::GetTimeInUs(rec.end_time_ - rec.start_time_));
      }

      std::string str_args = "";  // build arguments
      if (rec.api_type_ == API_TYPE_MPI) {
        const MpiArgs& args = rec.mpi_args_;

        bool isFirst = true;  // First argument can be zero and second non zero
        if (args.src_size != 0) {
          str_args += "\"ssize\": " + std::to_string(args.src_size);
          if (args.is_tagged) {
            str_args += ", \"src\": " + std::to_string(args.src_location);
            str_args += ", \"stag\": " + std::to_string(args.src_tag);
          }
          isFirst = false;
        }

        if (args.dst_size != 0) {
          str_args += (isFirst ? "" : ", ");
          str_args += "\"dsize\": " + std::to_string(args.dst_size);
          if (args.is_tagged) {
            str_args += ", \"dst\": " + std::to_string(args.dst_location);
            str_args += ", \"dtag\": " + std::to_string(args.dst_tag);
          }
        }

        if (args.mpi_counter >= 0) {
          str_args += ", \"mpi_counter\": " + std::to_string(args.mpi_counter);
        }
      } else if (rec.api_type_ == API_TYPE_ITT) {
        str_args = str_args + "\"" + rec.itt_args_.key + "\":[";
        str_args += ConvertDataToString(&rec.itt_args_);
        str_args += "]";
        if (rec.itt_args_.isIndirectData) {
          free(rec.itt_args_.data[0]);
        }
        IttArgs* args = rec.itt_args_.next;
        while (args != nullptr) {
          str_args += ",";
          str_args = str_args + "\"" + args->key + "\":[";
          str_args += ConvertDataToString(args);
          str_args += "]";
          IttArgs* toFree = args;
          args = args->next;
          free(toFree);
        }
        // reset count to 0 and type to API_TYPE_NONE
        rec.itt_args_.count = 0;
        rec.api_type_ = API_TYPE_NONE;
#if BUILD_WITH_OMP
      } else if (rec.api_type_ == API_TYPE_OMP) {
        OmpArgsToString(rec.omp_args_, str_args);
#endif /* BUILD_WITH_OMP */
      }

      if (!str_args.empty()) {
        str += ", \"args\": {" + str_args + "}";
      } else {
        str += ", \"id\": " + std::to_string(rec.id_);
      }

      // end
      str += "}";  // footer

      return str;
    }

#if BUILD_WITH_PERFETTO
    // Emit a Level Zero device kernel command as a Perfetto slice. Resolves the
    // ZE-specific pid/tid/kname (reusing the same track allocation as the JSON
    // path) and defers the shared slice emit to PerfettoEmitDeviceSlice.
    void PerfettoEmitDeviceEvent(ZeKernelCommandExecutionRecord& rec) {
      auto& rdt = GetRecentDeviceTimestamps(rec.device_, rec.engine_ordinal_, rec.engine_index_);
      uint32_t track = GetDeviceEventTrack(rdt, rec.start_time_, rec.end_time_);
      auto [pid, tid] = GetDevicePidTid(rec.device_, rec.engine_ordinal_, rec.engine_index_, pid_, rec.tid_, track);
      std::string kname = GetZeKernelCommandName(rec.kernel_command_id_, rec.group_count_, rec.mem_size_);
      // GetZeKernelCommandName may return a quoted string; unquote for the proto.
      if (!kname.empty() && kname.front() == '"' && kname.back() == '"') {
        kname = kname.substr(1, kname.size() - 2);
      }
      PerfettoEmitDeviceSlice(seq_id_, pid, tid, kname, rec.kid_, rec.implicit_scaling_,
                              rec.tile_, metrics_enabled_, rec.start_time_, rec.end_time_);
    }
#endif /* BUILD_WITH_PERFETTO */

    void FlushDeviceEvent(ZeKernelCommandExecutionRecord& rec) {
#if BUILD_WITH_PERFETTO
      if (UseProtobufOutput()) { PerfettoEmitDeviceEvent(rec); return; }
#endif /* BUILD_WITH_PERFETTO */
      logger_->Log(StringifyDeviceEvent(rec));
    }

    void FlushDeviceBuffer() {
      std::lock_guard<std::recursive_mutex> lock(logger_lock_);
      if (device_event_buffer_flushed_) {
        return;
      }

      for (int i = 0; i < current_device_event_buffer_slice_; i++) {
        for (int j = 0; j < slice_capacity_; j++) {
          FlushDeviceEvent(device_event_buffer_[i][j]);
        }
      }
      for (int j = 0; j < next_device_event_index_; j++) {
        FlushDeviceEvent(device_event_buffer_[current_device_event_buffer_slice_][j]);
      }

      current_device_event_buffer_slice_ = 0;
      next_device_event_index_ = 0;
      device_event_buffer_flushed_ = true;
    }

    void FlushHostEvent(HostEventRecord& rec) {
#if BUILD_WITH_PERFETTO
      if (UseProtobufOutput()) { PerfettoEmitHostEvent(rec, pid_, tid_, seq_id_, pending_flows_, host_track_emitted_); return; }
#endif /* BUILD_WITH_PERFETTO */
      logger_->Log(StringifyHostEvent(rec));
    }

    void FlushHostBuffer() {
      std::lock_guard<std::recursive_mutex> lock(logger_lock_);
      if (host_event_buffer_flushed_) {
        return;
      }

      for (int i = 0; i < current_host_event_buffer_slice_; i++) {
        for (int j = 0; j < slice_capacity_; j++) {
          FlushHostEvent(host_event_buffer_[i][j]);
        }
      }
      for (int j = 0; j < next_host_event_index_; j++) {
        FlushHostEvent(host_event_buffer_[current_host_event_buffer_slice_][j]);
      }
      current_host_event_buffer_slice_ = 0;
      next_host_event_index_ = 0;
      host_event_buffer_flushed_ = true;
    }

    void Finalize() {
      std::lock_guard<std::recursive_mutex> lock(logger_lock_);
      if (!finalized_.exchange(true)) {
        if (!device_event_buffer_flushed_) {
          for (int i = 0; i < current_device_event_buffer_slice_; i++) {
            for (int j = 0; j < slice_capacity_; j++) {
              FlushDeviceEvent(device_event_buffer_[i][j]);
            }
          }
          for (int j = 0; j < next_device_event_index_; j++) {
            FlushDeviceEvent(device_event_buffer_[current_device_event_buffer_slice_][j]);
          }
          device_event_buffer_flushed_ = true;
        }
        if (!host_event_buffer_flushed_) {
          for (int i = 0; i < current_host_event_buffer_slice_; i++) {
            for (int j = 0; j < slice_capacity_; j++) {
              FlushHostEvent(host_event_buffer_[i][j]);
            }
          }
          for (int j = 0; j < next_host_event_index_; j++) {
            FlushHostEvent(host_event_buffer_[current_host_event_buffer_slice_][j]);
          }
          host_event_buffer_flushed_ = true;
        }
      }
    }

    bool IsFinalized() {
      return finalized_.load(std::memory_order_acquire);
    }

  private:
    int32_t buffer_capacity_;
    int32_t slice_capacity_;  // each buffer can have multiple slices
    int32_t current_device_event_buffer_slice_;  // device slice in use
    int32_t current_host_event_buffer_slice_;  // host slice in use
    int32_t next_device_event_index_;  // next free device event in in-use slice
    int32_t next_host_event_index_;  // next free host event in in-use slice
    uint32_t tid_;
    uint32_t pid_;
#if BUILD_WITH_PERFETTO
    uint32_t seq_id_;  // Perfetto packet sequence id for this writer thread
    // Flow ids from FLOW_SOURCE/SINK records awaiting the next real host slice.
    PendingFlows pending_flows_;
    // Whether this buffer's host thread TrackDescriptor has been emitted (it is
    // emitted once on the first host event; the uuid is constant per buffer).
    bool host_track_emitted_ = false;
#endif /* BUILD_WITH_PERFETTO */
    std::vector<ZeKernelCommandExecutionRecord *> device_event_buffer_;
    std::vector<HostEventRecord *> host_event_buffer_;
    // device event timestampes cached are <device, engine_ordinal, engine_index> specific
    std::map<std::tuple<ze_device_handle_t, uint32_t, uint32_t>, std::vector<std::set<std::pair<uint64_t, uint64_t>, DeviceTimestampComparator>>> recent_device_timestamps_;
    bool flush_immediately_;
    bool host_event_buffer_flushed_;
    bool device_event_buffer_flushed_;
    std::atomic<bool> finalized_;
    bool metrics_enabled_;
};

thread_local TraceBuffer thread_local_buffer_;

#if BUILD_WITH_OPENCL
class ClTraceBuffer;
std::set<ClTraceBuffer *> *cl_trace_buffers_ = nullptr;
class ClTraceBuffer {
  public:
    ClTraceBuffer() : flush_immediately_(false) {
      std::string szstr = utils::GetEnv("UNITRACE_ChromeEventBufferSize");
      if (szstr.empty() || (szstr == "-1")) {
        buffer_capacity_ = -1;
        slice_capacity_ = BUFFER_SLICE_SIZE_DEFAULT;
      } else {
        buffer_capacity_ = std::stoi(szstr);
        if (buffer_capacity_ == 0) {
          buffer_capacity_ = 1;  // at least one event slot
          flush_immediately_ = true;
        }
        slice_capacity_ = buffer_capacity_;
      }
      ClKernelCommandExecutionRecord *der = (ClKernelCommandExecutionRecord *)(malloc(sizeof(ClKernelCommandExecutionRecord) * slice_capacity_));
      UniMemory::ExitIfOutOfMemory((void *)(der));

      device_event_buffer_.push_back(der);

      HostEventRecord *her = (HostEventRecord *)(malloc(sizeof(HostEventRecord) * slice_capacity_));
      UniMemory::ExitIfOutOfMemory((void *)(her));

      host_event_buffer_.push_back(her);
      tid_= utils::GetTid();
      pid_= utils::GetPid();
#if BUILD_WITH_PERFETTO
      // One packet sequence per writer thread; slices on the same track from the
      // same sequence stack correctly. Per-rank files are already separate.
      seq_id_ = perfetto_emit::NextSequenceId();
#endif /* BUILD_WITH_PERFETTO */

      current_device_event_buffer_slice_ = 0;
      current_host_event_buffer_slice_ = 0;
      next_device_event_index_ = 0;
      next_host_event_index_ = 0;
      device_event_buffer_flushed_ = false;
      host_event_buffer_flushed_ = false;
      finalized_.store(false, std::memory_order_release);
      if ((utils::GetEnv("UNITRACE_MetricQuery") == "1") || (utils::GetEnv("UNITRACE_KernelMetrics") == "1")) {
        metrics_enabled_ = true;
      } else {
        metrics_enabled_ = false;
      }

      std::lock_guard<std::recursive_mutex> lock(logger_lock_);  // use this lock to protect trace_buffers_

      if (cl_trace_buffers_ == nullptr) {
        cl_trace_buffers_ = new std::set<ClTraceBuffer *>;
        UniMemory::ExitIfOutOfMemory((void *)(cl_trace_buffers_));
      }
      cl_trace_buffers_->insert(this);
    }

    ~ClTraceBuffer() {
      std::lock_guard<std::recursive_mutex> lock(logger_lock_);
      if (!finalized_.exchange(true)) {
        // finalize if not finalized
        if (!device_event_buffer_flushed_) {
          for (int i = 0; i < current_device_event_buffer_slice_; i++) {
            for (int j = 0; j < slice_capacity_; j++) {
              FlushDeviceEvent(device_event_buffer_[i][j]);
            }
          }
          for (int j = 0; j < next_device_event_index_; j++) {
            FlushDeviceEvent(device_event_buffer_[current_device_event_buffer_slice_][j]);
          }
          device_event_buffer_flushed_ = true;
        }
        if (!host_event_buffer_flushed_) {
          for (int i = 0; i < current_host_event_buffer_slice_; i++) {
            for (int j = 0; j < slice_capacity_; j++) {
              FlushHostEvent(host_event_buffer_[i][j]);
            }
          }
          for (int j = 0; j < next_host_event_index_; j++) {
            FlushHostEvent(host_event_buffer_[current_host_event_buffer_slice_][j]);
          }
          host_event_buffer_flushed_ = true;
        }

        for (auto& slice : device_event_buffer_) {
          free(slice);
        }
        device_event_buffer_.clear();

        for (auto& slice : host_event_buffer_) {
          free(slice);
        }
        host_event_buffer_.clear();

        cl_trace_buffers_->erase(this);
      }
    }

    ClTraceBuffer(const ClTraceBuffer& that) = delete;
    ClTraceBuffer& operator=(const ClTraceBuffer& that) = delete;

    // device event timestampes cached are <device, queue> specific
    std::vector<std::set<std::pair<uint64_t, uint64_t>, DeviceTimestampComparator>>&
    GetRecentDeviceTimestamps(cl_device_id device, cl_command_queue queue) {
      auto it = recent_device_timestamps_.find({device, queue});
      if (it == recent_device_timestamps_.end()) {
        auto rdt = std::vector<std::set<std::pair<uint64_t, uint64_t>, DeviceTimestampComparator>>();
        rdt.push_back(std::set<std::pair<uint64_t, uint64_t>, DeviceTimestampComparator>());
        auto& ts = rdt[0];
        // populate the recent device timestamps with dummies
        for (int i = 0; i < num_device_timestamps_cahced_; i++) {
          ts.insert({0, i});
        }
        recent_device_timestamps_.insert({{device, queue}, std::move(rdt)});

        it = recent_device_timestamps_.find({device, queue});
      }

      return it->second;
    }

    ClKernelCommandExecutionRecord *GetDeviceEvent(void) {
      if (next_device_event_index_ ==  slice_capacity_) {
        if (buffer_capacity_ == -1) {
          ClKernelCommandExecutionRecord *der = (ClKernelCommandExecutionRecord *)(malloc(sizeof(ClKernelCommandExecutionRecord) * slice_capacity_));
          UniMemory::ExitIfOutOfMemory((void *)(der));

          device_event_buffer_.push_back(der);
          current_device_event_buffer_slice_++;
          next_device_event_index_ = 0;
        } else {
          FlushDeviceBuffer();
        }
      }
      return &(device_event_buffer_[current_device_event_buffer_slice_][next_device_event_index_]);
    }

    HostEventRecord *GetHostEvent(void) {
      if (next_host_event_index_ ==  slice_capacity_) {
        if (buffer_capacity_ == -1) {
          HostEventRecord *her = (HostEventRecord *)(malloc(sizeof(HostEventRecord) * slice_capacity_));
          UniMemory::ExitIfOutOfMemory((void *)(her));

          host_event_buffer_.push_back(her);
          current_host_event_buffer_slice_++;
          next_host_event_index_ = 0;
        } else {
          FlushHostBuffer();
        }
      }
      return &(host_event_buffer_[current_host_event_buffer_slice_][next_host_event_index_]);
    }

    void BufferHostEvent(void) {
      if (flush_immediately_) {
        std::lock_guard<std::recursive_mutex> lock(logger_lock_);
        FlushHostEvent(host_event_buffer_[current_host_event_buffer_slice_][next_host_event_index_]);
        // in case that flush_immediately_ is true, only one slice and one event slot, so set the flushed flag to true
        host_event_buffer_flushed_ = true;
      }
      else {
        next_host_event_index_++;
        host_event_buffer_flushed_ = false;
      }
    }

    void BufferDeviceEvent(void) {
      if (flush_immediately_) {
        std::lock_guard<std::recursive_mutex> lock(logger_lock_);
        FlushDeviceEvent(device_event_buffer_[current_device_event_buffer_slice_][next_device_event_index_]);
        // in case that flush_immediately_ is true, only one slice and one event slot, so set the flushed flag to true
        device_event_buffer_flushed_ = true;
      }
      else {
        next_device_event_index_++;
        device_event_buffer_flushed_ = false;
      }
    }

    uint32_t GetTid() { return tid_; }
    uint32_t GetPid() { return pid_; }

    std::string StringifyDeviceEvent(ClKernelCommandExecutionRecord& rec) {
      auto& rdt = GetRecentDeviceTimestamps(rec.device_, rec.queue_);
      uint32_t track = GetDeviceEventTrack(rdt, rec.start_time_, rec.end_time_);
      auto [pid, tid] = ClGetDevicePidTid(rec.pci_, rec.device_, rec.queue_, pid_, rec.tid_, track);
      std::string kname = GetClKernelCommandName(rec.kernel_command_id_);

      std::string str = ",\n{";

      str += "\"ph\": \"X\"";
      str += ", \"tid\": " + std::to_string(tid);
      str += ", \"pid\": " + std::to_string(pid);

      if (rec.implicit_scaling_) {
        str += ", \"name\": \"Tile #" + std::to_string(rec.tile_) + ": ";
        if (!kname.empty()) {
          if (kname[0] == '\"') {
            str += kname.substr(1, kname.size() - 2);
          } else {
            str += kname;
          }
        }
        str += "\"";
      } else {
        if (!kname.empty()) {
          if (kname[0] == '\"') {
            // name is already quoted
            str += ", \"name\": " + kname;
          } else {
            str += ", \"name\": \"" + kname + "\"";
          }
        }
      }

      str += ", \"cat\": \"gpu_op\"";
      str += ", \"ts\": " + std::to_string(UniTimer::GetEpochTimeInUs(rec.start_time_));
      str += ", \"dur\": " + std::to_string(UniTimer::GetTimeInUs(rec.end_time_ - rec.start_time_));
      str += ", \"args\": {\"id\": \"" + std::to_string(rec.kid_) + "\"";
      if (metrics_enabled_) {
        // viewing the metrics on the same local host, so no need to use https
        str += ", \"metrics\": \"http://localhost:8000/" + EncodeURI(kname) + "/" + std::to_string(rec.kid_) + "\"";
      }
      str += "}}";

      if (!rec.implicit_scaling_) {
        str += ",\n{";
        str += "\"ph\": \"t\"";
        str += ", \"tid\": " + std::to_string(tid);
        str += ", \"pid\": " + std::to_string(pid);
        str += ", \"name\": \"dep\"";
        str += ", \"cat\": \"CL_Flow_H2D_" + std::to_string(rec.kid_) + "_" + std::to_string(mpi_rank) + "\"";
        str += ", \"ts\": " + std::to_string(UniTimer::GetEpochTimeInUs(rec.start_time_));
        str += ", \"id\": " + std::to_string(rec.kid_);
        str += "},\n";

        str += "{";
        str += "\"ph\": \"s\"";
        str += ", \"tid\": " + std::to_string(tid);
        str += ", \"pid\": " + std::to_string(pid);
        str += ", \"name\": \"dep\"";
        str += ", \"cat\": \"CL_Flow_D2H_" + std::to_string(rec.kid_) + "_" + std::to_string(mpi_rank) + "\"";
        str += ", \"ts\": " + std::to_string(UniTimer::GetEpochTimeInUs(rec.start_time_));
        str += ", \"id\": " + std::to_string(rec.kid_);
        str += "}";
      }

      return str;
    }

#if BUILD_WITH_PERFETTO
    // Emit an OpenCL device kernel command as a Perfetto slice. Mirrors the L0
    // emitter: resolves the CL-specific pid/tid/kname and defers to the shared
    // PerfettoEmitDeviceSlice.
    void PerfettoEmitDeviceEvent(ClKernelCommandExecutionRecord& rec) {
      auto& rdt = GetRecentDeviceTimestamps(rec.device_, rec.queue_);
      uint32_t track = GetDeviceEventTrack(rdt, rec.start_time_, rec.end_time_);
      auto [pid, tid] = ClGetDevicePidTid(rec.pci_, rec.device_, rec.queue_, pid_, rec.tid_, track);
      std::string kname = GetClKernelCommandName(rec.kernel_command_id_);
      if (!kname.empty() && kname.front() == '"' && kname.back() == '"') {
        kname = kname.substr(1, kname.size() - 2);
      }
      PerfettoEmitDeviceSlice(seq_id_, pid, tid, kname, rec.kid_, rec.implicit_scaling_,
                              rec.tile_, metrics_enabled_, rec.start_time_, rec.end_time_);
    }
#endif /* BUILD_WITH_PERFETTO */

    void FlushDeviceEvent(ClKernelCommandExecutionRecord& rec) {
#if BUILD_WITH_PERFETTO
      if (UseProtobufOutput()) { PerfettoEmitDeviceEvent(rec); return; }
#endif /* BUILD_WITH_PERFETTO */
      logger_->Log(StringifyDeviceEvent(rec));
    }

    void FlushDeviceBuffer() {
      std::lock_guard<std::recursive_mutex> lock(logger_lock_);
      if (device_event_buffer_flushed_) {
        return;
      }

      for (int i = 0; i < current_device_event_buffer_slice_; i++) {
        for (int j = 0; j < slice_capacity_; j++) {
          FlushDeviceEvent(device_event_buffer_[i][j]);
        }
      }
      for (int j = 0; j < next_device_event_index_; j++) {
        FlushDeviceEvent(device_event_buffer_[current_device_event_buffer_slice_][j]);
      }

      current_device_event_buffer_slice_ = 0;
      next_device_event_index_ = 0;
      device_event_buffer_flushed_ = true;
    }

    std::string StringifyHostEvent(HostEventRecord& rec) {
      std::string str = ",\n{";  // header

      if (rec.type_ == EVENT_COMPLETE) {
        str += "\"ph\": \"X\"";
      } else if (rec.type_ == EVENT_DURATION_START) {
        str += "\"ph\": \"B\"";
      } else if (rec.type_ == EVENT_DURATION_END) {
        str += "\"ph\": \"E\"";
      } else if (rec.type_ == EVENT_FLOW_SOURCE) {
        str += "\"ph\": \"s\"";
      } else if (rec.type_ == EVENT_FLOW_SINK) {
        str += "\"ph\": \"t\"";
      } else if (rec.type_ == EVENT_MARK) {
        str += "\"ph\": \"R\"";
      } else {
        // should never get here
      }

      str += ", \"tid\": " + std::to_string(tid_);
      str += ", \"pid\": " + std::to_string(pid_);

      if (rec.type_ == EVENT_FLOW_SOURCE) {
        str += ", \"name\": \"dep\"";
        str += ", \"cat\": \"CL_Flow_H2D_" + std::to_string(rec.id_) + "_" + std::to_string(mpi_rank) + "\"";
      } else if (rec.type_ == EVENT_FLOW_SINK) {
        str += ", \"name\": \"dep\"";
        str += ", \"cat\": \"CL_Flow_D2H_" + std::to_string(rec.id_) + "_" + std::to_string(mpi_rank) + "\"";
      } else {
        if (rec.name_ != nullptr) {
          if (rec.name_[0] == '\"') {
            // name is already quoted
            str += ", \"name\": " + std::string(rec.name_);
          } else {
            str += ", \"name\": \"" + std::string(rec.name_) + "\"";
          }
        } else {
          if ((rec.api_id_ != XptiTracingId) && (rec.api_id_ != IttTracingId)) {
            str += ", \"name\": \"" + get_symbol(rec.api_id_) + "\"";
          }
        }
        str += ", \"cat\": \"cpu_op\"";
      }

      // free rec.name_. It is not needed any more
      if (rec.name_ != nullptr) {
        free(rec.name_);
        rec.name_ = nullptr;
      }

      // It is always present
      str += ", \"ts\": " + std::to_string(UniTimer::GetEpochTimeInUs(rec.start_time_));

      if (rec.type_ == EVENT_COMPLETE) {
        str += ", \"dur\": " + std::to_string(UniTimer::GetTimeInUs(rec.end_time_ - rec.start_time_));
      }

      std::string str_args = "";  // build arguments
      if (rec.api_type_ == API_TYPE_MPI) {
        const MpiArgs& args = rec.mpi_args_;

        bool isFirst = true;  // First argument can be zero and second non zero

        if (args.src_size != 0) {
          str_args += "\"ssize\": " + std::to_string(args.src_size);
          if (args.is_tagged) {
            str_args += ", \"src\": " + std::to_string(args.src_location);
            str_args += ", \"stag\": " + std::to_string(args.src_tag);
          }
          isFirst = false;
        }

        if (args.dst_size != 0) {
          str_args += (isFirst ? "" : ", ");
          str_args += "\"dsize\": " + std::to_string(args.dst_size);
          if (args.is_tagged) {
            str_args += ", \"dst\": " + std::to_string(args.dst_location);
            str_args += ", \"dtag\": " + std::to_string(args.dst_tag);
          }
        }

        if (args.mpi_counter >= 0) {
          str_args += ", \"mpi_counter\": " + std::to_string(args.mpi_counter);
        }

      } else if (rec.api_type_ == API_TYPE_ITT) {
        str_args = str_args + "\"" + rec.itt_args_.key + "\":[";
        str_args += ConvertDataToString(&rec.itt_args_);
        str_args += "]";
        if (rec.itt_args_.isIndirectData) {
          free(rec.itt_args_.data[0]);
        }
        IttArgs* args = rec.itt_args_.next;
        while (args != nullptr) {
          str_args += ",";
          str_args = str_args + "\"" + args->key + "\":[";
          str_args += ConvertDataToString(args);
          str_args += "]";
          IttArgs* toFree = args;
          args = args->next;
          free(toFree);
        }
        // reset count to 0 and type to API_TYPE_NONE
        rec.itt_args_.count = 0;
        rec.api_type_ = API_TYPE_NONE;
      }

      if (!str_args.empty()) {
        str += ", \"args\": {" + str_args + "}";
      } else {
        str += ", \"id\": " + std::to_string(rec.id_);
      }

      // end
      str += "}";  // footer

      return str;
    }

    void FlushHostEvent(HostEventRecord& rec) {
#if BUILD_WITH_PERFETTO
      if (UseProtobufOutput()) { PerfettoEmitHostEvent(rec, pid_, tid_, seq_id_, pending_flows_, host_track_emitted_); return; }
#endif /* BUILD_WITH_PERFETTO */
      logger_->Log(StringifyHostEvent(rec));
    }

    void FlushHostBuffer() {
      std::lock_guard<std::recursive_mutex> lock(logger_lock_);
      if (host_event_buffer_flushed_) {
        return;
      }

      for (int i = 0; i < current_host_event_buffer_slice_; i++) {
        for (int j = 0; j < slice_capacity_; j++) {
          FlushHostEvent(host_event_buffer_[i][j]);
        }
      }
      for (int j = 0; j < next_host_event_index_; j++) {
        FlushHostEvent(host_event_buffer_[current_host_event_buffer_slice_][j]);
      }
      current_host_event_buffer_slice_ = 0;
      next_host_event_index_ = 0;
      host_event_buffer_flushed_ = true;
    }

    void Finalize() {
      std::lock_guard<std::recursive_mutex> lock(logger_lock_);
      if (!finalized_.exchange(true)) {
        if (!device_event_buffer_flushed_) {
          for (int i = 0; i < current_device_event_buffer_slice_; i++) {
            for (int j = 0; j < slice_capacity_; j++) {
              FlushDeviceEvent(device_event_buffer_[i][j]);
            }
          }
          for (int j = 0; j < next_device_event_index_; j++) {
            FlushDeviceEvent(device_event_buffer_[current_device_event_buffer_slice_][j]);
          }
          device_event_buffer_flushed_ = true;
        }
        if (!host_event_buffer_flushed_) {
          for (int i = 0; i < current_host_event_buffer_slice_; i++) {
            for (int j = 0; j < slice_capacity_; j++) {
              FlushHostEvent(host_event_buffer_[i][j]);
            }
          }
          for (int j = 0; j < next_host_event_index_; j++) {
            FlushHostEvent(host_event_buffer_[current_host_event_buffer_slice_][j]);
          }
          host_event_buffer_flushed_ = true;
        }
      }
    }

    bool IsFinalized() {
      return finalized_.load(std::memory_order_acquire);
    }

  private:
    int32_t buffer_capacity_;
    int32_t slice_capacity_;  // each buffer can have multiple slices
    int32_t current_device_event_buffer_slice_;  // device slice in use
    int32_t current_host_event_buffer_slice_;  // host slice in use
    int32_t next_device_event_index_;  // next free device event in in-use slice
    int32_t next_host_event_index_;  // next free host event in in-use slice
    uint32_t tid_;
    uint32_t pid_;
#if BUILD_WITH_PERFETTO
    uint32_t seq_id_;  // Perfetto packet sequence id for this writer thread
    // Flow ids from FLOW_SOURCE/SINK records awaiting the next real host slice.
    PendingFlows pending_flows_;
    // Whether this buffer's host thread TrackDescriptor has been emitted (it is
    // emitted once on the first host event; the uuid is constant per buffer).
    bool host_track_emitted_ = false;
#endif /* BUILD_WITH_PERFETTO */
    std::vector<ClKernelCommandExecutionRecord *> device_event_buffer_;
    std::vector<HostEventRecord *> host_event_buffer_;
    // device event timestampes cached are <device, queue> specific
    std::map<std::tuple<cl_device_id, cl_command_queue>, std::vector<std::set<std::pair<uint64_t, uint64_t>, DeviceTimestampComparator>>> recent_device_timestamps_;
    bool flush_immediately_;
    bool host_event_buffer_flushed_;
    bool device_event_buffer_flushed_;
    std::atomic<bool> finalized_;
    bool metrics_enabled_;
};

thread_local ClTraceBuffer cl_thread_local_buffer_;
#endif /* BUILD_WITH_OPENCL */

class ChromeLogger {
  private:
    LoggerFactory* logger_factory_;
    std::string process_name_;
    double process_start_time_;
    bool flushed_ = false;  // true once Flush() has drained buffers and written the JSON close

    ChromeLogger(const char* process_name)
      : logger_factory_(LoggerFactory::Create())
    {
      process_name_ = process_name;

      std::string host = GetHostName();
      std::string rank = (utils::GetEnv("PMI_RANK").empty()) ? utils::GetEnv("PMIX_RANK") : utils::GetEnv("PMI_RANK");
      std::string host_proc_name = rank.empty() ? ("HOST<" + host + ">")
                                                : ("RANK " + rank + " HOST<" + host + ">");

#if !BUILD_WITH_PERFETTO
      if (utils::GetEnv("UNITRACE_OutputFormat") == "protobuf") {
        std::cerr << "[WARNING] --output-format=protobuf requested, but unitrace was built "
                     "without Perfetto support (BUILD_WITH_PERFETTO=OFF). Falling back to JSON." << std::endl;
      }
#endif /* !BUILD_WITH_PERFETTO */

#if BUILD_WITH_PERFETTO
      if (UseProtobufOutput()) {
        // Protobuf is opened in binary mode (4th GetLogger arg = binary).
        logger_ = logger_factory_->GetLogger(LOGGER_TYPE_CHROME_TRACE_UNITRACE, true, true, /*binary=*/true);
        if (!logger_) {
          UniMemory::ExitIfOutOfMemory((void *)(logger_.get()));
        }
        perfetto_emit::EmitLogger() = logger_;

        // Clock domain: REALTIME (epoch ns) by default -- globally meaningful
        // across hosts, enabling cross-rank Timeline Sync. Under
        // UNITRACE_SystemTime=1, GetEpochTime returns raw monotonic ns instead, so
        // resolve to MONOTONIC_RAW in that mode.
        perfetto_emit::ClockId() = (utils::GetEnv("UNITRACE_SystemTime") == "1")
                                       ? perfetto_emit::kClockMonotonicRaw
                                       : perfetto_emit::kClockRealtime;

        // Sample BOOTTIME and the event clock back-to-back so the ClockSnapshot
        // relates them accurately (needed for the trace processor's clock sync).
        uint64_t boot_ns = UniTimer::GetHostBootTimestamp();
        uint64_t now_ns = UniTimer::GetEpochTime(UniTimer::GetHostTimestamp());
        perfetto_emit::EmitClockSnapshot(now_ns, boot_ns, perfetto_emit::DescriptorSeqId());
        // Emitted once at construction, so direct emit -- no dedup needed.
        perfetto_emit::EmitProcessTrack(HostProcessTrackUuid(utils::GetPid()),
                                        utils::GetPid(), host_proc_name, now_ns,
                                        perfetto_emit::DescriptorSeqId());
        // After the clock snapshot so "no events -> delete file" still holds.
        logger_->SetEmptyPosition();
        return;
      }
#endif /* BUILD_WITH_PERFETTO */
      logger_ = logger_factory_->GetLogger(LOGGER_TYPE_CHROME_TRACE_UNITRACE, true, true);
      if (!logger_) {
        UniMemory::ExitIfOutOfMemory((void *)(logger_.get()));
      }

      process_start_time_ = UniTimer::GetEpochTimeInUs(UniTimer::GetHostTimestamp());

      logger_->Log("{ \"traceEvents\":[\n");

      std::string str("{\"ph\": \"M\", \"name\": \"process_name\", \"pid\": ");
      str += std::to_string(utils::GetPid()) + ", \"ts\": " + std::to_string(process_start_time_) + ", \"args\": {\"name\": \"";
      str += host_proc_name + "\"}}";

      logger_->Log(str);
      logger_->SetEmptyPosition();
    }

  public:
    ChromeLogger(const ChromeLogger& that) = delete;
    ChromeLogger& operator=(const ChromeLogger& that) = delete;

    ~ChromeLogger() {
      if (logger_ != nullptr) {
        std::string chrome_trace_file_name_ = logger_->GetLogFileName();

        logger_lock_.lock();

        if (!flushed_) {
          // Drain every registered trace buffer BEFORE the JSON is closed:
          if (trace_buffers_) {
            for (auto it = trace_buffers_->begin(); it != trace_buffers_->end();) {
              (*it)->Finalize();
              it = trace_buffers_->erase(it);
            }
          }

#if BUILD_WITH_OPENCL
          if (cl_trace_buffers_) {
            for (auto it = cl_trace_buffers_->begin(); it != cl_trace_buffers_->end();) {
              (*it)->Finalize();
              it = cl_trace_buffers_->erase(it);
            }
          }
#endif /* BUILD_WITH_OPENCL */

          // Write closing brackets so the JSON is valid
          // The protobuf stream is binary and self-terminating, so the
          // tags would corrupt it -- JSON-only.
          if (!UseProtobufOutput() && !logger_->IsEmpty()) {
            logger_->Log("\n]\n}\n");
            logger_->Flush();
          }
          flushed_ = true;
        }

        logger_lock_.unlock();

        if (logger_->IsEmpty()) {
          // no data has been logged
          std::cerr << "[INFO] No event of interest is logged for process " << utils::GetPid() << " (" << process_name_ << ")" << std::endl;
        } else {
          // The JSON closing tags (if any) are written above (or by an earlier
          // Flush()). The protobuf stream is self-terminating.
          std::cerr << "[INFO] Timeline is stored in " << chrome_trace_file_name_ << std::endl;
        }
      }
    }

    static ChromeLogger* Create(const char* process_name) {
      ChromeLogger *chrome_logger  = new ChromeLogger(process_name);
      UniMemory::ExitIfOutOfMemory((void *)(chrome_logger));
      return chrome_logger;
    }

    void Flush() {
      if (logger_ == nullptr) {
        return;
      }
      logger_lock_.lock();
      if (!flushed_) {
        if (trace_buffers_) {
          for (auto it = trace_buffers_->begin(); it != trace_buffers_->end(); ++it) {
            (*it)->FlushDeviceBuffer();
            (*it)->FlushHostBuffer();
          }
        }

#if BUILD_WITH_OPENCL
        if (cl_trace_buffers_) {
          for (auto it = cl_trace_buffers_->begin(); it != cl_trace_buffers_->end(); ++it) {
            (*it)->FlushDeviceBuffer();
            (*it)->FlushHostBuffer();
          }
        }
#endif /* BUILD_WITH_OPENCL */

        // Write closing brackets so the JSON is valid if the process terminates
        // abnormally. The protobuf stream is binary and self-terminating, so the
        // tags would corrupt it -- JSON-only.
        if (!UseProtobufOutput() && !logger_->IsEmpty()) {
          logger_->Log("\n]\n}\n");
          logger_->Flush();
        }
        flushed_ = true;
      }
      logger_lock_.unlock();
    }

    static void XptiLoggingCallback(EVENT_TYPE etype, const char *name, uint64_t start_ts, uint64_t end_ts) {
      if (!thread_local_buffer_.IsFinalized()) {
        HostEventRecord *rec = thread_local_buffer_.GetHostEvent();
        rec->type_ = etype;

        if (name != nullptr) {
          rec->name_ = strdup(name);
        } else {
          rec->name_ = nullptr;
        }

        rec->api_type_ = API_TYPE_NONE;
        rec->api_id_ = XptiTracingId;
        rec->start_time_ = start_ts;
        if (etype == EVENT_COMPLETE) {
          rec->end_time_ = end_ts;
        }
        rec->id_ = 0;
        thread_local_buffer_.BufferHostEvent();
      }
    }

#if BUILD_WITH_OMP
    static void OmptLoggingCallback(uint64_t event_id, uint64_t start_ts,
                                    uint64_t end_ts, EVENT_TYPE etype,
                                    OmpArgs *omp_args) {
      if (thread_local_buffer_.IsFinalized())
        return;

      HostEventRecord *rec = thread_local_buffer_.GetHostEvent();
      rec->name_ = nullptr;
      rec->type_ = etype;
      rec->api_id_ = OmpTracingId;
      rec->start_time_ = start_ts;
      rec->end_time_ = end_ts;
      rec->id_ = event_id;
      if (omp_args) {
        rec->api_type_ = API_TYPE_OMP;
        rec->omp_args_ = *omp_args;
      } else {
        rec->api_type_ = API_TYPE_NONE;
      }

      thread_local_buffer_.BufferHostEvent();
    }
#endif /* BUILD_WITH_OMP */

    static void IttLoggingCallback(const char *name, uint64_t start_ts, uint64_t end_ts, IttArgs* metadata_args) {
      if (!thread_local_buffer_.IsFinalized()) {
        HostEventRecord *rec = thread_local_buffer_.GetHostEvent();

        rec->type_ = EVENT_COMPLETE;

        if (name != nullptr) {
          rec->name_ = strdup(name);
        } else {
          rec->name_ = nullptr;
        }

        rec->api_id_ = IttTracingId;
        rec->start_time_ = start_ts;
        rec->end_time_ = end_ts;
        rec->id_ = 0;
        if ((metadata_args != nullptr) && (metadata_args->count != 0)) {
          rec->api_type_ = API_TYPE_ITT;
          rec->itt_args_ = *metadata_args;
        } else {
          // no arguments so set api_type_ to API_TYPE_NONE
          rec->api_type_ = API_TYPE_NONE;
          rec->itt_args_.count = 0;
        }

        thread_local_buffer_.BufferHostEvent();
      }
    }

    static void MpiLoggingCallback(const char *name, uint64_t start_ts, uint64_t end_ts, size_t src_size, int src_location, int src_tag,
                                   size_t dst_size, int dst_location, int dst_tag) {
      if (!thread_local_buffer_.IsFinalized()) {
        HostEventRecord *rec = thread_local_buffer_.GetHostEvent();
        rec->type_ = EVENT_COMPLETE;

        if (name != nullptr) {
          rec->name_ = strdup(name);
        } else {
          rec->name_ = nullptr;
        }

        rec->api_id_ = IttTracingId;
        rec->start_time_ = start_ts;
        rec->end_time_ = end_ts;
        rec->id_ = 0;
        rec->api_type_ = API_TYPE_MPI;
        rec->mpi_args_.src_size = src_size;
        rec->mpi_args_.src_location = src_location;
        rec->mpi_args_.src_tag = src_tag;
        rec->mpi_args_.dst_size = dst_size;
        rec->mpi_args_.dst_location = dst_location;
        rec->mpi_args_.dst_tag = dst_tag;
        rec->mpi_args_.mpi_counter = -1;
        rec->mpi_args_.is_tagged = true;

        thread_local_buffer_.BufferHostEvent();
      }
    }

    static void MpiInternalLoggingCallback(const char *name, uint64_t start_ts, uint64_t end_ts, int64_t mpi_counter, size_t src_size, size_t dst_size) {
      if (!thread_local_buffer_.IsFinalized()) {
        HostEventRecord *rec = thread_local_buffer_.GetHostEvent();
        rec->type_ = EVENT_COMPLETE;

        if (name != nullptr) {
          rec->name_ = strdup(name);
        } else {
          rec->name_ = nullptr;
        }

        rec->api_id_ = IttTracingId;
        rec->start_time_ = start_ts;
        rec->end_time_ = end_ts;
        rec->id_ = 0;
        rec->api_type_ = API_TYPE_MPI;
        rec->mpi_args_.mpi_counter = mpi_counter;
        rec->mpi_args_.src_size = src_size;
        rec->mpi_args_.dst_size = dst_size;
        rec->mpi_args_.is_tagged = false;

        thread_local_buffer_.BufferHostEvent();
      }
    }

    static void ZeChromeKernelLoggingCallback(uint64_t kid, uint64_t tid, uint64_t start, uint64_t end, uint32_t ordinal, uint32_t index, int32_t tile, const ze_device_handle_t device, const uint64_t kernel_command_id, bool implicit_scaling, const ze_group_count_t &group_count, size_t mem_size) {
      if (thread_local_buffer_.IsFinalized()) {
        return;
      }

      ZeKernelCommandExecutionRecord *rec = thread_local_buffer_.GetDeviceEvent();
      rec->kid_ = kid;
      rec->tid_ = tid;
      rec->tile_ = tile;
      rec->start_time_ = start;
      rec->end_time_ = end;
      rec->device_ = device;
      rec->engine_ordinal_ = ordinal;
      rec->engine_index_ = index;
      rec->implicit_scaling_ = implicit_scaling;
      rec->kernel_command_id_ = kernel_command_id;
      rec->group_count_ = group_count;
      rec->mem_size_ = mem_size;
      thread_local_buffer_.BufferDeviceEvent();
    }

    static void ChromeCallLoggingCallback(std::vector<uint64_t> *kids, FLOW_DIR flow_dir, API_TRACING_ID api_id,
                                          uint64_t started, uint64_t ended) {
      if (thread_local_buffer_.IsFinalized()) {
        return;
      }

      // The protobuf emitter attaches the flow ids to the next slice, so protobuf
      // must buffer the flow records before the EVENT_COMPLETE; JSON keeps the
      // COMPLETE first (its ph:s/ph:t follow).
      // NOTE: this body is identical to ClChromeCallLoggingCallback except for the
      // thread-local buffer it writes to; both would collapse if the ZE/CL trace
      // buffers were unified (templatized) -- see the TraceBuffer note above.
      bool flows_first = false;
#if BUILD_WITH_PERFETTO
      flows_first = UseProtobufOutput();
#endif /* BUILD_WITH_PERFETTO */

      auto buffer_complete = [&]() {
        HostEventRecord *rec = thread_local_buffer_.GetHostEvent();
        rec->type_ = EVENT_COMPLETE;
        rec->api_type_ = API_TYPE_NONE;
        rec->api_id_ = api_id;
        rec->start_time_ = started;
        rec->end_time_ = ended;
        rec->id_ = 0;
        rec->name_ = nullptr;
        thread_local_buffer_.BufferHostEvent();
      };

      if (!flows_first) {
        buffer_complete();
      }

      if ((kids != nullptr) && (flow_dir == FLOW_H2D)) {
        for (auto id : *kids) {
          HostEventRecord *rec = thread_local_buffer_.GetHostEvent();

          rec->type_ = EVENT_FLOW_SOURCE;
          rec->api_type_ = API_TYPE_NONE;
          rec->api_id_ = DummyTracingId;
          rec->start_time_ = started;
          rec->id_ = id;
          rec->name_ = nullptr;
          thread_local_buffer_.BufferHostEvent();
        }
      }
      if ((kids != nullptr) && (flow_dir == FLOW_D2H)) {
        for (auto id : *kids) {
          HostEventRecord *rec = thread_local_buffer_.GetHostEvent();

          rec->type_ = EVENT_FLOW_SINK;
          rec->api_type_ = API_TYPE_NONE;
          rec->api_id_ = DummyTracingId;
          rec->start_time_ = started;
          rec->id_ = id;
          rec->name_ = nullptr;
          thread_local_buffer_.BufferHostEvent();
        }
      }

      if (flows_first) {
        buffer_complete();
      }
    }

#if BUILD_WITH_OPENCL
    // OnenCL tracer callbacks.
    static void ClChromeKernelLoggingCallback(
      cl_device_pci_bus_info_khr& pci,
      cl_device_id device,
      cl_command_queue& queue,
      int tile,
      bool implicit,
      const uint64_t id,
      uint64_t started,
      uint64_t ended) {

      if (cl_thread_local_buffer_.IsFinalized()) {
        return;
      }

      PTI_ASSERT(ended > started);

      ClKernelCommandExecutionRecord *rec = cl_thread_local_buffer_.GetDeviceEvent();

      rec->tid_ = utils::GetTid();
      rec->tile_ = tile;
      rec->start_time_ = started;
      rec->end_time_ = ended;
      rec->device_ = device;
      rec->pci_ = pci;
      rec->queue_ = queue;
      rec->implicit_scaling_ = implicit;
      rec->kid_ = id;
      rec->kernel_command_id_ = id;
      cl_thread_local_buffer_.BufferDeviceEvent();
    }

    static void ClChromeCallLoggingCallback(std::vector<uint64_t> *kids, FLOW_DIR flow_dir, API_TRACING_ID api_id,
                                            uint64_t started, uint64_t ended) {
      if (cl_thread_local_buffer_.IsFinalized()) {
        return;
      }

      // See ChromeCallLoggingCallback: protobuf buffers the flow records before
      // the EVENT_COMPLETE so the emitter can attach them to that slice; JSON
      // keeps COMPLETE first. This body mirrors ChromeCallLoggingCallback exactly
      // but for the cl_ buffer -- another pair that a templatized TraceBuffer
      // would unify (see the TraceBuffer note above).
      bool flows_first = false;
#if BUILD_WITH_PERFETTO
      flows_first = UseProtobufOutput();
#endif /* BUILD_WITH_PERFETTO */

      auto buffer_complete = [&]() {
        HostEventRecord *rec = cl_thread_local_buffer_.GetHostEvent();
        rec->type_ = EVENT_COMPLETE;
        rec->api_type_ = API_TYPE_NONE;
        rec->api_id_ = api_id;
        rec->start_time_ = started;
        rec->end_time_ = ended;
        rec->id_ = 0;
        rec->name_ = nullptr;
        cl_thread_local_buffer_.BufferHostEvent();
      };

      if (!flows_first) {
        buffer_complete();
      }

      if ((kids != nullptr) && (flow_dir == FLOW_H2D)) {
        for (auto id : *kids) {
          HostEventRecord *rec = cl_thread_local_buffer_.GetHostEvent();

          rec->type_ = EVENT_FLOW_SOURCE;
          rec->api_type_ = API_TYPE_NONE;
          rec->api_id_ = DummyTracingId;
          rec->start_time_ = started;
          rec->id_ = id;
          rec->name_ = nullptr;
          cl_thread_local_buffer_.BufferHostEvent();
        }
      }

      if ((kids != nullptr) && (flow_dir == FLOW_D2H)) {
        for (auto id : *kids) {
          HostEventRecord *rec = cl_thread_local_buffer_.GetHostEvent();

          rec->type_ = EVENT_FLOW_SINK;
          rec->api_type_ = API_TYPE_NONE;
          rec->api_id_ = DummyTracingId;
          rec->start_time_ = started;
          rec->id_ = id;
          rec->name_ = nullptr;
          cl_thread_local_buffer_.BufferHostEvent();
        }
      }

      if (flows_first) {
        buffer_complete();
      }
    }
#endif /* BUILD_WITH_OPENCL */
};

#endif // PTI_TOOLS_UNITRACE_CHROME_LOGGER_H_

//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_COMMON_CHROME_LOGGER_H_
#define PTI_TOOLS_COMMON_CHROME_LOGGER_H_

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
#include "trace_options.h"
#include "unitimer.h"
#include "unikernel.h"
#include "unievent.h"
#include "unimemory.h"

#include "common_header.gen"

static inline std::string GetHostName(void) {
  char hname[256];

  gethostname(hname, sizeof(hname));
  hname[255] = 0;
  return hname;
}

static std::string rank = (utils::GetEnv("PMI_RANK").empty()) ? utils::GetEnv("PMIX_RANK") : utils::GetEnv("PMI_RANK");
static uint32_t mpi_rank = std::atoi(rank.c_str());

static std::string process_start_time = std::to_string(UniTimer::GetEpochTimeInUs(UniTimer::GetHostTimestamp()));
static std::string pmi_hostname = GetHostName();

std::string GetZeKernelCommandName(uint64_t id, ze_group_count_t& group_count, size_t size, bool detailed);
ze_pci_ext_properties_t *GetZeDevicePciPropertiesAndId(ze_device_handle_t device, int32_t *parent_device_id, int32_t *device_id, int32_t *subdevice_id);

static Logger* logger_ = nullptr;

constexpr unsigned char cpu_op = 0;
constexpr unsigned char gpu_op = 1;
constexpr unsigned char data_flow = 2;
constexpr unsigned char data_flow_sync = 3;
constexpr unsigned char cl_data_flow = 4;
constexpr unsigned char cl_data_flow_sync = 5;

/* TraceDataPacket: It is a raw data structure which will store traces.
 * At the time of file write it will be converted into to std::string and write into the file.
 * TODO: remove this structure for both OpenCl and Level0
 */
typedef struct TraceDataPacket_ {
  unsigned char ph;
  unsigned char cat;
  unsigned int rank;
  uint32_t tid;
  uint32_t pid;
  uint64_t id;
  uint64_t kernel_command_id;
  std::string name;
  std::string cname;
  uint64_t ts;
  uint64_t dur;
  std::string args;
  API_TRACING_ID api_id;

  /*
   * Stringify(): creates json format of string from the traceDataPacket
   */
  std::string Stringify() {
    std::string str = "{"; // header
  
    str += "\"ph\": \"";
    str += ph;
    str += "\"";

    str += ", \"tid\": " + std::to_string(tid);
    str += ", \"pid\": " + std::to_string(pid);

    if (api_id == ClKernelTracingId) {
      name = utils::Demangle(name.data());
    }
    else {
      if ((api_id != OpenClTracingId) && (api_id != XptiTracingId) && (api_id != IttTracingId) && (api_id != ZeKernelTracingId)) {
        // L0 kernel names are already demanged/
        name = get_symbol(api_id);
      }
    }
    if (!name.empty()) {
      if (name[0] == '\"') {
        // name is already quoted
        str += ", \"name\": " + name;
      }
      else {
        str += ", \"name\": \"" + name + "\"";
      }
    }
    if (!cname.empty()) {
      if (cname[0] == '\"') {
        // cname is already quoted
        str += ", \"cname\": " + cname;
      }
      else {
        str += ", \"cname\": \"" + cname + "\"";
      }
    }

    if (cat == cpu_op) {
      str += ", \"cat\": \"cpu_op\"";
    }
    else if (cat == gpu_op) {
      str += ", \"cat\": \"gpu_op\"";
    }
    else if (cat == data_flow) {
      str += ", \"cat\": \"Flow_H2D_" + std::to_string(id) + "_" + std::to_string(rank) + "\"";
    }
    else if (cat == data_flow_sync) {
      str += ", \"cat\": \"Flow_D2H_" + std::to_string(id) + "_" + std::to_string(rank) + "\"";
    }
    else if (cat == cl_data_flow) {
      str += ", \"cat\": \"CL_Flow_H2D_" + std::to_string(id) + "_" + std::to_string(rank) + "\"";
    }
    else if (cat == cl_data_flow_sync) {
      str += ", \"cat\": \"CL_Flow_D2H_" + std::to_string(id) + "_" + std::to_string(rank) + "\"";
    }

    // It is always present
    if (ts != (uint64_t)(-1)) {
      str += ", \"ts\": " + std::to_string(ts);
    }
    if (dur != (uint64_t)(-1)) {
      str += ", \"dur\": " + std::to_string(dur);
    }
    if (!args.empty()) {
      str += ", \"args\": {" + args + "}";
    } else {
      str += ", \"id\": " + std::to_string(id);
    }
    // end
    str += "},\n"; // footer
  
    return str;
  }
} TraceDataPacket;

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
};

struct ZeDevicePidKeyCompare {
  bool operator()(const ZeDevicePidKey& lhs, const ZeDevicePidKey& rhs) const {
    return (memcmp((char *)(&lhs), (char *)(&rhs), sizeof(ZeDevicePidKey)) < 0);
  }
};
static std::map<ZeDevicePidKey, std::tuple<uint32_t,uint64_t>, ZeDevicePidKeyCompare> device_pid_map_;
struct ZeDeviceTidKeyCompare {
  bool operator()(const ZeDeviceTidKey& lhs, const ZeDeviceTidKey& rhs) const {
    return (memcmp((char *)(&lhs), (char *)(&rhs), sizeof(ZeDeviceTidKey)) < 0);
  }
};

static std::map<ZeDeviceTidKey, std::tuple<uint32_t, uint32_t, uint64_t>, ZeDeviceTidKeyCompare> device_tid_map_;

static uint32_t next_device_pid_ = (uint32_t)(~0) - (mpi_rank * (1 << 13));	// each rank has no more than (1 << 13) threads
static uint32_t next_device_tid_ = (uint32_t)(~0) - (mpi_rank * (1 << 13));    // each rank has no more than (1 << 13) threads

static std::tuple<uint32_t, uint32_t> GetDevicePidTid(ze_device_handle_t device, uint32_t engine_ordinal,
  uint32_t engine_index, int host_pid, int host_tid) {
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
  
  auto it = device_tid_map_.find({props->address, parent_device_id, device_id, subdevice_id, engine_ordinal, engine_index, host_pid, host_tid});
  if (it != device_tid_map_.cend()) {
    device_pid = std::get<0>(it->second);
    device_tid = std::get<1>(it->second);
  }
  else {
    auto it2 = device_pid_map_.find({props->address, parent_device_id, device_id, subdevice_id, host_pid});
    if (it2 != device_pid_map_.cend()) {
      device_pid = std::get<0>(it2->second);
    }
    else {
      device_pid = next_device_pid_--;
      auto start_time = UniTimer::GetEpochTimeInUs(UniTimer::GetHostTimestamp());
      device_pid_map_.insert({{props->address, parent_device_id, device_id, subdevice_id, host_pid}, std::make_tuple(device_pid, start_time)});
    }
    device_tid = next_device_tid_--;
    auto start_time = UniTimer::GetEpochTimeInUs(UniTimer::GetHostTimestamp());
    device_tid_map_.insert({{props->address, parent_device_id, device_id, subdevice_id, engine_ordinal, engine_index, host_pid, host_tid}, std::make_tuple(device_pid, device_tid,start_time)});
  }

  return std::tuple<uint32_t, uint32_t>(device_pid, device_tid);
}

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
};

struct ClDevicePidKeyCompare {
  bool operator()(const ClDevicePidKey& lhs, const ClDevicePidKey& rhs) const {
    return (memcmp((char *)(&lhs), (char *)(&rhs), sizeof(ClDevicePidKey)) < 0);
  }
};
static std::map<ClDevicePidKey, std::tuple<uint32_t,uint64_t>, ClDevicePidKeyCompare> cl_device_pid_map_;
struct ClDeviceTidKeyCompare {
  bool operator()(const ClDeviceTidKey& lhs, const ClDeviceTidKey& rhs) const {
    return (memcmp((char *)(&lhs), (char *)(&rhs), sizeof(ClDeviceTidKey)) < 0);
  }
};

static std::map<ClDeviceTidKey, std::tuple<uint32_t, uint32_t, uint64_t>, ClDeviceTidKeyCompare> cl_device_tid_map_;

static std::tuple<uint32_t, uint32_t> ClGetDevicePidTid(cl_device_pci_bus_info_khr& pci, cl_device_id device, cl_command_queue queue, int host_pid, int host_tid) {
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
   
  auto it = cl_device_tid_map_.find({pci, device, queue, host_pid, host_tid});
  if (it != cl_device_tid_map_.cend()) {
    device_pid = std::get<0>(it->second);
    device_tid = std::get<1>(it->second);
  }
  else {
    auto it2 = cl_device_pid_map_.find({pci, device, host_pid});
    if (it2 != cl_device_pid_map_.cend()) {
      device_pid = std::get<0>(it2->second);
    }
    else {
      device_pid = next_device_pid_--;
      auto start_time = UniTimer::GetEpochTimeInUs(UniTimer::GetHostTimestamp());
      cl_device_pid_map_.insert({{pci, device, host_pid}, std::make_tuple(device_pid, start_time)});
    }
    device_tid = next_device_tid_--;
    auto start_time = UniTimer::GetEpochTimeInUs(UniTimer::GetHostTimestamp());
    cl_device_tid_map_.insert({{pci, device, queue, host_pid, host_tid}, std::make_tuple(device_pid, device_tid, start_time)});
  }

  return std::tuple<uint32_t, uint32_t>(device_pid, device_tid);
}

class TraceBuffer;
std::set<TraceBuffer *> *trace_buffers_ = nullptr;

#define BUFFER_SLICE_SIZE_DEFAULT	(0x1 << 20)

class TraceBuffer {
  public:
    TraceBuffer() {
      std::string szstr = utils::GetEnv("UNITRACE_ChromeEventBufferSize");
      if (szstr.empty() || (szstr == "-1")) {
        buffer_capacity_ = -1;
        slice_capacity_ = BUFFER_SLICE_SIZE_DEFAULT;
      }
      else {
        buffer_capacity_ = std::stoi(szstr);
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

      current_device_event_buffer_slice_ = 0;
      current_host_event_buffer_slice_ = 0;
      next_device_event_index_ = 0;
      next_host_event_index_ = 0;
      device_event_buffer_flushed_ = false;
      host_event_buffer_flushed_ = false;
      finalized_.store(false, std::memory_order_release);

      std::lock_guard<std::recursive_mutex> lock(logger_lock_);	// use this lock to protect trace_buffers_
      
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
      next_host_event_index_++;
      host_event_buffer_flushed_ = false;
    }

    void BufferDeviceEvent(void) {
      next_device_event_index_++;
      device_event_buffer_flushed_ = false;
    }

    uint32_t GetTid() { return tid_; }
    uint32_t GetPid() { return pid_; }

    void FlushDeviceEvent(ZeKernelCommandExecutionRecord& rec) {
      auto [device_pid, device_tid] = GetDevicePidTid(rec.device_, rec.engine_ordinal_, rec.engine_index_, GetPid(), rec.tid_);
      {
        TraceDataPacket pkt{};
        pkt.ph = 'X';
        pkt.tid = device_tid;
        pkt.pid = device_pid;
        pkt.kernel_command_id = rec.kernel_command_id_;
        if (rec.implicit_scaling_) {
          pkt.name = "Tile #" + std::to_string(rec.tile_) + ": " + GetZeKernelCommandName(rec.kernel_command_id_, rec.group_count_, rec.mem_size_);
        }
        else {
          pkt.name = GetZeKernelCommandName(rec.kernel_command_id_, rec.group_count_, rec.mem_size_);
        }
        pkt.api_id = ZeKernelTracingId;
        pkt.ts = UniTimer::GetEpochTimeInUs(rec.start_time_);
        //pkt.dur = UniTimer::GetEpochTimeInUs(rec.end_time_) - UniTimer::GetEpochTimeInUs(rec.start_time_);
        pkt.dur = UniTimer::GetTimeInUs(rec.end_time_ - rec.start_time_);
        pkt.cat = gpu_op;
        pkt.args = "\"id\": \"" + std::to_string(rec.kid_) + "\"";
        logger_->Log(pkt.Stringify());
      }

      if (!rec.implicit_scaling_) {
        {
          TraceDataPacket pkt{};
          pkt.ph = 't';
          pkt.tid = device_tid;
          pkt.pid = device_pid;
          pkt.api_id = DepTracingId;
          pkt.id = rec.kid_;
          pkt.ts = UniTimer::GetEpochTimeInUs(rec.start_time_);
          pkt.dur = (uint64_t)(-1);
          pkt.cat = data_flow;
          pkt.rank = mpi_rank;
          logger_->Log(pkt.Stringify());
        }

        {
          TraceDataPacket pkt{};
          pkt.ph = 's';
          pkt.tid = device_tid;
          pkt.pid = device_pid;
          pkt.api_id = DepTracingId;
          pkt.id = rec.kid_;
          pkt.ts = UniTimer::GetEpochTimeInUs(rec.start_time_);
          pkt.dur = (uint64_t)(-1);
          pkt.cat = data_flow_sync;
          pkt.rank = mpi_rank;
          logger_->Log(pkt.Stringify());
        }
      }
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
      TraceDataPacket pkt{};

      if (rec.type_ == EVENT_COMPLETE) {
        pkt.ph = 'X';
        pkt.dur = UniTimer::GetTimeInUs(rec.end_time_ - rec.start_time_);

/*
        std::string str_kids = "";
        if (kids == nullptr) {
          str_kids = "0";
        }
        else {
          int i = 0;
          for (auto id : *kids) {
            if (i != 0) {
              str_kids += ",";
            }
            str_kids += std::to_string(id);
            i++;
          }
        }
        pkt.args = "\"id\": \"" + str_kids + "\"";
*/
        pkt.cat = cpu_op;
      }
      else if (rec.type_ == EVENT_DURATION_START) {
        pkt.ph = 'B';
        pkt.dur = (uint64_t)(-1);
        pkt.cat = cpu_op;
      }
      else if (rec.type_ == EVENT_DURATION_END) {
        pkt.ph = 'E';
        pkt.dur = (uint64_t)(-1);
        pkt.cat = cpu_op;
      }
      else if (rec.type_ == EVENT_FLOW_SOURCE) {
        pkt.ph = 's';
        pkt.id = rec.id_;
        pkt.dur = (uint64_t)(-1);
        pkt.cat = data_flow;
      }
      else if (rec.type_ == EVENT_FLOW_SINK) {
        pkt.ph = 't';
        pkt.id = rec.id_;
        pkt.dur = (uint64_t)(-1);
        pkt.cat = data_flow_sync;
      }
      else if (rec.type_ == EVENT_MARK) {
        pkt.ph = 'R';
        pkt.dur = (uint64_t)(-1);
        pkt.cat = cpu_op;
      }
      else {
        // should never get here
      }

      pkt.tid = GetTid();
      pkt.pid = GetPid();
      pkt.api_id = rec.api_id_;
      pkt.ts = UniTimer::GetEpochTimeInUs(rec.start_time_);
      //pkt.dur = UniTimer::GetEpochTimeInUs(ended) - UniTimer::GetEpochTimeInUs(started);
      pkt.rank = mpi_rank;
      pkt.name = rec.name_;

      logger_->Log(pkt.Stringify());
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
    int32_t slice_capacity_;	// each buffer can have multiple slices
    int32_t current_device_event_buffer_slice_;	// device slice in use
    int32_t current_host_event_buffer_slice_;	// host slice in use
    int32_t next_device_event_index_;	// next free device event in in-use slice
    int32_t next_host_event_index_;	// next free host event in in-use slice
    uint32_t tid_;
    uint32_t pid_;
    std::vector<ZeKernelCommandExecutionRecord *> device_event_buffer_;
    std::vector<HostEventRecord *> host_event_buffer_;
    bool host_event_buffer_flushed_;
    bool device_event_buffer_flushed_;
    std::atomic<bool> finalized_;
};

thread_local TraceBuffer thread_local_buffer_;

// TODO: unify trace buffers for both L0 and OCL
class ClTraceBuffer;
std::set<ClTraceBuffer *> *cl_trace_buffers_ = nullptr;
class ClTraceBuffer {
  public:
    ClTraceBuffer() {
      if (utils::GetEnv("UNITRACE_ChromeEventBufferSize").empty()) {
        max_num_buffered_events_ = -1;
      }
      else {
        max_num_buffered_events_ = std::stoi(utils::GetEnv("UNITRACE_ChromeEventBufferSize"));
      }
      if (max_num_buffered_events_ > 0) {
        buffer_.reserve(max_num_buffered_events_);
      }
      tid_= utils::GetTid();
      pid_= utils::GetPid();
      flushed_ = false;
      finalized_.store(false, std::memory_order_release);
      std::lock_guard<std::recursive_mutex> lock(logger_lock_);
      
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
        if (!flushed_) {
          for(auto& ele : buffer_) {
            logger_->Log(ele.Stringify());
          }
          cl_trace_buffers_->erase(this);
          flushed_ = true;
        }
      }
    }

    ClTraceBuffer(const ClTraceBuffer& that) = delete;
    ClTraceBuffer& operator=(const ClTraceBuffer& that) = delete;

    void Buffer(TraceDataPacket& pkt) {
      buffer_.push_back(pkt);
      flushed_ = false;
      if (max_num_buffered_events_ != -1) {
        if (buffer_.size() >= max_num_buffered_events_) {
          FlushBuffer();
        }
      }
    }

    void BufferWithPidTid(TraceDataPacket& pkt) {
      pkt.tid = tid_;
      pkt.pid = pid_;
      buffer_.push_back(pkt);
      flushed_ = false;
      if (max_num_buffered_events_ != -1) {
        if (buffer_.size() >= max_num_buffered_events_) {
          FlushBuffer();
        }
      }
    }

    uint32_t GetTid() { return tid_; }
    uint32_t GetPid() { return pid_; }

    void FlushBuffer() {
      std::lock_guard<std::recursive_mutex> lock(logger_lock_);
      if (flushed_) {
        return;
      }

      for(auto& ele : buffer_) {
        logger_->Log(ele.Stringify());
      }
      buffer_.clear();
      flushed_ = true;
    }
    
    void Finalize() {
      
      std::lock_guard<std::recursive_mutex> lock(logger_lock_);
      if (!finalized_.exchange(true)) {
        // finalize if not finalized
        if (!flushed_) {
          for(auto& ele : buffer_) {
            logger_->Log(ele.Stringify());
          }
          flushed_ = true;
        }
      }
    }
    
    bool IsFinalized() {
      return finalized_.load(std::memory_order_acquire);
    }
    
  private:
    int32_t max_num_buffered_events_;
    uint32_t tid_;
    uint32_t pid_;
    std::vector<TraceDataPacket> buffer_;
    bool flushed_;
    std::atomic<bool> finalized_;
};

thread_local ClTraceBuffer cl_thread_local_buffer_;

class ChromeLogger {
  private:
    TraceOptions options_;
    bool filtering_on_ = true;
    bool filter_in_ = false;  // --filter-in suggests only include/collect these kernel names in output (filter-out is reverse of this and excludes/drops these)
    std::set<std::string> filter_strings_set_;
    std::string chrome_trace_file_name_;
    Correlator* correlator_ = nullptr;
    ChromeLogger(const TraceOptions& options, Correlator* correlator, const char* filename) : options_(options) {
      correlator_ = correlator;
      chrome_trace_file_name_ = TraceOptions::GetChromeTraceFileName(filename);
      if (this->CheckOption(TRACE_OUTPUT_DIR_PATH)) {
          std::string dir = utils::GetEnv("UNITRACE_TraceOutputDir");
          chrome_trace_file_name_ = (dir + '/' + chrome_trace_file_name_);
      }
      std::string tmpString;
      if (this->CheckOption(TRACE_KERNEL_NAME_FILTER)) {
          if (this->CheckOption(TRACE_K_NAME_FILTER_IN)) filter_in_ = true; 
	  tmpString = utils::GetEnv("UNITRACE_TraceKernelString");
	  filter_strings_set_.insert(tmpString);
      } else if (this->CheckOption(TRACE_K_NAME_FILTER_FILE)) {
              if (this->CheckOption(TRACE_K_NAME_FILTER_IN)) filter_in_ = true; 
	      std::string kernel_file =  utils::GetEnv("UNITRACE_TraceKernelFilePath"); 
	      std::ifstream kfile(kernel_file, std::ios::in);
              PTI_ASSERT(kfile.fail() != 1 && kfile.eof() != 1);
	      while (!kfile.eof()) {
                kfile >> tmpString;
		filter_strings_set_.insert(tmpString);
	      }
      } else {
          filtering_on_ = false;
	  filter_strings_set_.insert("ALL");
      }
      logger_ = new Logger(chrome_trace_file_name_.c_str(), true, true);
      UniMemory::ExitIfOutOfMemory((void *)(logger_));

      logger_->Log("{ \"traceEvents\":[\n");
      logger_->Flush();
    };
  public:
    ChromeLogger(const ChromeLogger& that) = delete;
    ChromeLogger& operator=(const ChromeLogger& that) = delete;

    ~ChromeLogger() {
      if (logger_ != nullptr) {
        logger_lock_.lock();
        if (trace_buffers_) {
          for (auto it = trace_buffers_->begin(); it != trace_buffers_->end();) {
            (*it)->Finalize();
            it = trace_buffers_->erase(it);
          }
        }

        if (cl_trace_buffers_) {
          for (auto it = cl_trace_buffers_->begin(); it != cl_trace_buffers_->end();) {
            (*it)->Finalize();
            it = cl_trace_buffers_->erase(it);
          }
        }

        logger_lock_.unlock();

        std::string str("{\"ph\": \"M\", \"name\": \"process_name\", \"pid\": ");

        str += std::to_string(utils::GetPid()) + ", \"ts\": " + process_start_time + ", \"args\": {\"name\": \"";

        if (rank.empty()) {
          str += "HOST<" + pmi_hostname + ">\"}}";
        }
        else {
          str += "RANK " + std::to_string(mpi_rank) + " HOST<" + pmi_hostname + ">\"}}";
        }
            
        const std::lock_guard<std::mutex> lock(device_pid_tid_map_lock_);

        for (auto it = device_pid_map_.cbegin(); it != device_pid_map_.cend(); it++) {
          uint32_t device_pid = std::get<0>(it->second);
          str += ",\n{\"ph\": \"M\", \"name\": \"process_name\", \"pid\": " + std::to_string(device_pid) +
                 ", \"ts\": " + std::to_string(std::get<1>(it->second)) + ", \"args\": {\"name\": \"";
          if (rank.empty()) {
            str += "DEVICE<" + pmi_hostname + ">";
          }
          else {
            str += "RANK " + std::to_string(mpi_rank) + " DEVICE<" + pmi_hostname + ">";
          }

          char str2[128];
          snprintf(str2, sizeof(str2), "%x", it->first.pci_addr_.domain);
          str += std::string(str2) + ":";
          snprintf(str2, sizeof(str2), "%x", it->first.pci_addr_.bus);
          str += std::string(str2) + ":";
          snprintf(str2, sizeof(str2), "%x", it->first.pci_addr_.device);
          str += std::string(str2) + ":";
          snprintf(str2, sizeof(str2), "%x", it->first.pci_addr_.function);
          str += std::string(str2);

          if (it->first.parent_device_id_ >= 0) {
            str += " #" + std::to_string(it->first.parent_device_id_) + "." + std::to_string(it->first.subdevice_id_);
          }
          else {
            str += " #" + std::to_string(it->first.device_id_);
          }

          str += "\"}}"; 
        }
        for (auto it = device_tid_map_.cbegin(); it != device_tid_map_.cend(); it++) {
          uint32_t device_pid = std::get<0>(it->second);
          uint32_t device_tid = std::get<1>(it->second);
          str += ",\n{\"ph\": \"M\", \"name\": \"thread_name\", \"pid\": " + std::to_string(device_pid) + ", \"tid\": " +
                 std::to_string(device_tid) + ", \"ts\": " + std::to_string(std::get<2>(it->second)) + ", \"args\": {\"name\": \"";
          if (device_logging_no_thread_) {
            if (device_logging_no_engine_) {
              str += "L0\"}}";
            }
            else {
              str += "L0 Engine<" + std::to_string(it->first.engine_ordinal_) + "," + std::to_string(it->first.engine_index_) + ">\"}}"; 
            }
          }
          else {
            if (device_logging_no_engine_) {
              str += "Thread " + std::to_string(it->first.host_tid_) + " L0\"}}"; 
            }
            else {
              str += "Thread " + std::to_string(it->first.host_tid_) + " L0 Engine<" + std::to_string(it->first.engine_ordinal_) +
                     "," + std::to_string(it->first.engine_index_) + ">\"}}"; 
            }
          }
        }
    
        for (auto it = cl_device_pid_map_.cbegin(); it != cl_device_pid_map_.cend(); it++) {
          uint32_t device_pid = std::get<0>(it->second);
          str += ",\n{\"ph\": \"M\", \"name\": \"process_name\", \"pid\": " + std::to_string(device_pid) +
                 ", \"ts\": " + std::to_string(std::get<1>(it->second)) + ", \"args\": {\"name\": \"";
          if (rank.empty()) {
            str += "DEVICE<" + pmi_hostname + ">";
          }
          else {
            str += "RANK " + std::to_string(mpi_rank) + " DEVICE<" + pmi_hostname + ">";
          }

          char str2[128];
          snprintf(str2, sizeof(str2), "%x", it->first.pci_addr_.pci_domain);
          str += std::string(str2) + ":";
          snprintf(str2, sizeof(str2), "%x", it->first.pci_addr_.pci_bus);
          str += std::string(str2) + ":";
          snprintf(str2, sizeof(str2), "%x", it->first.pci_addr_.pci_device);
          str += std::string(str2) + ":";
          snprintf(str2, sizeof(str2), "%x", it->first.pci_addr_.pci_function);
          str += std::string(str2);

          str += "\"}}"; 
        }
        for (auto it = cl_device_tid_map_.cbegin(); it != cl_device_tid_map_.cend(); it++) {
          uint32_t device_pid = std::get<0>(it->second);
          uint32_t device_tid = std::get<1>(it->second);
          str += ",\n{\"ph\": \"M\", \"name\": \"thread_name\", \"pid\": " + std::to_string(device_pid) +
                 ", \"tid\": " + std::to_string(device_tid) +
                 ", \"ts\": " + std::to_string(std::get<2>(it->second)) + ", \"args\": {\"name\": \"";
          if (device_logging_no_thread_) {
            if (device_logging_no_engine_) {
              str += "CL\"}}";
            }
            else {
              char str2[128];

              snprintf(str2, sizeof(str2), "%p", it->first.queue_);
              str += "CL Queue<" + std::string(str2) + ">\"}}"; 
            }
          }
          else {
            if (device_logging_no_engine_) {
              str += "Thread " + std::to_string(it->first.host_tid_) + " CL\"}}"; 
            }
            else {
              char str2[128];

              snprintf(str2, sizeof(str2), "%p", it->first.queue_);
              str += "Thread " + std::to_string(it->first.host_tid_) + " CL Queue<" + std::string(str2) + ">\"}}"; 
            }
          }
        }
    
        str += "\n]\n}\n";
      
        logger_->Log(str);
        delete logger_;
        std::cerr << "[INFO] Timeline is stored in " << chrome_trace_file_name_ << std::endl;
      }
    };

    static ChromeLogger* Create(const TraceOptions& options, Correlator* correlator, const char* filename) {
      ChromeLogger *chrome_logger  = new ChromeLogger(options, correlator, filename);
      UniMemory::ExitIfOutOfMemory((void *)(chrome_logger));
      return chrome_logger;
    };

    bool CheckOption(uint32_t option) {
      return options_.CheckFlag(option);
    }

    static void XptiLoggingCallback(EVENT_TYPE etype, const char *name, uint64_t start_ts, uint64_t end_ts) {
      if (!thread_local_buffer_.IsFinalized()) {
        HostEventRecord *rec = thread_local_buffer_.GetHostEvent();

        rec->type_ = etype;
        rec->name_ = name;
        rec->api_id_ = XptiTracingId;
        rec->start_time_ = start_ts;
        if (etype == EVENT_COMPLETE) {
          rec->end_time_ = end_ts;
        }
        rec->id_ = 0;
        thread_local_buffer_.BufferHostEvent();
      }
    }

    static void IttLoggingCallback(const char *name, uint64_t start_ts, uint64_t end_ts) {
      if (!thread_local_buffer_.IsFinalized()) {
        HostEventRecord *rec = thread_local_buffer_.GetHostEvent();

        rec->type_ = EVENT_COMPLETE;
        rec->name_ = name;
        rec->api_id_ = IttTracingId;
        rec->start_time_ = start_ts;
        rec->end_time_ = end_ts;
        rec->id_ = 0;
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

    // OnenCL tracer callbacks.
    // TODO: remove TraceDataPacket for performance
    static void ClChromeKernelLoggingCallback(
      cl_device_pci_bus_info_khr& pci,
      cl_device_id device,
      cl_command_queue& queue,
      int tile,
      bool implicit, 
      const uint64_t id,
      const std::string& name,
      uint64_t queued,
      uint64_t submitted,
      uint64_t started,
      uint64_t ended) {

      if (cl_thread_local_buffer_.IsFinalized()) {
        return;
      }

      auto [device_pid, device_tid] = ClGetDevicePidTid(pci, device, queue, utils::GetPid(), utils::GetTid());
  
      PTI_ASSERT(ended > started);
      if (implicit) {
        {
          TraceDataPacket pkt;
          pkt.ph = 'X';
          pkt.tid = device_tid,
          pkt.pid = device_pid;
          pkt.name = "Tile #" + std::to_string(tile) + ": " + name;
          pkt.ts = UniTimer::GetEpochTimeInUs(started);
          //pkt.dur = UniTimer::GetEpochTimeInUs(ended) - UniTimer::GetEpochTimeInUs(started);
          pkt.dur = UniTimer::GetTimeInUs(ended - started);
          pkt.args = "\"id\": \""+std::to_string(id)+"\"";
          pkt.cat = gpu_op;
          pkt.api_id = ClKernelTracingId;
          cl_thread_local_buffer_.Buffer(pkt);
        }

        {
          TraceDataPacket pkt;
          pkt.ph = 's';
          pkt.tid = device_tid;
          pkt.pid = device_pid;
          pkt.api_id = DepTracingId;
          pkt.id = id;
          pkt.ts = UniTimer::GetEpochTimeInUs(started);
          pkt.dur = (uint64_t)(-1);
          pkt.cat = cl_data_flow;
          pkt.rank = mpi_rank;
          cl_thread_local_buffer_.Buffer(pkt);
        }

        {
          TraceDataPacket pkt;
          pkt.ph = 's';
          pkt.tid = device_tid;
          pkt.pid = device_pid;
          pkt.api_id = DepTracingId;
          pkt.id = id;
          pkt.ts = UniTimer::GetEpochTimeInUs(started);
          pkt.dur = (uint64_t)(-1);
          pkt.cat = cl_data_flow_sync;
          pkt.rank = mpi_rank;
          cl_thread_local_buffer_.Buffer(pkt);
        }
      }
      else {
        {
          TraceDataPacket pkt;
          pkt.ph = 'X';
          pkt.tid = device_tid;
          pkt.pid = device_pid;
          pkt.name = name;
          pkt.ts = UniTimer::GetEpochTimeInUs(started);
          //pkt.dur = UniTimer::GetEpochTimeInUs(ended) - UniTimer::GetEpochTimeInUs(started)
          pkt.dur = UniTimer::GetTimeInUs(ended - started);
          pkt.args = "\"id\": \""+std::to_string(id)+"\"";
          pkt.cat = gpu_op;
          pkt.api_id = ClKernelTracingId;
          cl_thread_local_buffer_.Buffer(pkt);
        }
        {
          TraceDataPacket pkt;
          pkt.ph = 's';
          pkt.tid = device_tid;
          pkt.pid = device_pid;
          pkt.api_id = DepTracingId;
          pkt.id = id;
          pkt.ts = UniTimer::GetEpochTimeInUs(started);
          pkt.dur = (uint64_t)(-1);
          pkt.cat = cl_data_flow;
          pkt.rank = mpi_rank;
          cl_thread_local_buffer_.Buffer(pkt);
        }

        {
          TraceDataPacket pkt;
          pkt.ph = 's';
          pkt.tid = device_tid;
          pkt.pid = device_pid;
          pkt.api_id = DepTracingId;
          pkt.id = id;
          pkt.ts = UniTimer::GetEpochTimeInUs(started);
          pkt.dur = (uint64_t)(-1);
          pkt.cat = cl_data_flow_sync;
          pkt.rank = mpi_rank;
          cl_thread_local_buffer_.Buffer(pkt);
        }
      }
    }

    static void ChromeCallLoggingCallback(std::vector<uint64_t> *kids, FLOW_DIR flow_dir, API_TRACING_ID api_id,
      uint64_t started, uint64_t ended) {

      if (thread_local_buffer_.IsFinalized()) {
        return;
      }

      HostEventRecord *rec = thread_local_buffer_.GetHostEvent();

      rec->type_ = EVENT_COMPLETE;
      rec->api_id_ = api_id;
      rec->start_time_ = started;
      rec->end_time_ = ended;
      rec->id_ = 0;
      thread_local_buffer_.BufferHostEvent();

      if ((kids != nullptr) && (flow_dir == FLOW_H2D)) {
        for (auto id : *kids) {
          rec = thread_local_buffer_.GetHostEvent();

          rec->type_ = EVENT_FLOW_SOURCE;
          rec->api_id_ = DepTracingId;
          rec->start_time_ = started;
          rec->id_ = id;
          thread_local_buffer_.BufferHostEvent();
        }
      }

      if ((kids != nullptr) && (flow_dir == FLOW_D2H)) {
        for (auto id : *kids) {
          rec = thread_local_buffer_.GetHostEvent();

          rec->type_ = EVENT_FLOW_SINK;
          rec->api_id_ = DepTracingId;
          rec->start_time_ = started;
          rec->id_ = id;
          thread_local_buffer_.BufferHostEvent();
        }
      }
    }

    static void ClChromeCallLoggingCallback(std::vector<uint64_t> *kids, FLOW_DIR flow_dir, const std::string& name,
      uint64_t started, uint64_t ended) {
      if (thread_local_buffer_.IsFinalized()) {
        return;
      }

      {
        TraceDataPacket pkt{};
        pkt.ph = 'X';
        pkt.tid = utils::GetTid();
        pkt.pid = utils::GetPid();
        pkt.name = name;
        pkt.api_id = OpenClTracingId;
        pkt.ts = UniTimer::GetEpochTimeInUs(started);
        //pkt.dur = UniTimer::GetEpochTimeInUs(ended) - UniTimer::GetEpochTimeInUs(started);
        pkt.dur = UniTimer::GetTimeInUs(ended - started);
        pkt.cat = cpu_op;

        std::string str_kids="";
        if (kids == nullptr) {
          str_kids = "0";
        }
        else {
          int i = 0;
          for (auto id : *kids) {
            if (i != 0) {
              str_kids += ",";
            }
            str_kids += std::to_string(id);
            i++;
          }
        }
        pkt.args = "\"id\": \"" + str_kids + "\"";
        cl_thread_local_buffer_.Buffer(pkt);
      }

      if ((kids != nullptr) && (flow_dir == FLOW_H2D)) {
        for (auto id : *kids) {
          TraceDataPacket pkt{};
          pkt.ph = 's';
          pkt.tid = utils::GetTid();
          pkt.pid = utils::GetPid();
          pkt.api_id = DepTracingId;
          pkt.id = id;
          pkt.ts = UniTimer::GetEpochTimeInUs(started);
          pkt.dur = (uint64_t)(-1);
          pkt.cat = data_flow;
          pkt.rank = mpi_rank;
          cl_thread_local_buffer_.Buffer(pkt);
        }
      }

      if ((kids != nullptr) && (flow_dir == FLOW_D2H)) {
        for (auto id : *kids) {
          TraceDataPacket pkt{};
          pkt.ph = 't';
          pkt.tid = utils::GetTid();
          pkt.pid = utils::GetPid();
          pkt.api_id = DepTracingId;
          pkt.id = id;
          pkt.ts = UniTimer::GetEpochTimeInUs(started);
          pkt.dur = (uint64_t)(-1);
          pkt.cat = data_flow_sync;
          pkt.rank = mpi_rank;
          cl_thread_local_buffer_.Buffer(pkt);
        }
      }
    }
};

#endif // PTI_TOOLS_COMMON_CHROME_LOGGER_H_

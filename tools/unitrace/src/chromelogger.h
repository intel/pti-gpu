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
#include "trace_options.h"
#include "unitimer.h"
#include "unikernel.h"
#include "unievent.h"
#include "unimemory.h"

#include "common_header.gen"

static inline std::string GetHostName(void) {
  char hname[256];
#ifdef _WIN32
  DWORD size = sizeof(hname);
  GetComputerNameA(hname, &size);
#else
  gethostname(hname, sizeof(hname));
#endif
  hname[255] = 0;
  return hname;
}

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
static uint32_t mpi_rank = std::atoi(rank.c_str());

static std::string pmi_hostname = GetHostName();

std::string GetZeKernelCommandName(uint64_t id, ze_group_count_t& group_count, size_t size, bool detailed);
ze_pci_ext_properties_t *GetZeDevicePciPropertiesAndId(ze_device_handle_t device, int32_t *parent_device_id, int32_t *device_id, int32_t *subdevice_id);
std::string GetClKernelCommandName(uint64_t id);

static Logger* logger_ = nullptr;

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
  
  ZeDeviceTidKey tid_key; memset(&tid_key, 0, sizeof(ZeDeviceTidKey));
  tid_key.pci_addr_ = props->address;
  tid_key.parent_device_id_ = parent_device_id;
  tid_key.device_id_ = device_id;
  tid_key.subdevice_id_ = subdevice_id;
  tid_key.engine_ordinal_ = engine_ordinal;
  tid_key.engine_index_ = engine_index;
  tid_key.host_pid_ = host_pid;
  tid_key.host_tid_ = host_tid;

  auto it = device_tid_map_.find(tid_key);
  if (it != device_tid_map_.cend()) {
    device_pid = std::get<0>(it->second);
    device_tid = std::get<1>(it->second);
  }
  else {
    ZeDevicePidKey pid_key; memset(&pid_key, 0, sizeof(ZeDevicePidKey));
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
    }
    device_tid = next_device_tid_--;
    auto start_time = UniTimer::GetEpochTimeInUs(UniTimer::GetHostTimestamp());
    device_tid_map_.insert({tid_key, std::make_tuple(device_pid, device_tid,start_time)});
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

  ClDeviceTidKey tid_key; memset(&tid_key, 0, sizeof(ClDeviceTidKey));
  tid_key.pci_addr_ = pci;
  tid_key.device_ = device;
  tid_key.queue_ = queue;
  tid_key.host_pid_ = host_pid;
  tid_key.host_tid_ = host_tid;

  auto it = cl_device_tid_map_.find(tid_key);
  if (it != cl_device_tid_map_.cend()) {
    device_pid = std::get<0>(it->second);
    device_tid = std::get<1>(it->second);
  }
  else {
    ClDevicePidKey pid_key; memset(&pid_key, 0, sizeof(ClDevicePidKey));
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
    }
    device_tid = next_device_tid_--;
    auto start_time = UniTimer::GetEpochTimeInUs(UniTimer::GetHostTimestamp());
    cl_device_tid_map_.insert({tid_key, std::make_tuple(device_pid, device_tid, start_time)});
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
      if ((utils::GetEnv("UNITRACE_MetricQuery") == "1") || (utils::GetEnv("UNITRACE_KernelMetrics") == "1")) {
        metrics_enabled_ = true;
      }
      else {
        metrics_enabled_ = false;
      }

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

    std::string StringifyDeviceEvent(ZeKernelCommandExecutionRecord& rec) {
      auto [pid, tid] = GetDevicePidTid(rec.device_, rec.engine_ordinal_, rec.engine_index_, pid_, rec.tid_);
      std::string kname = GetZeKernelCommandName(rec.kernel_command_id_, rec.group_count_, rec.mem_size_);
      std::string str = "{";

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
        str += ", \"metrics\": \"https://localhost:8000/" + EncodeURI(kname) + "/" + std::to_string(rec.kid_) + "\"";
      }
      str += "}},\n";

      if (!rec.implicit_scaling_) {
        str += "{";
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
        str += "},\n";
      }

      return str;
    }

    std::string StringifyHostEvent(HostEventRecord& rec) {
      std::string str = "{";  // header

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
      if (rec.api_type_ == MPI) {
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
      } else if (rec.api_type_ == CCL) {
        const CclArgs& args = rec.ccl_args_;
        str_args += "\"ssize\": " + std::to_string(args.buff_size);
      }

      if (!str_args.empty()) {
        str += ", \"args\": {" + str_args + "}";
      } else {
        str += ", \"id\": " + std::to_string(rec.id_);;
      }

      // end
      str += "},\n";  // footer

      return str;
    }

    void FlushDeviceEvent(ZeKernelCommandExecutionRecord& rec) {
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
    bool metrics_enabled_;
};

thread_local TraceBuffer thread_local_buffer_;

class ClTraceBuffer;
std::set<ClTraceBuffer *> *cl_trace_buffers_ = nullptr;
class ClTraceBuffer {
  public:
    ClTraceBuffer() {
      std::string szstr = utils::GetEnv("UNITRACE_ChromeEventBufferSize");
      if (szstr.empty() || (szstr == "-1")) {
        buffer_capacity_ = -1;
        slice_capacity_ = BUFFER_SLICE_SIZE_DEFAULT;
      } else {
        buffer_capacity_ = std::stoi(szstr);
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

      std::lock_guard<std::recursive_mutex> lock(logger_lock_);	// use this lock to protect trace_buffers_

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
      next_host_event_index_++;
      host_event_buffer_flushed_ = false;
    }

    void BufferDeviceEvent(void) {
      next_device_event_index_++;
      device_event_buffer_flushed_ = false;
    }

    uint32_t GetTid() { return tid_; }
    uint32_t GetPid() { return pid_; }

    std::string StringifyDeviceEvent(ClKernelCommandExecutionRecord& rec) {
      auto [pid, tid] = ClGetDevicePidTid(rec.pci_, rec.device_, rec.queue_, pid_, rec.tid_);
      std::string kname = GetClKernelCommandName(rec.kernel_command_id_);

      std::string str = "{";

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
        str += ", \"metrics\": \"https://localhost:8000/" + EncodeURI(kname) + "/" + std::to_string(rec.kid_) + "\"";
      }
      str += "}},\n";

      if (!rec.implicit_scaling_) {
        str += "{";
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
        str += "},\n";
      }

      return str;
    }

    void FlushDeviceEvent(ClKernelCommandExecutionRecord& rec) {
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
      std::string str = "{";  // header

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
      if (rec.api_type_ == MPI) {
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

      } else if (rec.api_type_ == CCL) {
        const CclArgs& args = rec.ccl_args_;
        str_args += "\"ssize\": " + std::to_string(args.buff_size);
      }

      if (!str_args.empty()) {
        str += ", \"args\": {" + str_args + "}";
      } else {
        str += ", \"id\": " + std::to_string(rec.id_);
      }

      // end
      str += "},\n";  // footer

      return str;
    }

    void FlushHostEvent(HostEventRecord& rec) {
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
    int32_t slice_capacity_;	// each buffer can have multiple slices
    int32_t current_device_event_buffer_slice_;	// device slice in use
    int32_t current_host_event_buffer_slice_;	// host slice in use
    int32_t next_device_event_index_;	// next free device event in in-use slice
    int32_t next_host_event_index_;	// next free host event in in-use slice
    uint32_t tid_;
    uint32_t pid_;
    std::vector<ClKernelCommandExecutionRecord *> device_event_buffer_;
    std::vector<HostEventRecord *> host_event_buffer_;
    bool host_event_buffer_flushed_;
    bool device_event_buffer_flushed_;
    std::atomic<bool> finalized_;
    bool metrics_enabled_;
};

thread_local ClTraceBuffer cl_thread_local_buffer_;

class ChromeLogger {
  private:
    TraceOptions options_;
    bool filtering_on_ = true;
    bool filter_in_ = false;  // --filter-in suggests only include/collect these kernel names in output (filter-out is reverse of this and excludes/drops these)
    std::set<std::string> filter_strings_set_;
    std::string process_name_;
    std::string chrome_trace_file_name_;
    std::iostream::pos_type data_start_pos_;
    uint64_t process_start_time_;

    ChromeLogger(const TraceOptions& options, const char* filename) : options_(options) {
      process_start_time_ = UniTimer::GetEpochTimeInUs(UniTimer::GetHostTimestamp());
      process_name_ = filename;
      chrome_trace_file_name_ = TraceOptions::GetChromeTraceFileName(filename);
      if (this->CheckOption(TRACE_OUTPUT_DIR_PATH)) {
          std::string dir = utils::GetEnv("UNITRACE_TraceOutputDir");
          chrome_trace_file_name_ = (dir + '/' + chrome_trace_file_name_);
      }
      std::string tmpString;
      if (this->CheckOption(TRACE_KERNEL_NAME_FILTER)) {
        if (this->CheckOption(TRACE_K_NAME_FILTER_IN)) {
          filter_in_ = true;
        }
        tmpString = utils::GetEnv("UNITRACE_TraceKernelString");
        filter_strings_set_.insert(tmpString);
      } else if (this->CheckOption(TRACE_K_NAME_FILTER_FILE)) {
        if (this->CheckOption(TRACE_K_NAME_FILTER_IN)) {
          filter_in_ = true;
        }
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
      data_start_pos_ = logger_->GetLogFilePosition();
    }

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

        str += std::to_string(utils::GetPid()) + ", \"ts\": " + std::to_string(process_start_time_) + ", \"args\": {\"name\": \"";

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

        if (logger_->GetLogFilePosition() == data_start_pos_) {
          // no data has been logged
          // remove the log file, but close it first
          delete logger_;
          if (std::remove(chrome_trace_file_name_.c_str()) == 0) {
            std::cerr << "[INFO] No event of interest is logged for process " << utils::GetPid() << " (" << process_name_ << ")" << std::endl;
          } else {
            std::cerr << "[INFO] No event of interest is logged for process " << utils::GetPid() << " (" << process_name_ << ") in file " << chrome_trace_file_name_ << std::endl;
          }
        } else {
          str += "\n]\n}\n";
          logger_->Log(str);
          delete logger_;
          std::cerr << "[INFO] Timeline is stored in " << chrome_trace_file_name_ << std::endl;
        }
      }
    }

    static ChromeLogger* Create(const TraceOptions& options, const char* filename) {
      ChromeLogger *chrome_logger  = new ChromeLogger(options, filename);
      UniMemory::ExitIfOutOfMemory((void *)(chrome_logger));
      return chrome_logger;
    }

    bool CheckOption(uint32_t option) {
      return options_.CheckFlag(option);
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

        if (name != nullptr) {
          rec->name_ = strdup(name);
        } else {
          rec->name_ = nullptr;
        }

        rec->api_id_ = IttTracingId;
        rec->start_time_ = start_ts;
        rec->end_time_ = end_ts;
        rec->id_ = 0;
        thread_local_buffer_.BufferHostEvent();
      }
    }

    static void CclLoggingCallback(const char *name, uint64_t start_ts, uint64_t end_ts, uint64_t buff_size) {
      if (!thread_local_buffer_.IsFinalized()) {
        HostEventRecord *rec = thread_local_buffer_.GetHostEvent();

        if (name != nullptr) {
          rec->name_ = strdup(name);
        } else {
          rec->name_ = nullptr;
        }

        rec->type_ = EVENT_COMPLETE;
        rec->api_id_ = IttTracingId;
        rec->start_time_ = start_ts;
        rec->end_time_ = end_ts;
        rec->id_ = 0;
        rec->api_type_ = CCL;
        rec->ccl_args_.buff_size = buff_size;
        thread_local_buffer_.BufferHostEvent();
      }
    }

    static void MpiLoggingCallback(const char *name, uint64_t start_ts, uint64_t end_ts,size_t src_size, int src_location, int src_tag,
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
        rec->api_type_ = MPI;
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
        rec->api_type_ = MPI;
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
      rec->name_ = nullptr;
      thread_local_buffer_.BufferHostEvent();

      if ((kids != nullptr) && (flow_dir == FLOW_H2D)) {
        for (auto id : *kids) {
          rec = thread_local_buffer_.GetHostEvent();

          rec->type_ = EVENT_FLOW_SOURCE;
          rec->api_id_ = DummyTracingId;
          rec->start_time_ = started;
          rec->id_ = id;
          rec->name_ = nullptr;
          thread_local_buffer_.BufferHostEvent();
        }
      }
      if ((kids != nullptr) && (flow_dir == FLOW_D2H)) {
        for (auto id : *kids) {
          rec = thread_local_buffer_.GetHostEvent();

          rec->type_ = EVENT_FLOW_SINK;
          rec->api_id_ = DummyTracingId;
          rec->start_time_ = started;
          rec->id_ = id;
          rec->name_ = nullptr;
          thread_local_buffer_.BufferHostEvent();
        }
      }
    }

    static void ClChromeCallLoggingCallback(std::vector<uint64_t> *kids, FLOW_DIR flow_dir, API_TRACING_ID api_id,
      uint64_t started, uint64_t ended) {
      if (cl_thread_local_buffer_.IsFinalized()) {
        return;
      }

      HostEventRecord *rec = cl_thread_local_buffer_.GetHostEvent();
      rec->type_ = EVENT_COMPLETE;
      rec->api_id_ = api_id;
      rec->start_time_ = started;
      rec->end_time_ = ended;
      rec->id_ = 0;
      rec->name_ = nullptr;
      cl_thread_local_buffer_.BufferHostEvent();

      if ((kids != nullptr) && (flow_dir == FLOW_H2D)) {
        for (auto id : *kids) {
          rec = cl_thread_local_buffer_.GetHostEvent();

          rec->type_ = EVENT_FLOW_SOURCE;
          rec->api_id_ = DummyTracingId;
          rec->start_time_ = started;
          rec->id_ = id;
          rec->name_ = nullptr;
          cl_thread_local_buffer_.BufferHostEvent();
        }
      }

      if ((kids != nullptr) && (flow_dir == FLOW_D2H)) {
        for (auto id : *kids) {
          rec = cl_thread_local_buffer_.GetHostEvent();

          rec->type_ = EVENT_FLOW_SINK;
          rec->api_id_ = DummyTracingId;
          rec->start_time_ = started;
          rec->id_ = id;
          rec->name_ = nullptr;
          cl_thread_local_buffer_.BufferHostEvent();
        }
      }
    }
};

#endif // PTI_TOOLS_UNITRACE_CHROME_LOGGER_H_

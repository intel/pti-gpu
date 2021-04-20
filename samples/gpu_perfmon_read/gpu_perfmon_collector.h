//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_GPU_PERFMON_READ_GPU_INST_COUNT_COLLECTOR_H_
#define PTI_SAMPLES_GPU_PERFMON_READ_GPU_INST_COUNT_COLLECTOR_H_

#include <iostream>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>
#include <vector>

#include "gen_binary_decoder.h"
#include "gtpin_utils.h"

struct PerfMonData {
  uint32_t freq;
  uint32_t cycles;
  uint32_t pm;
  uint32_t skipped;
};

struct PerfMonValue {
  uint64_t cycles;
  uint64_t pm;
};

struct KernelData {
  std::string name;
  uint32_t call_count;
  std::vector<uint8_t> binary;
  std::map<int32_t, PerfMonValue> block_map;
};

struct MemoryLocation {
  int32_t offset;
  GTPinMem location;
};

using KernelMemoryMap = std::map< GTPinKernel, std::vector<MemoryLocation> >;
using KernelDataMap = std::map<GTPinKernel, KernelData>;

class GpuPerfMonCollector {
 public: // Interface
  static GpuPerfMonCollector* Create() {
    return new GpuPerfMonCollector();
  }

  ~GpuPerfMonCollector() {}

  const KernelDataMap& GetKernelDataMap() const {
    return kernel_data_map_;
  }

  static void PrintResults(const KernelDataMap& kernel_data_map) {
    if (kernel_data_map.size() == 0) {
      return;
    }

    iga_gen_t arch = utils::gtpin::GetArch(GTPin_GetGenVersion());
    if (arch == IGA_GEN_INVALID) {
      std::cerr << "[WARNING] Unknown GPU architecture" << std::endl;
      return;
    }

    for (auto data : kernel_data_map) {
      GenBinaryDecoder decoder(data.second.binary, arch);

      std::vector<Instruction> instruction_list =
        decoder.Disassemble();
      PTI_ASSERT(instruction_list.size() > 0);

      std::vector< std::pair<int32_t, PerfMonValue> > block_list;
      for (auto block : data.second.block_map) {
        block_list.push_back(std::make_pair(block.first, block.second));
      }
      PTI_ASSERT(block_list.size() > 0);

      uint64_t total_cycles = 0;
      uint64_t total_pm = 0;
      for (auto value : block_list) {
        total_cycles += value.second.cycles;
        total_pm += value.second.pm;
      }

      if (total_cycles == 0) {
        continue;
      }

      std::stringstream ss;
      ss << "=== " << data.second.name << " (runs " <<
        data.second.call_count  << " times) ===";
      std::string prologue = ss.str();
      std::string epilogue(prologue.size(), '=');
      std::cerr << prologue << std::endl;

      size_t block_id = 1;
      for (auto instruction : instruction_list) {
        uint32_t block_offset = (block_id < block_list.size()) ?
          block_list[block_id].first : UINT32_MAX;
        if (instruction.offset >= block_offset) {
          ++block_id;
          std::cerr << std::endl;
        }

        if (instruction.offset == instruction_list.front().offset ||
            instruction.offset >= block_offset) {
          uint64_t pm = block_list[block_id - 1].second.pm;
          float percent = 100.0f * pm / total_cycles;
          std::cerr << "[" << std::setw(7) << std::setprecision(2) <<
            std::fixed << std::setfill(' ') << percent << "%]";
        } else {
          std::cerr << "[" << std::setw(8) << std::setfill(' ') << "-" << "]";
        }

        std::cerr << " 0x" << std::setw(4) << std::setfill('0') << std::hex <<
          std::uppercase << instruction.offset << ": " << instruction.text <<
          std::endl;
      }

      std::cerr << "Total PM percentage: " <<  std::setprecision(2) <<
        std::fixed << 100.0f * total_pm / total_cycles << "%" << std::endl;
      std::cerr << std::endl;
    }
  }

 private: // Implementation Details
  GpuPerfMonCollector() {
    utils::gtpin::KnobAddBool("silent_warnings", false);
    utils::gtpin::KnobAddInt("allow_sregs", 0);
    utils::gtpin::KnobAddInt("use_global_ra", 1);

    GTPin_OnKernelBuild(OnKernelBuild, this);
    GTPin_OnKernelRun(OnKernelRun, this);
    GTPin_OnKernelComplete(OnKernelComplete, this);

    GTPIN_Start();
  }

  void AddKernelMemoryList(
      GTPinKernel kernel,
      const std::vector<MemoryLocation>& kernel_memory_list) {
    PTI_ASSERT(kernel_memory_list.size() > 0);
    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(kernel_memory_map_.count(kernel) == 0);
    kernel_memory_map_[kernel] = kernel_memory_list;
  }

  std::vector<MemoryLocation> GetKernelMemoryList(GTPinKernel kernel) {
    const std::lock_guard<std::mutex> lock(lock_);
    auto it = kernel_memory_map_.find(kernel);
    if (it == kernel_memory_map_.end()) {
      return std::vector<MemoryLocation>();
    }
    return it->second;
  }

  void AddKernelData(GTPinKernel kernel, const KernelData& kernel_data) {
    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(kernel_data_map_.count(kernel) == 0);
    kernel_data_map_[kernel] = kernel_data;
  }

  void AppendKernelBlockValue(
      GTPinKernel kernel, int32_t offset, PerfMonValue value) {
    PTI_ASSERT(offset >= 0);
    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(kernel_data_map_.count(kernel) == 1);
    PTI_ASSERT(kernel_data_map_[kernel].block_map.count(offset) == 1);
    kernel_data_map_[kernel].block_map[offset].cycles += value.cycles;
    kernel_data_map_[kernel].block_map[offset].pm += value.pm;
  }

  void AppendKernelCallCount(GTPinKernel kernel, uint32_t call_count) {
    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(kernel_data_map_.count(kernel) == 1);
    kernel_data_map_[kernel].call_count += call_count;
  }

 private: // Callbacks
  static void OnKernelBuild(GTPinKernel kernel, void* data) {
    GTPINTOOL_STATUS status = GTPINTOOL_STATUS_SUCCESS;
    uint32_t num_regs = GTPin_PerfmonAvailableRegInstrument(kernel);

    std::vector<MemoryLocation> kernel_memory_list;
    KernelData kernel_data{};

    for (GTPinBBL block = GTPin_BBLHead(kernel); GTPin_BBLValid(block);
         block = GTPin_BBLNext(block)) {
      GTPinINS head = GTPin_InsHead(block);
      PTI_ASSERT(GTPin_InsValid(head));
      int32_t offset =  GTPin_InsOffset(head);

      GTPinINS tail = GTPin_InsTail(block);
      PTI_ASSERT(GTPin_InsValid(tail));

      if (GTPin_InsIsEOT(head)) {
        continue;
      }

      if (GTPin_InsIsChangingIP(tail)) {
        if (head == tail) {
          continue;
        } else {
          tail = GTPin_InsPrev(tail);
          PTI_ASSERT(GTPin_InsValid(tail));
        }
      }

      status = GTPin_PerfmonInstrumentPre(head);
      PTI_ASSERT(status == GTPINTOOL_STATUS_SUCCESS);

      GTPinMem mem = nullptr;
      status = GTPin_MemClaim(kernel, sizeof(PerfMonData), &mem);
      PTI_ASSERT(status == GTPINTOOL_STATUS_SUCCESS);

      status = GTPin_PerfmonInstrumentPost_Mem(tail, mem, num_regs);
      PTI_ASSERT(status == GTPINTOOL_STATUS_SUCCESS);

      kernel_memory_list.push_back({offset, mem});

      PTI_ASSERT(kernel_data.block_map.count(offset) == 0);
      kernel_data.block_map[offset] = {0, 0};

      if (num_regs > 0) {
        --num_regs;
      }
    }

    uint32_t kernel_binary_size = 0;
    status = GTPin_GetKernelBinary(kernel, 0, nullptr, &kernel_binary_size);
    PTI_ASSERT(status == GTPINTOOL_STATUS_SUCCESS);

    kernel_data.binary.resize(kernel_binary_size);
    status = GTPin_GetKernelBinary(
        kernel, kernel_binary_size,
        reinterpret_cast<char*>(kernel_data.binary.data()),
        nullptr);
    PTI_ASSERT(status == GTPINTOOL_STATUS_SUCCESS);

    char kernel_name[MAX_STR_SIZE];
    status = GTPin_KernelGetName(kernel, MAX_STR_SIZE, kernel_name, nullptr);
    PTI_ASSERT(status == GTPINTOOL_STATUS_SUCCESS);

    kernel_data.name = kernel_name;
    kernel_data.call_count = 0;

    GpuPerfMonCollector* collector =
      reinterpret_cast<GpuPerfMonCollector*>(data);
    PTI_ASSERT(collector != nullptr);

    collector->AddKernelMemoryList(kernel, kernel_memory_list);
    collector->AddKernelData(kernel, kernel_data);
  }

  static void OnKernelRun(GTPinKernelExec kernelExec, void* data) {
    GTPINTOOL_STATUS status = GTPINTOOL_STATUS_SUCCESS;
    GTPin_KernelProfilingActive(kernelExec, 1);
    PTI_ASSERT(status == GTPINTOOL_STATUS_SUCCESS);
  }

  static void OnKernelComplete(GTPinKernelExec kernelExec, void* data) {
    GTPINTOOL_STATUS status = GTPINTOOL_STATUS_SUCCESS;

    GpuPerfMonCollector* collector =
      reinterpret_cast<GpuPerfMonCollector*>(data);
    PTI_ASSERT(collector != nullptr);

    GTPinKernel kernel = GTPin_KernelExec_GetKernel(kernelExec);

    for (auto block : collector->GetKernelMemoryList(kernel)) {
      uint32_t thread_count = GTPin_MemSampleLength(block.location);
      PTI_ASSERT(thread_count > 0);

      uint64_t total_cycles = 0, total_pm = 0;
      PerfMonData value{};
      for (uint32_t tid = 0; tid < thread_count; ++tid) {
          status = GTPin_MemRead(
              block.location, tid, sizeof(PerfMonData),
              reinterpret_cast<char*>(&value), nullptr);
          PTI_ASSERT(status == GTPINTOOL_STATUS_SUCCESS);
          total_cycles += value.cycles;
          total_pm += value.pm;
      }

      collector->AppendKernelBlockValue(
          kernel, block.offset, {total_cycles, total_pm});
    }

    collector->AppendKernelCallCount(kernel, 1);
  }

 private: // Data
  KernelMemoryMap kernel_memory_map_;
  KernelDataMap kernel_data_map_;
  std::mutex lock_;
};

#endif // PTI_SAMPLES_GPU_PERFMON_READ_GPU_INST_COUNT_COLLECTOR_H_
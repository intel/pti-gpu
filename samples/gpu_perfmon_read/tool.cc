//==============================================================
// Copyright © 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <iostream>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>
#include <vector>

#include "gen_binary_decoder.h"
#include "gtpin_utils.h"

class ToolContext;
static ToolContext* context = nullptr;

// External Tool Interface ////////////////////////////////////////////////////

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
void Usage() {
  std::cout <<
    "Usage: ./gpu_perfmon_read[.exe] <application> <args>" <<
    std::endl;
}

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
int ParseArgs(int argc, char* argv[]) {
  return 1;
}

extern "C"
#if defined(_WIN32)
__declspec(dllexport)
#endif
void SetToolEnv() {
  utils::SetEnv("ZET_ENABLE_API_TRACING_EXP=1");
}

// Internal Tool Functionality ////////////////////////////////////////////////

struct PerfmonData {
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

class ToolContext {
 public:
  ToolContext() {}

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

  const KernelDataMap& GetKernelDataMap() const {
    return kernel_data_map_;
  }

 private:
  KernelMemoryMap kernel_memory_map_;
  KernelDataMap kernel_data_map_;
  std::mutex lock_;
};

static void OnKernelBuild(GTPinKernel kernel, void* v) {
  GTPINTOOL_STATUS status = GTPINTOOL_STATUS_SUCCESS;
  PTI_ASSERT(context != nullptr);

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
    status = GTPin_MemClaim(kernel, sizeof(PerfmonData), &mem);
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

  context->AddKernelMemoryList(kernel, kernel_memory_list);
  context->AddKernelData(kernel, kernel_data);
}

static void OnKernelRun(GTPinKernelExec kernelExec, void* v) {
  GTPINTOOL_STATUS status = GTPINTOOL_STATUS_SUCCESS;
  GTPin_KernelProfilingActive(kernelExec, 1);
  PTI_ASSERT(status == GTPINTOOL_STATUS_SUCCESS);
}

static void OnKernelComplete(GTPinKernelExec kernelExec, void* v) {
  GTPINTOOL_STATUS status = GTPINTOOL_STATUS_SUCCESS;
  PTI_ASSERT(context != nullptr);

  GTPinKernel kernel = GTPin_KernelExec_GetKernel(kernelExec);

   for (auto block : context->GetKernelMemoryList(kernel)) {
    uint32_t thread_count = GTPin_MemSampleLength(block.location);
    PTI_ASSERT(thread_count > 0);

    uint64_t total_cycles = 0, total_pm = 0;
    PerfmonData value{};
    for (uint32_t tid = 0; tid < thread_count; ++tid) {
        status = GTPin_MemRead(block.location, tid, sizeof(PerfmonData),
            reinterpret_cast<char*>(&value), nullptr);
        PTI_ASSERT(status == GTPINTOOL_STATUS_SUCCESS);
        total_cycles += value.cycles;
        total_pm += value.pm;
    }

    context->AppendKernelBlockValue(kernel, block.offset,
                                    {total_cycles, total_pm});
  }

  context->AppendKernelCallCount(kernel, 1);
}

static void PrintResults() {
  PTI_ASSERT(context != nullptr);

  const KernelDataMap& kernel_data_map = context->GetKernelDataMap();
  if (kernel_data_map.size() == 0) {
    return;
  }

  iga_gen_t arch = utils::gtpin::GetArch(GTPin_GetGenVersion());
  if (arch == IGA_GEN_INVALID) {
    std::cout << "[WARNING] Unknown GPU architecture" << std::endl;
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
    std::cout << prologue << std::endl;

    size_t block_id = 1;
    for (auto instruction : instruction_list) {
      uint32_t block_offset = (block_id < block_list.size()) ?
        block_list[block_id].first : UINT32_MAX;
      if (instruction.offset >= block_offset) {
        ++block_id;
        std::cout << std::endl;
      }

      if (instruction.offset == instruction_list.front().offset ||
          instruction.offset >= block_offset) {
        uint64_t pm = block_list[block_id - 1].second.pm;
        float percent = 100.0f * pm / total_cycles;
        std::cout << "[" << std::setw(7) << std::setprecision(2) <<
          std::fixed << std::setfill(' ') << percent << "%]";
      } else {
        std::cout << "[" << std::setw(8) << std::setfill(' ') << "-" << "]";
      }

      std::cout << " 0x" << std::setw(4) << std::setfill('0') << std::hex <<
        std::uppercase << instruction.offset << ": " << instruction.text <<
        std::endl;
    }

    std::cout << "Total PM percentage: " <<  std::setprecision(2) <<
      std::fixed << 100.0f * total_pm / total_cycles << "%" << std::endl;
    std::cout << epilogue << std::endl;
    std::cout << "[INFO] Job is successfully completed" << std::endl;
  }
}

// Internal Tool Interface ////////////////////////////////////////////////////

void EnableProfiling() {
  PTI_ASSERT(context == nullptr);
  context = new ToolContext();
  PTI_ASSERT(context != nullptr);

  utils::gtpin::KnobAddBool("silent_warnings", true);
  utils::gtpin::KnobAddInt("allow_sregs", 0);

  if (!utils::GetEnv("PTI_GEN12").empty()) {
    std::cout << "[INFO] Experimental GTPin mode: GEN12" << std::endl;
    utils::gtpin::KnobAddBool("gen12_1", true);
  }

  GTPin_OnKernelBuild(OnKernelBuild, nullptr);
  GTPin_OnKernelRun(OnKernelRun, nullptr);
  GTPin_OnKernelComplete(OnKernelComplete, nullptr);

  GTPIN_Start();
}

void DisableProfiling() {
  if (context != nullptr) {
    PrintResults();
    delete context;
  }
}
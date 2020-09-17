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
    "Usage: ./gpu_inst_count[.exe] <application> <args>" <<
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

struct KernelData {
  std::string name;
  uint32_t call_count;
  std::vector<uint8_t> binary;
  std::map<int32_t, uint64_t> block_map;
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
      GTPinKernel kernel, int32_t offset, uint64_t value) {
    PTI_ASSERT(offset >= 0);

    const std::lock_guard<std::mutex> lock(lock_);
    PTI_ASSERT(kernel_data_map_.count(kernel) == 1);
    PTI_ASSERT(kernel_data_map_[kernel].block_map.count(offset) == 1);
    kernel_data_map_[kernel].block_map[offset] += value;
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

  std::vector<MemoryLocation> kernel_memory_list;
  KernelData kernel_data{};

  for (GTPinBBL block = GTPin_BBLHead(kernel); GTPin_BBLValid(block);
       block = GTPin_BBLNext(block)) {
    GTPinINS head = GTPin_InsHead(block);
    PTI_ASSERT(GTPin_InsValid(head));
    int32_t offset =  GTPin_InsOffset(head);

    GTPinMem mem = nullptr;
    status = GTPin_MemClaim(kernel, sizeof(uint32_t), &mem);
    PTI_ASSERT(status == GTPINTOOL_STATUS_SUCCESS);
    PTI_ASSERT(mem != nullptr);

    status = GTPin_OpcodeprofInstrument(head, mem);
    PTI_ASSERT(status == GTPINTOOL_STATUS_SUCCESS);

    kernel_memory_list.push_back({offset, mem});

    PTI_ASSERT(kernel_data.block_map.count(offset) == 0);
    kernel_data.block_map[offset] = 0;
  }

  uint32_t kernel_binary_size = 0;
  status = GTPin_GetKernelBinary(kernel, 0, nullptr, &kernel_binary_size);
  PTI_ASSERT(status == GTPINTOOL_STATUS_SUCCESS);
  PTI_ASSERT(kernel_binary_size > 0);

  kernel_data.binary.resize(kernel_binary_size);
  status = GTPin_GetKernelBinary(kernel, kernel_binary_size,
                                 reinterpret_cast<char*>(
                                    kernel_data.binary.data()),
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

static void OnKernelRun(GTPinKernelExec kernel_exec, void* v) {
  GTPINTOOL_STATUS status = GTPINTOOL_STATUS_SUCCESS;
  GTPin_KernelProfilingActive(kernel_exec, 1);
  PTI_ASSERT(status == GTPINTOOL_STATUS_SUCCESS);
}

static void OnKernelComplete(GTPinKernelExec kernel_exec, void* v) {
  GTPINTOOL_STATUS status = GTPINTOOL_STATUS_SUCCESS;
  PTI_ASSERT(context != nullptr);

  GTPinKernel kernel = GTPin_KernelExec_GetKernel(kernel_exec);

  for (auto block : context->GetKernelMemoryList(kernel)) {
    uint32_t thread_count = GTPin_MemSampleLength(block.location);
    PTI_ASSERT(thread_count > 0);

    uint32_t total = 0, value = 0;
    for (uint32_t tid = 0; tid < thread_count; ++tid) {
        status = GTPin_MemRead(block.location, tid, sizeof(uint32_t),
            reinterpret_cast<char*>(&value), nullptr);
        PTI_ASSERT(status == GTPINTOOL_STATUS_SUCCESS);
        total += value;
    }

    context->AppendKernelBlockValue(kernel, block.offset, total);
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
    std::stringstream ss;
    ss << "=== " << data.second.name << " (runs " <<
      data.second.call_count  << " times) ===";
    std::string prologue = ss.str();
    std::string epilogue(prologue.size(), '=');
    std::cout << prologue << std::endl;

    GenBinaryDecoder decoder(data.second.binary, arch);

    std::vector<Instruction> instruction_list =
      decoder.Disassemble();
    PTI_ASSERT(instruction_list.size() > 0);

    std::vector< std::pair<int32_t, uint64_t> > block_list;
    for (auto block : data.second.block_map) {
      block_list.push_back(std::make_pair(block.first, block.second));
    }

    size_t block_id = 1;
    for (auto instruction : instruction_list) {
      int32_t block_offset = (block_id < block_list.size()) ?
        block_list[block_id].first : INT32_MAX;
      if (instruction.offset >= block_offset) {
        ++block_id;
      }

      uint64_t count = block_list[block_id - 1].second /
        data.second.call_count;
      std::cout << "[" << std::setw(10) << std::setfill(' ') << std::dec <<
        count << "] 0x" << std::setw(4) << std::setfill('0') << std::hex <<
        std::uppercase << instruction.offset << ": " << instruction.text <<
        std::endl;
    }

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
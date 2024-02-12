//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_GTPIN_RESULTS_H
#define PTI_GTPIN_RESULTS_H

#include <api/gtpin_api.h>

#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

#include "def_gpu_gtpin.hpp"

/**
 * @file Definition of base classes of results of profiling work. Delivered
 * classses add tool-specific information
 */

namespace gtpin {
namespace gtpin_prof {

/// Per point of interest(instrumentation point) data
struct ResultData {
 public:
  ResultData() = default;
  virtual ~ResultData() = default;
};

/// Data for each kernel run
struct InvocationData {
 public:
  InvocationData() = default;
  InvocationData(const GTPinKernelExecDesriptor& execDescr);
  virtual ~InvocationData() = default;
  size_t kernelId = -1;
  KernelRun runNum = 0;
  KernelRun globalRunNum = 0;
  uint64_t gtpinDispatchId = -1;

  std::vector<std::shared_ptr<ResultData>> data;
};

struct AsmRecord {
  AsmRecord(const InstructionOffset instructionOffset, const std::string asmLineOrig);

  const InstructionOffset instructionOffset;
  const std::string asmLineOrig;
};

/// Data for each kernel
struct KernelData {
 public:
  KernelData(const std::string name, const GtKernelId id, const std::vector<AsmRecord> origAsm);
  KernelData(IGtKernelInstrument& instrumentor);
  virtual ~KernelData() = default;

  const std::string kernelName;
  const GtKernelId kernelId;
  const std::vector<AsmRecord> origAsm;
  const std::vector<uint8_t> origBinary;
  size_t totalRuns = 0;

  std::unordered_map<KernelRun, std::shared_ptr<InvocationData>> invocations;
};

/// Applicaiton data
struct ProfilerData {
 public:
  virtual ~ProfilerData() = default;

  std::string toolName = "Profiler";
  std::map<GtKernelId, std::shared_ptr<KernelData>> kernels;
};

}  // namespace gtpin_prof
}  // namespace gtpin

#endif  // PTI_GTPIN_RESULTS_H

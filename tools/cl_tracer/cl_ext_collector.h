//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_CL_TRACER_CL_EXT_COLLECTOR_H_
#define PTI_TOOLS_CL_TRACER_CL_EXT_COLLECTOR_H_

#include <CL/cl.h>

#include "correlator.h"
#include "pti_assert.h"

class ClApiCollector;

class ClExtCollector {
 public:
  static ClExtCollector* Create(
      ClApiCollector* cpu_collector, ClApiCollector* gpu_collector) {
    PTI_ASSERT(cpu_collector != nullptr);
    PTI_ASSERT(gpu_collector != nullptr);
    if (instance_ == nullptr) {
      instance_ = new ClExtCollector(cpu_collector, gpu_collector);
    }
    return instance_;
  }

  static void Destroy() {
    if (instance_ != nullptr) {
      delete instance_;
    }
  }

  static ClExtCollector* GetInstance() {
    return instance_;
  }

  template <cl_device_type DEVICE_TYPE>
  uint64_t GetTimestamp() const {
    if (DEVICE_TYPE == CL_DEVICE_TYPE_GPU) {
      PTI_ASSERT(gpu_collector_ != nullptr);
      return GetTimestampGPU();
    } else {
      PTI_ASSERT(cpu_collector_ != nullptr);
      return GetTimestampCPU();
    }
  }

  uint64_t GetTimestampCPU() const;
  uint64_t GetTimestampGPU() const;

  template <cl_device_type DEVICE_TYPE>
  void AddFunctionTime(const char* function_name, uint64_t time) {
    if (DEVICE_TYPE == CL_DEVICE_TYPE_GPU) {
      PTI_ASSERT(gpu_collector_ != nullptr);
      AddFunctionTimeGPU(function_name, time);
    } else {
      PTI_ASSERT(cpu_collector_ != nullptr);
      AddFunctionTimeCPU(function_name, time);
    }
  }

  void AddFunctionTimeCPU(const char* function_name, uint64_t time);
  void AddFunctionTimeGPU(const char* function_name, uint64_t time);

  template <cl_device_type DEVICE_TYPE>
  bool IsCallTracing() const {
    if (DEVICE_TYPE == CL_DEVICE_TYPE_GPU) {
      PTI_ASSERT(gpu_collector_ != nullptr);
      return IsCallTracingGPU();
    } else {
      PTI_ASSERT(cpu_collector_ != nullptr);
      return IsCallTracingCPU();
    }
  }

  bool IsCallTracingCPU() const;
  bool IsCallTracingGPU() const;

  template <cl_device_type DEVICE_TYPE>
  bool NeedPid() const {
    if (DEVICE_TYPE == CL_DEVICE_TYPE_GPU) {
      PTI_ASSERT(gpu_collector_ != nullptr);
      return NeedPidGPU();
    } else {
      PTI_ASSERT(cpu_collector_ != nullptr);
      return NeedPidCPU();
    }
  }

  bool NeedPidCPU() const;
  bool NeedPidGPU() const;

  template <cl_device_type DEVICE_TYPE>
  bool NeedTid() const {
    if (DEVICE_TYPE == CL_DEVICE_TYPE_GPU) {
      PTI_ASSERT(gpu_collector_ != nullptr);
      return NeedTidGPU();
    } else {
      PTI_ASSERT(cpu_collector_ != nullptr);
      return NeedTidCPU();
    }
  }

  bool NeedTidCPU() const;
  bool NeedTidGPU() const;

  template <cl_device_type DEVICE_TYPE>
  void Log(const std::string& message) const {
    if (DEVICE_TYPE == CL_DEVICE_TYPE_GPU) {
      PTI_ASSERT(gpu_collector_ != nullptr);
      LogGPU(message);
    } else {
      PTI_ASSERT(cpu_collector_ != nullptr);
      LogCPU(message);
    }
  }

  void LogCPU(const std::string& message) const;
  void LogGPU(const std::string& message) const;

  template <cl_device_type DEVICE_TYPE>
  void Callback(
      const char* function_name, uint64_t start, uint64_t end) const {
    if (DEVICE_TYPE == CL_DEVICE_TYPE_GPU) {
      PTI_ASSERT(gpu_collector_ != nullptr);
      CallbackGPU(function_name, start, end);
    } else {
      PTI_ASSERT(cpu_collector_ != nullptr);
      CallbackCPU(function_name, start, end);
    }
  }

  void CallbackCPU(
      const char* function_name, uint64_t start, uint64_t end) const;
  void CallbackGPU(
      const char* function_name, uint64_t start, uint64_t end) const;

 private:
  ClExtCollector(ClApiCollector* cpu_collector, ClApiCollector* gpu_collector)
      : cpu_collector_(cpu_collector), gpu_collector_(gpu_collector) {
    PTI_ASSERT(cpu_collector_ != nullptr);
    PTI_ASSERT(gpu_collector_ != nullptr);
  }

 private:
  static ClExtCollector* instance_;
  ClApiCollector* cpu_collector_ = nullptr;
  ClApiCollector* gpu_collector_ = nullptr;
};

#endif // PTI_TOOLS_CL_TRACER_CL_EXT_COLLECTOR_H_
//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================


#include <iomanip>
#include <iostream>

#include "ze_metric_collector.h"

struct MetricResult {
  uint64_t inst_alu0 = 0;
  uint64_t inst_alu1 = 0;
  uint64_t inst_xmx = 0;
  uint64_t inst_send = 0;
  uint64_t inst_ctrl = 0;
};

const uint32_t kInstructionLength = 20;

static ZeMetricCollector* metric_collector = nullptr;

static std::chrono::steady_clock::time_point start;

// External Tool Interface ////////////////////////////////////////////////////

extern "C" PTI_EXPORT
void Usage() {
  std::cout <<
    "Usage: ./ze_metric_streamer[.exe] <application> <args>" <<
    std::endl;
}

extern "C" PTI_EXPORT
int ParseArgs(int argc, char* argv[]) {
  return 1;
}

extern "C" PTI_EXPORT
void SetToolEnv() {
  utils::SetEnv("ZET_ENABLE_METRICS", "1");
}

// Internal Tool Functionality ////////////////////////////////////////////////

static MetricResult GetMetricResult() {
  PTI_ASSERT(metric_collector != nullptr);

  std::vector<zet_typed_value_t> report_list =
    metric_collector->GetReportList();
  if (report_list.size() == 0) {
    return MetricResult();
  }

  MetricResult metric_result;

  int inst_alu0_id = metric_collector->GetMetricId("XVE_INST_EXECUTED_ALU0_ALL");
  PTI_ASSERT(inst_alu0_id > 0);
  int inst_alu1_id = metric_collector->GetMetricId("XVE_INST_EXECUTED_ALU1_ALL");
  PTI_ASSERT(inst_alu1_id > 0);
  int inst_xmx_id = metric_collector->GetMetricId("XVE_INST_EXECUTED_XMX_ALL");
  PTI_ASSERT(inst_xmx_id > 0);
  int inst_send_id = metric_collector->GetMetricId("XVE_INST_EXECUTED_SEND_ALL");
  PTI_ASSERT(inst_send_id > 0);
  int inst_ctrl_id = metric_collector->GetMetricId("XVE_INST_EXECUTED_CONTROL_ALL");
  PTI_ASSERT(inst_ctrl_id > 0);

  uint32_t report_size = metric_collector->GetReportSize();
  PTI_ASSERT(report_size > 0);

  const zet_typed_value_t* report = report_list.data();
  while (report < report_list.data() + report_list.size()) {
    PTI_ASSERT(report[inst_alu0_id].type == ZET_VALUE_TYPE_UINT64);
    uint64_t inst_alu0 = report[inst_alu0_id].value.ui64;
    PTI_ASSERT(report[inst_alu1_id].type == ZET_VALUE_TYPE_UINT64);
    uint64_t inst_alu1 = report[inst_alu1_id].value.ui64;
    PTI_ASSERT(report[inst_xmx_id].type == ZET_VALUE_TYPE_UINT64);
    uint64_t inst_xmx = report[inst_xmx_id].value.ui64;
    PTI_ASSERT(report[inst_send_id].type == ZET_VALUE_TYPE_UINT64);
    uint64_t inst_send = report[inst_send_id].value.ui64;
    PTI_ASSERT(report[inst_ctrl_id].type == ZET_VALUE_TYPE_UINT64);
    uint64_t inst_ctrl = report[inst_ctrl_id].value.ui64;

    metric_result.inst_alu0 += inst_alu0;
    metric_result.inst_alu1 += inst_alu1;
    metric_result.inst_xmx += inst_xmx;
    metric_result.inst_send += inst_send;
    metric_result.inst_ctrl += inst_ctrl;
  }

  return metric_result;
}

static void PrintResults() {
  std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
  std::chrono::duration<uint64_t, std::nano> time = end - start;

  MetricResult metric_result = GetMetricResult();

  std::cerr << std::endl;
  std::cerr << "=== Device Metrics: ===" << std::endl;
  std::cerr << std::endl;

  std::cerr << std::setw(kInstructionLength) << "Inst executed alu0" << "," <<
    std::setw(kInstructionLength) << "Inst executed alu1" << "," <<
    std::setw(kInstructionLength) << "Inst executed xmx" << "," <<
    std::setw(kInstructionLength) << "Inst executed send" << "," <<
    std::setw(kInstructionLength) << "Inst executed ctrl" << std::endl;

  std::cerr << std::setw(kInstructionLength) << metric_result.inst_alu0 << "," <<
    std::setw(kInstructionLength) << metric_result.inst_alu1 << "," <<
    std::setw(kInstructionLength) << metric_result.inst_xmx << "," <<
    std::setw(kInstructionLength) << metric_result.inst_send << "," <<
    std::setw(kInstructionLength) << metric_result.inst_ctrl << std::endl;

  std::cerr << std::endl;
}

// Internal Tool Interface ////////////////////////////////////////////////////

void EnableProfiling() {
  ze_result_t status = ZE_RESULT_SUCCESS;
  status = zeInit(ZE_INIT_FLAG_GPU_ONLY);
  PTI_ASSERT(status == ZE_RESULT_SUCCESS);

  ze_driver_handle_t driver = utils::ze::GetGpuDriver();
  ze_device_handle_t device = utils::ze::GetGpuDevice();
  if (device == nullptr || driver == nullptr) {
    std::cout << "[WARNING] Unable to find target device" << std::endl;
    return;
  }

  metric_collector =
    ZeMetricCollector::Create(driver, device, "ComputeBasic");
  if (metric_collector == nullptr) {
    return;
  }

  start = std::chrono::steady_clock::now();
}

void DisableProfiling() {
  if (metric_collector != nullptr) {
    PTI_ASSERT(metric_collector != nullptr);
    metric_collector->DisableCollection();
    PrintResults();
    delete metric_collector;
  }
}

//==============================================================
// Copyright © 2019-2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <math.h>
#include <string.h>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <set>

#include <CL/cl.h>

#include "cl_utils.h"

#define A_VALUE 0.128f
#define B_VALUE 0.256f
#define MAX_EPS 1.0e-4f

struct HardwareThreadInfo {
  cl_ulong start;
  cl_ulong end;
  cl_uint tid;
  cl_uint euid;
  cl_uint ssid;
  cl_uint sid;
};

struct HardwareKernelInfo {
  cl_ulong cycles;
  cl_ulong thread_count;
  cl_ulong eu_count;
  cl_ulong subslice_count;
  cl_ulong slice_count;
  cl_ulong total_samples;
  cl_ulong bad_samples;
};

const char* kKernelSource =
  "ulong __attribute__((overloadable)) intel_get_cycle_counter();\n"
  "uint  __attribute__((overloadable)) intel_get_hw_thread_id();\n"
  "uint  __attribute__((overloadable)) intel_get_slice_id();\n"
  "uint  __attribute__((overloadable)) intel_get_subslice_id();\n"
  "uint  __attribute__((overloadable)) intel_get_eu_id();\n"
  "uint  __attribute__((overloadable)) intel_get_eu_thread_id();\n"
  "\n"
  "struct HardwareThreadInfo {\n"
  "  ulong start;\n"
  "  ulong end;\n"
  "  uint tid;\n"
  "  uint euid;\n"
  "  uint ssid;\n"
  "  uint sid;\n"
  "};\n"
  "\n"
  "__kernel void SmartGEMM(__global float* a, __global float* b,\n"
  "                        __global float* c, unsigned size,\n"
  "                        __global struct HardwareThreadInfo* info,\n"
  "                        int simd_width) {\n"
  "\n"
  "  ulong start = intel_get_cycle_counter();\n"
  "\n"
  "  int j = get_global_id(0);\n"
  "  int i = get_global_id(1);\n"
  "  float sum = 0.0f;\n"
  "  for (unsigned k = 0; k < size; ++k) {\n"
  "    sum += a[i * size + k] * b[k * size + j];\n"
  "  }\n"
  "  c[i * size + j] = sum;\n"
  "\n"
  "  ulong end = intel_get_cycle_counter();\n"
  "\n"
  "  int id = (i * size + j) / simd_width;\n"
  "  info[id].start = start;\n"
  "  info[id].end = end;\n"
  "  info[id].tid = intel_get_eu_thread_id();\n"
  "  info[id].euid = intel_get_eu_id();\n"
  "  info[id].ssid = intel_get_subslice_id();\n"
  "  info[id].sid = intel_get_slice_id();\n"    
  "}";

static float Check(const std::vector<float>& a, float value) {
  PTI_ASSERT(value > MAX_EPS);

  float eps = 0.0f;
  for (size_t i = 0; i < a.size(); ++i) {
    eps += fabs((a[i] - value) / value);
  }

  return eps / a.size();
}

static HardwareKernelInfo ProcessHardwareInfo(
    const std::vector<HardwareThreadInfo>& info) {
  cl_ulong total_samples = info.size();
  cl_ulong bad_samples = 0;
  cl_ulong total_cycles = 0;
  
  for (auto item : info) {
    if (item.start < item.end) {
      total_cycles += item.end - item.start;
    } else {
      ++bad_samples;
    }
  }

  std::set<unsigned> tid;
  std::set<unsigned> euid;
  std::set<unsigned> sid;
  std::set<unsigned> ssid;

  for (auto item : info) {
    tid.insert(item.tid);
    euid.insert(item.euid);
    ssid.insert(item.ssid);
    sid.insert(item.sid);
  }

  cl_ulong average_thread_cycles = bad_samples < total_samples ?
    total_cycles / (total_samples - bad_samples) : 0;
  return { average_thread_cycles,
           tid.size(), euid.size(),
           ssid.size(), sid.size(),
           total_samples, bad_samples };
}

static void PrintInfo(const std::vector<HardwareKernelInfo>& info) {
  HardwareKernelInfo kernel_info = { 0 };
  PTI_ASSERT(info.size() > 0);

  cl_ulong total_cycles = 0;
  for (auto item : info) {
    total_cycles += item.cycles;
    kernel_info.total_samples += item.total_samples;
    kernel_info.bad_samples += item.bad_samples;
    kernel_info.thread_count = (std::max)(
        kernel_info.thread_count, item.thread_count);
    kernel_info.eu_count = (std::max)(kernel_info.eu_count, item.eu_count);
    kernel_info.subslice_count = (std::max)(
        kernel_info.subslice_count, item.subslice_count);
    kernel_info.slice_count = (std::max)(
        kernel_info.slice_count, item.slice_count);
  }
  kernel_info.cycles = total_cycles / info.size();

  PTI_ASSERT(kernel_info.total_samples > 0);

  std::string prologue = "======== GEMM Instrumentation Results ========";
  std::string epilogue(prologue.size(), '=');
  std::cout << prologue << std::endl;
  float bad_samples_percent = 100.0f * kernel_info.bad_samples /
    kernel_info.total_samples;
  if (bad_samples_percent > 90.0f) {
    std::cout << "Too many bad samples (" << std::setprecision(2) <<
      std::fixed << bad_samples_percent << "%) : Looks like OpenCL " <<
      "built-in intrinsics are not supported" << std::endl;
  } else {    
    std::cout << "Samples collected: " <<  kernel_info.total_samples <<
      " (" << std::setprecision(2) << std::fixed << bad_samples_percent <<
      "% of bad samples)" << std::endl;
    std::cout << "Estimated average HW thread duration: ~ " <<
      kernel_info.cycles / NSEC_IN_USEC << " us (" << kernel_info.cycles <<
      " cycles)" << std::endl;
    std::cout << "Estimated number of HW threads per EU: " <<
      kernel_info.thread_count << std::endl;
    std::cout << "Estimated number of EUs per subslice: " <<
      kernel_info.eu_count << std::endl;
    std::cout << "Estimated number of subslices per slice: " <<
      kernel_info.subslice_count << std::endl;
    std::cout << "Estimated number of slices: " << kernel_info.slice_count <<
      std::endl;
    std::cout << "Estimated total number EUs: " << kernel_info.eu_count *
      kernel_info.subslice_count * kernel_info.slice_count << std::endl;
    std::cout << "Estimated total number of HW threads: " <<
      kernel_info.thread_count * kernel_info.eu_count *
      kernel_info.subslice_count * kernel_info.slice_count << std::endl;
  }
  std::cout << epilogue << std::endl;
}

static float RunAndCheck(cl_device_id device, cl_kernel kernel,
                         cl_command_queue queue,
                         const std::vector<float>& a,
                         const std::vector<float>& b,
                         std::vector<float>& c,
                         unsigned size, float expected_result,
                         std::vector<HardwareKernelInfo>& kernel_info) {
  PTI_ASSERT(kernel != nullptr && queue != nullptr);
  
  PTI_ASSERT(size > 0);
  PTI_ASSERT(a.size() == size * size);
  PTI_ASSERT(b.size() == size * size);
  PTI_ASSERT(c.size() == size * size);

  int simd_width = utils::cl::GetSimdWidth(device, kernel);
  PTI_ASSERT(simd_width >= 1 && simd_width <= 32);

  int hardware_thread_count = (size * size + simd_width - 1) / simd_width;
  std::vector<HardwareThreadInfo> info(hardware_thread_count);

  cl_int status = CL_SUCCESS;    
  cl_context context = utils::cl::GetContext(kernel);
  PTI_ASSERT(context != nullptr);

  cl_mem dev_a = clCreateBuffer(context, CL_MEM_READ_ONLY,
                                a.size() * sizeof(float),
                                nullptr, &status);
  PTI_ASSERT(status == CL_SUCCESS && dev_a != nullptr);
  cl_mem dev_b = clCreateBuffer(context, CL_MEM_READ_ONLY,
                                b.size() * sizeof(float),
                                nullptr, &status);
  PTI_ASSERT(status == CL_SUCCESS && dev_b != nullptr);
  cl_mem dev_c = clCreateBuffer(context, CL_MEM_WRITE_ONLY,
                                c.size() * sizeof(float),
                                nullptr, &status);
  PTI_ASSERT(status == CL_SUCCESS && dev_c != nullptr);

  cl_mem dev_info = clCreateBuffer(context, CL_MEM_WRITE_ONLY,
                                   info.size() * sizeof(HardwareThreadInfo),
                                   nullptr, &status);
  PTI_ASSERT(status == CL_SUCCESS);

  status = clEnqueueWriteBuffer(queue, dev_a, CL_FALSE, 0,
                                a.size() * sizeof(float),
                                a.data(), 0, nullptr, nullptr);
  PTI_ASSERT(status == CL_SUCCESS);
  status = clEnqueueWriteBuffer(queue, dev_b, CL_FALSE, 0,
                                b.size() * sizeof(float),
                                b.data(), 0, nullptr, nullptr);
  PTI_ASSERT(status == CL_SUCCESS);

  status = clSetKernelArg(kernel, 0, sizeof(cl_mem), &dev_a);
  PTI_ASSERT(status == CL_SUCCESS);
  status = clSetKernelArg(kernel, 1, sizeof(cl_mem), &dev_b);
  PTI_ASSERT(status == CL_SUCCESS);
  status = clSetKernelArg(kernel, 2, sizeof(cl_mem), &dev_c);
  PTI_ASSERT(status == CL_SUCCESS);
  status = clSetKernelArg(kernel, 3, sizeof(unsigned), &size);
  PTI_ASSERT(status == CL_SUCCESS);
  status = clSetKernelArg(kernel, 4, sizeof(cl_mem), &dev_info);
  PTI_ASSERT(status == CL_SUCCESS);
  status = clSetKernelArg(kernel, 5, sizeof(int), &simd_width);
  PTI_ASSERT(status == CL_SUCCESS);

  size_t global_work_size[]{size, size};
  cl_event event = nullptr;
  status = clEnqueueNDRangeKernel(queue, kernel, 2, nullptr, global_work_size,
                                  nullptr, 0, nullptr, &event);
  PTI_ASSERT(status == CL_SUCCESS);
  status = clFinish(queue);
  PTI_ASSERT(status == CL_SUCCESS);

  status = clEnqueueReadBuffer(queue, dev_c, CL_TRUE, 0,
                               c.size() * sizeof(float),
                               c.data(), 0, nullptr, nullptr);
  PTI_ASSERT(status == CL_SUCCESS);

  status = clEnqueueReadBuffer(queue, dev_info, CL_TRUE, 0,
                               info.size() * sizeof(HardwareThreadInfo),
                               const_cast<HardwareThreadInfo*>(info.data()),
                               0, nullptr, nullptr);
  PTI_ASSERT(status == CL_SUCCESS);

  status = clReleaseMemObject(dev_info);
  PTI_ASSERT(status == CL_SUCCESS);
  status = clReleaseMemObject(dev_a);
  PTI_ASSERT(status == CL_SUCCESS);
  status = clReleaseMemObject(dev_b);
  PTI_ASSERT(status == CL_SUCCESS);
  status = clReleaseMemObject(dev_c);
  PTI_ASSERT(status == CL_SUCCESS);
  
  cl_ulong start = 0, end = 0;
  status = clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START,
                                   sizeof(cl_ulong), &start, nullptr);
  PTI_ASSERT(status == CL_SUCCESS);
  status = clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END,
                                   sizeof(cl_ulong), &end, nullptr);
  PTI_ASSERT(status == CL_SUCCESS);
  
  double time = static_cast<double>(end - start) / NSEC_IN_SEC;
  std::cout << "Matrix multiplication time: " << time <<
    " sec" << std::endl;

  kernel_info.push_back(ProcessHardwareInfo(info));
  return Check(c, expected_result);
}

static void Compute(cl_device_id device, const std::vector<float>& a,
                    const std::vector<float>& b, std::vector<float>& c,
                    unsigned size, unsigned repeat_count,
                    float expected_result) {
  PTI_ASSERT(device != nullptr);
  cl_int status = CL_SUCCESS;

  cl_context context = clCreateContext(nullptr, 1, &device, nullptr,
                                       nullptr, &status);
  PTI_ASSERT(status == CL_SUCCESS && context != nullptr);

  cl_queue_properties props[] = { CL_QUEUE_PROPERTIES,
                                  CL_QUEUE_PROFILING_ENABLE, 0 };
  cl_command_queue queue = clCreateCommandQueueWithProperties(
    context, device, props, &status);
  PTI_ASSERT(status == CL_SUCCESS && queue != nullptr);

  cl_program program = clCreateProgramWithSource(context, 1, &kKernelSource,
                                                 nullptr, &status);
  PTI_ASSERT(status == CL_SUCCESS && program != nullptr);
  status = clBuildProgram(program, 1, &device, nullptr, nullptr, nullptr);
  PTI_ASSERT(status == CL_SUCCESS);

  cl_kernel kernel = clCreateKernel(program, "SmartGEMM", &status);
  PTI_ASSERT(status == CL_SUCCESS && kernel != nullptr);

  std::vector<HardwareKernelInfo> kernel_info;

  for (unsigned i = 0; i < repeat_count; ++i) {
    float eps = RunAndCheck(device, kernel, queue, a, b, c, size,
                            expected_result, kernel_info);
    std::cout << "Results are " << ((eps < MAX_EPS) ? "" : "IN") <<
      "CORRECT with accuracy: " << eps << std::endl;
  }

  status = clReleaseKernel(kernel);
  PTI_ASSERT(status == CL_SUCCESS);
  status = clReleaseProgram(program);
  PTI_ASSERT(status == CL_SUCCESS);
  status = clReleaseCommandQueue(queue);
  PTI_ASSERT(status == CL_SUCCESS);
  status = clReleaseContext(context);
  PTI_ASSERT(status == CL_SUCCESS);

  PrintInfo(kernel_info);
}

int main(int argc, char* argv[]) {
  cl_device_id device = utils::cl::GetIntelDevice(CL_DEVICE_TYPE_GPU);
  if (device == nullptr) {
    std::cout << "Unable to find target device" << std::endl;
    return 0;
  }

  unsigned size = 1024;
  if (argc > 1) {
    size = std::stoul(argv[1]);
  }

  unsigned repeat_count = 4;
  if (argc > 2) {
    repeat_count = std::stoul(argv[2]);
  }

  std::cout << "OpenCL Matrix Multiplication (matrix size: " << size <<
    " x " << size << ", repeats " << repeat_count << " times)" << std::endl;
  std::cout << "Target device: " << utils::cl::GetDeviceName(device) <<
    std::endl;

  std::vector<float> a(size * size, A_VALUE);
  std::vector<float> b(size * size, B_VALUE);
  std::vector<float> c(size * size, 0.0f);

  auto start = std::chrono::steady_clock::now();
  float expected_result = A_VALUE * B_VALUE * size;
  Compute(device, a, b, c, size, repeat_count, expected_result);
  auto end = std::chrono::steady_clock::now();
  std::chrono::duration<float> time = end - start;

  std::cout << "Total execution time: " << time.count() <<
    " sec" << std::endl;
  return 0;
}
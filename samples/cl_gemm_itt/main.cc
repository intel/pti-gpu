//==============================================================
// Copyright © 2019-2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <math.h>
#include <string.h>

#include <chrono>
#include <iostream>

#include <CL/cl.h>
#include <ITT/ittnotify.h>

#include "cl_utils.h"

#define A_VALUE 0.128f
#define B_VALUE 0.256f
#define MAX_EPS 1.0e-4f

const char* kKernelSource =
  "__kernel void GEMM(__global float* a, __global float* b,\n"
  "                   __global float* c, unsigned size) {\n"
  "  int j = get_global_id(0);\n"
  "  int i = get_global_id(1);\n"
  "  float sum = 0.0f;\n"
  "  for (unsigned k = 0; k < size; ++k) {\n"
  "    sum += a[i * size + k] * b[k * size + j];\n"
  "  }\n"
  "  c[i * size + j] = sum;\n"
  "}";

// Pointer to global ITT domain
__itt_domain* domain = nullptr;

static inline void StartTask(const char* name) {
  PTI_ASSERT(domain != nullptr);
  __itt_string_handle* handle = __itt_string_handle_create(name);
  PTI_ASSERT(handle != nullptr);
  __itt_task_begin(domain, __itt_null, __itt_null, handle);
}

static inline void EndTask() {
  __itt_task_end(domain);
}

static float Check(const std::vector<float>& a, float value) {
  PTI_ASSERT(value > MAX_EPS);

  float eps = 0.0f;
  for (size_t i = 0; i < a.size(); ++i) {
    eps += fabs((a[i] - value) / value);
  }

  return eps / a.size();
}

static float RunAndCheck(cl_kernel kernel, cl_command_queue queue,
                         const std::vector<float>& a,
                         const std::vector<float>& b,
                         std::vector<float>& c,
                         unsigned size, float expected_result) {
  PTI_ASSERT(kernel != nullptr && queue != nullptr);
  
  PTI_ASSERT(size > 0);
  PTI_ASSERT(a.size() == size * size);
  PTI_ASSERT(b.size() == size * size);
  PTI_ASSERT(c.size() == size * size);

  StartTask("GEMM");

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

  status = clReleaseMemObject(dev_a);
  PTI_ASSERT(status == CL_SUCCESS);
  status = clReleaseMemObject(dev_b);
  PTI_ASSERT(status == CL_SUCCESS);
  status = clReleaseMemObject(dev_c);
  PTI_ASSERT(status == CL_SUCCESS);
  
  EndTask();

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

  StartTask("Check");
  float eps = Check(c, expected_result);
  EndTask();

  return eps;
}

static void Compute(cl_device_id device, const std::vector<float>& a,
                    const std::vector<float>& b, std::vector<float>& c,
                    unsigned size, unsigned repeat_count,
                    float expected_result) {
  PTI_ASSERT(device != nullptr);
  cl_int status = CL_SUCCESS;

  StartTask("KernelCreation");

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

  cl_kernel kernel = clCreateKernel(program, "GEMM", &status);
  PTI_ASSERT(status == CL_SUCCESS && kernel != nullptr);

  EndTask();

  StartTask("KernelRun");
  for (unsigned i = 0; i < repeat_count; ++i) {
    float eps = RunAndCheck(kernel, queue, a, b, c, size, expected_result);
    std::cout << "Results are " << ((eps < MAX_EPS) ? "" : "IN") <<
      "CORRECT with accuracy: " << eps << std::endl;
  }
  EndTask();

  StartTask("KernelDestruction");
  status = clReleaseKernel(kernel);
  PTI_ASSERT(status == CL_SUCCESS);
  status = clReleaseProgram(program);
  PTI_ASSERT(status == CL_SUCCESS);
  status = clReleaseCommandQueue(queue);
  PTI_ASSERT(status == CL_SUCCESS);
  status = clReleaseContext(context);
  PTI_ASSERT(status == CL_SUCCESS);\
  EndTask();
}

int main(int argc, char* argv[]) {
  cl_device_type type = CL_DEVICE_TYPE_GPU;
  if (argc > 1 && strcmp(argv[1], "cpu") == 0) {
    type = CL_DEVICE_TYPE_CPU;
  }

  cl_device_id device = utils::cl::GetIntelDevice(type);
  if (device == nullptr) {
    std::cout << "Unable to find target device" << std::endl;
    return 0;
  }

  unsigned size = 1024;
  if (argc > 2) {
    size = std::stoul(argv[2]);
  }

  unsigned repeat_count = 4;
  if (argc > 3) {
    repeat_count = std::stoul(argv[3]);
  }

  std::cout << "OpenCL Matrix Multiplication (matrix size: " << size <<
    " x " << size << ", repeats " << repeat_count << " times)" << std::endl;
  std::cout << "Target device: " << utils::cl::GetDeviceName(device) <<
    std::endl;

  domain = __itt_domain_create("CL.GEMM.Global");
  PTI_ASSERT(domain != nullptr);
  __itt_frame_begin_v3(domain, nullptr);

  StartTask("Prework");
  std::vector<float> a(size * size, A_VALUE);
  std::vector<float> b(size * size, B_VALUE);
  std::vector<float> c(size * size, 0.0f);
  EndTask();

  StartTask("Computation");
  auto start = std::chrono::steady_clock::now();
  float expected_result = A_VALUE * B_VALUE * size;
  Compute(device, a, b, c, size, repeat_count, expected_result);
  auto end = std::chrono::steady_clock::now();
  std::chrono::duration<float> time = end - start;
  EndTask();

  __itt_frame_end_v3(domain, nullptr);

  std::cout << "Total execution time: " << time.count() << " sec" <<
    std::endl;
  return 0;
}
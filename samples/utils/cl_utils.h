//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_UTILS_CL_UTILS_H_
#define PTI_SAMPLES_UTILS_CL_UTILS_H_

#include <string.h>

#include <string>
#include <vector>

#include <CL/cl.h>

#include "utils.h"

namespace utils {
namespace cl {

inline cl_device_id GetIntelDevice(cl_device_type type) {
  cl_int status = CL_SUCCESS;

  cl_uint platform_count = 0;
  status = clGetPlatformIDs(0, nullptr, &platform_count);
  if (status != CL_SUCCESS || platform_count == 0) return nullptr;

  std::vector<cl_platform_id> platform_list(platform_count, nullptr);
  status = clGetPlatformIDs(platform_count, platform_list.data(), nullptr);
  PTI_ASSERT(status == CL_SUCCESS);

  cl_device_id target = nullptr;
  for (cl_uint i = 0; i < platform_count; ++i) {
    cl_uint device_count = 0;

    status = clGetDeviceIDs(platform_list[i], type, 0, nullptr,
                            &device_count);
    if (status != CL_SUCCESS || device_count == 0) continue;

    std::vector<cl_device_id> device_list(device_count, nullptr);
    status = clGetDeviceIDs(platform_list[i], type, device_count,
                            device_list.data(), nullptr);
    PTI_ASSERT(status == CL_SUCCESS);

    char vendor[MAX_STR_SIZE] = { 0 };
    for (cl_uint j = 0; j < device_count; ++j) {
        status = clGetDeviceInfo(device_list[j], CL_DEVICE_VENDOR,
                                 MAX_STR_SIZE, vendor, nullptr);
        PTI_ASSERT(status == CL_SUCCESS);

        if (strstr(vendor, "Intel") != nullptr) {
            target = device_list[j];
            break;
        }
    }

    if (target != nullptr) {
      break;
    }
  }

  return target;
}

inline std::string GetKernelName(cl_kernel kernel) {
  PTI_ASSERT(kernel != nullptr);

  char name[MAX_STR_SIZE] = { 0 };
  cl_int status = CL_SUCCESS;

  status = clGetKernelInfo(kernel, CL_KERNEL_FUNCTION_NAME,
                           MAX_STR_SIZE, name, nullptr);
  PTI_ASSERT(status == CL_SUCCESS);

  return name;
}

inline std::string GetDeviceName(cl_device_id device) {
  PTI_ASSERT(device != nullptr);

  char name[MAX_STR_SIZE] = { 0 };
  cl_int status = clGetDeviceInfo(
      device, CL_DEVICE_NAME, MAX_STR_SIZE, name, nullptr);
  PTI_ASSERT(status == CL_SUCCESS);

  return name;
}

inline cl_device_type GetDeviceType(cl_device_id device) {
  PTI_ASSERT(device != nullptr);

  cl_device_type type = CL_DEVICE_TYPE_ALL;
  cl_int status = clGetDeviceInfo(
      device, CL_DEVICE_TYPE, sizeof(type), &type, nullptr);
  PTI_ASSERT(status == CL_SUCCESS);
  PTI_ASSERT(type != CL_DEVICE_TYPE_ALL);

  return type;
}

inline cl_program GetProgram(cl_kernel kernel) {
  PTI_ASSERT(kernel != nullptr);

  cl_int status = CL_SUCCESS;
  cl_program program = nullptr;
  
  status = clGetKernelInfo(kernel, CL_KERNEL_PROGRAM, sizeof(cl_program),
                           &program, nullptr);
  PTI_ASSERT(status == CL_SUCCESS);

  return program;
}

inline cl_context GetContext(cl_kernel kernel) {
  PTI_ASSERT(kernel != nullptr);

  cl_int status = CL_SUCCESS;
  cl_context context = nullptr;
  
  status = clGetKernelInfo(kernel, CL_KERNEL_CONTEXT, sizeof(cl_context),
                           &context, nullptr);
  PTI_ASSERT(status == CL_SUCCESS);

  return context;
}

inline std::vector<cl_device_id> GetDeviceList(cl_program program) {
  PTI_ASSERT(program != nullptr);

  cl_int status = CL_SUCCESS;
  cl_uint device_count = 0;
  status = clGetProgramInfo(program, CL_PROGRAM_NUM_DEVICES, sizeof(cl_uint),
                            &device_count, nullptr);
  PTI_ASSERT(status == CL_SUCCESS);
  if (device_count == 0) {
    return std::vector<cl_device_id>();
  }

  std::vector<cl_device_id> device_list(device_count);
  status = clGetProgramInfo(program, CL_PROGRAM_DEVICES,
                            device_count * sizeof(cl_device_id),
                            device_list.data(), nullptr);
  PTI_ASSERT(status == CL_SUCCESS);

  return device_list;
}

inline cl_queue_properties* EnableQueueProfiling(
    const cl_queue_properties* props) {
  cl_queue_properties* props_with_prof = nullptr;

  if (props == nullptr) {
    props_with_prof = new cl_queue_properties[3];
    props_with_prof[0] = CL_QUEUE_PROPERTIES;
    props_with_prof[1] = CL_QUEUE_PROFILING_ENABLE;
    props_with_prof[2] = 0;
  } else {
    int queue_props_id = -1;
    int props_count = 0;
    while (props[props_count] != 0) {
      if (props[props_count] == CL_QUEUE_PROPERTIES) {
        queue_props_id = props_count;
        ++props_count;
      } else if (props[props_count] == CL_QUEUE_SIZE) {
        ++props_count;
      }
      ++props_count;
    }
    PTI_ASSERT(props[props_count] == 0);

    if (queue_props_id >= 0 && queue_props_id + 1 < props_count) {
      props_with_prof = new cl_queue_properties[props_count + 1];
      for (int i = 0; i < props_count; ++i) {
        props_with_prof[i] = props[i];
      }
      props_with_prof[queue_props_id + 1] |=
        static_cast<unsigned long>(CL_QUEUE_PROFILING_ENABLE);
      props_with_prof[props_count] = 0;
    } else {
      props_with_prof = new cl_queue_properties[props_count + 3];
      for (int i = 0; i < props_count; ++i) {
        props_with_prof[i] = props[i];
      }
      props_with_prof[props_count] = CL_QUEUE_PROPERTIES;
      props_with_prof[props_count + 1] = CL_QUEUE_PROFILING_ENABLE;
      props_with_prof[props_count + 2] = 0;
    }
  }

  return props_with_prof;
}

inline size_t GetSimdWidth(cl_device_id device, cl_kernel kernel) {
  PTI_ASSERT(device != nullptr && kernel != nullptr);

  cl_int status = CL_SUCCESS;
  size_t simd_width = 0;
  status =
    clGetKernelWorkGroupInfo(kernel, device,
                             CL_KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE,
                             sizeof(size_t), &simd_width, nullptr);
  PTI_ASSERT(status == CL_SUCCESS);

  return simd_width;
}

inline cl_command_queue GetCommandQueue(cl_event event) {
  PTI_ASSERT(event != nullptr);

  cl_int status = CL_SUCCESS;
  cl_command_queue queue = nullptr;
  status = clGetEventInfo(event, CL_EVENT_COMMAND_QUEUE,
                          sizeof(cl_command_queue), &queue, nullptr);
  PTI_ASSERT(status == CL_SUCCESS);

  return queue;
}

inline cl_device_id GetDevice(cl_command_queue queue) {
  PTI_ASSERT(queue != nullptr);

  cl_int status = CL_SUCCESS;
  cl_device_id device = nullptr;
  status = clGetCommandQueueInfo(queue, CL_QUEUE_DEVICE,
                                 sizeof(cl_device_id), &device, nullptr);
  PTI_ASSERT(status == CL_SUCCESS);

  return device;
}

inline cl_ulong GetEventTimestamp(cl_event event, cl_profiling_info info) {
  PTI_ASSERT(event != nullptr);

  cl_int status = CL_SUCCESS;
  cl_ulong start = 0;

  status = clGetEventProfilingInfo(
      event, info, sizeof(cl_ulong), &start, nullptr);
  PTI_ASSERT(status == CL_SUCCESS);
  return start;
}

inline cl_ulong GetGpuTimestamp() {
  cl_ulong timestamp = 0;
#if defined(_WIN32)
  BOOL success = QueryPerformanceCounter(&timestamp);
  PTI_ASSERT(success);
#else
  timespec tp{0, 0};
  int status = clock_gettime(CLOCK_MONOTONIC_RAW, &tp);
  PTI_ASSERT(status == 0);
  timestamp = NSEC_IN_SEC * tp.tv_sec + tp.tv_nsec;
#endif
  return timestamp;
}

inline cl_ulong GetCpuTimestamp() {
  cl_ulong timestamp = 0;
#if defined(_WIN32)
  BOOL success = QueryPerformanceCounter(&timestamp);
  PTI_ASSERT(success);
#else
  timespec tp{0, 0};
  int status = clock_gettime(CLOCK_MONOTONIC, &tp);
  PTI_ASSERT(status == 0);
  timestamp = NSEC_IN_SEC * tp.tv_sec + tp.tv_nsec;
#endif
  return timestamp;
}

} // namespace cl
} // namespace utils

#endif // PTI_SAMPLES_UTILS_CL_UTILS_H_
//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_UTILS_CL_UTILS_H_
#define PTI_UTILS_CL_UTILS_H_

#include <string.h>

#include <string>
#include <vector>

#include <CL/cl.h>

#include "utils.h"

#define CL_KERNEL_MAX_SUB_GROUP_SIZE_FOR_NDRANGE_KHR 0x2033

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

inline bool CheckExtension(cl_device_id device, const char* extension) {
  cl_int status = CL_SUCCESS;

  size_t size = 0;
  status = clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, 0, nullptr, &size);
  PTI_ASSERT(status == CL_SUCCESS);

  if (size == 0) {
    return false;
  }

  std::vector<char> buffer(size);
  status = clGetDeviceInfo(
      device, CL_DEVICE_EXTENSIONS, size, buffer.data(), nullptr);
  PTI_ASSERT(status == CL_SUCCESS);

  std::string extensions(buffer.begin(), buffer.end());
  if (extensions.find(extension) != std::string::npos) {
    return true;
  }

  return false;
}

inline size_t GetKernelSimdWidth(cl_device_id device, cl_kernel kernel) {
  PTI_ASSERT(device != nullptr && kernel != nullptr);
  cl_int status = CL_SUCCESS;

  if (!CheckExtension(device, "cl_intel_subgroups")) {
    return 0;
  }

  typedef cl_int (*clGetKernelSubGroupInfoKHR)(
      cl_kernel kernel, cl_device_id device,
      cl_kernel_sub_group_info param_name, size_t input_value_size,
      const void* input_value, size_t param_value_size,
      void* param_value, size_t* param_value_size_ret);

  cl_platform_id platform = nullptr;
  status = clGetDeviceInfo(
      device, CL_DEVICE_PLATFORM, sizeof(cl_platform_id), &platform, nullptr);
  PTI_ASSERT(status == CL_SUCCESS);
  PTI_ASSERT(platform != nullptr);

  clGetKernelSubGroupInfoKHR func =
    reinterpret_cast<clGetKernelSubGroupInfoKHR>(
        clGetExtensionFunctionAddressForPlatform(
            platform, "clGetKernelSubGroupInfoKHR"));
  PTI_ASSERT(func != nullptr);

  size_t local_size[3]{0, 0, 0};

  size_t simd_width = 0;
  status = func(
      kernel, device, CL_KERNEL_MAX_SUB_GROUP_SIZE_FOR_NDRANGE_KHR,
      sizeof(size_t[3]), local_size, sizeof(size_t), &simd_width, nullptr);
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

inline cl_int GetEventStatus(cl_event event) {
  PTI_ASSERT(event != nullptr);
  cl_int event_status = CL_QUEUED;
  cl_int status = clGetEventInfo(
      event, CL_EVENT_COMMAND_EXECUTION_STATUS,
      sizeof(cl_int), &event_status, nullptr);
  PTI_ASSERT(status == CL_SUCCESS);
  return event_status;
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

inline const char* GetErrorString(cl_int error) {
  switch (error) {
    case CL_SUCCESS:
      return "CL_SUCCESS";
    case CL_DEVICE_NOT_FOUND:
      return "CL_DEVICE_NOT_FOUND";
    case CL_DEVICE_NOT_AVAILABLE:
      return "CL_DEVICE_NOT_AVAILABLE";
    case CL_COMPILER_NOT_AVAILABLE:
      return "CL_COMPILER_NOT_AVAILABLE";
    case CL_MEM_OBJECT_ALLOCATION_FAILURE:
      return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
    case CL_OUT_OF_RESOURCES:
      return "CL_OUT_OF_RESOURCES";
    case CL_OUT_OF_HOST_MEMORY:
      return "CL_OUT_OF_HOST_MEMORY";
    case CL_PROFILING_INFO_NOT_AVAILABLE:
      return "CL_PROFILING_INFO_NOT_AVAILABLE";
    case CL_MEM_COPY_OVERLAP:
      return "CL_MEM_COPY_OVERLAP";
    case CL_IMAGE_FORMAT_MISMATCH:
      return "CL_IMAGE_FORMAT_MISMATCH";
    case CL_IMAGE_FORMAT_NOT_SUPPORTED:
      return "CL_IMAGE_FORMAT_NOT_SUPPORTED";
    case CL_BUILD_PROGRAM_FAILURE:
      return "CL_BUILD_PROGRAM_FAILURE";
    case CL_MAP_FAILURE:
      return "CL_MAP_FAILURE";
    case CL_MISALIGNED_SUB_BUFFER_OFFSET:
      return "CL_MISALIGNED_SUB_BUFFER_OFFSET";
    case CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST:
      return "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST";
    case CL_COMPILE_PROGRAM_FAILURE:
      return "CL_COMPILE_PROGRAM_FAILURE";
    case CL_LINKER_NOT_AVAILABLE:
      return "CL_LINKER_NOT_AVAILABLE";
    case CL_LINK_PROGRAM_FAILURE:
      return "CL_LINK_PROGRAM_FAILURE";
    case CL_DEVICE_PARTITION_FAILED:
      return "CL_DEVICE_PARTITION_FAILED";
    case CL_KERNEL_ARG_INFO_NOT_AVAILABLE:
      return "CL_KERNEL_ARG_INFO_NOT_AVAILABLE";
    case CL_INVALID_VALUE:
      return "CL_INVALID_VALUE";
    case CL_INVALID_DEVICE_TYPE:
      return "CL_INVALID_DEVICE_TYPE";
    case CL_INVALID_PLATFORM:
      return "CL_INVALID_PLATFORM";
    case CL_INVALID_DEVICE:
      return "CL_INVALID_DEVICE";
    case CL_INVALID_CONTEXT:
      return "CL_INVALID_CONTEXT";
    case CL_INVALID_QUEUE_PROPERTIES:
      return "CL_INVALID_QUEUE_PROPERTIES";
    case CL_INVALID_COMMAND_QUEUE:
      return "CL_INVALID_COMMAND_QUEUE";
    case CL_INVALID_HOST_PTR:
      return "CL_INVALID_HOST_PTR";
    case CL_INVALID_MEM_OBJECT:
      return "CL_INVALID_MEM_OBJECT";
    case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR:
      return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
    case CL_INVALID_IMAGE_SIZE:
      return "CL_INVALID_IMAGE_SIZE";
    case CL_INVALID_SAMPLER:
      return "CL_INVALID_SAMPLER";
    case CL_INVALID_BINARY:
      return "CL_INVALID_BINARY";
    case CL_INVALID_BUILD_OPTIONS:
      return "CL_INVALID_BUILD_OPTIONS";
    case CL_INVALID_PROGRAM:
      return "CL_INVALID_PROGRAM";
    case CL_INVALID_PROGRAM_EXECUTABLE:
      return "CL_INVALID_PROGRAM_EXECUTABLE";
    case CL_INVALID_KERNEL_NAME:
      return "CL_INVALID_KERNEL_NAME";
    case CL_INVALID_KERNEL_DEFINITION:
      return "CL_INVALID_KERNEL_DEFINITION";
    case CL_INVALID_KERNEL:
      return "CL_INVALID_KERNEL";
    case CL_INVALID_ARG_INDEX:
      return "CL_INVALID_ARG_INDEX";
    case CL_INVALID_ARG_VALUE:
      return "CL_INVALID_ARG_VALUE";
    case CL_INVALID_ARG_SIZE:
      return "CL_INVALID_ARG_SIZE";
    case CL_INVALID_KERNEL_ARGS:
      return "CL_INVALID_KERNEL_ARGS";
    case CL_INVALID_WORK_DIMENSION:
      return "CL_INVALID_WORK_DIMENSION";
    case CL_INVALID_WORK_GROUP_SIZE:
      return "CL_INVALID_WORK_GROUP_SIZE";
    case CL_INVALID_WORK_ITEM_SIZE:
      return "CL_INVALID_WORK_ITEM_SIZE";
    case CL_INVALID_GLOBAL_OFFSET:
      return "CL_INVALID_GLOBAL_OFFSET";
    case CL_INVALID_EVENT_WAIT_LIST:
      return "CL_INVALID_EVENT_WAIT_LIST";
    case CL_INVALID_EVENT:
      return "CL_INVALID_EVENT";
    case CL_INVALID_OPERATION:
      return "CL_INVALID_OPERATION";
    case CL_INVALID_GL_OBJECT:
      return "CL_INVALID_GL_OBJECT";
    case CL_INVALID_BUFFER_SIZE:
      return "CL_INVALID_BUFFER_SIZE";
    case CL_INVALID_MIP_LEVEL:
      return "CL_INVALID_MIP_LEVEL";
    case CL_INVALID_GLOBAL_WORK_SIZE:
      return "CL_INVALID_GLOBAL_WORK_SIZE";
    case CL_INVALID_PROPERTY:
      return "CL_INVALID_PROPERTY";
    case CL_INVALID_IMAGE_DESCRIPTOR:
      return "CL_INVALID_IMAGE_DESCRIPTOR";
    case CL_INVALID_COMPILER_OPTIONS:
      return "CL_INVALID_COMPILER_OPTIONS";
    case CL_INVALID_LINKER_OPTIONS:
      return "CL_INVALID_LINKER_OPTIONS";
    case CL_INVALID_DEVICE_PARTITION_COUNT:
      return "CL_INVALID_DEVICE_PARTITION_COUNT";
    case CL_INVALID_PIPE_SIZE:
      return "CL_INVALID_PIPE_SIZE";
    case CL_INVALID_DEVICE_QUEUE:
      return "CL_INVALID_DEVICE_QUEUE";
    default:
      break;
  }
  return "UNKNOWN";
}

} // namespace cl
} // namespace utils

#endif // PTI_UTILS_CL_UTILS_H_
//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_CL_TRACER_CL_EXT_CALLBACKS_H_
#define PTI_TOOLS_CL_TRACER_CL_EXT_CALLBACKS_H_

#include <sstream>

#include <CL/cl.h>
#include <CL/cl_ext_private.h>

#include "cl_ext_collector.h"
#include "cl_utils.h"
#include "trace_guard.h"

static void* GetFunctionAddress(const char* function_name, cl_device_type device_type) {
  cl_int status = CL_SUCCESS;

  cl_device_id device = utils::cl::GetIntelDevice(device_type);
  if (device == nullptr) {
    return nullptr;
  }

  cl_platform_id platform = nullptr;
  status = clGetDeviceInfo(
      device, CL_DEVICE_PLATFORM, sizeof(cl_platform_id), &platform, nullptr);
  PTI_ASSERT(status == CL_SUCCESS);
  PTI_ASSERT(platform != nullptr);

  return clGetExtensionFunctionAddressForPlatform(platform, function_name);
}

template <cl_device_type DEVICE_TYPE>
static void* clHostMemAllocINTEL(
    cl_context context,
    const cl_mem_properties_intel* properties,
    size_t size,
    cl_uint alignment,
    cl_int* errcode_ret) {
  TraceGuard guard;
  const char* function_name = "clHostMemAllocINTEL";

  ClExtCollector* collector = ClExtCollector::GetInstance();
  PTI_ASSERT(collector != nullptr);

  cl_int current_error = CL_SUCCESS;

  uint64_t start = collector->GetTimestamp<DEVICE_TYPE>();

  if (collector->IsCallLogging<DEVICE_TYPE>()) {
    std::stringstream stream;
    stream << ">>>> [" << start << "] ";
    if (collector->NeedPid<DEVICE_TYPE>()) {
      stream << "<PID:" << utils::GetPid() << "> ";
    }
    if (collector->NeedTid<DEVICE_TYPE>()) {
      stream << "<TID:" << utils::GetTid() << "> ";
    }
    stream << function_name << ":";

    stream << " context = " << context;
    stream << " properties = " << properties;
    stream << " size = " << size;
    stream << " alignment = " << alignment;
    stream << " errcode_ret = " << errcode_ret;
    stream << std::endl;

    if (errcode_ret == nullptr) {
      errcode_ret = &current_error;
    }

    collector->Log<DEVICE_TYPE>(stream.str());
  }

  decltype(clHostMemAllocINTEL<DEVICE_TYPE>)* function =
    reinterpret_cast<decltype(clHostMemAllocINTEL<DEVICE_TYPE>)*>(
        GetFunctionAddress(function_name, DEVICE_TYPE));
  void* result = function(context, properties, size, alignment, errcode_ret);

  uint64_t end = collector->GetTimestamp<DEVICE_TYPE>();
  collector->AddFunctionTime<DEVICE_TYPE>(function_name, end - start);

  if (collector->IsCallLogging<DEVICE_TYPE>()) {
    std::stringstream stream;
    stream << "<<<< [" << end << "] ";
    if (collector->NeedPid<DEVICE_TYPE>()) {
      stream << "<PID:" << utils::GetPid() << "> ";
    }
    if (collector->NeedTid<DEVICE_TYPE>()) {
      stream << "<TID:" << utils::GetTid() << "> ";
    }
    stream << function_name;
    stream << " [" << (end - start) << " ns]";

    stream << " result = " << result;

    PTI_ASSERT(errcode_ret != nullptr);
    stream << " -> " << utils::cl::GetErrorString(*errcode_ret);
    stream << " (" << *errcode_ret << ")";
    stream << std::endl;

    collector->Log<DEVICE_TYPE>(stream.str());
  }

  collector->Callback<DEVICE_TYPE>(function_name, start, end);

  return result;
}

template <cl_device_type DEVICE_TYPE>
static void* clDeviceMemAllocINTEL(
    cl_context context,
    cl_device_id device,
    const cl_mem_properties_intel* properties,
    size_t size,
    cl_uint alignment,
    cl_int* errcode_ret) {
  TraceGuard guard;
  const char* function_name = "clDeviceMemAllocINTEL";

  ClExtCollector* collector = ClExtCollector::GetInstance();
  PTI_ASSERT(collector != nullptr);

  cl_int current_error = CL_SUCCESS;

  uint64_t start = collector->GetTimestamp<DEVICE_TYPE>();

  if (collector->IsCallLogging<DEVICE_TYPE>()) {
    std::stringstream stream;
    stream << ">>>> [" << start << "] ";
    if (collector->NeedPid<DEVICE_TYPE>()) {
      stream << "<PID:" << utils::GetPid() << "> ";
    }
    if (collector->NeedTid<DEVICE_TYPE>()) {
      stream << "<TID:" << utils::GetTid() << "> ";
    }
    stream << function_name << ":";

    stream << " context = " << context;
    stream << " device = " << device;
    stream << " properties = " << properties;
    stream << " size = " << size;
    stream << " alignment = " << alignment;
    stream << " errcode_ret = " << errcode_ret;
    stream << std::endl;

    if (errcode_ret == nullptr) {
      errcode_ret = &current_error;
    }

    collector->Log<DEVICE_TYPE>(stream.str());
  }

  decltype(clDeviceMemAllocINTEL<DEVICE_TYPE>)* function =
    reinterpret_cast<decltype(clDeviceMemAllocINTEL<DEVICE_TYPE>)*>(
        GetFunctionAddress(function_name, DEVICE_TYPE));
  void* result = function(
      context, device, properties, size, alignment, errcode_ret);

  uint64_t end = collector->GetTimestamp<DEVICE_TYPE>();
  collector->AddFunctionTime<DEVICE_TYPE>(function_name, end - start);

  if (collector->IsCallLogging<DEVICE_TYPE>()) {
    std::stringstream stream;
    stream << "<<<< [" << end << "] ";
    if (collector->NeedPid<DEVICE_TYPE>()) {
      stream << "<PID:" << utils::GetPid() << "> ";
    }
    if (collector->NeedTid<DEVICE_TYPE>()) {
      stream << "<TID:" << utils::GetTid() << "> ";
    }
    stream << function_name;
    stream << " [" << (end - start) << " ns]";

    stream << " result = " << result;

    PTI_ASSERT(errcode_ret != nullptr);
    stream << " -> " << utils::cl::GetErrorString(*errcode_ret);
    stream << " (" << *errcode_ret << ")";
    stream << std::endl;

    collector->Log<DEVICE_TYPE>(stream.str());
  }

  collector->Callback<DEVICE_TYPE>(function_name, start, end);

  return result;
}

template <cl_device_type DEVICE_TYPE>
static void* clSharedMemAllocINTEL(
    cl_context context,
    cl_device_id device,
    const cl_mem_properties_intel* properties,
    size_t size,
    cl_uint alignment,
    cl_int* errcode_ret) {
  TraceGuard guard;
  const char* function_name = "clSharedMemAllocINTEL";

  ClExtCollector* collector = ClExtCollector::GetInstance();
  PTI_ASSERT(collector != nullptr);

  cl_int current_error = CL_SUCCESS;

  uint64_t start = collector->GetTimestamp<DEVICE_TYPE>();

  if (collector->IsCallLogging<DEVICE_TYPE>()) {
    std::stringstream stream;
    stream << ">>>> [" << start << "] ";
    if (collector->NeedPid<DEVICE_TYPE>()) {
      stream << "<PID:" << utils::GetPid() << "> ";
    }
    if (collector->NeedTid<DEVICE_TYPE>()) {
      stream << "<TID:" << utils::GetTid() << "> ";
    }
    stream << function_name << ":";

    stream << " context = " << context;
    stream << " device = " << device;
    stream << " properties = " << properties;
    stream << " size = " << size;
    stream << " alignment = " << alignment;
    stream << " errcode_ret = " << errcode_ret;
    stream << std::endl;

    if (errcode_ret == nullptr) {
      errcode_ret = &current_error;
    }

    collector->Log<DEVICE_TYPE>(stream.str());
  }

  decltype(clSharedMemAllocINTEL<DEVICE_TYPE>)* function =
    reinterpret_cast<decltype(clSharedMemAllocINTEL<DEVICE_TYPE>)*>(
        GetFunctionAddress(function_name, DEVICE_TYPE));
  void* result = function(
      context, device, properties, size, alignment, errcode_ret);

  uint64_t end = collector->GetTimestamp<DEVICE_TYPE>();
  collector->AddFunctionTime<DEVICE_TYPE>(function_name, end - start);

  if (collector->IsCallLogging<DEVICE_TYPE>()) {
    std::stringstream stream;
    stream << "<<<< [" << end << "] ";
    if (collector->NeedPid<DEVICE_TYPE>()) {
      stream << "<PID:" << utils::GetPid() << "> ";
    }
    if (collector->NeedTid<DEVICE_TYPE>()) {
      stream << "<TID:" << utils::GetTid() << "> ";
    }
    stream << function_name;
    stream << " [" << (end - start) << " ns]";

    stream << " result = " << result;

    PTI_ASSERT(errcode_ret != nullptr);
    stream << " -> " << utils::cl::GetErrorString(*errcode_ret);
    stream << " (" << *errcode_ret << ")";
    stream << std::endl;

    collector->Log<DEVICE_TYPE>(stream.str());
  }

  collector->Callback<DEVICE_TYPE>(function_name, start, end);

  return result;
}

template <cl_device_type DEVICE_TYPE>
static cl_int clMemFreeINTEL(
    cl_context context,
    void* ptr) {
  TraceGuard guard;
  const char* function_name = "clMemFreeINTEL";

  ClExtCollector* collector = ClExtCollector::GetInstance();
  PTI_ASSERT(collector != nullptr);

  cl_int current_error = CL_SUCCESS;

  uint64_t start = collector->GetTimestamp<DEVICE_TYPE>();

  if (collector->IsCallLogging<DEVICE_TYPE>()) {
    std::stringstream stream;
    stream << ">>>> [" << start << "] ";
    if (collector->NeedPid<DEVICE_TYPE>()) {
      stream << "<PID:" << utils::GetPid() << "> ";
    }
    if (collector->NeedTid<DEVICE_TYPE>()) {
      stream << "<TID:" << utils::GetTid() << "> ";
    }
    stream << function_name << ":";

    stream << " context = " << context;
    stream << " ptr = " << ptr;
    stream << std::endl;

    collector->Log<DEVICE_TYPE>(stream.str());
  }

  decltype(clMemFreeINTEL<DEVICE_TYPE>)* function =
    reinterpret_cast<decltype(clMemFreeINTEL<DEVICE_TYPE>)*>(
        GetFunctionAddress(function_name, DEVICE_TYPE));
  cl_int result = function(context, ptr);

  uint64_t end = collector->GetTimestamp<DEVICE_TYPE>();
  collector->AddFunctionTime<DEVICE_TYPE>(function_name, end - start);

  if (collector->IsCallLogging<DEVICE_TYPE>()) {
    std::stringstream stream;
    stream << "<<<< [" << end << "] ";
    if (collector->NeedPid<DEVICE_TYPE>()) {
      stream << "<PID:" << utils::GetPid() << "> ";
    }
    if (collector->NeedTid<DEVICE_TYPE>()) {
      stream << "<TID:" << utils::GetTid() << "> ";
    }
    stream << function_name;
    stream << " [" << (end - start) << " ns]";

    stream << " -> " << utils::cl::GetErrorString(result);
    stream << " (" << result << ")";
    stream << std::endl;

    collector->Log<DEVICE_TYPE>(stream.str());
  }

  collector->Callback<DEVICE_TYPE>(function_name, start, end);

  return result;
}

template <cl_device_type DEVICE_TYPE>
static cl_int clGetMemAllocInfoINTEL(
    cl_context context,
    const void* ptr,
    cl_mem_info_intel param_name,
    size_t param_value_size,
    void* param_value,
    size_t* param_value_size_ret) {
  TraceGuard guard;
  const char* function_name = "clGetMemAllocInfoINTEL";

  ClExtCollector* collector = ClExtCollector::GetInstance();
  PTI_ASSERT(collector != nullptr);

  cl_int current_error = CL_SUCCESS;

  uint64_t start = collector->GetTimestamp<DEVICE_TYPE>();

  if (collector->IsCallLogging<DEVICE_TYPE>()) {
    std::stringstream stream;
    stream << ">>>> [" << start << "] ";
    if (collector->NeedPid<DEVICE_TYPE>()) {
      stream << "<PID:" << utils::GetPid() << "> ";
    }
    if (collector->NeedTid<DEVICE_TYPE>()) {
      stream << "<TID:" << utils::GetTid() << "> ";
    }
    stream << function_name << ":";

    stream << " context = " << context;
    stream << " ptr = " << ptr;
    stream << " param_name = " << param_name;
    stream << " param_value_size = " << param_value_size;
    stream << " param_value = " << param_value;
    stream << " param_value_size_ret = " << param_value_size_ret;
    stream << std::endl;

    collector->Log<DEVICE_TYPE>(stream.str());
  }

  decltype(clGetMemAllocInfoINTEL<DEVICE_TYPE>)* function =
    reinterpret_cast<decltype(clGetMemAllocInfoINTEL<DEVICE_TYPE>)*>(
        GetFunctionAddress(function_name, DEVICE_TYPE));
  cl_int result = function(
      context, ptr, param_name, param_value_size,
      param_value, param_value_size_ret);

  uint64_t end = collector->GetTimestamp<DEVICE_TYPE>();
  collector->AddFunctionTime<DEVICE_TYPE>(function_name, end - start);

  if (collector->IsCallLogging<DEVICE_TYPE>()) {
    std::stringstream stream;
    stream << "<<<< [" << end << "] ";
    if (collector->NeedPid<DEVICE_TYPE>()) {
      stream << "<PID:" << utils::GetPid() << "> ";
    }
    if (collector->NeedTid<DEVICE_TYPE>()) {
      stream << "<TID:" << utils::GetTid() << "> ";
    }
    stream << function_name;
    stream << " [" << (end - start) << " ns]";

    stream << " -> " << utils::cl::GetErrorString(result);
    stream << " (" << result << ")";
    stream << std::endl;

    collector->Log<DEVICE_TYPE>(stream.str());
  }

  collector->Callback<DEVICE_TYPE>(function_name, start, end);

  return result;
}

template <cl_device_type DEVICE_TYPE>
static cl_int clSetKernelArgMemPointerINTEL(
    cl_kernel kernel,
    cl_uint arg_index,
    const void* arg_value) {
  TraceGuard guard;
  const char* function_name = "clSetKernelArgMemPointerINTEL";

  ClExtCollector* collector = ClExtCollector::GetInstance();
  PTI_ASSERT(collector != nullptr);

  cl_int current_error = CL_SUCCESS;

  uint64_t start = collector->GetTimestamp<DEVICE_TYPE>();

  if (collector->IsCallLogging<DEVICE_TYPE>()) {
    std::stringstream stream;
    stream << ">>>> [" << start << "] ";
    if (collector->NeedPid<DEVICE_TYPE>()) {
      stream << "<PID:" << utils::GetPid() << "> ";
    }
    if (collector->NeedTid<DEVICE_TYPE>()) {
      stream << "<TID:" << utils::GetTid() << "> ";
    }
    stream << function_name << ":";

    stream << " kernel = " << kernel;
    stream << " arg_index = " << arg_index;
    stream << " arg_value = " << arg_value;
    stream << std::endl;

    collector->Log<DEVICE_TYPE>(stream.str());
  }

  decltype(clSetKernelArgMemPointerINTEL<DEVICE_TYPE>)* function =
    reinterpret_cast<decltype(clSetKernelArgMemPointerINTEL<DEVICE_TYPE>)*>(
        GetFunctionAddress(function_name, DEVICE_TYPE));
  cl_int result = function(kernel, arg_index, arg_value);

  uint64_t end = collector->GetTimestamp<DEVICE_TYPE>();
  collector->AddFunctionTime<DEVICE_TYPE>(function_name, end - start);

  if (collector->IsCallLogging<DEVICE_TYPE>()) {
    std::stringstream stream;
    stream << "<<<< [" << end << "] ";
    if (collector->NeedPid<DEVICE_TYPE>()) {
      stream << "<PID:" << utils::GetPid() << "> ";
    }
    if (collector->NeedTid<DEVICE_TYPE>()) {
      stream << "<TID:" << utils::GetTid() << "> ";
    }
    stream << function_name;
    stream << " [" << (end - start) << " ns]";

    stream << " -> " << utils::cl::GetErrorString(result);
    stream << " (" << result << ")";
    stream << std::endl;

    collector->Log<DEVICE_TYPE>(stream.str());
  }

  collector->Callback<DEVICE_TYPE>(function_name, start, end);

  return result;
}

template <cl_device_type DEVICE_TYPE>
static cl_int clEnqueueMemcpyINTEL(
    cl_command_queue command_queue,
    cl_bool blocking,
    void* dst_ptr,
    const void* src_ptr,
    size_t size,
    cl_uint num_events_in_wait_list,
    const cl_event* event_wait_list,
    cl_event* event) {
  TraceGuard guard;
  const char* function_name = "clEnqueueMemcpyINTEL";

  ClExtCollector* collector = ClExtCollector::GetInstance();
  PTI_ASSERT(collector != nullptr);

  cl_int current_error = CL_SUCCESS;

  uint64_t start = collector->GetTimestamp<DEVICE_TYPE>();

  if (collector->IsCallLogging<DEVICE_TYPE>()) {
    std::stringstream stream;
    stream << ">>>> [" << start << "] ";
    if (collector->NeedPid<DEVICE_TYPE>()) {
      stream << "<PID:" << utils::GetPid() << "> ";
    }
    if (collector->NeedTid<DEVICE_TYPE>()) {
      stream << "<TID:" << utils::GetTid() << "> ";
    }
    stream << function_name << ":";

    stream << " command_queue = " << command_queue;
    stream << " blocking = " << blocking;
    stream << " dst_ptr = " << dst_ptr;
    stream << " src_ptr = " << src_ptr;
    stream << " size = " << size;
    stream << " num_events_in_wait_list = " << num_events_in_wait_list;
    stream << " event_wait_list = " << event_wait_list;
    stream << " event = " << event;
    stream << std::endl;

    collector->Log<DEVICE_TYPE>(stream.str());
  }

  decltype(clEnqueueMemcpyINTEL<DEVICE_TYPE>)* function =
    reinterpret_cast<decltype(clEnqueueMemcpyINTEL<DEVICE_TYPE>)*>(
        GetFunctionAddress(function_name, DEVICE_TYPE));
  cl_int result = function(
      command_queue, blocking, dst_ptr, src_ptr,
      size, num_events_in_wait_list, event_wait_list, event);

  uint64_t end = collector->GetTimestamp<DEVICE_TYPE>();
  collector->AddFunctionTime<DEVICE_TYPE>(function_name, end - start);

  if (collector->IsCallLogging<DEVICE_TYPE>()) {
    std::stringstream stream;
    stream << "<<<< [" << end << "] ";
    if (collector->NeedPid<DEVICE_TYPE>()) {
      stream << "<PID:" << utils::GetPid() << "> ";
    }
    if (collector->NeedTid<DEVICE_TYPE>()) {
      stream << "<TID:" << utils::GetTid() << "> ";
    }
    stream << function_name;
    stream << " [" << (end - start) << " ns]";

    stream << " -> " << utils::cl::GetErrorString(result);
    stream << " (" << result << ")";
    stream << std::endl;

    collector->Log<DEVICE_TYPE>(stream.str());
  }

  collector->Callback<DEVICE_TYPE>(function_name, start, end);

  return result;
}

template <cl_device_type DEVICE_TYPE>
static cl_int clGetDeviceGlobalVariablePointerINTEL(
    cl_device_id device,
    cl_program program,
    const char* global_variable_name,
    size_t* global_variable_size_ret,
    void** global_variable_pointer_ret) {
  TraceGuard guard;
  const char* function_name = "clGetDeviceGlobalVariablePointerINTEL";

  ClExtCollector* collector = ClExtCollector::GetInstance();
  PTI_ASSERT(collector != nullptr);

  cl_int current_error = CL_SUCCESS;

  uint64_t start = collector->GetTimestamp<DEVICE_TYPE>();

  if (collector->IsCallLogging<DEVICE_TYPE>()) {
    std::stringstream stream;
    stream << ">>>> [" << start << "] ";
    if (collector->NeedPid<DEVICE_TYPE>()) {
      stream << "<PID:" << utils::GetPid() << "> ";
    }
    if (collector->NeedTid<DEVICE_TYPE>()) {
      stream << "<TID:" << utils::GetTid() << "> ";
    }
    stream << function_name << ":";

    stream << " device = " << device;
    stream << " program = " << program;
    stream << " global_variable_name = " << global_variable_name;
    stream << " global_variable_size_ret = " << global_variable_size_ret;
    stream << " global_variable_pointer_ret = " << global_variable_pointer_ret;
    stream << std::endl;

    collector->Log<DEVICE_TYPE>(stream.str());
  }

  decltype(clGetDeviceGlobalVariablePointerINTEL<DEVICE_TYPE>)* function =
    reinterpret_cast<decltype(
          clGetDeviceGlobalVariablePointerINTEL<DEVICE_TYPE>)*>(
              GetFunctionAddress(function_name, DEVICE_TYPE));
  cl_int result = function(
      device, program, global_variable_name,
      global_variable_size_ret, global_variable_pointer_ret);

  uint64_t end = collector->GetTimestamp<DEVICE_TYPE>();
  collector->AddFunctionTime<DEVICE_TYPE>(function_name, end - start);

  if (collector->IsCallLogging<DEVICE_TYPE>()) {
    std::stringstream stream;
    stream << "<<<< [" << end << "] ";
    if (collector->NeedPid<DEVICE_TYPE>()) {
      stream << "<PID:" << utils::GetPid() << "> ";
    }
    if (collector->NeedTid<DEVICE_TYPE>()) {
      stream << "<TID:" << utils::GetTid() << "> ";
    }
    stream << function_name;
    stream << " [" << (end - start) << " ns]";

    stream << " -> " << utils::cl::GetErrorString(result);
    stream << " (" << result << ")";
    stream << std::endl;

    collector->Log<DEVICE_TYPE>(stream.str());
  }

  collector->Callback<DEVICE_TYPE>(function_name, start, end);

  return result;
}

template <cl_device_type DEVICE_TYPE>
static cl_int clGetKernelSuggestedLocalWorkSizeINTEL(
    cl_command_queue command_queue,
    cl_kernel kernel,
    cl_uint workDim,
    const size_t* global_work_offset,
    const size_t* global_work_size,
    size_t* suggested_local_work_size) {
  TraceGuard guard;
  const char* function_name = "clGetKernelSuggestedLocalWorkSizeINTEL";

  ClExtCollector* collector = ClExtCollector::GetInstance();
  PTI_ASSERT(collector != nullptr);

  cl_int current_error = CL_SUCCESS;

  uint64_t start = collector->GetTimestamp<DEVICE_TYPE>();

  if (collector->IsCallLogging<DEVICE_TYPE>()) {
    std::stringstream stream;
    stream << ">>>> [" << start << "] ";
    if (collector->NeedPid<DEVICE_TYPE>()) {
      stream << "<PID:" << utils::GetPid() << "> ";
    }
    if (collector->NeedTid<DEVICE_TYPE>()) {
      stream << "<TID:" << utils::GetTid() << "> ";
    }
    stream << function_name << ":";

    stream << " command_queue = " << command_queue;
    stream << " kernel = " << kernel;
    stream << " workDim = " << workDim;
    stream << " global_work_offset = " << global_work_offset;
    stream << " global_work_size = " << global_work_size;
    stream << " suggested_local_work_size = " << suggested_local_work_size;
    stream << std::endl;

    collector->Log<DEVICE_TYPE>(stream.str());
  }

  decltype(clGetKernelSuggestedLocalWorkSizeINTEL<DEVICE_TYPE>)* function =
    reinterpret_cast<decltype(
          clGetKernelSuggestedLocalWorkSizeINTEL<DEVICE_TYPE>)*>(
              GetFunctionAddress(function_name, DEVICE_TYPE));
  cl_int result = function(
      command_queue, kernel, workDim, global_work_offset,
      global_work_size, suggested_local_work_size);

  uint64_t end = collector->GetTimestamp<DEVICE_TYPE>();
  collector->AddFunctionTime<DEVICE_TYPE>(function_name, end - start);

  if (collector->IsCallLogging<DEVICE_TYPE>()) {
    std::stringstream stream;
    stream << "<<<< [" << end << "] ";
    if (collector->NeedPid<DEVICE_TYPE>()) {
      stream << "<PID:" << utils::GetPid() << "> ";
    }
    if (collector->NeedTid<DEVICE_TYPE>()) {
      stream << "<TID:" << utils::GetTid() << "> ";
    }
    stream << function_name;
    stream << " [" << (end - start) << " ns]";

    stream << " suggested_local_work_size = " << suggested_local_work_size;
    if (suggested_local_work_size != nullptr) {
      stream << " (" << *suggested_local_work_size << ")";
    }

    stream << " -> " << utils::cl::GetErrorString(result);
    stream << " (" << result << ")";
    stream << std::endl;

    collector->Log<DEVICE_TYPE>(stream.str());
  }

  collector->Callback<DEVICE_TYPE>(function_name, start, end);

  return result;
}

#endif // PTI_TOOLS_CL_TRACER_CL_EXT_CALLBACKS_H_

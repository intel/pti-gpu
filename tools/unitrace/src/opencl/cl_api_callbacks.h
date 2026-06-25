//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_UNITRACE_CL_API_CALLBACKS_H_
#define PTI_TOOLS_UNITRACE_CL_API_CALLBACKS_H_

#include "utils_host.h"

static thread_local cl_int current_error = CL_SUCCESS;

static void clGetSupportedImageFormatsOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetSupportedImageFormats* params =
    reinterpret_cast<const cl_params_clGetSupportedImageFormats*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " context = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->context)));
  log_msg += " flags = " + std::to_string(*(params->flags));
  log_msg += " imageType = " + std::to_string(*(params->imageType));
  log_msg += " numEntries = " + std::to_string(*(params->numEntries));
  log_msg += " imageFormats = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->imageFormats)));
  log_msg += " numImageFormats = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->numImageFormats)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetSupportedImageFormatsOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string(end - start) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetKernelInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetKernelInfo* params =
    reinterpret_cast<const cl_params_clGetKernelInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " kernel = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->kernel)));
  log_msg += " paramName = " + std::to_string(*(params->paramName));
  log_msg += " paramValueSize = " + std::to_string(*(params->paramValueSize));
  log_msg += " paramValue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->paramValue)));
  log_msg += " paramValueSizeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->paramValueSizeRet)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetKernelInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string(end - start) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clCompileProgramOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCompileProgram* params =
    reinterpret_cast<const cl_params_clCompileProgram*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " program = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->program)));
  log_msg += " numDevices = " + std::to_string(*(params->numDevices));
  log_msg += " deviceList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->deviceList)));
  if (*(params->options) == nullptr) {
    log_msg += " options = 0";
  } else if (strlen(*(params->options)) == 0) {
    log_msg += " options = \"\"";
  } else {
    log_msg += " options = \"" + std::string(*(params->options)) + "\"";
  }
  log_msg += " numInputHeaders = " + std::to_string(*(params->numInputHeaders));
  log_msg += " inputHeaders = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->inputHeaders)));
  log_msg += " headerIncludeNames = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->headerIncludeNames)));
  log_msg += " funcNotify = " + ToHexString(reinterpret_cast<uintptr_t>(*reinterpret_cast<decltype(params->funcNotify)*>(params->funcNotify)));
  log_msg += " userData = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->userData)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clCompileProgramOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string(end - start) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clSetEventCallbackOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clSetEventCallback* params =
    reinterpret_cast<const cl_params_clSetEventCallback*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += " commandExecCallbackType = " +
    std::to_string(*(params->commandExecCallbackType));
  log_msg += " funcNotify = " + ToHexString(reinterpret_cast<uintptr_t>(*reinterpret_cast<decltype(params->funcNotify)*>(params->funcNotify)));
  log_msg += " userData = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->userData)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clSetEventCallbackOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string(end - start) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clUnloadPlatformCompilerOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clUnloadPlatformCompiler* params =
    reinterpret_cast<const cl_params_clUnloadPlatformCompiler*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " platform = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->platform)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clUnloadPlatformCompilerOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetPlatformIDsOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetPlatformIDs* params =
    reinterpret_cast<const cl_params_clGetPlatformIDs*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " numEntries = " + std::to_string(*(params->numEntries));
  log_msg += " platforms = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->platforms)));
  log_msg += " numPlatforms = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->numPlatforms)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetPlatformIDsOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clUnloadCompilerOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clUnloadCompiler* params =
    reinterpret_cast<const cl_params_clUnloadCompiler*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += "\n";

  collector->Log(log_msg);
}

static void clUnloadCompilerOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueBarrierWithWaitListOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueBarrierWithWaitList* params =
    reinterpret_cast<const cl_params_clEnqueueBarrierWithWaitList*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += " numEventsInWaitList = " + std::to_string(*(params->numEventsInWaitList));
  log_msg += " eventWaitList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->eventWaitList)));
  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueBarrierWithWaitListOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueMapBufferOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueMapBuffer* params =
    reinterpret_cast<const cl_params_clEnqueueMapBuffer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += " buffer = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->buffer)));
  log_msg += " blockingMap = " + std::to_string(*(params->blockingMap));
  log_msg += " mapFlags = " + std::to_string(*(params->mapFlags));
  log_msg += " offset = " + std::to_string(*(params->offset));
  log_msg += " cb = " + std::to_string(*(params->cb));
  log_msg += " numEventsInWaitList = " + std::to_string(*(params->numEventsInWaitList));
  log_msg += " eventWaitList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->eventWaitList)));
  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += " errcodeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->errcodeRet)));
  log_msg += "\n";

  collector->Log(log_msg);

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clEnqueueMapBufferOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  const cl_params_clEnqueueMapBuffer* params =
    reinterpret_cast<const cl_params_clEnqueueMapBuffer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  void ** result =
    reinterpret_cast<void **>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  log_msg += " result = " + ToHexString(reinterpret_cast<uintptr_t>(*result));

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(**(params->errcodeRet));
  log_msg += " (" + std::to_string(**(params->errcodeRet)) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clCreateImage3DOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateImage3D* params =
    reinterpret_cast<const cl_params_clCreateImage3D*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " context = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->context)));
  log_msg += " flags = " + std::to_string(*(params->flags));
  log_msg += " imageFormat = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->imageFormat)));
  log_msg += " imageWidth = " + ToHexString(static_cast<uintptr_t>(*(params->imageWidth)));
  log_msg += " imageHeight = " + ToHexString(static_cast<uintptr_t>(*(params->imageHeight)));
  log_msg += " imageDepth = " + ToHexString(static_cast<uintptr_t>(*(params->imageDepth)));
  log_msg += " imageRowPitch = " + ToHexString(static_cast<uintptr_t>(*(params->imageRowPitch)));
  log_msg += " imageSlicePitch = " + ToHexString(static_cast<uintptr_t>(*(params->imageSlicePitch)));
  log_msg += " hostPtr = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->hostPtr)));
  log_msg += " errcodeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->errcodeRet)));
  log_msg += "\n";

  collector->Log(log_msg);

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateImage3DOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  const cl_params_clCreateImage3D* params =
    reinterpret_cast<const cl_params_clCreateImage3D*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_mem* result =
    reinterpret_cast<cl_mem*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  log_msg += " result = " + ToHexString(reinterpret_cast<uintptr_t>(*result));

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(**(params->errcodeRet));
  log_msg += " (" + std::to_string(**(params->errcodeRet)) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetKernelArgInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetKernelArgInfo* params =
    reinterpret_cast<const cl_params_clGetKernelArgInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " kernel = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->kernel)));
  log_msg += " argIndx = " + std::to_string(*(params->argIndx));
  log_msg += " paramName = " + std::to_string(*(params->paramName));
  log_msg += " paramValueSize = " + std::to_string(*(params->paramValueSize));
  log_msg += " paramValue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->paramValue)));
  log_msg += " paramValueSizeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->paramValueSizeRet)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetKernelArgInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueSVMFreeOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueSVMFree* params =
    reinterpret_cast<const cl_params_clEnqueueSVMFree*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += " numSvmPointers = " + std::to_string(*(params->numSvmPointers));
  log_msg += " svmPointers = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->svmPointers)));
  log_msg += " pfnFreeFunc = " + ToHexString(reinterpret_cast<uintptr_t>(*reinterpret_cast<decltype(params->pfnFreeFunc)*>(params->pfnFreeFunc)));
  log_msg += " userData = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->userData)));
  log_msg += " numEventsInWaitList = " + std::to_string(*(params->numEventsInWaitList));
  log_msg += " eventWaitList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->eventWaitList)));
  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueSVMFreeOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueCopyImageToBufferOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueCopyImageToBuffer* params =
    reinterpret_cast<const cl_params_clEnqueueCopyImageToBuffer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += " srcImage = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->srcImage)));
  log_msg += " dstBuffer = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->dstBuffer)));
  log_msg += " srcOrigin = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->srcOrigin)));
  log_msg += " region = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->region)));
  log_msg += " dstOffset = " + std::to_string(*(params->dstOffset));
  log_msg += " numEventsInWaitList = " + std::to_string(*(params->numEventsInWaitList));
  log_msg += " eventWaitList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->eventWaitList)));
  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueCopyImageToBufferOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetContextInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetContextInfo* params =
    reinterpret_cast<const cl_params_clGetContextInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " context = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->context)));
  log_msg += " paramName = " + std::to_string(*(params->paramName));
  log_msg += " paramValueSize = " + std::to_string(*(params->paramValueSize));
  log_msg += " paramValue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->paramValue)));
  log_msg += " paramValueSizeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->paramValueSizeRet)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetContextInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clRetainCommandQueueOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clRetainCommandQueue* params =
    reinterpret_cast<const cl_params_clRetainCommandQueue*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clRetainCommandQueueOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueWriteImageOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueWriteImage* params =
    reinterpret_cast<const cl_params_clEnqueueWriteImage*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += " image = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->image)));
  log_msg += " blockingWrite = " + std::to_string(*(params->blockingWrite));
  log_msg += " origin = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->origin)));
  log_msg += " region = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->region)));
  log_msg += " inputRowPitch = " + std::to_string(*(params->inputRowPitch));
  log_msg += " inputSlicePitch = " + std::to_string(*(params->inputSlicePitch));
  log_msg += " ptr = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->ptr)));
  log_msg += " numEventsInWaitList = " + std::to_string(*(params->numEventsInWaitList));
  log_msg += " eventWaitList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->eventWaitList)));
  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueWriteImageOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueWaitForEventsOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueWaitForEvents* params =
    reinterpret_cast<const cl_params_clEnqueueWaitForEvents*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += " numEvents = " + std::to_string(*(params->numEvents));
  log_msg += " eventList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->eventList)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueWaitForEventsOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueSVMUnmapOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueSVMUnmap* params =
    reinterpret_cast<const cl_params_clEnqueueSVMUnmap*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += " svmPtr = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->svmPtr)));
  log_msg += " numEventsInWaitList = " + std::to_string(*(params->numEventsInWaitList));
  log_msg += " eventWaitList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->eventWaitList)));
  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueSVMUnmapOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clCreateProgramWithBinaryOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateProgramWithBinary* params =
    reinterpret_cast<const cl_params_clCreateProgramWithBinary*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " context = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->context)));
  log_msg += " numDevices = " + std::to_string(*(params->numDevices));
  log_msg += " deviceList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->deviceList)));
  log_msg += " lengths = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->lengths)));
  log_msg += " binaries = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->binaries)));
  log_msg += " binaryStatus = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->binaryStatus)));
  log_msg += " errcodeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->errcodeRet)));
  log_msg += "\n";

  collector->Log(log_msg);

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateProgramWithBinaryOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  const cl_params_clCreateProgramWithBinary* params =
    reinterpret_cast<const cl_params_clCreateProgramWithBinary*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_program* result =
    reinterpret_cast<cl_program*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  log_msg += " result = " + ToHexString(reinterpret_cast<uintptr_t>(*result));

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(**(params->errcodeRet));
  log_msg += " (" + std::to_string(**(params->errcodeRet)) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueFillImageOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueFillImage* params =
    reinterpret_cast<const cl_params_clEnqueueFillImage*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += " image = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->image)));
  log_msg += " fillColor = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->fillColor)));
  log_msg += " origin = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->origin)));
  log_msg += " region = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->region)));
  log_msg += " numEventsInWaitList = " + std::to_string(*(params->numEventsInWaitList));
  log_msg += " eventWaitList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->eventWaitList)));
  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueFillImageOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clCreateFromGLTexture2DOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateFromGLTexture2D* params =
    reinterpret_cast<const cl_params_clCreateFromGLTexture2D*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " context = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->context)));
  log_msg += " flags = " + std::to_string(*(params->flags));
  log_msg += " target = " + std::to_string(*(params->target));
  log_msg += " miplevel = " + std::to_string(*(params->miplevel));
  log_msg += " texture = " + std::to_string(*(params->texture));
  log_msg += " errcodeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->errcodeRet)));
  log_msg += "\n";

  collector->Log(log_msg);

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateFromGLTexture2DOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  const cl_params_clCreateFromGLTexture2D* params =
    reinterpret_cast<const cl_params_clCreateFromGLTexture2D*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_mem* result =
    reinterpret_cast<cl_mem*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  log_msg += " result = " + ToHexString(reinterpret_cast<uintptr_t>(*result));

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(**(params->errcodeRet));
  log_msg += " (" + std::to_string(**(params->errcodeRet)) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clSetKernelExecInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clSetKernelExecInfo* params =
    reinterpret_cast<const cl_params_clSetKernelExecInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " kernel = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->kernel)));
  log_msg += " paramName = " + std::to_string(*(params->paramName));
  log_msg += " paramValueSize = " + std::to_string(*(params->paramValueSize));
  log_msg += " paramValue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->paramValue)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clSetKernelExecInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueReleaseGLObjectsOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueReleaseGLObjects* params =
    reinterpret_cast<const cl_params_clEnqueueReleaseGLObjects*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += " numObjects = " + std::to_string(*(params->numObjects));
  log_msg += " memObjects = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->memObjects)));
  log_msg += " numEventsInWaitList = " + std::to_string(*(params->numEventsInWaitList));
  log_msg += " eventWaitList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->eventWaitList)));
  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueReleaseGLObjectsOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetDeviceIDsOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetDeviceIDs* params =
    reinterpret_cast<const cl_params_clGetDeviceIDs*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " platform = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->platform)));
  log_msg += " deviceType = " + ToHexString(static_cast<uintptr_t>(*(params->deviceType)));
  log_msg += " numEntries = " + std::to_string(*(params->numEntries));
  log_msg += " devices = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->devices)));
  log_msg += " numDevices = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->numDevices)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetDeviceIDsOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clReleaseMemObjectOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clReleaseMemObject* params =
    reinterpret_cast<const cl_params_clReleaseMemObject*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " memobj = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->memobj)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clReleaseMemObjectOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetGLObjectInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetGLObjectInfo* params =
    reinterpret_cast<const cl_params_clGetGLObjectInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " memobj = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->memobj)));
  log_msg += " glObjectType = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->glObjectType)));
  log_msg += " glObjectName = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->glObjectName)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetGLObjectInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clCreateFromGLRenderbufferOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateFromGLRenderbuffer* params =
    reinterpret_cast<const cl_params_clCreateFromGLRenderbuffer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " context = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->context)));
  log_msg += " flags = " + std::to_string(*(params->flags));
  log_msg += " renderbuffer = " + std::to_string(*(params->renderbuffer));
  log_msg += " errcodeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->errcodeRet)));
  log_msg += "\n";

  collector->Log(log_msg);

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateFromGLRenderbufferOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  const cl_params_clCreateFromGLRenderbuffer* params =
    reinterpret_cast<const cl_params_clCreateFromGLRenderbuffer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_mem* result =
    reinterpret_cast<cl_mem*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  log_msg += " result = " + ToHexString(reinterpret_cast<uintptr_t>(*result));

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(**(params->errcodeRet));
  log_msg += " (" + std::to_string(**(params->errcodeRet)) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clReleaseContextOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clReleaseContext* params =
    reinterpret_cast<const cl_params_clReleaseContext*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " context = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->context)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clReleaseContextOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueUnmapMemObjectOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueUnmapMemObject* params =
    reinterpret_cast<const cl_params_clEnqueueUnmapMemObject*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += " memobj = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->memobj)));
  log_msg += " mappedPtr = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->mappedPtr)));
  log_msg += " numEventsInWaitList = " + std::to_string(*(params->numEventsInWaitList));
  log_msg += " eventWaitList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->eventWaitList)));
  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueUnmapMemObjectOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clCreateContextOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateContext* params =
    reinterpret_cast<const cl_params_clCreateContext*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " properties = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->properties)));
  log_msg += " numDevices = " + std::to_string(*(params->numDevices));
  log_msg += " devices = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->devices)));
  log_msg += " funcNotify = " + ToHexString(reinterpret_cast<uintptr_t>(*reinterpret_cast<decltype(params->funcNotify)*>(params->funcNotify)));
  log_msg += " userData = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->userData)));
  log_msg += " errcodeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->errcodeRet)));
  log_msg += "\n";

  collector->Log(log_msg);

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateContextOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  const cl_params_clCreateContext* params =
    reinterpret_cast<const cl_params_clCreateContext*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_context* result =
    reinterpret_cast<cl_context*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  log_msg += " result = " + ToHexString(reinterpret_cast<uintptr_t>(*result));

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(**(params->errcodeRet));
  log_msg += " (" + std::to_string(**(params->errcodeRet)) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetHostTimerOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetHostTimer* params =
    reinterpret_cast<const cl_params_clGetHostTimer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " device = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->device)));
  log_msg += " hostTimestamp = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->hostTimestamp)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetHostTimerOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetPipeInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetPipeInfo* params =
    reinterpret_cast<const cl_params_clGetPipeInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " pipe = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->pipe)));
  log_msg += " paramName = " + std::to_string(*(params->paramName));
  log_msg += " paramValueSize = " + std::to_string(*(params->paramValueSize));
  log_msg += " paramValue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->paramValue)));
  log_msg += " paramValueSizeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->paramValueSizeRet)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetPipeInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueAcquireGLObjectsOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueAcquireGLObjects* params =
    reinterpret_cast<const cl_params_clEnqueueAcquireGLObjects*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += " numObjects = " + std::to_string(*(params->numObjects));
  log_msg += " memObjects = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->memObjects)));
  log_msg += " numEventsInWaitList = " + std::to_string(*(params->numEventsInWaitList));
  log_msg += " eventWaitList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->eventWaitList)));
  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueAcquireGLObjectsOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetKernelWorkGroupInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetKernelWorkGroupInfo* params =
    reinterpret_cast<const cl_params_clGetKernelWorkGroupInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " kernel = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->kernel)));
  log_msg += " device = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->device)));
  log_msg += " paramName = " + std::to_string(*(params->paramName));
  log_msg += " paramValueSize = " + std::to_string(*(params->paramValueSize));
  log_msg += " paramValue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->paramValue)));
  log_msg += " paramValueSizeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->paramValueSizeRet)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetKernelWorkGroupInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clCreateImage2DOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateImage2D* params =
    reinterpret_cast<const cl_params_clCreateImage2D*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " context = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->context)));
  log_msg += " flags = " + std::to_string(*(params->flags));
  log_msg += " imageFormat = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->imageFormat)));
  log_msg += " imageWidth = " + ToHexString(static_cast<uintptr_t>(*(params->imageWidth)));
  log_msg += " imageHeight = " + ToHexString(static_cast<uintptr_t>(*(params->imageHeight)));
  log_msg += " imageRowPitch = " + ToHexString(static_cast<uintptr_t>(*(params->imageRowPitch)));
  log_msg += " hostPtr = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->hostPtr)));
  log_msg += " errcodeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->errcodeRet)));
  log_msg += "\n";

  collector->Log(log_msg);

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateImage2DOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  const cl_params_clCreateImage2D* params =
    reinterpret_cast<const cl_params_clCreateImage2D*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_mem* result =
    reinterpret_cast<cl_mem*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  log_msg += " result = " + ToHexString(reinterpret_cast<uintptr_t>(*result));

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(**(params->errcodeRet));
  log_msg += " (" + std::to_string(**(params->errcodeRet)) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clCreateContextFromTypeOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateContextFromType* params =
    reinterpret_cast<const cl_params_clCreateContextFromType*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " properties = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->properties)));
  log_msg += " deviceType = " + ToHexString(static_cast<uintptr_t>(*(params->deviceType)));
  log_msg += " funcNotify = " + ToHexString(reinterpret_cast<uintptr_t>(*reinterpret_cast<decltype(params->funcNotify)*>(params->funcNotify)));
  log_msg += " userData = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->userData)));
  log_msg += " errcodeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->errcodeRet)));
  log_msg += "\n";

  collector->Log(log_msg);

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateContextFromTypeOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  const cl_params_clCreateContextFromType* params =
    reinterpret_cast<const cl_params_clCreateContextFromType*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_context* result =
    reinterpret_cast<cl_context*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  log_msg += " result = " + ToHexString(reinterpret_cast<uintptr_t>(*result));

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(**(params->errcodeRet));
  log_msg += " (" + std::to_string(**(params->errcodeRet)) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clRetainProgramOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clRetainProgram* params =
    reinterpret_cast<const cl_params_clRetainProgram*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " program = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->program)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clRetainProgramOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clCreateProgramWithSourceOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateProgramWithSource* params =
    reinterpret_cast<const cl_params_clCreateProgramWithSource*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " context = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->context)));
  log_msg += " count = " + std::to_string(*(params->count));
  log_msg += " strings = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->strings)));
  log_msg += " lengths = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->lengths)));
  log_msg += " errcodeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->errcodeRet)));
  log_msg += "\n";

  collector->Log(log_msg);

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateProgramWithSourceOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  const cl_params_clCreateProgramWithSource* params =
    reinterpret_cast<const cl_params_clCreateProgramWithSource*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_program* result =
    reinterpret_cast<cl_program*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  log_msg += " result = " + ToHexString(reinterpret_cast<uintptr_t>(*result));

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(**(params->errcodeRet));
  log_msg += " (" + std::to_string(**(params->errcodeRet)) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetMemObjectInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetMemObjectInfo* params =
    reinterpret_cast<const cl_params_clGetMemObjectInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " memobj = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->memobj)));
  log_msg += " paramName = " + std::to_string(*(params->paramName));
  log_msg += " paramValueSize = " + std::to_string(*(params->paramValueSize));
  log_msg += " paramValue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->paramValue)));
  log_msg += " paramValueSizeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->paramValueSizeRet)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetMemObjectInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clLinkProgramOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clLinkProgram* params =
    reinterpret_cast<const cl_params_clLinkProgram*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " context = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->context)));
  log_msg += " numDevices = " + std::to_string(*(params->numDevices));
  log_msg += " deviceList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->deviceList)));
  if (*(params->options) == nullptr) {
    log_msg += " options = 0";
  } else if (strlen(*(params->options)) == 0) {
    log_msg += " options = \"\"";
  } else {
    log_msg += " options = \"" + std::string(*(params->options)) + "\"";
  }
  log_msg += " numInputPrograms = " + std::to_string(*(params->numInputPrograms));
  log_msg += " inputPrograms = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->inputPrograms)));
  log_msg += " funcNotify = " + ToHexString(reinterpret_cast<uintptr_t>(*reinterpret_cast<decltype(params->funcNotify)*>(params->funcNotify)));
  log_msg += " userData = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->userData)));
  log_msg += " errcodeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->errcodeRet)));
  log_msg += "\n";

  collector->Log(log_msg);

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clLinkProgramOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  const cl_params_clLinkProgram* params =
    reinterpret_cast<const cl_params_clLinkProgram*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_program* result =
    reinterpret_cast<cl_program*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  log_msg += " result = " + ToHexString(reinterpret_cast<uintptr_t>(*result));

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(**(params->errcodeRet));
  log_msg += " (" + std::to_string(**(params->errcodeRet)) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clCreateSamplerWithPropertiesOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateSamplerWithProperties* params =
    reinterpret_cast<const cl_params_clCreateSamplerWithProperties*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " context = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->context)));
  log_msg += " samplerProperties = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->samplerProperties)));
  log_msg += " errcodeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->errcodeRet)));
  log_msg += "\n";

  collector->Log(log_msg);

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateSamplerWithPropertiesOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  const cl_params_clCreateSamplerWithProperties* params =
    reinterpret_cast<const cl_params_clCreateSamplerWithProperties*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_sampler* result =
    reinterpret_cast<cl_sampler*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  log_msg += " result = " + ToHexString(reinterpret_cast<uintptr_t>(*result));

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(**(params->errcodeRet));
  log_msg += " (" + std::to_string(**(params->errcodeRet)) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clRetainSamplerOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clRetainSampler* params =
    reinterpret_cast<const cl_params_clRetainSampler*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " sampler = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->sampler)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clRetainSamplerOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clCreateFromGLTexture3DOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateFromGLTexture3D* params =
    reinterpret_cast<const cl_params_clCreateFromGLTexture3D*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " context = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->context)));
  log_msg += " flags = " + std::to_string(*(params->flags));
  log_msg += " target = " + std::to_string(*(params->target));
  log_msg += " miplevel = " + std::to_string(*(params->miplevel));
  log_msg += " texture = " + std::to_string(*(params->texture));
  log_msg += " errcodeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->errcodeRet)));
  log_msg += "\n";

  collector->Log(log_msg);

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateFromGLTexture3DOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  const cl_params_clCreateFromGLTexture3D* params =
    reinterpret_cast<const cl_params_clCreateFromGLTexture3D*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_mem* result =
    reinterpret_cast<cl_mem*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  log_msg += " result = " + ToHexString(reinterpret_cast<uintptr_t>(*result));

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(**(params->errcodeRet));
  log_msg += " (" + std::to_string(**(params->errcodeRet)) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueMapImageOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueMapImage* params =
    reinterpret_cast<const cl_params_clEnqueueMapImage*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += " image = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->image)));
  log_msg += " blockingMap = " + std::to_string(*(params->blockingMap));
  log_msg += " mapFlags = " + std::to_string(*(params->mapFlags));
  log_msg += " origin = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->origin)));
  log_msg += " region = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->region)));
  log_msg += " imageRowPitch = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->imageRowPitch)));
  log_msg += " imageSlicePitch = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->imageSlicePitch)));
  log_msg += " numEventsInWaitList = " + std::to_string(*(params->numEventsInWaitList));
  log_msg += " eventWaitList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->eventWaitList)));
  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += " errcodeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->errcodeRet)));
  log_msg += "\n";

  collector->Log(log_msg);

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clEnqueueMapImageOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  const cl_params_clEnqueueMapImage* params =
    reinterpret_cast<const cl_params_clEnqueueMapImage*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  void ** result =
    reinterpret_cast<void **>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  log_msg += " result = " + ToHexString(reinterpret_cast<uintptr_t>(*result));

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(**(params->errcodeRet));
  log_msg += " (" + std::to_string(**(params->errcodeRet)) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueWriteBufferOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueWriteBuffer* params =
    reinterpret_cast<const cl_params_clEnqueueWriteBuffer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += " buffer = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->buffer)));
  log_msg += " blockingWrite = " + std::to_string(*(params->blockingWrite));
  log_msg += " offset = " + std::to_string(*(params->offset));
  log_msg += " cb = " + std::to_string(*(params->cb));
  log_msg += " ptr = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->ptr)));
  log_msg += " numEventsInWaitList = " + std::to_string(*(params->numEventsInWaitList));
  log_msg += " eventWaitList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->eventWaitList)));
  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueWriteBufferOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);

  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueCopyImageOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueCopyImage* params =
    reinterpret_cast<const cl_params_clEnqueueCopyImage*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += " srcImage = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->srcImage)));
  log_msg += " dstImage = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->dstImage)));
  log_msg += " srcOrigin = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->srcOrigin)));
  log_msg += " dstOrigin = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->dstOrigin)));
  log_msg += " region = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->region)));
  log_msg += " numEventsInWaitList = " + std::to_string(*(params->numEventsInWaitList));
  log_msg += " eventWaitList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->eventWaitList)));
  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueCopyImageOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetExtensionFunctionAddressOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetExtensionFunctionAddress* params =
    reinterpret_cast<const cl_params_clGetExtensionFunctionAddress*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  if (*(params->funcName) == nullptr) {
    log_msg += " funcName = 0";
  } else if (strlen(*(params->funcName)) == 0) {
    log_msg += " funcName = \"\"";
  } else {
    log_msg += " funcName = \"" + std::string(*(params->funcName)) + "\"";
  }
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetExtensionFunctionAddressOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  void** result =
    reinterpret_cast<void**>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  log_msg += " result = " + ToHexString(reinterpret_cast<uintptr_t>(*result));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueReadBufferRectOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueReadBufferRect* params =
    reinterpret_cast<const cl_params_clEnqueueReadBufferRect*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += " buffer = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->buffer)));
  log_msg += " blockingRead = " + std::to_string(*(params->blockingRead));
  log_msg += " bufferOrigin = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->bufferOrigin)));
  log_msg += " hostOrigin = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->hostOrigin)));
  log_msg += " region = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->region)));
  log_msg += " bufferRowPitch = " + ToHexString(static_cast<uintptr_t>(*(params->bufferRowPitch)));
  log_msg += " bufferSlicePitch = " + ToHexString(static_cast<uintptr_t>(*(params->bufferSlicePitch)));
  log_msg += " hostRowPitch = " + std::to_string(*(params->hostRowPitch));
  log_msg += " hostSlicePitch = " + std::to_string(*(params->hostSlicePitch));
  log_msg += " ptr = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->ptr)));
  log_msg += " numEventsInWaitList = " + std::to_string(*(params->numEventsInWaitList));
  log_msg += " eventWaitList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->eventWaitList)));
  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueReadBufferRectOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clCreateSubDevicesOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateSubDevices* params =
    reinterpret_cast<const cl_params_clCreateSubDevices*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " inDevice = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->inDevice)));
  log_msg += " properties = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->properties)));
  log_msg += " numDevices = " + std::to_string(*(params->numDevices));
  log_msg += " outDevices = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->outDevices)));
  log_msg += " numDevicesRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->numDevicesRet)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clCreateSubDevicesOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetDeviceAndHostTimerOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetDeviceAndHostTimer* params =
    reinterpret_cast<const cl_params_clGetDeviceAndHostTimer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " device = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->device)));
  log_msg += " deviceTimestamp = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->deviceTimestamp)));
  log_msg += " hostTimestamp = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->hostTimestamp)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetDeviceAndHostTimerOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clReleaseSamplerOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clReleaseSampler* params =
    reinterpret_cast<const cl_params_clReleaseSampler*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " sampler = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->sampler)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clReleaseSamplerOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueTaskOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueTask* params =
    reinterpret_cast<const cl_params_clEnqueueTask*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += " kernel = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->kernel)));
  if (*(params->kernel) != nullptr) {
    std::string kernel_name = utils::cl::GetKernelName(
        *(params->kernel), collector->Demangle());
    if (!kernel_name.empty()) {
      log_msg += " (" + kernel_name + ")";
    }
  }
  log_msg += " numEventsInWaitList = " + std::to_string(*(params->numEventsInWaitList));
  log_msg += " eventWaitList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->eventWaitList)));
  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueTaskOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clFinishOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clFinish* params =
    reinterpret_cast<const cl_params_clFinish*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clFinishOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetEventInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetEventInfo* params =
    reinterpret_cast<const cl_params_clGetEventInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += " paramName = " + std::to_string(*(params->paramName));
  log_msg += " paramValueSize = " + std::to_string(*(params->paramValueSize));
  log_msg += " paramValue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->paramValue)));
  log_msg += " paramValueSizeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->paramValueSizeRet)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetEventInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetEventProfilingInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetEventProfilingInfo* params =
    reinterpret_cast<const cl_params_clGetEventProfilingInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += " paramName = " + std::to_string(*(params->paramName));
  log_msg += " paramValueSize = " + std::to_string(*(params->paramValueSize));
  log_msg += " paramValue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->paramValue)));
  log_msg += " paramValueSizeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->paramValueSizeRet)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetEventProfilingInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clSetKernelArgSVMPointerOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clSetKernelArgSVMPointer* params =
    reinterpret_cast<const cl_params_clSetKernelArgSVMPointer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " kernel = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->kernel)));
  log_msg += " argIndex = " + std::to_string(*(params->argIndex));
  log_msg += " argValue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->argValue)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clSetKernelArgSVMPointerOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clCreateImageOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateImage* params =
    reinterpret_cast<const cl_params_clCreateImage*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " context = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->context)));
  log_msg += " flags = " + std::to_string(*(params->flags));
  log_msg += " imageFormat = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->imageFormat)));
  log_msg += " imageDesc = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->imageDesc)));
  log_msg += " hostPtr = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->hostPtr)));
  log_msg += " errcodeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->errcodeRet)));
  log_msg += "\n";

  collector->Log(log_msg);

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateImageOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  const cl_params_clCreateImage* params =
    reinterpret_cast<const cl_params_clCreateImage*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_mem* result =
    reinterpret_cast<cl_mem*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  log_msg += " result = " + ToHexString(reinterpret_cast<uintptr_t>(*result));

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(**(params->errcodeRet));
  log_msg += " (" + std::to_string(**(params->errcodeRet)) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueSVMMemcpyOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueSVMMemcpy* params =
    reinterpret_cast<const cl_params_clEnqueueSVMMemcpy*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += " blockingCopy = " + std::to_string(*(params->blockingCopy));
  log_msg += " dstPtr = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->dstPtr)));
  log_msg += " srcPtr = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->srcPtr)));
  log_msg += " size = " + std::to_string(*(params->size));
  log_msg += " numEventsInWaitList = " + std::to_string(*(params->numEventsInWaitList));
  log_msg += " eventWaitList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->eventWaitList)));
  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueSVMMemcpyOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clReleaseKernelOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clReleaseKernel* params =
    reinterpret_cast<const cl_params_clReleaseKernel*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " kernel = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->kernel)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clReleaseKernelOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueNativeKernelOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueNativeKernel* params =
    reinterpret_cast<const cl_params_clEnqueueNativeKernel*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += " userFunc = " + ToHexString(reinterpret_cast<uintptr_t>(*reinterpret_cast<decltype(params->userFunc)*>(params->userFunc)));
  log_msg += " args = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->args)));
  log_msg += " cbArgs = " + std::to_string(*(params->cbArgs));
  log_msg += " numMemObjects = " + std::to_string(*(params->numMemObjects));
  log_msg += " memList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->memList)));
  log_msg += " argsMemLoc = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->argsMemLoc)));
  log_msg += " numEventsInWaitList = " + std::to_string(*(params->numEventsInWaitList));
  log_msg += " eventWaitList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->eventWaitList)));
  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueNativeKernelOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clCreateKernelsInProgramOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateKernelsInProgram* params =
    reinterpret_cast<const cl_params_clCreateKernelsInProgram*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " program = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->program)));
  log_msg += " numKernels = " + std::to_string(*(params->numKernels));
  log_msg += " kernels = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->kernels)));
  log_msg += " numKernelsRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->numKernelsRet)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clCreateKernelsInProgramOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clSetCommandQueuePropertyOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clSetCommandQueueProperty* params =
    reinterpret_cast<const cl_params_clSetCommandQueueProperty*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += " properties = " + ToHexString(static_cast<uintptr_t>(*(params->properties)));
  log_msg += " enable = " + std::to_string(*(params->enable));
  log_msg += " oldProperties = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->oldProperties)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clSetCommandQueuePropertyOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetDeviceInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetDeviceInfo* params =
    reinterpret_cast<const cl_params_clGetDeviceInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " device = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->device)));
  log_msg += " paramName = " + std::to_string(*(params->paramName));
  log_msg += " paramValueSize = " + std::to_string(*(params->paramValueSize));
  log_msg += " paramValue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->paramValue)));
  log_msg += " paramValueSizeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->paramValueSizeRet)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetDeviceInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueNDRangeKernelOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueNDRangeKernel* params =
    reinterpret_cast<const cl_params_clEnqueueNDRangeKernel*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += " kernel = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->kernel)));
  if (*(params->kernel) != nullptr) {
    std::string kernel_name = utils::cl::GetKernelName(
        *(params->kernel), collector->Demangle());
    if (!kernel_name.empty()) {
      log_msg += " (" + kernel_name + ")";
    }
  }
  log_msg += " workDim = " + std::to_string(*(params->workDim));
  log_msg += " globalWorkOffset = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->globalWorkOffset)));
  if (*(params->globalWorkOffset) != nullptr && *(params->workDim) > 0) {
    log_msg += " {" + std::to_string((*(params->globalWorkOffset))[0]);
    for (cl_uint i = 1; i < *(params->workDim); ++i) {
      log_msg += ", " + std::to_string((*(params->globalWorkOffset))[i]);
    }
    log_msg += "}";
  }
  log_msg += " globalWorkSize = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->globalWorkSize)));
  if (*(params->globalWorkSize) != nullptr && *(params->workDim) > 0) {
    log_msg += " {" + std::to_string((*(params->globalWorkSize))[0]);
    for (cl_uint i = 1; i < *(params->workDim); ++i) {
      log_msg += ", " + std::to_string((*(params->globalWorkSize))[i]);
    }
    log_msg += "}";
  }
  log_msg += " localWorkSize = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->localWorkSize)));
  if (*(params->localWorkSize) != nullptr && *(params->workDim) > 0) {
    log_msg += " {" + std::to_string((*(params->localWorkSize))[0]);
    for (cl_uint i = 1; i < *(params->workDim); ++i) {
      log_msg += ", " + std::to_string((*(params->localWorkSize))[i]);
    }
    log_msg += "}";
  }
  log_msg += " numEventsInWaitList = " + std::to_string(*(params->numEventsInWaitList));
  log_msg += " eventWaitList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->eventWaitList)));
  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueNDRangeKernelOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);

  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clReleaseProgramOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clReleaseProgram* params =
    reinterpret_cast<const cl_params_clReleaseProgram*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " program = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->program)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clReleaseProgramOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clCreateFromGLBufferOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateFromGLBuffer* params =
    reinterpret_cast<const cl_params_clCreateFromGLBuffer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " context = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->context)));
  log_msg += " flags = " + std::to_string(*(params->flags));
  log_msg += " bufobj = " + std::to_string(*(params->bufobj));
  log_msg += " errcodeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->errcodeRet)));
  log_msg += "\n";

  collector->Log(log_msg);

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateFromGLBufferOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  const cl_params_clCreateFromGLBuffer* params =
    reinterpret_cast<const cl_params_clCreateFromGLBuffer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_mem* result =
    reinterpret_cast<cl_mem*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  log_msg += " result = " + ToHexString(reinterpret_cast<uintptr_t>(*result));

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(**(params->errcodeRet));
  log_msg += " (" + std::to_string(**(params->errcodeRet)) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetGLTextureInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetGLTextureInfo* params =
    reinterpret_cast<const cl_params_clGetGLTextureInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " memobj = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->memobj)));
  log_msg += " paramName = " + std::to_string(*(params->paramName));
  log_msg += " paramValueSize = " + std::to_string(*(params->paramValueSize));
  log_msg += " paramValue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->paramValue)));
  log_msg += " paramValueSizeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->paramValueSizeRet)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetGLTextureInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clSetDefaultDeviceCommandQueueOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clSetDefaultDeviceCommandQueue* params =
    reinterpret_cast<const cl_params_clSetDefaultDeviceCommandQueue*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " context = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->context)));
  log_msg += " device = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->device)));
  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clSetDefaultDeviceCommandQueueOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clCreatePipeOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreatePipe* params =
    reinterpret_cast<const cl_params_clCreatePipe*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " context = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->context)));
  log_msg += " flags = " + std::to_string(*(params->flags));
  log_msg += " pipePacketSize = " + std::to_string(*(params->pipePacketSize));
  log_msg += " pipeMaxPackets = " + std::to_string(*(params->pipeMaxPackets));
  log_msg += " properties = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->properties)));
  log_msg += " errcodeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->errcodeRet)));
  log_msg += "\n";

  collector->Log(log_msg);

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreatePipeOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  const cl_params_clCreatePipe* params =
    reinterpret_cast<const cl_params_clCreatePipe*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_mem* result =
    reinterpret_cast<cl_mem*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  log_msg += " result = " + ToHexString(reinterpret_cast<uintptr_t>(*result));

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(**(params->errcodeRet));
  log_msg += " (" + std::to_string(**(params->errcodeRet)) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetPlatformInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetPlatformInfo* params =
    reinterpret_cast<const cl_params_clGetPlatformInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " platform = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->platform)));
  log_msg += " paramName = " + std::to_string(*(params->paramName));
  log_msg += " paramValueSize = " + std::to_string(*(params->paramValueSize));
  log_msg += " paramValue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->paramValue)));
  log_msg += " paramValueSizeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->paramValueSizeRet)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetPlatformInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueReadBufferOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueReadBuffer* params =
    reinterpret_cast<const cl_params_clEnqueueReadBuffer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += " buffer = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->buffer)));
  log_msg += " blockingRead = " + std::to_string(*(params->blockingRead));
  log_msg += " offset = " + std::to_string(*(params->offset));
  log_msg += " cb = " + std::to_string(*(params->cb));
  log_msg += " ptr = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->ptr)));
  log_msg += " numEventsInWaitList = " + std::to_string(*(params->numEventsInWaitList));
  log_msg += " eventWaitList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->eventWaitList)));
  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueReadBufferOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);

  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clSetMemObjectDestructorCallbackOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clSetMemObjectDestructorCallback* params =
    reinterpret_cast<const cl_params_clSetMemObjectDestructorCallback*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " memobj = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->memobj)));
  log_msg += " funcNotify = " + ToHexString(reinterpret_cast<uintptr_t>(*reinterpret_cast<decltype(params->funcNotify)*>(params->funcNotify)));
  log_msg += " userData = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->userData)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clSetMemObjectDestructorCallbackOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetKernelSubGroupInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetKernelSubGroupInfo* params =
    reinterpret_cast<const cl_params_clGetKernelSubGroupInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " kernel = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->kernel)));
  log_msg += " device = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->device)));
  log_msg += " paramName = " + std::to_string(*(params->paramName));
  log_msg += " inputValueSize = " + std::to_string(*(params->inputValueSize));
  log_msg += " inputValue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->inputValue)));
  log_msg += " paramValueSize = " + std::to_string(*(params->paramValueSize));
  log_msg += " paramValue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->paramValue)));
  log_msg += " paramValueSizeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->paramValueSizeRet)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetKernelSubGroupInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueCopyBufferRectOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueCopyBufferRect* params =
    reinterpret_cast<const cl_params_clEnqueueCopyBufferRect*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += " srcBuffer = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->srcBuffer)));
  log_msg += " dstBuffer = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->dstBuffer)));
  log_msg += " srcOrigin = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->srcOrigin)));
  log_msg += " dstOrigin = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->dstOrigin)));
  log_msg += " region = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->region)));
  log_msg += " srcRowPitch = " + std::to_string(*(params->srcRowPitch));
  log_msg += " srcSlicePitch = " + std::to_string(*(params->srcSlicePitch));
  log_msg += " dstRowPitch = " + std::to_string(*(params->dstRowPitch));
  log_msg += " dstSlicePitch = " + std::to_string(*(params->dstSlicePitch));
  log_msg += " numEventsInWaitList = " + std::to_string(*(params->numEventsInWaitList));
  log_msg += " eventWaitList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->eventWaitList)));
  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueCopyBufferRectOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clWaitForEventsOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clWaitForEvents* params =
    reinterpret_cast<const cl_params_clWaitForEvents*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " numEvents = " + std::to_string(*(params->numEvents));
  log_msg += " eventList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->eventList)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clWaitForEventsOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueSVMMigrateMemOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueSVMMigrateMem* params =
    reinterpret_cast<const cl_params_clEnqueueSVMMigrateMem*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += " numSvmPointers = " + std::to_string(*(params->numSvmPointers));
  log_msg += " svmPointers = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->svmPointers)));
  log_msg += " sizes = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->sizes)));
  log_msg += " flags = " + std::to_string(*(params->flags));
  log_msg += " numEventsInWaitList = " + std::to_string(*(params->numEventsInWaitList));
  log_msg += " eventWaitList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->eventWaitList)));
  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueSVMMigrateMemOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clRetainKernelOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clRetainKernel* params =
    reinterpret_cast<const cl_params_clRetainKernel*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " kernel = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->kernel)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clRetainKernelOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clCreateCommandQueueWithPropertiesOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateCommandQueueWithProperties* params =
    reinterpret_cast<const cl_params_clCreateCommandQueueWithProperties*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " context = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->context)));
  log_msg += " device = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->device)));
  log_msg += " properties = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->properties)));
  log_msg += " errcodeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->errcodeRet)));
  log_msg += "\n";

  collector->Log(log_msg);

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateCommandQueueWithPropertiesOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  const cl_params_clCreateCommandQueueWithProperties* params =
    reinterpret_cast<const cl_params_clCreateCommandQueueWithProperties*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_command_queue* result =
    reinterpret_cast<cl_command_queue*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  log_msg += " result = " + ToHexString(reinterpret_cast<uintptr_t>(*result));

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(**(params->errcodeRet));
  log_msg += " (" + std::to_string(**(params->errcodeRet)) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clCreateProgramWithBuiltInKernelsOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateProgramWithBuiltInKernels* params =
    reinterpret_cast<const cl_params_clCreateProgramWithBuiltInKernels*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " context = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->context)));
  log_msg += " numDevices = " + std::to_string(*(params->numDevices));
  log_msg += " deviceList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->deviceList)));
  if (*(params->kernelNames) == nullptr) {
    log_msg += " kernelNames = 0";
  } else if (strlen(*(params->kernelNames)) == 0) {
    log_msg += " kernelNames = \"\"";
  } else {
    log_msg += " kernelNames = \"" + std::string(*(params->kernelNames)) + "\"";
  }
  log_msg += " errcodeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->errcodeRet)));
  log_msg += "\n";

  collector->Log(log_msg);

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateProgramWithBuiltInKernelsOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  const cl_params_clCreateProgramWithBuiltInKernels* params =
    reinterpret_cast<const cl_params_clCreateProgramWithBuiltInKernels*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_program* result =
    reinterpret_cast<cl_program*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  log_msg += " result = " + ToHexString(reinterpret_cast<uintptr_t>(*result));

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(**(params->errcodeRet));
  log_msg += " (" + std::to_string(**(params->errcodeRet)) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clCreateBufferOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateBuffer* params =
    reinterpret_cast<const cl_params_clCreateBuffer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " context = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->context)));
  log_msg += " flags = " + std::to_string(*(params->flags));
  log_msg += " size = " + std::to_string(*(params->size));
  log_msg += " hostPtr = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->hostPtr)));
  log_msg += " errcodeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->errcodeRet)));
  log_msg += "\n";

  collector->Log(log_msg);

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateBufferOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  const cl_params_clCreateBuffer* params =
    reinterpret_cast<const cl_params_clCreateBuffer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_mem* result =
    reinterpret_cast<cl_mem*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  log_msg += " result = " + ToHexString(reinterpret_cast<uintptr_t>(*result));

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(**(params->errcodeRet));
  log_msg += " (" + std::to_string(**(params->errcodeRet)) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetProgramBuildInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetProgramBuildInfo* params =
    reinterpret_cast<const cl_params_clGetProgramBuildInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " program = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->program)));
  log_msg += " device = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->device)));
  log_msg += " paramName = " + std::to_string(*(params->paramName));
  log_msg += " paramValueSize = " + std::to_string(*(params->paramValueSize));
  log_msg += " paramValue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->paramValue)));
  log_msg += " paramValueSizeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->paramValueSizeRet)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetProgramBuildInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueFillBufferOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueFillBuffer* params =
    reinterpret_cast<const cl_params_clEnqueueFillBuffer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += " buffer = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->buffer)));
  log_msg += " pattern = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->pattern)));
  log_msg += " patternSize = " + std::to_string(*(params->patternSize));
  log_msg += " offset = " + std::to_string(*(params->offset));
  log_msg += " size = " + std::to_string(*(params->size));
  log_msg += " numEventsInWaitList = " + std::to_string(*(params->numEventsInWaitList));
  log_msg += " eventWaitList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->eventWaitList)));
  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueFillBufferOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueReadImageOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueReadImage* params =
    reinterpret_cast<const cl_params_clEnqueueReadImage*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += " image = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->image)));
  log_msg += " blockingRead = " + std::to_string(*(params->blockingRead));
  log_msg += " origin = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->origin)));
  log_msg += " region = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->region)));
  log_msg += " rowPitch = " + std::to_string(*(params->rowPitch));
  log_msg += " slicePitch = " + std::to_string(*(params->slicePitch));
  log_msg += " ptr = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->ptr)));
  log_msg += " numEventsInWaitList = " + std::to_string(*(params->numEventsInWaitList));
  log_msg += " eventWaitList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->eventWaitList)));
  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueReadImageOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueWriteBufferRectOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueWriteBufferRect* params =
    reinterpret_cast<const cl_params_clEnqueueWriteBufferRect*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += " buffer = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->buffer)));
  log_msg += " blockingWrite = " + std::to_string(*(params->blockingWrite));
  log_msg += " bufferOrigin = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->bufferOrigin)));
  log_msg += " hostOrigin = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->hostOrigin)));
  log_msg += " region = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->region)));
  log_msg += " bufferRowPitch = " + ToHexString(static_cast<uintptr_t>(*(params->bufferRowPitch)));
  log_msg += " bufferSlicePitch = " + ToHexString(static_cast<uintptr_t>(*(params->bufferSlicePitch)));
  log_msg += " hostRowPitch = " + std::to_string(*(params->hostRowPitch));
  log_msg += " hostSlicePitch = " + std::to_string(*(params->hostSlicePitch));
  log_msg += " ptr = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->ptr)));
  log_msg += " numEventsInWaitList = " + std::to_string(*(params->numEventsInWaitList));
  log_msg += " eventWaitList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->eventWaitList)));
  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueWriteBufferRectOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueCopyBufferToImageOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueCopyBufferToImage* params =
    reinterpret_cast<const cl_params_clEnqueueCopyBufferToImage*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += " srcBuffer = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->srcBuffer)));
  log_msg += " dstImage = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->dstImage)));
  log_msg += " srcOffset = " + std::to_string(*(params->srcOffset));
  log_msg += " dstOrigin = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->dstOrigin)));
  log_msg += " region = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->region)));
  log_msg += " numEventsInWaitList = " + std::to_string(*(params->numEventsInWaitList));
  log_msg += " eventWaitList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->eventWaitList)));
  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueCopyBufferToImageOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetExtensionFunctionAddressForPlatformOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetExtensionFunctionAddressForPlatform* params =
    reinterpret_cast<
      const cl_params_clGetExtensionFunctionAddressForPlatform*>(
          data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " platform = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->platform)));
  if (*(params->funcName) == nullptr) {
    log_msg += " funcName = 0";
  } else if (strlen(*(params->funcName)) == 0) {
    log_msg += " funcName = \"\"";
  } else {
    log_msg += " funcName = \"" + std::string(*(params->funcName)) + "\"";
  }
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetExtensionFunctionAddressForPlatformOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  void** result =
    reinterpret_cast<void**>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  log_msg += " result = " + ToHexString(reinterpret_cast<uintptr_t>(*result));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clSetKernelArgOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clSetKernelArg* params =
    reinterpret_cast<const cl_params_clSetKernelArg*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " kernel = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->kernel)));
  log_msg += " argIndex = " + std::to_string(*(params->argIndex));
  log_msg += " argSize = " + std::to_string(*(params->argSize));
  log_msg += " argValue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->argValue)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clSetKernelArgOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clReleaseDeviceOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clReleaseDevice* params =
    reinterpret_cast<const cl_params_clReleaseDevice*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " device = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->device)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clReleaseDeviceOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clCreateSubBufferOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateSubBuffer* params =
    reinterpret_cast<const cl_params_clCreateSubBuffer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " buffer = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->buffer)));
  log_msg += " flags = " + std::to_string(*(params->flags));
  log_msg += " bufferCreateType = " + std::to_string(*(params->bufferCreateType));
  log_msg += " bufferCreateInfo = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->bufferCreateInfo)));
  log_msg += " errcodeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->errcodeRet)));
  log_msg += "\n";

  collector->Log(log_msg);

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateSubBufferOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  const cl_params_clCreateSubBuffer* params =
    reinterpret_cast<const cl_params_clCreateSubBuffer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_mem* result =
    reinterpret_cast<cl_mem*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  log_msg += " result = " + ToHexString(reinterpret_cast<uintptr_t>(*result));

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(**(params->errcodeRet));
  log_msg += " (" + std::to_string(**(params->errcodeRet)) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueMigrateMemObjectsOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueMigrateMemObjects* params =
    reinterpret_cast<const cl_params_clEnqueueMigrateMemObjects*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += " numMemObjects = " + std::to_string(*(params->numMemObjects));
  log_msg += " memObjects = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->memObjects)));
  log_msg += " flags = " + std::to_string(*(params->flags));
  log_msg += " numEventsInWaitList = " + std::to_string(*(params->numEventsInWaitList));
  log_msg += " eventWaitList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->eventWaitList)));
  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueMigrateMemObjectsOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clCreateCommandQueueOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateCommandQueue* params =
    reinterpret_cast<const cl_params_clCreateCommandQueue*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " context = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->context)));
  log_msg += " device = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->device)));
  log_msg += " properties = " + ToHexString(static_cast<uintptr_t>(*(params->properties)));
  log_msg += " errcodeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->errcodeRet)));
  log_msg += "\n";

  collector->Log(log_msg);

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateCommandQueueOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  const cl_params_clCreateCommandQueue* params =
    reinterpret_cast<const cl_params_clCreateCommandQueue*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_command_queue* result =
    reinterpret_cast<cl_command_queue*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  log_msg += " result = " + ToHexString(reinterpret_cast<uintptr_t>(*result));

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(**(params->errcodeRet));
  log_msg += " (" + std::to_string(**(params->errcodeRet)) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueSVMMemFillOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueSVMMemFill* params =
    reinterpret_cast<const cl_params_clEnqueueSVMMemFill*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += " svmPtr = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->svmPtr)));
  log_msg += " pattern = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->pattern)));
  log_msg += " patternSize = " + std::to_string(*(params->patternSize));
  log_msg += " size = " + std::to_string(*(params->size));
  log_msg += " numEventsInWaitList = " + std::to_string(*(params->numEventsInWaitList));
  log_msg += " eventWaitList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->eventWaitList)));
  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueSVMMemFillOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clReleaseCommandQueueOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clReleaseCommandQueue* params =
    reinterpret_cast<const cl_params_clReleaseCommandQueue*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clReleaseCommandQueueOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueCopyBufferOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueCopyBuffer* params =
    reinterpret_cast<const cl_params_clEnqueueCopyBuffer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += " srcBuffer = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->srcBuffer)));
  log_msg += " dstBuffer = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->dstBuffer)));
  log_msg += " srcOffset = " + std::to_string(*(params->srcOffset));
  log_msg += " dstOffset = " + std::to_string(*(params->dstOffset));
  log_msg += " cb = " + std::to_string(*(params->cb));
  log_msg += " numEventsInWaitList = " + std::to_string(*(params->numEventsInWaitList));
  log_msg += " eventWaitList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->eventWaitList)));
  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueCopyBufferOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetCommandQueueInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetCommandQueueInfo* params =
    reinterpret_cast<const cl_params_clGetCommandQueueInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += " paramName = " + std::to_string(*(params->paramName));
  log_msg += " paramValueSize = " + std::to_string(*(params->paramValueSize));
  log_msg += " paramValue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->paramValue)));
  log_msg += " paramValueSizeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->paramValueSizeRet)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetCommandQueueInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clBuildProgramOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clBuildProgram* params =
    reinterpret_cast<const cl_params_clBuildProgram*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " program = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->program)));
  log_msg += " numDevices = " + std::to_string(*(params->numDevices));
  log_msg += " deviceList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->deviceList)));
  if (*(params->options) == nullptr) {
    log_msg += " options = 0";
  } else if (strlen(*(params->options)) == 0) {
    log_msg += " options = \"\"";
  } else {
    log_msg += " options = \"" + std::string(*(params->options)) + "\"";
  }
  log_msg += " funcNotify = " + ToHexString(reinterpret_cast<uintptr_t>(*reinterpret_cast<decltype(params->funcNotify)*>(params->funcNotify)));
  log_msg += " userData = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->userData)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clBuildProgramOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clRetainContextOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clRetainContext* params =
    reinterpret_cast<const cl_params_clRetainContext*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " context = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->context)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clRetainContextOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueBarrierOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueBarrier* params =
    reinterpret_cast<const cl_params_clEnqueueBarrier*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueBarrierOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clRetainDeviceOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clRetainDevice* params =
    reinterpret_cast<const cl_params_clRetainDevice*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " device = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->device)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clRetainDeviceOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueSVMMapOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueSVMMap* params =
    reinterpret_cast<const cl_params_clEnqueueSVMMap*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += " blockingMap = " + std::to_string(*(params->blockingMap));
  log_msg += " mapFlags = " + std::to_string(*(params->mapFlags));
  log_msg += " svmPtr = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->svmPtr)));
  log_msg += " size = " + std::to_string(*(params->size));
  log_msg += " numEventsInWaitList = " + std::to_string(*(params->numEventsInWaitList));
  log_msg += " eventWaitList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->eventWaitList)));
  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueSVMMapOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clRetainMemObjectOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clRetainMemObject* params =
    reinterpret_cast<const cl_params_clRetainMemObject*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " memobj = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->memobj)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clRetainMemObjectOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clSetUserEventStatusOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clSetUserEventStatus* params =
    reinterpret_cast<const cl_params_clSetUserEventStatus*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += " executionStatus = " + std::to_string(*(params->executionStatus));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clSetUserEventStatusOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clCreateUserEventOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateUserEvent* params =
    reinterpret_cast<const cl_params_clCreateUserEvent*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " context = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->context)));
  log_msg += " errcodeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->errcodeRet)));
  log_msg += "\n";

  collector->Log(log_msg);

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateUserEventOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  const cl_params_clCreateUserEvent* params =
    reinterpret_cast<const cl_params_clCreateUserEvent*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_event* result =
    reinterpret_cast<cl_event*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  log_msg += " result = " + ToHexString(reinterpret_cast<uintptr_t>(*result));

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(**(params->errcodeRet));
  log_msg += " (" + std::to_string(**(params->errcodeRet)) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetSamplerInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetSamplerInfo* params =
    reinterpret_cast<const cl_params_clGetSamplerInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " sampler = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->sampler)));
  log_msg += " paramName = " + std::to_string(*(params->paramName));
  log_msg += " paramValueSize = " + std::to_string(*(params->paramValueSize));
  log_msg += " paramValue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->paramValue)));
  log_msg += " paramValueSizeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->paramValueSizeRet)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetSamplerInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueMarkerOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueMarker* params =
    reinterpret_cast<const cl_params_clEnqueueMarker*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueMarkerOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clCreateKernelOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateKernel* params =
    reinterpret_cast<const cl_params_clCreateKernel*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " program = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->program)));
  if (*(params->kernelName) == nullptr) {
    log_msg += " kernelName = 0";
  } else if (strlen(*(params->kernelName)) == 0) {
    log_msg += " kernelName = \"\"";
  } else {
    log_msg += " kernelName = \"" + std::string(*(params->kernelName)) + "\"";
    if (collector->Demangle()) {
      log_msg += " (" + utils::Demangle(*(params->kernelName)) + ")";
    }
  }
  log_msg += " errcodeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->errcodeRet)));
  log_msg += "\n";

  collector->Log(log_msg);

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateKernelOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  const cl_params_clCreateKernel* params =
    reinterpret_cast<const cl_params_clCreateKernel*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_kernel* result =
    reinterpret_cast<cl_kernel*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  log_msg += " result = " + ToHexString(reinterpret_cast<uintptr_t>(*result));

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(**(params->errcodeRet));
  log_msg += " (" + std::to_string(**(params->errcodeRet)) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetProgramInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetProgramInfo* params =
    reinterpret_cast<const cl_params_clGetProgramInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " program = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->program)));
  log_msg += " paramName = " + std::to_string(*(params->paramName));
  log_msg += " paramValueSize = " + std::to_string(*(params->paramValueSize));
  log_msg += " paramValue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->paramValue)));
  log_msg += " paramValueSizeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->paramValueSizeRet)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetProgramInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clSVMAllocOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clSVMAlloc* params =
    reinterpret_cast<const cl_params_clSVMAlloc*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " context = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->context)));
  log_msg += " flags = " + std::to_string(*(params->flags));
  log_msg += " size = " + std::to_string(*(params->size));
  log_msg += " alignment = " + std::to_string(*(params->alignment));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clSVMAllocOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  void ** result =
    reinterpret_cast<void **>(
        data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  log_msg += " result = " + ToHexString(reinterpret_cast<uintptr_t>(*result));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clRetainEventOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clRetainEvent* params =
    reinterpret_cast<const cl_params_clRetainEvent*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clRetainEventOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clCloneKernelOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCloneKernel* params =
    reinterpret_cast<const cl_params_clCloneKernel*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " sourceKernel = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->sourceKernel)));
  log_msg += " errcodeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->errcodeRet)));
  log_msg += "\n";

  collector->Log(log_msg);

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCloneKernelOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  const cl_params_clCloneKernel* params =
    reinterpret_cast<const cl_params_clCloneKernel*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_kernel* result =
    reinterpret_cast<cl_kernel*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  log_msg += " result = " + ToHexString(reinterpret_cast<uintptr_t>(*result));

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(**(params->errcodeRet));
  log_msg += " (" + std::to_string(**(params->errcodeRet)) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetImageInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetImageInfo* params =
    reinterpret_cast<const cl_params_clGetImageInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " image = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->image)));
  log_msg += " paramName = " + std::to_string(*(params->paramName));
  log_msg += " paramValueSize = " + std::to_string(*(params->paramValueSize));
  log_msg += " paramValue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->paramValue)));
  log_msg += " paramValueSizeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->paramValueSizeRet)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clGetImageInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clFlushOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clFlush* params =
    reinterpret_cast<const cl_params_clFlush*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clFlushOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueMarkerWithWaitListOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueMarkerWithWaitList* params =
    reinterpret_cast<const cl_params_clEnqueueMarkerWithWaitList*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " commandQueue = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->commandQueue)));
  log_msg += " numEventsInWaitList = " + std::to_string(*(params->numEventsInWaitList));
  log_msg += " eventWaitList = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->eventWaitList)));
  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clEnqueueMarkerWithWaitListOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clCreateProgramWithILOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateProgramWithIL* params =
    reinterpret_cast<const cl_params_clCreateProgramWithIL*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " context = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->context)));
  log_msg += " il = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->il)));
  log_msg += " length = " + std::to_string(*(params->length));
  log_msg += " errcodeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->errcodeRet)));
  log_msg += "\n";

  collector->Log(log_msg);

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateProgramWithILOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  const cl_params_clCreateProgramWithIL* params =
    reinterpret_cast<const cl_params_clCreateProgramWithIL*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_program* result =
    reinterpret_cast<cl_program*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  log_msg += " result = " + ToHexString(reinterpret_cast<uintptr_t>(*result));

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(**(params->errcodeRet));
  log_msg += " (" + std::to_string(**(params->errcodeRet)) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clCreateSamplerOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateSampler* params =
    reinterpret_cast<const cl_params_clCreateSampler*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " context = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->context)));
  log_msg += " normalizedCoords = " + std::to_string(*(params->normalizedCoords));
  log_msg += " addressingMode = " + std::to_string(*(params->addressingMode));
  log_msg += " filterMode = " + std::to_string(*(params->filterMode));
  log_msg += " errcodeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->errcodeRet)));
  log_msg += "\n";

  collector->Log(log_msg);

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateSamplerOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  const cl_params_clCreateSampler* params =
    reinterpret_cast<const cl_params_clCreateSampler*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_sampler* result =
    reinterpret_cast<cl_sampler*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  log_msg += " result = " + ToHexString(reinterpret_cast<uintptr_t>(*result));

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(**(params->errcodeRet));
  log_msg += " (" + std::to_string(**(params->errcodeRet)) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clCreateFromGLTextureOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateFromGLTexture* params =
    reinterpret_cast<const cl_params_clCreateFromGLTexture*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " context = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->context)));
  log_msg += " flags = " + std::to_string(*(params->flags));
  log_msg += " target = " + std::to_string(*(params->target));
  log_msg += " miplevel = " + std::to_string(*(params->miplevel));
  log_msg += " texture = " + std::to_string(*(params->texture));
  log_msg += " errcodeRet = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->errcodeRet)));
  log_msg += "\n";

  collector->Log(log_msg);

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateFromGLTextureOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  const cl_params_clCreateFromGLTexture* params =
    reinterpret_cast<const cl_params_clCreateFromGLTexture*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_mem* result =
    reinterpret_cast<cl_mem*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  log_msg += " result = " + ToHexString(reinterpret_cast<uintptr_t>(*result));

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(**(params->errcodeRet));
  log_msg += " (" + std::to_string(**(params->errcodeRet)) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clSVMFreeOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clSVMFree* params =
    reinterpret_cast<const cl_params_clSVMFree*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " context = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->context)));
  log_msg += " svmPointer = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->svmPointer)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clSVMFreeOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  log_msg += "\n";

  collector->Log(log_msg);
}

static void clReleaseEventOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clReleaseEvent* params =
    reinterpret_cast<const cl_params_clReleaseEvent*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::string log_msg = ">>>> [" + std::to_string(start) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName) + ":";

  log_msg += " event = " + ToHexString(reinterpret_cast<uintptr_t>(*(params->event)));
  log_msg += "\n";

  collector->Log(log_msg);
}

static void clReleaseEventOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::string log_msg = "<<<< [" + std::to_string(end) + "] ";
  if (collector->NeedPid()) {
    log_msg += "<PID:" + std::to_string(utils::GetPid()) + "> ";
  }
  if (collector->NeedTid()) {
    log_msg += "<TID:" + std::to_string(utils::GetTid()) + "> ";
  }
  log_msg += std::string(data->functionName);
  log_msg += " [" + std::to_string((end - start)) + " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  log_msg += " -> ";
  log_msg += utils::cl::GetErrorString(*error);
  log_msg += " (" + std::to_string(*error) + ")";
  log_msg += "\n";

  collector->Log(log_msg);
}

void OnEnterFunction(
    ClFunctionId function, cl_callback_data* data,
    uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  switch (function) {
    case CL_FUNCTION_clBuildProgram:
      clBuildProgramOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clCloneKernel:
      clCloneKernelOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clCompileProgram:
      clCompileProgramOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clCreateBuffer:
      clCreateBufferOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clCreateCommandQueue:
      clCreateCommandQueueOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clCreateCommandQueueWithProperties:
      clCreateCommandQueueWithPropertiesOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clCreateContext:
      clCreateContextOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clCreateContextFromType:
      clCreateContextFromTypeOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clCreateFromGLBuffer:
      clCreateFromGLBufferOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clCreateFromGLRenderbuffer:
      clCreateFromGLRenderbufferOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clCreateFromGLTexture:
      clCreateFromGLTextureOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clCreateFromGLTexture2D:
      clCreateFromGLTexture2DOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clCreateFromGLTexture3D:
      clCreateFromGLTexture3DOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clCreateImage:
      clCreateImageOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clCreateImage2D:
      clCreateImage2DOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clCreateImage3D:
      clCreateImage3DOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clCreateKernel:
      clCreateKernelOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clCreateKernelsInProgram:
      clCreateKernelsInProgramOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clCreatePipe:
      clCreatePipeOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clCreateProgramWithBinary:
      clCreateProgramWithBinaryOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clCreateProgramWithBuiltInKernels:
      clCreateProgramWithBuiltInKernelsOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clCreateProgramWithIL:
      clCreateProgramWithILOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clCreateProgramWithSource:
      clCreateProgramWithSourceOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clCreateSampler:
      clCreateSamplerOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clCreateSamplerWithProperties:
      clCreateSamplerWithPropertiesOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clCreateSubBuffer:
      clCreateSubBufferOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clCreateSubDevices:
      clCreateSubDevicesOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clCreateUserEvent:
      clCreateUserEventOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clEnqueueAcquireGLObjects:
      clEnqueueAcquireGLObjectsOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clEnqueueBarrier:
      clEnqueueBarrierOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clEnqueueBarrierWithWaitList:
      clEnqueueBarrierWithWaitListOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clEnqueueCopyBuffer:
      clEnqueueCopyBufferOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clEnqueueCopyBufferRect:
      clEnqueueCopyBufferRectOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clEnqueueCopyBufferToImage:
      clEnqueueCopyBufferToImageOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clEnqueueCopyImage:
      clEnqueueCopyImageOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clEnqueueCopyImageToBuffer:
      clEnqueueCopyImageToBufferOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clEnqueueFillBuffer:
      clEnqueueFillBufferOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clEnqueueFillImage:
      clEnqueueFillImageOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clEnqueueMapBuffer:
      clEnqueueMapBufferOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clEnqueueMapImage:
      clEnqueueMapImageOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clEnqueueMarker:
      clEnqueueMarkerOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clEnqueueMarkerWithWaitList:
      clEnqueueMarkerWithWaitListOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clEnqueueMigrateMemObjects:
      clEnqueueMigrateMemObjectsOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clEnqueueNDRangeKernel:
      clEnqueueNDRangeKernelOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clEnqueueNativeKernel:
      clEnqueueNativeKernelOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clEnqueueReadBuffer:
      clEnqueueReadBufferOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clEnqueueReadBufferRect:
      clEnqueueReadBufferRectOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clEnqueueReadImage:
      clEnqueueReadImageOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clEnqueueReleaseGLObjects:
      clEnqueueReleaseGLObjectsOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clEnqueueSVMFree:
      clEnqueueSVMFreeOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clEnqueueSVMMap:
      clEnqueueSVMMapOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clEnqueueSVMMemFill:
      clEnqueueSVMMemFillOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clEnqueueSVMMemcpy:
      clEnqueueSVMMemcpyOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clEnqueueSVMMigrateMem:
      clEnqueueSVMMigrateMemOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clEnqueueSVMUnmap:
      clEnqueueSVMUnmapOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clEnqueueTask:
      clEnqueueTaskOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clEnqueueUnmapMemObject:
      clEnqueueUnmapMemObjectOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clEnqueueWaitForEvents:
      clEnqueueWaitForEventsOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clEnqueueWriteBuffer:
      clEnqueueWriteBufferOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clEnqueueWriteBufferRect:
      clEnqueueWriteBufferRectOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clEnqueueWriteImage:
      clEnqueueWriteImageOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clFinish:
      clFinishOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clFlush:
      clFlushOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clGetCommandQueueInfo:
      clGetCommandQueueInfoOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clGetContextInfo:
      clGetContextInfoOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clGetDeviceAndHostTimer:
      clGetDeviceAndHostTimerOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clGetDeviceIDs:
      clGetDeviceIDsOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clGetDeviceInfo:
      clGetDeviceInfoOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clGetEventInfo:
      clGetEventInfoOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clGetEventProfilingInfo:
      clGetEventProfilingInfoOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clGetExtensionFunctionAddress:
      clGetExtensionFunctionAddressOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clGetExtensionFunctionAddressForPlatform:
      clGetExtensionFunctionAddressForPlatformOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clGetGLObjectInfo:
      clGetGLObjectInfoOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clGetGLTextureInfo:
      clGetGLTextureInfoOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clGetHostTimer:
      clGetHostTimerOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clGetImageInfo:
      clGetImageInfoOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clGetKernelArgInfo:
      clGetKernelArgInfoOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clGetKernelInfo:
      clGetKernelInfoOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clGetKernelSubGroupInfo:
      clGetKernelSubGroupInfoOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clGetKernelWorkGroupInfo:
      clGetKernelWorkGroupInfoOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clGetMemObjectInfo:
      clGetMemObjectInfoOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clGetPipeInfo:
      clGetPipeInfoOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clGetPlatformIDs:
      clGetPlatformIDsOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clGetPlatformInfo:
      clGetPlatformInfoOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clGetProgramBuildInfo:
      clGetProgramBuildInfoOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clGetProgramInfo:
      clGetProgramInfoOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clGetSamplerInfo:
      clGetSamplerInfoOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clGetSupportedImageFormats:
      clGetSupportedImageFormatsOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clLinkProgram:
      clLinkProgramOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clReleaseCommandQueue:
      clReleaseCommandQueueOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clReleaseContext:
      clReleaseContextOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clReleaseDevice:
      clReleaseDeviceOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clReleaseEvent:
      clReleaseEventOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clReleaseKernel:
      clReleaseKernelOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clReleaseMemObject:
      clReleaseMemObjectOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clReleaseProgram:
      clReleaseProgramOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clReleaseSampler:
      clReleaseSamplerOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clRetainCommandQueue:
      clRetainCommandQueueOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clRetainContext:
      clRetainContextOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clRetainDevice:
      clRetainDeviceOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clRetainEvent:
      clRetainEventOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clRetainKernel:
      clRetainKernelOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clRetainMemObject:
      clRetainMemObjectOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clRetainProgram:
      clRetainProgramOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clRetainSampler:
      clRetainSamplerOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clSVMAlloc:
      clSVMAllocOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clSVMFree:
      clSVMFreeOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clSetCommandQueueProperty:
      clSetCommandQueuePropertyOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clSetDefaultDeviceCommandQueue:
      clSetDefaultDeviceCommandQueueOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clSetEventCallback:
      clSetEventCallbackOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clSetKernelArg:
      clSetKernelArgOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clSetKernelArgSVMPointer:
      clSetKernelArgSVMPointerOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clSetKernelExecInfo:
      clSetKernelExecInfoOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clSetMemObjectDestructorCallback:
      clSetMemObjectDestructorCallbackOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clSetUserEventStatus:
      clSetUserEventStatusOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clUnloadCompiler:
      clUnloadCompilerOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clUnloadPlatformCompiler:
      clUnloadPlatformCompilerOnEnter(data, start, collector);
      break;
    case CL_FUNCTION_clWaitForEvents:
      clWaitForEventsOnEnter(data, start, collector);
      break;
    default:
      break;
  }
}

void OnExitFunction(
    ClFunctionId function, cl_callback_data* data,
    uint64_t start, uint64_t end, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  switch (function) {
    case CL_FUNCTION_clBuildProgram:
      clBuildProgramOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clCloneKernel:
      clCloneKernelOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clCompileProgram:
      clCompileProgramOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clCreateBuffer:
      clCreateBufferOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clCreateCommandQueue:
      clCreateCommandQueueOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clCreateCommandQueueWithProperties:
      clCreateCommandQueueWithPropertiesOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clCreateContext:
      clCreateContextOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clCreateContextFromType:
      clCreateContextFromTypeOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clCreateFromGLBuffer:
      clCreateFromGLBufferOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clCreateFromGLRenderbuffer:
      clCreateFromGLRenderbufferOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clCreateFromGLTexture:
      clCreateFromGLTextureOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clCreateFromGLTexture2D:
      clCreateFromGLTexture2DOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clCreateFromGLTexture3D:
      clCreateFromGLTexture3DOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clCreateImage:
      clCreateImageOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clCreateImage2D:
      clCreateImage2DOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clCreateImage3D:
      clCreateImage3DOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clCreateKernel:
      clCreateKernelOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clCreateKernelsInProgram:
      clCreateKernelsInProgramOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clCreatePipe:
      clCreatePipeOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clCreateProgramWithBinary:
      clCreateProgramWithBinaryOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clCreateProgramWithBuiltInKernels:
      clCreateProgramWithBuiltInKernelsOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clCreateProgramWithIL:
      clCreateProgramWithILOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clCreateProgramWithSource:
      clCreateProgramWithSourceOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clCreateSampler:
      clCreateSamplerOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clCreateSamplerWithProperties:
      clCreateSamplerWithPropertiesOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clCreateSubBuffer:
      clCreateSubBufferOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clCreateSubDevices:
      clCreateSubDevicesOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clCreateUserEvent:
      clCreateUserEventOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clEnqueueAcquireGLObjects:
      clEnqueueAcquireGLObjectsOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clEnqueueBarrier:
      clEnqueueBarrierOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clEnqueueBarrierWithWaitList:
      clEnqueueBarrierWithWaitListOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clEnqueueCopyBuffer:
      clEnqueueCopyBufferOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clEnqueueCopyBufferRect:
      clEnqueueCopyBufferRectOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clEnqueueCopyBufferToImage:
      clEnqueueCopyBufferToImageOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clEnqueueCopyImage:
      clEnqueueCopyImageOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clEnqueueCopyImageToBuffer:
      clEnqueueCopyImageToBufferOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clEnqueueFillBuffer:
      clEnqueueFillBufferOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clEnqueueFillImage:
      clEnqueueFillImageOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clEnqueueMapBuffer:
      clEnqueueMapBufferOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clEnqueueMapImage:
      clEnqueueMapImageOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clEnqueueMarker:
      clEnqueueMarkerOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clEnqueueMarkerWithWaitList:
      clEnqueueMarkerWithWaitListOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clEnqueueMigrateMemObjects:
      clEnqueueMigrateMemObjectsOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clEnqueueNDRangeKernel:
      clEnqueueNDRangeKernelOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clEnqueueNativeKernel:
      clEnqueueNativeKernelOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clEnqueueReadBuffer:
      clEnqueueReadBufferOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clEnqueueReadBufferRect:
      clEnqueueReadBufferRectOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clEnqueueReadImage:
      clEnqueueReadImageOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clEnqueueReleaseGLObjects:
      clEnqueueReleaseGLObjectsOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clEnqueueSVMFree:
      clEnqueueSVMFreeOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clEnqueueSVMMap:
      clEnqueueSVMMapOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clEnqueueSVMMemFill:
      clEnqueueSVMMemFillOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clEnqueueSVMMemcpy:
      clEnqueueSVMMemcpyOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clEnqueueSVMMigrateMem:
      clEnqueueSVMMigrateMemOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clEnqueueSVMUnmap:
      clEnqueueSVMUnmapOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clEnqueueTask:
      clEnqueueTaskOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clEnqueueUnmapMemObject:
      clEnqueueUnmapMemObjectOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clEnqueueWaitForEvents:
      clEnqueueWaitForEventsOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clEnqueueWriteBuffer:
      clEnqueueWriteBufferOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clEnqueueWriteBufferRect:
      clEnqueueWriteBufferRectOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clEnqueueWriteImage:
      clEnqueueWriteImageOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clFinish:
      clFinishOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clFlush:
      clFlushOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clGetCommandQueueInfo:
      clGetCommandQueueInfoOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clGetContextInfo:
      clGetContextInfoOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clGetDeviceAndHostTimer:
      clGetDeviceAndHostTimerOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clGetDeviceIDs:
      clGetDeviceIDsOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clGetDeviceInfo:
      clGetDeviceInfoOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clGetEventInfo:
      clGetEventInfoOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clGetEventProfilingInfo:
      clGetEventProfilingInfoOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clGetExtensionFunctionAddress:
      clGetExtensionFunctionAddressOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clGetExtensionFunctionAddressForPlatform:
      clGetExtensionFunctionAddressForPlatformOnExit(
          data, start, end, collector);
      break;
    case CL_FUNCTION_clGetGLObjectInfo:
      clGetGLObjectInfoOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clGetGLTextureInfo:
      clGetGLTextureInfoOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clGetHostTimer:
      clGetHostTimerOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clGetImageInfo:
      clGetImageInfoOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clGetKernelArgInfo:
      clGetKernelArgInfoOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clGetKernelInfo:
      clGetKernelInfoOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clGetKernelSubGroupInfo:
      clGetKernelSubGroupInfoOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clGetKernelWorkGroupInfo:
      clGetKernelWorkGroupInfoOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clGetMemObjectInfo:
      clGetMemObjectInfoOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clGetPipeInfo:
      clGetPipeInfoOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clGetPlatformIDs:
      clGetPlatformIDsOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clGetPlatformInfo:
      clGetPlatformInfoOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clGetProgramBuildInfo:
      clGetProgramBuildInfoOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clGetProgramInfo:
      clGetProgramInfoOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clGetSamplerInfo:
      clGetSamplerInfoOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clGetSupportedImageFormats:
      clGetSupportedImageFormatsOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clLinkProgram:
      clLinkProgramOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clReleaseCommandQueue:
      clReleaseCommandQueueOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clReleaseContext:
      clReleaseContextOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clReleaseDevice:
      clReleaseDeviceOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clReleaseEvent:
      clReleaseEventOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clReleaseKernel:
      clReleaseKernelOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clReleaseMemObject:
      clReleaseMemObjectOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clReleaseProgram:
      clReleaseProgramOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clReleaseSampler:
      clReleaseSamplerOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clRetainCommandQueue:
      clRetainCommandQueueOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clRetainContext:
      clRetainContextOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clRetainDevice:
      clRetainDeviceOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clRetainEvent:
      clRetainEventOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clRetainKernel:
      clRetainKernelOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clRetainMemObject:
      clRetainMemObjectOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clRetainProgram:
      clRetainProgramOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clRetainSampler:
      clRetainSamplerOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clSVMAlloc:
      clSVMAllocOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clSVMFree:
      clSVMFreeOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clSetCommandQueueProperty:
      clSetCommandQueuePropertyOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clSetDefaultDeviceCommandQueue:
      clSetDefaultDeviceCommandQueueOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clSetEventCallback:
      clSetEventCallbackOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clSetKernelArg:
      clSetKernelArgOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clSetKernelArgSVMPointer:
      clSetKernelArgSVMPointerOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clSetKernelExecInfo:
      clSetKernelExecInfoOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clSetMemObjectDestructorCallback:
      clSetMemObjectDestructorCallbackOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clSetUserEventStatus:
      clSetUserEventStatusOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clUnloadCompiler:
      clUnloadCompilerOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clUnloadPlatformCompiler:
      clUnloadPlatformCompilerOnExit(data, start, end, collector);
      break;
    case CL_FUNCTION_clWaitForEvents:
      clWaitForEventsOnExit(data, start, end, collector);
      break;
    default:
      break;
  }
}

#endif /* PTI_TOOLS_UNITRACE_CL_API_CALLBACKS_H_ */

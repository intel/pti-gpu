//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_CL_TRACER_CL_API_CALLBACKS_H_
#define PTI_TOOLS_CL_TRACER_CL_API_CALLBACKS_H_

#include <sstream>

static thread_local cl_int current_error = CL_SUCCESS;

static void clGetSupportedImageFormatsOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetSupportedImageFormats* params =
    reinterpret_cast<const cl_params_clGetSupportedImageFormats*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " context = " << *(params->context);
  stream << " flags = " << *(params->flags);
  stream << " imageType = " << *(params->imageType);
  stream << " numEntries = " << *(params->numEntries);
  stream << " imageFormats = " << *(params->imageFormats);
  stream << " numImageFormats = " << *(params->numImageFormats);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetSupportedImageFormatsOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetKernelInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetKernelInfo* params =
    reinterpret_cast<const cl_params_clGetKernelInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " kernel = " << *(params->kernel);
  stream << " paramName = " << *(params->paramName);
  stream << " paramValueSize = " << *(params->paramValueSize);
  stream << " paramValue = " << *(params->paramValue);
  stream << " paramValueSizeRet = " << *(params->paramValueSizeRet);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetKernelInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clCompileProgramOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCompileProgram* params =
    reinterpret_cast<const cl_params_clCompileProgram*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " program = " << *(params->program);
  stream << " numDevices = " << *(params->numDevices);
  stream << " deviceList = " << *(params->deviceList);
  if (*(params->options) == nullptr) {
    stream << " options = " << "0";
  } else if (strlen(*(params->options)) == 0) {
    stream << " options = \"\"";
  } else {
    stream << " options = \"" << *(params->options) << "\"";
  }
  stream << " numInputHeaders = " << *(params->numInputHeaders);
  stream << " inputHeaders = " << *(params->inputHeaders);
  stream << " headerIncludeNames = " << *(params->headerIncludeNames);
  stream << " funcNotify = " << *(params->funcNotify);
  stream << " userData = " << *(params->userData);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clCompileProgramOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clSetEventCallbackOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clSetEventCallback* params =
    reinterpret_cast<const cl_params_clSetEventCallback*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " event = " << *(params->event);
  stream << " commandExecCallbackType = " <<
    *(params->commandExecCallbackType);
  stream << " funcNotify = " << *(params->funcNotify);
  stream << " userData = " << *(params->userData);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clSetEventCallbackOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clUnloadPlatformCompilerOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clUnloadPlatformCompiler* params =
    reinterpret_cast<const cl_params_clUnloadPlatformCompiler*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " platform = " << *(params->platform);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clUnloadPlatformCompilerOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetPlatformIDsOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetPlatformIDs* params =
    reinterpret_cast<const cl_params_clGetPlatformIDs*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " numEntries = " << *(params->numEntries);
  stream << " platforms = " << *(params->platforms);
  stream << " numPlatforms = " << *(params->numPlatforms);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetPlatformIDsOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clUnloadCompilerOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clUnloadCompiler* params =
    reinterpret_cast<const cl_params_clUnloadCompiler*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << std::endl;

  collector->Log(stream.str());
}

static void clUnloadCompilerOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueBarrierWithWaitListOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueBarrierWithWaitList* params =
    reinterpret_cast<const cl_params_clEnqueueBarrierWithWaitList*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << " numEventsInWaitList = " << *(params->numEventsInWaitList);
  stream << " eventWaitList = " << *(params->eventWaitList);
  stream << " event = " << *(params->event);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueBarrierWithWaitListOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueMapBufferOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueMapBuffer* params =
    reinterpret_cast<const cl_params_clEnqueueMapBuffer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << " buffer = " << *(params->buffer);
  stream << " blockingMap = " << *(params->blockingMap);
  stream << " mapFlags = " << *(params->mapFlags);
  stream << " offset = " << *(params->offset);
  stream << " cb = " << *(params->cb);
  stream << " numEventsInWaitList = " << *(params->numEventsInWaitList);
  stream << " eventWaitList = " << *(params->eventWaitList);
  stream << " event = " << *(params->event);
  stream << " errcodeRet = " << *(params->errcodeRet);
  stream << std::endl;

  collector->Log(stream.str());

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clEnqueueMapBufferOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  const cl_params_clEnqueueMapBuffer* params =
    reinterpret_cast<const cl_params_clEnqueueMapBuffer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  void ** result =
    reinterpret_cast<void **>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  stream << " result = " << *result;

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  stream << " -> " << utils::cl::GetErrorString(**(params->errcodeRet));
  stream << " (" << **(params->errcodeRet) << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clCreateImage3DOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateImage3D* params =
    reinterpret_cast<const cl_params_clCreateImage3D*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " context = " << *(params->context);
  stream << " flags = " << *(params->flags);
  stream << " imageFormat = " << *(params->imageFormat);
  stream << " imageWidth = " << *(params->imageWidth);
  stream << " imageHeight = " << *(params->imageHeight);
  stream << " imageDepth = " << *(params->imageDepth);
  stream << " imageRowPitch = " << *(params->imageRowPitch);
  stream << " imageSlicePitch = " << *(params->imageSlicePitch);
  stream << " hostPtr = " << *(params->hostPtr);
  stream << " errcodeRet = " << *(params->errcodeRet);
  stream << std::endl;

  collector->Log(stream.str());

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateImage3DOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  const cl_params_clCreateImage3D* params =
    reinterpret_cast<const cl_params_clCreateImage3D*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_mem* result =
    reinterpret_cast<cl_mem*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  stream << " result = " << *result;

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  stream << " -> " << utils::cl::GetErrorString(**(params->errcodeRet));
  stream << " (" << **(params->errcodeRet) << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetKernelArgInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetKernelArgInfo* params =
    reinterpret_cast<const cl_params_clGetKernelArgInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " kernel = " << *(params->kernel);
  stream << " argIndx = " << *(params->argIndx);
  stream << " paramName = " << *(params->paramName);
  stream << " paramValueSize = " << *(params->paramValueSize);
  stream << " paramValue = " << *(params->paramValue);
  stream << " paramValueSizeRet = " << *(params->paramValueSizeRet);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetKernelArgInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueSVMFreeOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueSVMFree* params =
    reinterpret_cast<const cl_params_clEnqueueSVMFree*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << " numSvmPointers = " << *(params->numSvmPointers);
  stream << " svmPointers = " << *(params->svmPointers);
  stream << " pfnFreeFunc = " << *(params->pfnFreeFunc);
  stream << " userData = " << *(params->userData);
  stream << " numEventsInWaitList = " << *(params->numEventsInWaitList);
  stream << " eventWaitList = " << *(params->eventWaitList);
  stream << " event = " << *(params->event);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueSVMFreeOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueCopyImageToBufferOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueCopyImageToBuffer* params =
    reinterpret_cast<const cl_params_clEnqueueCopyImageToBuffer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << " srcImage = " << *(params->srcImage);
  stream << " dstBuffer = " << *(params->dstBuffer);
  stream << " srcOrigin = " << *(params->srcOrigin);
  stream << " region = " << *(params->region);
  stream << " dstOffset = " << *(params->dstOffset);
  stream << " numEventsInWaitList = " << *(params->numEventsInWaitList);
  stream << " eventWaitList = " << *(params->eventWaitList);
  stream << " event = " << *(params->event);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueCopyImageToBufferOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetContextInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetContextInfo* params =
    reinterpret_cast<const cl_params_clGetContextInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " context = " << *(params->context);
  stream << " paramName = " << *(params->paramName);
  stream << " paramValueSize = " << *(params->paramValueSize);
  stream << " paramValue = " << *(params->paramValue);
  stream << " paramValueSizeRet = " << *(params->paramValueSizeRet);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetContextInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clRetainCommandQueueOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clRetainCommandQueue* params =
    reinterpret_cast<const cl_params_clRetainCommandQueue*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clRetainCommandQueueOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueWriteImageOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueWriteImage* params =
    reinterpret_cast<const cl_params_clEnqueueWriteImage*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << " image = " << *(params->image);
  stream << " blockingWrite = " << *(params->blockingWrite);
  stream << " origin = " << *(params->origin);
  stream << " region = " << *(params->region);
  stream << " inputRowPitch = " << *(params->inputRowPitch);
  stream << " inputSlicePitch = " << *(params->inputSlicePitch);
  stream << " ptr = " << *(params->ptr);
  stream << " numEventsInWaitList = " << *(params->numEventsInWaitList);
  stream << " eventWaitList = " << *(params->eventWaitList);
  stream << " event = " << *(params->event);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueWriteImageOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueWaitForEventsOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueWaitForEvents* params =
    reinterpret_cast<const cl_params_clEnqueueWaitForEvents*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << " numEvents = " << *(params->numEvents);
  stream << " eventList = " << *(params->eventList);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueWaitForEventsOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueSVMUnmapOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueSVMUnmap* params =
    reinterpret_cast<const cl_params_clEnqueueSVMUnmap*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << " svmPtr = " << *(params->svmPtr);
  stream << " numEventsInWaitList = " << *(params->numEventsInWaitList);
  stream << " eventWaitList = " << *(params->eventWaitList);
  stream << " event = " << *(params->event);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueSVMUnmapOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clCreateProgramWithBinaryOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateProgramWithBinary* params =
    reinterpret_cast<const cl_params_clCreateProgramWithBinary*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " context = " << *(params->context);
  stream << " numDevices = " << *(params->numDevices);
  stream << " deviceList = " << *(params->deviceList);
  stream << " lengths = " << *(params->lengths);
  stream << " binaries = " << *(params->binaries);
  stream << " binaryStatus = " << *(params->binaryStatus);
  stream << " errcodeRet = " << *(params->errcodeRet);
  stream << std::endl;

  collector->Log(stream.str());

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateProgramWithBinaryOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  const cl_params_clCreateProgramWithBinary* params =
    reinterpret_cast<const cl_params_clCreateProgramWithBinary*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_program* result =
    reinterpret_cast<cl_program*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  stream << " result = " << *result;

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  stream << " -> " << utils::cl::GetErrorString(**(params->errcodeRet));
  stream << " (" << **(params->errcodeRet) << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueFillImageOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueFillImage* params =
    reinterpret_cast<const cl_params_clEnqueueFillImage*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << " image = " << *(params->image);
  stream << " fillColor = " << *(params->fillColor);
  stream << " origin = " << *(params->origin);
  stream << " region = " << *(params->region);
  stream << " numEventsInWaitList = " << *(params->numEventsInWaitList);
  stream << " eventWaitList = " << *(params->eventWaitList);
  stream << " event = " << *(params->event);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueFillImageOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clCreateFromGLTexture2DOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateFromGLTexture2D* params =
    reinterpret_cast<const cl_params_clCreateFromGLTexture2D*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " context = " << *(params->context);
  stream << " flags = " << *(params->flags);
  stream << " target = " << *(params->target);
  stream << " miplevel = " << *(params->miplevel);
  stream << " texture = " << *(params->texture);
  stream << " errcodeRet = " << *(params->errcodeRet);
  stream << std::endl;

  collector->Log(stream.str());

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateFromGLTexture2DOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  const cl_params_clCreateFromGLTexture2D* params =
    reinterpret_cast<const cl_params_clCreateFromGLTexture2D*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_mem* result =
    reinterpret_cast<cl_mem*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  stream << " result = " << *result;

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  stream << " -> " << utils::cl::GetErrorString(**(params->errcodeRet));
  stream << " (" << **(params->errcodeRet) << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clSetKernelExecInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clSetKernelExecInfo* params =
    reinterpret_cast<const cl_params_clSetKernelExecInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " kernel = " << *(params->kernel);
  stream << " paramName = " << *(params->paramName);
  stream << " paramValueSize = " << *(params->paramValueSize);
  stream << " paramValue = " << *(params->paramValue);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clSetKernelExecInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueReleaseGLObjectsOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueReleaseGLObjects* params =
    reinterpret_cast<const cl_params_clEnqueueReleaseGLObjects*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << " numObjects = " << *(params->numObjects);
  stream << " memObjects = " << *(params->memObjects);
  stream << " numEventsInWaitList = " << *(params->numEventsInWaitList);
  stream << " eventWaitList = " << *(params->eventWaitList);
  stream << " event = " << *(params->event);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueReleaseGLObjectsOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetDeviceIDsOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetDeviceIDs* params =
    reinterpret_cast<const cl_params_clGetDeviceIDs*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " platform = " << *(params->platform);
  stream << " deviceType = " << *(params->deviceType);
  stream << " numEntries = " << *(params->numEntries);
  stream << " devices = " << *(params->devices);
  stream << " numDevices = " << *(params->numDevices);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetDeviceIDsOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clReleaseMemObjectOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clReleaseMemObject* params =
    reinterpret_cast<const cl_params_clReleaseMemObject*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " memobj = " << *(params->memobj);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clReleaseMemObjectOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetGLObjectInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetGLObjectInfo* params =
    reinterpret_cast<const cl_params_clGetGLObjectInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " memobj = " << *(params->memobj);
  stream << " glObjectType = " << *(params->glObjectType);
  stream << " glObjectName = " << *(params->glObjectName);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetGLObjectInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clCreateFromGLRenderbufferOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateFromGLRenderbuffer* params =
    reinterpret_cast<const cl_params_clCreateFromGLRenderbuffer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " context = " << *(params->context);
  stream << " flags = " << *(params->flags);
  stream << " renderbuffer = " << *(params->renderbuffer);
  stream << " errcodeRet = " << *(params->errcodeRet);
  stream << std::endl;

  collector->Log(stream.str());

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateFromGLRenderbufferOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  const cl_params_clCreateFromGLRenderbuffer* params =
    reinterpret_cast<const cl_params_clCreateFromGLRenderbuffer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_mem* result =
    reinterpret_cast<cl_mem*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  stream << " result = " << *result;

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  stream << " -> " << utils::cl::GetErrorString(**(params->errcodeRet));
  stream << " (" << **(params->errcodeRet) << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clReleaseContextOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clReleaseContext* params =
    reinterpret_cast<const cl_params_clReleaseContext*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " context = " << *(params->context);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clReleaseContextOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueUnmapMemObjectOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueUnmapMemObject* params =
    reinterpret_cast<const cl_params_clEnqueueUnmapMemObject*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << " memobj = " << *(params->memobj);
  stream << " mappedPtr = " << *(params->mappedPtr);
  stream << " numEventsInWaitList = " << *(params->numEventsInWaitList);
  stream << " eventWaitList = " << *(params->eventWaitList);
  stream << " event = " << *(params->event);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueUnmapMemObjectOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clCreateContextOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateContext* params =
    reinterpret_cast<const cl_params_clCreateContext*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " properties = " << *(params->properties);
  stream << " numDevices = " << *(params->numDevices);
  stream << " devices = " << *(params->devices);
  stream << " funcNotify = " << *(params->funcNotify);
  stream << " userData = " << *(params->userData);
  stream << " errcodeRet = " << *(params->errcodeRet);
  stream << std::endl;

  collector->Log(stream.str());

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateContextOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  const cl_params_clCreateContext* params =
    reinterpret_cast<const cl_params_clCreateContext*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_context* result =
    reinterpret_cast<cl_context*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  stream << " result = " << *result;

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  stream << " -> " << utils::cl::GetErrorString(**(params->errcodeRet));
  stream << " (" << **(params->errcodeRet) << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetHostTimerOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetHostTimer* params =
    reinterpret_cast<const cl_params_clGetHostTimer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " device = " << *(params->device);
  stream << " hostTimestamp = " << *(params->hostTimestamp);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetHostTimerOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetPipeInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetPipeInfo* params =
    reinterpret_cast<const cl_params_clGetPipeInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " pipe = " << *(params->pipe);
  stream << " paramName = " << *(params->paramName);
  stream << " paramValueSize = " << *(params->paramValueSize);
  stream << " paramValue = " << *(params->paramValue);
  stream << " paramValueSizeRet = " << *(params->paramValueSizeRet);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetPipeInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueAcquireGLObjectsOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueAcquireGLObjects* params =
    reinterpret_cast<const cl_params_clEnqueueAcquireGLObjects*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << " numObjects = " << *(params->numObjects);
  stream << " memObjects = " << *(params->memObjects);
  stream << " numEventsInWaitList = " << *(params->numEventsInWaitList);
  stream << " eventWaitList = " << *(params->eventWaitList);
  stream << " event = " << *(params->event);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueAcquireGLObjectsOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetKernelWorkGroupInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetKernelWorkGroupInfo* params =
    reinterpret_cast<const cl_params_clGetKernelWorkGroupInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " kernel = " << *(params->kernel);
  stream << " device = " << *(params->device);
  stream << " paramName = " << *(params->paramName);
  stream << " paramValueSize = " << *(params->paramValueSize);
  stream << " paramValue = " << *(params->paramValue);
  stream << " paramValueSizeRet = " << *(params->paramValueSizeRet);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetKernelWorkGroupInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clCreateImage2DOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateImage2D* params =
    reinterpret_cast<const cl_params_clCreateImage2D*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " context = " << *(params->context);
  stream << " flags = " << *(params->flags);
  stream << " imageFormat = " << *(params->imageFormat);
  stream << " imageWidth = " << *(params->imageWidth);
  stream << " imageHeight = " << *(params->imageHeight);
  stream << " imageRowPitch = " << *(params->imageRowPitch);
  stream << " hostPtr = " << *(params->hostPtr);
  stream << " errcodeRet = " << *(params->errcodeRet);
  stream << std::endl;

  collector->Log(stream.str());

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateImage2DOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  const cl_params_clCreateImage2D* params =
    reinterpret_cast<const cl_params_clCreateImage2D*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_mem* result =
    reinterpret_cast<cl_mem*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  stream << " result = " << *result;

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  stream << " -> " << utils::cl::GetErrorString(**(params->errcodeRet));
  stream << " (" << **(params->errcodeRet) << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clCreateContextFromTypeOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateContextFromType* params =
    reinterpret_cast<const cl_params_clCreateContextFromType*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " properties = " << *(params->properties);
  stream << " deviceType = " << *(params->deviceType);
  stream << " funcNotify = " << *(params->funcNotify);
  stream << " userData = " << *(params->userData);
  stream << " errcodeRet = " << *(params->errcodeRet);
  stream << std::endl;

  collector->Log(stream.str());

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateContextFromTypeOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  const cl_params_clCreateContextFromType* params =
    reinterpret_cast<const cl_params_clCreateContextFromType*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_context* result =
    reinterpret_cast<cl_context*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  stream << " result = " << *result;

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  stream << " -> " << utils::cl::GetErrorString(**(params->errcodeRet));
  stream << " (" << **(params->errcodeRet) << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clRetainProgramOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clRetainProgram* params =
    reinterpret_cast<const cl_params_clRetainProgram*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " program = " << *(params->program);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clRetainProgramOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clCreateProgramWithSourceOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateProgramWithSource* params =
    reinterpret_cast<const cl_params_clCreateProgramWithSource*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " context = " << *(params->context);
  stream << " count = " << *(params->count);
  stream << " strings = " << *(params->strings);
  stream << " lengths = " << *(params->lengths);
  stream << " errcodeRet = " << *(params->errcodeRet);
  stream << std::endl;

  collector->Log(stream.str());

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateProgramWithSourceOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  const cl_params_clCreateProgramWithSource* params =
    reinterpret_cast<const cl_params_clCreateProgramWithSource*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_program* result =
    reinterpret_cast<cl_program*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  stream << " result = " << *result;

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  stream << " -> " << utils::cl::GetErrorString(**(params->errcodeRet));
  stream << " (" << **(params->errcodeRet) << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetMemObjectInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetMemObjectInfo* params =
    reinterpret_cast<const cl_params_clGetMemObjectInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " memobj = " << *(params->memobj);
  stream << " paramName = " << *(params->paramName);
  stream << " paramValueSize = " << *(params->paramValueSize);
  stream << " paramValue = " << *(params->paramValue);
  stream << " paramValueSizeRet = " << *(params->paramValueSizeRet);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetMemObjectInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clLinkProgramOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clLinkProgram* params =
    reinterpret_cast<const cl_params_clLinkProgram*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " context = " << *(params->context);
  stream << " numDevices = " << *(params->numDevices);
  stream << " deviceList = " << *(params->deviceList);
  if (*(params->options) == nullptr) {
    stream << " options = " << "0";
  } else if (strlen(*(params->options)) == 0) {
    stream << " options = \"\"";
  } else {
    stream << " options = \"" << *(params->options) << "\"";
  }
  stream << " numInputPrograms = " << *(params->numInputPrograms);
  stream << " inputPrograms = " << *(params->inputPrograms);
  stream << " funcNotify = " << *(params->funcNotify);
  stream << " userData = " << *(params->userData);
  stream << " errcodeRet = " << *(params->errcodeRet);
  stream << std::endl;

  collector->Log(stream.str());

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clLinkProgramOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  const cl_params_clLinkProgram* params =
    reinterpret_cast<const cl_params_clLinkProgram*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_program* result =
    reinterpret_cast<cl_program*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  stream << " result = " << *result;

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  stream << " -> " << utils::cl::GetErrorString(**(params->errcodeRet));
  stream << " (" << **(params->errcodeRet) << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clCreateSamplerWithPropertiesOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateSamplerWithProperties* params =
    reinterpret_cast<const cl_params_clCreateSamplerWithProperties*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " context = " << *(params->context);
  stream << " samplerProperties = " << *(params->samplerProperties);
  stream << " errcodeRet = " << *(params->errcodeRet);
  stream << std::endl;

  collector->Log(stream.str());

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateSamplerWithPropertiesOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  const cl_params_clCreateSamplerWithProperties* params =
    reinterpret_cast<const cl_params_clCreateSamplerWithProperties*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_sampler* result =
    reinterpret_cast<cl_sampler*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  stream << " result = " << *result;

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  stream << " -> " << utils::cl::GetErrorString(**(params->errcodeRet));
  stream << " (" << **(params->errcodeRet) << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clRetainSamplerOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clRetainSampler* params =
    reinterpret_cast<const cl_params_clRetainSampler*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " sampler = " << *(params->sampler);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clRetainSamplerOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clCreateFromGLTexture3DOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateFromGLTexture3D* params =
    reinterpret_cast<const cl_params_clCreateFromGLTexture3D*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " context = " << *(params->context);
  stream << " flags = " << *(params->flags);
  stream << " target = " << *(params->target);
  stream << " miplevel = " << *(params->miplevel);
  stream << " texture = " << *(params->texture);
  stream << " errcodeRet = " << *(params->errcodeRet);
  stream << std::endl;

  collector->Log(stream.str());

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateFromGLTexture3DOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  const cl_params_clCreateFromGLTexture3D* params =
    reinterpret_cast<const cl_params_clCreateFromGLTexture3D*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_mem* result =
    reinterpret_cast<cl_mem*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  stream << " result = " << *result;

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  stream << " -> " << utils::cl::GetErrorString(**(params->errcodeRet));
  stream << " (" << **(params->errcodeRet) << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueMapImageOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueMapImage* params =
    reinterpret_cast<const cl_params_clEnqueueMapImage*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << " image = " << *(params->image);
  stream << " blockingMap = " << *(params->blockingMap);
  stream << " mapFlags = " << *(params->mapFlags);
  stream << " origin = " << *(params->origin);
  stream << " region = " << *(params->region);
  stream << " imageRowPitch = " << *(params->imageRowPitch);
  stream << " imageSlicePitch = " << *(params->imageSlicePitch);
  stream << " numEventsInWaitList = " << *(params->numEventsInWaitList);
  stream << " eventWaitList = " << *(params->eventWaitList);
  stream << " event = " << *(params->event);
  stream << " errcodeRet = " << *(params->errcodeRet);
  stream << std::endl;

  collector->Log(stream.str());

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clEnqueueMapImageOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  const cl_params_clEnqueueMapImage* params =
    reinterpret_cast<const cl_params_clEnqueueMapImage*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  void ** result =
    reinterpret_cast<void **>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  stream << " result = " << *result;

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  stream << " -> " << utils::cl::GetErrorString(**(params->errcodeRet));
  stream << " (" << **(params->errcodeRet) << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueWriteBufferOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueWriteBuffer* params =
    reinterpret_cast<const cl_params_clEnqueueWriteBuffer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << " buffer = " << *(params->buffer);
  stream << " blockingWrite = " << *(params->blockingWrite);
  stream << " offset = " << *(params->offset);
  stream << " cb = " << *(params->cb);
  stream << " ptr = " << *(params->ptr);
  stream << " numEventsInWaitList = " << *(params->numEventsInWaitList);
  stream << " eventWaitList = " << *(params->eventWaitList);
  stream << " event = " << *(params->event);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueWriteBufferOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);

  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  if (collector->GetKernelId() > 0) {
    stream << "(" << collector->GetKernelId() << ")";
  }
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueCopyImageOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueCopyImage* params =
    reinterpret_cast<const cl_params_clEnqueueCopyImage*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << " srcImage = " << *(params->srcImage);
  stream << " dstImage = " << *(params->dstImage);
  stream << " srcOrigin = " << *(params->srcOrigin);
  stream << " dstOrigin = " << *(params->dstOrigin);
  stream << " region = " << *(params->region);
  stream << " numEventsInWaitList = " << *(params->numEventsInWaitList);
  stream << " eventWaitList = " << *(params->eventWaitList);
  stream << " event = " << *(params->event);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueCopyImageOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetExtensionFunctionAddressOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetExtensionFunctionAddress* params =
    reinterpret_cast<const cl_params_clGetExtensionFunctionAddress*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  if (*(params->funcName) == nullptr) {
    stream << " funcName = " << "0";
  } else if (strlen(*(params->funcName)) == 0) {
    stream << " funcName = \"\"";
  } else {
    stream << " funcName = \"" << *(params->funcName) << "\"";
  }
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetExtensionFunctionAddressOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  void** result =
    reinterpret_cast<void**>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  stream << " result = " << *result;
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueReadBufferRectOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueReadBufferRect* params =
    reinterpret_cast<const cl_params_clEnqueueReadBufferRect*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << " buffer = " << *(params->buffer);
  stream << " blockingRead = " << *(params->blockingRead);
  stream << " bufferOrigin = " << *(params->bufferOrigin);
  stream << " hostOrigin = " << *(params->hostOrigin);
  stream << " region = " << *(params->region);
  stream << " bufferRowPitch = " << *(params->bufferRowPitch);
  stream << " bufferSlicePitch = " << *(params->bufferSlicePitch);
  stream << " hostRowPitch = " << *(params->hostRowPitch);
  stream << " hostSlicePitch = " << *(params->hostSlicePitch);
  stream << " ptr = " << *(params->ptr);
  stream << " numEventsInWaitList = " << *(params->numEventsInWaitList);
  stream << " eventWaitList = " << *(params->eventWaitList);
  stream << " event = " << *(params->event);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueReadBufferRectOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clCreateSubDevicesOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateSubDevices* params =
    reinterpret_cast<const cl_params_clCreateSubDevices*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " inDevice = " << *(params->inDevice);
  stream << " properties = " << *(params->properties);
  stream << " numDevices = " << *(params->numDevices);
  stream << " outDevices = " << *(params->outDevices);
  stream << " numDevicesRet = " << *(params->numDevicesRet);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clCreateSubDevicesOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetDeviceAndHostTimerOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetDeviceAndHostTimer* params =
    reinterpret_cast<const cl_params_clGetDeviceAndHostTimer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " device = " << *(params->device);
  stream << " deviceTimestamp = " << *(params->deviceTimestamp);
  stream << " hostTimestamp = " << *(params->hostTimestamp);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetDeviceAndHostTimerOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clReleaseSamplerOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clReleaseSampler* params =
    reinterpret_cast<const cl_params_clReleaseSampler*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " sampler = " << *(params->sampler);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clReleaseSamplerOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueTaskOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueTask* params =
    reinterpret_cast<const cl_params_clEnqueueTask*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << " kernel = " << *(params->kernel);
  if (*(params->kernel) != nullptr) {
    std::string kernel_name = utils::cl::GetKernelName(
        *(params->kernel), collector->Demangle());
    if (!kernel_name.empty()) {
      stream << " (" << kernel_name << ")";
    }
  }
  stream << " numEventsInWaitList = " << *(params->numEventsInWaitList);
  stream << " eventWaitList = " << *(params->eventWaitList);
  stream << " event = " << *(params->event);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueTaskOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clFinishOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clFinish* params =
    reinterpret_cast<const cl_params_clFinish*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clFinishOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetEventInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetEventInfo* params =
    reinterpret_cast<const cl_params_clGetEventInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " event = " << *(params->event);
  stream << " paramName = " << *(params->paramName);
  stream << " paramValueSize = " << *(params->paramValueSize);
  stream << " paramValue = " << *(params->paramValue);
  stream << " paramValueSizeRet = " << *(params->paramValueSizeRet);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetEventInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetEventProfilingInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetEventProfilingInfo* params =
    reinterpret_cast<const cl_params_clGetEventProfilingInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " event = " << *(params->event);
  stream << " paramName = " << *(params->paramName);
  stream << " paramValueSize = " << *(params->paramValueSize);
  stream << " paramValue = " << *(params->paramValue);
  stream << " paramValueSizeRet = " << *(params->paramValueSizeRet);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetEventProfilingInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clSetKernelArgSVMPointerOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clSetKernelArgSVMPointer* params =
    reinterpret_cast<const cl_params_clSetKernelArgSVMPointer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " kernel = " << *(params->kernel);
  stream << " argIndex = " << *(params->argIndex);
  stream << " argValue = " << *(params->argValue);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clSetKernelArgSVMPointerOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clCreateImageOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateImage* params =
    reinterpret_cast<const cl_params_clCreateImage*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " context = " << *(params->context);
  stream << " flags = " << *(params->flags);
  stream << " imageFormat = " << *(params->imageFormat);
  stream << " imageDesc = " << *(params->imageDesc);
  stream << " hostPtr = " << *(params->hostPtr);
  stream << " errcodeRet = " << *(params->errcodeRet);
  stream << std::endl;

  collector->Log(stream.str());

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateImageOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  const cl_params_clCreateImage* params =
    reinterpret_cast<const cl_params_clCreateImage*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_mem* result =
    reinterpret_cast<cl_mem*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  stream << " result = " << *result;

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  stream << " -> " << utils::cl::GetErrorString(**(params->errcodeRet));
  stream << " (" << **(params->errcodeRet) << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueSVMMemcpyOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueSVMMemcpy* params =
    reinterpret_cast<const cl_params_clEnqueueSVMMemcpy*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << " blockingCopy = " << *(params->blockingCopy);
  stream << " dstPtr = " << *(params->dstPtr);
  stream << " srcPtr = " << *(params->srcPtr);
  stream << " size = " << *(params->size);
  stream << " numEventsInWaitList = " << *(params->numEventsInWaitList);
  stream << " eventWaitList = " << *(params->eventWaitList);
  stream << " event = " << *(params->event);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueSVMMemcpyOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clReleaseKernelOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clReleaseKernel* params =
    reinterpret_cast<const cl_params_clReleaseKernel*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " kernel = " << *(params->kernel);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clReleaseKernelOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueNativeKernelOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueNativeKernel* params =
    reinterpret_cast<const cl_params_clEnqueueNativeKernel*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << " userFunc = " << *(params->userFunc);
  stream << " args = " << *(params->args);
  stream << " cbArgs = " << *(params->cbArgs);
  stream << " numMemObjects = " << *(params->numMemObjects);
  stream << " memList = " << *(params->memList);
  stream << " argsMemLoc = " << *(params->argsMemLoc);
  stream << " numEventsInWaitList = " << *(params->numEventsInWaitList);
  stream << " eventWaitList = " << *(params->eventWaitList);
  stream << " event = " << *(params->event);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueNativeKernelOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clCreateKernelsInProgramOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateKernelsInProgram* params =
    reinterpret_cast<const cl_params_clCreateKernelsInProgram*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " program = " << *(params->program);
  stream << " numKernels = " << *(params->numKernels);
  stream << " kernels = " << *(params->kernels);
  stream << " numKernelsRet = " << *(params->numKernelsRet);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clCreateKernelsInProgramOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clSetCommandQueuePropertyOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clSetCommandQueueProperty* params =
    reinterpret_cast<const cl_params_clSetCommandQueueProperty*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << " properties = " << *(params->properties);
  stream << " enable = " << *(params->enable);
  stream << " oldProperties = " << *(params->oldProperties);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clSetCommandQueuePropertyOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetDeviceInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetDeviceInfo* params =
    reinterpret_cast<const cl_params_clGetDeviceInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " device = " << *(params->device);
  stream << " paramName = " << *(params->paramName);
  stream << " paramValueSize = " << *(params->paramValueSize);
  stream << " paramValue = " << *(params->paramValue);
  stream << " paramValueSizeRet = " << *(params->paramValueSizeRet);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetDeviceInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueNDRangeKernelOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueNDRangeKernel* params =
    reinterpret_cast<const cl_params_clEnqueueNDRangeKernel*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << " kernel = " << *(params->kernel);
  if (*(params->kernel) != nullptr) {
    std::string kernel_name = utils::cl::GetKernelName(
        *(params->kernel), collector->Demangle());
    if (!kernel_name.empty()) {
      stream << " (" << kernel_name << ")";
    }
  }
  stream << " workDim = " << *(params->workDim);
  stream << " globalWorkOffset = " << *(params->globalWorkOffset);
  if (*(params->globalWorkOffset) != nullptr && *(params->workDim) > 0) {
    stream << " {" << (*(params->globalWorkOffset))[0];
    for (cl_uint i = 1; i < *(params->workDim); ++i) {
      stream << ", " << (*(params->globalWorkOffset))[i];
    }
    stream << "}";
  }
  stream << " globalWorkSize = " << *(params->globalWorkSize);
  if (*(params->globalWorkSize) != nullptr && *(params->workDim) > 0) {
    stream << " {" << (*(params->globalWorkSize))[0];
    for (cl_uint i = 1; i < *(params->workDim); ++i) {
      stream << ", " << (*(params->globalWorkSize))[i];
    }
    stream << "}";
  }
  stream << " localWorkSize = " << *(params->localWorkSize);
  if (*(params->localWorkSize) != nullptr && *(params->workDim) > 0) {
    stream << " {" << (*(params->localWorkSize))[0];
    for (cl_uint i = 1; i < *(params->workDim); ++i) {
      stream << ", " << (*(params->localWorkSize))[i];
    }
    stream << "}";
  }
  stream << " numEventsInWaitList = " << *(params->numEventsInWaitList);
  stream << " eventWaitList = " << *(params->eventWaitList);
  stream << " event = " << *(params->event);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueNDRangeKernelOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);

  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  if (collector->GetKernelId() > 0) {
    stream << "(" << collector->GetKernelId() << ")";
  }
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clReleaseProgramOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clReleaseProgram* params =
    reinterpret_cast<const cl_params_clReleaseProgram*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " program = " << *(params->program);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clReleaseProgramOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clCreateFromGLBufferOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateFromGLBuffer* params =
    reinterpret_cast<const cl_params_clCreateFromGLBuffer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " context = " << *(params->context);
  stream << " flags = " << *(params->flags);
  stream << " bufobj = " << *(params->bufobj);
  stream << " errcodeRet = " << *(params->errcodeRet);
  stream << std::endl;

  collector->Log(stream.str());

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateFromGLBufferOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  const cl_params_clCreateFromGLBuffer* params =
    reinterpret_cast<const cl_params_clCreateFromGLBuffer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_mem* result =
    reinterpret_cast<cl_mem*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  stream << " result = " << *result;

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  stream << " -> " << utils::cl::GetErrorString(**(params->errcodeRet));
  stream << " (" << **(params->errcodeRet) << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetGLTextureInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetGLTextureInfo* params =
    reinterpret_cast<const cl_params_clGetGLTextureInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " memobj = " << *(params->memobj);
  stream << " paramName = " << *(params->paramName);
  stream << " paramValueSize = " << *(params->paramValueSize);
  stream << " paramValue = " << *(params->paramValue);
  stream << " paramValueSizeRet = " << *(params->paramValueSizeRet);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetGLTextureInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clSetDefaultDeviceCommandQueueOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clSetDefaultDeviceCommandQueue* params =
    reinterpret_cast<const cl_params_clSetDefaultDeviceCommandQueue*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " context = " << *(params->context);
  stream << " device = " << *(params->device);
  stream << " commandQueue = " << *(params->commandQueue);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clSetDefaultDeviceCommandQueueOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clCreatePipeOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreatePipe* params =
    reinterpret_cast<const cl_params_clCreatePipe*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " context = " << *(params->context);
  stream << " flags = " << *(params->flags);
  stream << " pipePacketSize = " << *(params->pipePacketSize);
  stream << " pipeMaxPackets = " << *(params->pipeMaxPackets);
  stream << " properties = " << *(params->properties);
  stream << " errcodeRet = " << *(params->errcodeRet);
  stream << std::endl;

  collector->Log(stream.str());

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreatePipeOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  const cl_params_clCreatePipe* params =
    reinterpret_cast<const cl_params_clCreatePipe*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_mem* result =
    reinterpret_cast<cl_mem*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  stream << " result = " << *result;

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  stream << " -> " << utils::cl::GetErrorString(**(params->errcodeRet));
  stream << " (" << **(params->errcodeRet) << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetPlatformInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetPlatformInfo* params =
    reinterpret_cast<const cl_params_clGetPlatformInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " platform = " << *(params->platform);
  stream << " paramName = " << *(params->paramName);
  stream << " paramValueSize = " << *(params->paramValueSize);
  stream << " paramValue = " << *(params->paramValue);
  stream << " paramValueSizeRet = " << *(params->paramValueSizeRet);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetPlatformInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueReadBufferOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueReadBuffer* params =
    reinterpret_cast<const cl_params_clEnqueueReadBuffer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << " buffer = " << *(params->buffer);
  stream << " blockingRead = " << *(params->blockingRead);
  stream << " offset = " << *(params->offset);
  stream << " cb = " << *(params->cb);
  stream << " ptr = " << *(params->ptr);
  stream << " numEventsInWaitList = " << *(params->numEventsInWaitList);
  stream << " eventWaitList = " << *(params->eventWaitList);
  stream << " event = " << *(params->event);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueReadBufferOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);

  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  if (collector->GetKernelId() > 0) {
    stream << "(" << collector->GetKernelId() << ")";
  }
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clSetMemObjectDestructorCallbackOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clSetMemObjectDestructorCallback* params =
    reinterpret_cast<const cl_params_clSetMemObjectDestructorCallback*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " memobj = " << *(params->memobj);
  stream << " funcNotify = " << *(params->funcNotify);
  stream << " userData = " << *(params->userData);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clSetMemObjectDestructorCallbackOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetKernelSubGroupInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetKernelSubGroupInfo* params =
    reinterpret_cast<const cl_params_clGetKernelSubGroupInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " kernel = " << *(params->kernel);
  stream << " device = " << *(params->device);
  stream << " paramName = " << *(params->paramName);
  stream << " inputValueSize = " << *(params->inputValueSize);
  stream << " inputValue = " << *(params->inputValue);
  stream << " paramValueSize = " << *(params->paramValueSize);
  stream << " paramValue = " << *(params->paramValue);
  stream << " paramValueSizeRet = " << *(params->paramValueSizeRet);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetKernelSubGroupInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueCopyBufferRectOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueCopyBufferRect* params =
    reinterpret_cast<const cl_params_clEnqueueCopyBufferRect*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << " srcBuffer = " << *(params->srcBuffer);
  stream << " dstBuffer = " << *(params->dstBuffer);
  stream << " srcOrigin = " << *(params->srcOrigin);
  stream << " dstOrigin = " << *(params->dstOrigin);
  stream << " region = " << *(params->region);
  stream << " srcRowPitch = " << *(params->srcRowPitch);
  stream << " srcSlicePitch = " << *(params->srcSlicePitch);
  stream << " dstRowPitch = " << *(params->dstRowPitch);
  stream << " dstSlicePitch = " << *(params->dstSlicePitch);
  stream << " numEventsInWaitList = " << *(params->numEventsInWaitList);
  stream << " eventWaitList = " << *(params->eventWaitList);
  stream << " event = " << *(params->event);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueCopyBufferRectOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clWaitForEventsOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clWaitForEvents* params =
    reinterpret_cast<const cl_params_clWaitForEvents*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " numEvents = " << *(params->numEvents);
  stream << " eventList = " << *(params->eventList);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clWaitForEventsOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueSVMMigrateMemOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueSVMMigrateMem* params =
    reinterpret_cast<const cl_params_clEnqueueSVMMigrateMem*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << " numSvmPointers = " << *(params->numSvmPointers);
  stream << " svmPointers = " << *(params->svmPointers);
  stream << " sizes = " << *(params->sizes);
  stream << " flags = " << *(params->flags);
  stream << " numEventsInWaitList = " << *(params->numEventsInWaitList);
  stream << " eventWaitList = " << *(params->eventWaitList);
  stream << " event = " << *(params->event);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueSVMMigrateMemOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clRetainKernelOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clRetainKernel* params =
    reinterpret_cast<const cl_params_clRetainKernel*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " kernel = " << *(params->kernel);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clRetainKernelOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clCreateCommandQueueWithPropertiesOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateCommandQueueWithProperties* params =
    reinterpret_cast<const cl_params_clCreateCommandQueueWithProperties*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " context = " << *(params->context);
  stream << " device = " << *(params->device);
  stream << " properties = " << *(params->properties);
  stream << " errcodeRet = " << *(params->errcodeRet);
  stream << std::endl;

  collector->Log(stream.str());

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateCommandQueueWithPropertiesOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  const cl_params_clCreateCommandQueueWithProperties* params =
    reinterpret_cast<const cl_params_clCreateCommandQueueWithProperties*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_command_queue* result =
    reinterpret_cast<cl_command_queue*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  stream << " result = " << *result;

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  stream << " -> " << utils::cl::GetErrorString(**(params->errcodeRet));
  stream << " (" << **(params->errcodeRet) << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clCreateProgramWithBuiltInKernelsOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateProgramWithBuiltInKernels* params =
    reinterpret_cast<const cl_params_clCreateProgramWithBuiltInKernels*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " context = " << *(params->context);
  stream << " numDevices = " << *(params->numDevices);
  stream << " deviceList = " << *(params->deviceList);
  if (*(params->kernelNames) == nullptr) {
    stream << " kernelNames = " << "0";
  } else if (strlen(*(params->kernelNames)) == 0) {
    stream << " kernelNames = \"\"";
  } else {
    stream << " kernelNames = \"" << *(params->kernelNames) << "\"";
  }
  stream << " errcodeRet = " << *(params->errcodeRet);
  stream << std::endl;

  collector->Log(stream.str());

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateProgramWithBuiltInKernelsOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  const cl_params_clCreateProgramWithBuiltInKernels* params =
    reinterpret_cast<const cl_params_clCreateProgramWithBuiltInKernels*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_program* result =
    reinterpret_cast<cl_program*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  stream << " result = " << *result;

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  stream << " -> " << utils::cl::GetErrorString(**(params->errcodeRet));
  stream << " (" << **(params->errcodeRet) << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clCreateBufferOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateBuffer* params =
    reinterpret_cast<const cl_params_clCreateBuffer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " context = " << *(params->context);
  stream << " flags = " << *(params->flags);
  stream << " size = " << *(params->size);
  stream << " hostPtr = " << *(params->hostPtr);
  stream << " errcodeRet = " << *(params->errcodeRet);
  stream << std::endl;

  collector->Log(stream.str());

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateBufferOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  const cl_params_clCreateBuffer* params =
    reinterpret_cast<const cl_params_clCreateBuffer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_mem* result =
    reinterpret_cast<cl_mem*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  stream << " result = " << *result;

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  stream << " -> " << utils::cl::GetErrorString(**(params->errcodeRet));
  stream << " (" << **(params->errcodeRet) << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetProgramBuildInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetProgramBuildInfo* params =
    reinterpret_cast<const cl_params_clGetProgramBuildInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " program = " << *(params->program);
  stream << " device = " << *(params->device);
  stream << " paramName = " << *(params->paramName);
  stream << " paramValueSize = " << *(params->paramValueSize);
  stream << " paramValue = " << *(params->paramValue);
  stream << " paramValueSizeRet = " << *(params->paramValueSizeRet);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetProgramBuildInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueFillBufferOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueFillBuffer* params =
    reinterpret_cast<const cl_params_clEnqueueFillBuffer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << " buffer = " << *(params->buffer);
  stream << " pattern = " << *(params->pattern);
  stream << " patternSize = " << *(params->patternSize);
  stream << " offset = " << *(params->offset);
  stream << " size = " << *(params->size);
  stream << " numEventsInWaitList = " << *(params->numEventsInWaitList);
  stream << " eventWaitList = " << *(params->eventWaitList);
  stream << " event = " << *(params->event);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueFillBufferOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueReadImageOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueReadImage* params =
    reinterpret_cast<const cl_params_clEnqueueReadImage*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << " image = " << *(params->image);
  stream << " blockingRead = " << *(params->blockingRead);
  stream << " origin = " << *(params->origin);
  stream << " region = " << *(params->region);
  stream << " rowPitch = " << *(params->rowPitch);
  stream << " slicePitch = " << *(params->slicePitch);
  stream << " ptr = " << *(params->ptr);
  stream << " numEventsInWaitList = " << *(params->numEventsInWaitList);
  stream << " eventWaitList = " << *(params->eventWaitList);
  stream << " event = " << *(params->event);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueReadImageOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueWriteBufferRectOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueWriteBufferRect* params =
    reinterpret_cast<const cl_params_clEnqueueWriteBufferRect*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << " buffer = " << *(params->buffer);
  stream << " blockingWrite = " << *(params->blockingWrite);
  stream << " bufferOrigin = " << *(params->bufferOrigin);
  stream << " hostOrigin = " << *(params->hostOrigin);
  stream << " region = " << *(params->region);
  stream << " bufferRowPitch = " << *(params->bufferRowPitch);
  stream << " bufferSlicePitch = " << *(params->bufferSlicePitch);
  stream << " hostRowPitch = " << *(params->hostRowPitch);
  stream << " hostSlicePitch = " << *(params->hostSlicePitch);
  stream << " ptr = " << *(params->ptr);
  stream << " numEventsInWaitList = " << *(params->numEventsInWaitList);
  stream << " eventWaitList = " << *(params->eventWaitList);
  stream << " event = " << *(params->event);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueWriteBufferRectOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueCopyBufferToImageOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueCopyBufferToImage* params =
    reinterpret_cast<const cl_params_clEnqueueCopyBufferToImage*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << " srcBuffer = " << *(params->srcBuffer);
  stream << " dstImage = " << *(params->dstImage);
  stream << " srcOffset = " << *(params->srcOffset);
  stream << " dstOrigin = " << *(params->dstOrigin);
  stream << " region = " << *(params->region);
  stream << " numEventsInWaitList = " << *(params->numEventsInWaitList);
  stream << " eventWaitList = " << *(params->eventWaitList);
  stream << " event = " << *(params->event);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueCopyBufferToImageOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetExtensionFunctionAddressForPlatformOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetExtensionFunctionAddressForPlatform* params =
    reinterpret_cast<
      const cl_params_clGetExtensionFunctionAddressForPlatform*>(
          data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " platform = " << *(params->platform);
  if (*(params->funcName) == nullptr) {
    stream << " funcName = " << "0";
  } else if (strlen(*(params->funcName)) == 0) {
    stream << " funcName = \"\"";
  } else {
    stream << " funcName = \"" << *(params->funcName) << "\"";
  }
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetExtensionFunctionAddressForPlatformOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  void** result =
    reinterpret_cast<void**>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  stream << " result = " << *result;
  stream << std::endl;

  collector->Log(stream.str());
}

static void clSetKernelArgOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clSetKernelArg* params =
    reinterpret_cast<const cl_params_clSetKernelArg*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " kernel = " << *(params->kernel);
  stream << " argIndex = " << *(params->argIndex);
  stream << " argSize = " << *(params->argSize);
  stream << " argValue = " << *(params->argValue);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clSetKernelArgOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clReleaseDeviceOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clReleaseDevice* params =
    reinterpret_cast<const cl_params_clReleaseDevice*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " device = " << *(params->device);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clReleaseDeviceOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clCreateSubBufferOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateSubBuffer* params =
    reinterpret_cast<const cl_params_clCreateSubBuffer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " buffer = " << *(params->buffer);
  stream << " flags = " << *(params->flags);
  stream << " bufferCreateType = " << *(params->bufferCreateType);
  stream << " bufferCreateInfo = " << *(params->bufferCreateInfo);
  stream << " errcodeRet = " << *(params->errcodeRet);
  stream << std::endl;

  collector->Log(stream.str());

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateSubBufferOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  const cl_params_clCreateSubBuffer* params =
    reinterpret_cast<const cl_params_clCreateSubBuffer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_mem* result =
    reinterpret_cast<cl_mem*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  stream << " result = " << *result;

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  stream << " -> " << utils::cl::GetErrorString(**(params->errcodeRet));
  stream << " (" << **(params->errcodeRet) << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueMigrateMemObjectsOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueMigrateMemObjects* params =
    reinterpret_cast<const cl_params_clEnqueueMigrateMemObjects*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << " numMemObjects = " << *(params->numMemObjects);
  stream << " memObjects = " << *(params->memObjects);
  stream << " flags = " << *(params->flags);
  stream << " numEventsInWaitList = " << *(params->numEventsInWaitList);
  stream << " eventWaitList = " << *(params->eventWaitList);
  stream << " event = " << *(params->event);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueMigrateMemObjectsOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clCreateCommandQueueOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateCommandQueue* params =
    reinterpret_cast<const cl_params_clCreateCommandQueue*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " context = " << *(params->context);
  stream << " device = " << *(params->device);
  stream << " properties = " << *(params->properties);
  stream << " errcodeRet = " << *(params->errcodeRet);
  stream << std::endl;

  collector->Log(stream.str());

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateCommandQueueOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  const cl_params_clCreateCommandQueue* params =
    reinterpret_cast<const cl_params_clCreateCommandQueue*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_command_queue* result =
    reinterpret_cast<cl_command_queue*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  stream << " result = " << *result;

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  stream << " -> " << utils::cl::GetErrorString(**(params->errcodeRet));
  stream << " (" << **(params->errcodeRet) << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueSVMMemFillOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueSVMMemFill* params =
    reinterpret_cast<const cl_params_clEnqueueSVMMemFill*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << " svmPtr = " << *(params->svmPtr);
  stream << " pattern = " << *(params->pattern);
  stream << " patternSize = " << *(params->patternSize);
  stream << " size = " << *(params->size);
  stream << " numEventsInWaitList = " << *(params->numEventsInWaitList);
  stream << " eventWaitList = " << *(params->eventWaitList);
  stream << " event = " << *(params->event);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueSVMMemFillOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clReleaseCommandQueueOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clReleaseCommandQueue* params =
    reinterpret_cast<const cl_params_clReleaseCommandQueue*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clReleaseCommandQueueOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueCopyBufferOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueCopyBuffer* params =
    reinterpret_cast<const cl_params_clEnqueueCopyBuffer*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << " srcBuffer = " << *(params->srcBuffer);
  stream << " dstBuffer = " << *(params->dstBuffer);
  stream << " srcOffset = " << *(params->srcOffset);
  stream << " dstOffset = " << *(params->dstOffset);
  stream << " cb = " << *(params->cb);
  stream << " numEventsInWaitList = " << *(params->numEventsInWaitList);
  stream << " eventWaitList = " << *(params->eventWaitList);
  stream << " event = " << *(params->event);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueCopyBufferOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetCommandQueueInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetCommandQueueInfo* params =
    reinterpret_cast<const cl_params_clGetCommandQueueInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << " paramName = " << *(params->paramName);
  stream << " paramValueSize = " << *(params->paramValueSize);
  stream << " paramValue = " << *(params->paramValue);
  stream << " paramValueSizeRet = " << *(params->paramValueSizeRet);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetCommandQueueInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clBuildProgramOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clBuildProgram* params =
    reinterpret_cast<const cl_params_clBuildProgram*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " program = " << *(params->program);
  stream << " numDevices = " << *(params->numDevices);
  stream << " deviceList = " << *(params->deviceList);
  if (*(params->options) == nullptr) {
    stream << " options = " << "0";
  } else if (strlen(*(params->options)) == 0) {
    stream << " options = \"\"";
  } else {
    stream << " options = \"" << *(params->options) << "\"";
  }
  stream << " funcNotify = " << *(params->funcNotify);
  stream << " userData = " << *(params->userData);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clBuildProgramOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clRetainContextOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clRetainContext* params =
    reinterpret_cast<const cl_params_clRetainContext*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " context = " << *(params->context);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clRetainContextOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueBarrierOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueBarrier* params =
    reinterpret_cast<const cl_params_clEnqueueBarrier*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueBarrierOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clRetainDeviceOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clRetainDevice* params =
    reinterpret_cast<const cl_params_clRetainDevice*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " device = " << *(params->device);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clRetainDeviceOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueSVMMapOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueSVMMap* params =
    reinterpret_cast<const cl_params_clEnqueueSVMMap*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << " blockingMap = " << *(params->blockingMap);
  stream << " mapFlags = " << *(params->mapFlags);
  stream << " svmPtr = " << *(params->svmPtr);
  stream << " size = " << *(params->size);
  stream << " numEventsInWaitList = " << *(params->numEventsInWaitList);
  stream << " eventWaitList = " << *(params->eventWaitList);
  stream << " event = " << *(params->event);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueSVMMapOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clRetainMemObjectOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clRetainMemObject* params =
    reinterpret_cast<const cl_params_clRetainMemObject*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " memobj = " << *(params->memobj);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clRetainMemObjectOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clSetUserEventStatusOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clSetUserEventStatus* params =
    reinterpret_cast<const cl_params_clSetUserEventStatus*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " event = " << *(params->event);
  stream << " executionStatus = " << *(params->executionStatus);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clSetUserEventStatusOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clCreateUserEventOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateUserEvent* params =
    reinterpret_cast<const cl_params_clCreateUserEvent*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " context = " << *(params->context);
  stream << " errcodeRet = " << *(params->errcodeRet);
  stream << std::endl;

  collector->Log(stream.str());

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateUserEventOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  const cl_params_clCreateUserEvent* params =
    reinterpret_cast<const cl_params_clCreateUserEvent*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_event* result =
    reinterpret_cast<cl_event*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  stream << " result = " << *result;

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  stream << " -> " << utils::cl::GetErrorString(**(params->errcodeRet));
  stream << " (" << **(params->errcodeRet) << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetSamplerInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetSamplerInfo* params =
    reinterpret_cast<const cl_params_clGetSamplerInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " sampler = " << *(params->sampler);
  stream << " paramName = " << *(params->paramName);
  stream << " paramValueSize = " << *(params->paramValueSize);
  stream << " paramValue = " << *(params->paramValue);
  stream << " paramValueSizeRet = " << *(params->paramValueSizeRet);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetSamplerInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueMarkerOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueMarker* params =
    reinterpret_cast<const cl_params_clEnqueueMarker*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << " event = " << *(params->event);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueMarkerOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clCreateKernelOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateKernel* params =
    reinterpret_cast<const cl_params_clCreateKernel*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " program = " << *(params->program);
  if (*(params->kernelName) == nullptr) {
    stream << " kernelName = " << "0";
  } else if (strlen(*(params->kernelName)) == 0) {
    stream << " kernelName = \"\"";
  } else {
    stream << " kernelName = \"" << *(params->kernelName) << "\"";
    if (collector->Demangle()) {
      stream << " (" << utils::Demangle(*(params->kernelName)) << ")";
    }
  }
  stream << " errcodeRet = " << *(params->errcodeRet);
  stream << std::endl;

  collector->Log(stream.str());

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateKernelOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  const cl_params_clCreateKernel* params =
    reinterpret_cast<const cl_params_clCreateKernel*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_kernel* result =
    reinterpret_cast<cl_kernel*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  stream << " result = " << *result;

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  stream << " -> " << utils::cl::GetErrorString(**(params->errcodeRet));
  stream << " (" << **(params->errcodeRet) << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetProgramInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetProgramInfo* params =
    reinterpret_cast<const cl_params_clGetProgramInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " program = " << *(params->program);
  stream << " paramName = " << *(params->paramName);
  stream << " paramValueSize = " << *(params->paramValueSize);
  stream << " paramValue = " << *(params->paramValue);
  stream << " paramValueSizeRet = " << *(params->paramValueSizeRet);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetProgramInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clSVMAllocOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clSVMAlloc* params =
    reinterpret_cast<const cl_params_clSVMAlloc*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " context = " << *(params->context);
  stream << " flags = " << *(params->flags);
  stream << " size = " << *(params->size);
  stream << " alignment = " << *(params->alignment);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clSVMAllocOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  void ** result =
    reinterpret_cast<void **>(
        data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  stream << " result = " << *result;
  stream << std::endl;

  collector->Log(stream.str());
}

static void clRetainEventOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clRetainEvent* params =
    reinterpret_cast<const cl_params_clRetainEvent*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " event = " << *(params->event);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clRetainEventOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clCloneKernelOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCloneKernel* params =
    reinterpret_cast<const cl_params_clCloneKernel*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " sourceKernel = " << *(params->sourceKernel);
  stream << " errcodeRet = " << *(params->errcodeRet);
  stream << std::endl;

  collector->Log(stream.str());

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCloneKernelOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  const cl_params_clCloneKernel* params =
    reinterpret_cast<const cl_params_clCloneKernel*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_kernel* result =
    reinterpret_cast<cl_kernel*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  stream << " result = " << *result;

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  stream << " -> " << utils::cl::GetErrorString(**(params->errcodeRet));
  stream << " (" << **(params->errcodeRet) << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetImageInfoOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clGetImageInfo* params =
    reinterpret_cast<const cl_params_clGetImageInfo*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " image = " << *(params->image);
  stream << " paramName = " << *(params->paramName);
  stream << " paramValueSize = " << *(params->paramValueSize);
  stream << " paramValue = " << *(params->paramValue);
  stream << " paramValueSizeRet = " << *(params->paramValueSizeRet);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clGetImageInfoOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clFlushOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clFlush* params =
    reinterpret_cast<const cl_params_clFlush*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clFlushOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueMarkerWithWaitListOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clEnqueueMarkerWithWaitList* params =
    reinterpret_cast<const cl_params_clEnqueueMarkerWithWaitList*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " commandQueue = " << *(params->commandQueue);
  stream << " numEventsInWaitList = " << *(params->numEventsInWaitList);
  stream << " eventWaitList = " << *(params->eventWaitList);
  stream << " event = " << *(params->event);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clEnqueueMarkerWithWaitListOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clCreateProgramWithILOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateProgramWithIL* params =
    reinterpret_cast<const cl_params_clCreateProgramWithIL*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " context = " << *(params->context);
  stream << " il = " << *(params->il);
  stream << " length = " << *(params->length);
  stream << " errcodeRet = " << *(params->errcodeRet);
  stream << std::endl;

  collector->Log(stream.str());

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateProgramWithILOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  const cl_params_clCreateProgramWithIL* params =
    reinterpret_cast<const cl_params_clCreateProgramWithIL*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_program* result =
    reinterpret_cast<cl_program*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  stream << " result = " << *result;

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  stream << " -> " << utils::cl::GetErrorString(**(params->errcodeRet));
  stream << " (" << **(params->errcodeRet) << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clCreateSamplerOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateSampler* params =
    reinterpret_cast<const cl_params_clCreateSampler*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " context = " << *(params->context);
  stream << " normalizedCoords = " << *(params->normalizedCoords);
  stream << " addressingMode = " << *(params->addressingMode);
  stream << " filterMode = " << *(params->filterMode);
  stream << " errcodeRet = " << *(params->errcodeRet);
  stream << std::endl;

  collector->Log(stream.str());

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateSamplerOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  const cl_params_clCreateSampler* params =
    reinterpret_cast<const cl_params_clCreateSampler*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_sampler* result =
    reinterpret_cast<cl_sampler*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  stream << " result = " << *result;

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  stream << " -> " << utils::cl::GetErrorString(**(params->errcodeRet));
  stream << " (" << **(params->errcodeRet) << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clCreateFromGLTextureOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clCreateFromGLTexture* params =
    reinterpret_cast<const cl_params_clCreateFromGLTexture*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " context = " << *(params->context);
  stream << " flags = " << *(params->flags);
  stream << " target = " << *(params->target);
  stream << " miplevel = " << *(params->miplevel);
  stream << " texture = " << *(params->texture);
  stream << " errcodeRet = " << *(params->errcodeRet);
  stream << std::endl;

  collector->Log(stream.str());

  if (*(params->errcodeRet) == nullptr) {
    *(params->errcodeRet) = &current_error;
  }
}

static void clCreateFromGLTextureOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  const cl_params_clCreateFromGLTexture* params =
    reinterpret_cast<const cl_params_clCreateFromGLTexture*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  cl_mem* result =
    reinterpret_cast<cl_mem*>(data->functionReturnValue);
  PTI_ASSERT(result != nullptr);
  stream << " result = " << *result;

  PTI_ASSERT(*(params->errcodeRet) != nullptr);
  stream << " -> " << utils::cl::GetErrorString(**(params->errcodeRet));
  stream << " (" << **(params->errcodeRet) << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

static void clSVMFreeOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clSVMFree* params =
    reinterpret_cast<const cl_params_clSVMFree*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " context = " << *(params->context);
  stream << " svmPointer = " << *(params->svmPointer);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clSVMFreeOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  stream << std::endl;

  collector->Log(stream.str());
}

static void clReleaseEventOnEnter(
    cl_callback_data* data, uint64_t start, ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  const cl_params_clReleaseEvent* params =
    reinterpret_cast<const cl_params_clReleaseEvent*>(
        data->functionParams);
  PTI_ASSERT(params != nullptr);

  std::stringstream stream;
  stream << ">>>> [" << start << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName << ":";

  stream << " event = " << *(params->event);
  stream << std::endl;

  collector->Log(stream.str());
}

static void clReleaseEventOnExit(
    cl_callback_data* data, uint64_t start, uint64_t end,
    ClCollector* collector) {
  PTI_ASSERT(collector != nullptr);
  std::stringstream stream;
  stream << "<<<< [" << end << "] ";
  if (collector->NeedPid()) {
    stream << "<PID:" << utils::GetPid() << "> ";
  }
  if (collector->NeedTid()) {
    stream << "<TID:" << utils::GetTid() << "> ";
  }
  stream << data->functionName;
  stream << " [" << (end - start) << " ns]";

  cl_int* error = reinterpret_cast<cl_int*>(data->functionReturnValue);
  PTI_ASSERT(error != nullptr);

  stream << " -> " << utils::cl::GetErrorString(*error);
  stream << " (" << *error << ")";
  stream << std::endl;

  collector->Log(stream.str());
}

void OnEnterFunction(
    cl_function_id function, cl_callback_data* data,
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
    cl_function_id function, cl_callback_data* data,
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

#endif // PTI_TOOLS_CL_TRACER_CL_API_CALLBACKS_H_

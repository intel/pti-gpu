//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_GTPIN_DEF_GPU_H
#define PTI_GTPIN_DEF_GPU_H

#include <api/gtpin_api.h>
#include <stdint.h>

#include <cstdint>
#include <iostream>

#include "pti_assert.h"

/**
 * @file def_gpu.hpp
 * @brief This file contains the definition of GPU-related classes and types used by GTPin.
 */

#define STR_HELPER(...) #__VA_ARGS__
#define STR(...) STR_HELPER(__VA_ARGS__)

#define GTPIN_LAST_ERROR_STR \
  (std::string("GTPin error:\n") + gtpin::GTPin_GetCore()->LastError().ToString() + "\n")

#ifdef _WIN32
#define DEL '\\'
#else
#define DEL '/'
#endif

#define MAX_SIMD_WIDTH_BYTES 4
#define MAX_SIMD_WIDTH 8 * MAX_SIMD_WIDTH_BYTES
#define MAX_SIMD_WIDTH_STR STR(MAX_SIMD_WIDTH)
#define CACHE_LINE_SIZE_BYTES 64
#define CACHE_LINE_SIZE_BITS 8 * CACHE_LINE_SIZE_BYTES

namespace gtpin_prof {

typedef enum { SUCCESS = 0, ERROR, NOTHING_TO_INSTRUMENT, WRONG_CONTROL, STOP, ACTIVE } PROF_STATUS;

typedef uint32_t KernelRun;  //!< kernel run index
typedef uint32_t KernelId;   //!< unique kernel ID
static_assert(sizeof(gtpin::GtKernelId) <=
              sizeof(KernelId));     // KernelId should be conversable from GtKernelId
typedef uint32_t InstructionOffset;  //!< instruction offset
typedef uint64_t DispatchId;         //!< dispatch ID (unique for each kernel invocation); should be
                              //!< conversable from return value of IGtKernelDispatch::DispatchId()

class GTPinTool;
using GTPinToolSPtr = std::shared_ptr<GTPinTool>;
class ApplicationData;
using ApplicationDataSPtr = std::shared_ptr<ApplicationData>;
class KernelData;
using KernelDataSPtr = std::shared_ptr<KernelData>;
class InvocationData;
using InvocationDataSPtr = std::shared_ptr<InvocationData>;
class ResultData;
using ResultDataSPtr = std::shared_ptr<ResultData>;
class ControlBase;
using ControlBaseSPtr = std::shared_ptr<ControlBase>;
class WriterBase;
using WriterBaseSPtr = std::shared_ptr<WriterBase>;
class ResultDataCommon;
using ResultDataCommonSPtr = std::shared_ptr<ResultDataCommon>;
class ToolFactory;
using ToolFactorySPtr = std::shared_ptr<ToolFactory>;
class SiteOfInstrument;
using SiteOfInstrumentSPtr = std::shared_ptr<SiteOfInstrument>;

/**
 * @class KernelBuildDescriptor
 * @brief Class that describes a kernel during the build process.
 */
class KernelBuildDescriptor {
 public:
  KernelBuildDescriptor(const gtpin::IGtKernelInstrument& instrumentor)
      : kernelName(instrumentor.Kernel().Name().Get()), kernelId(instrumentor.Kernel().Id()){};

  std::string kernelName;  //!< full kernel name
  KernelId kernelId;       //!< unique ID
};

/**
 * @class KernelExecDescriptor
 * @brief The KernelExecDescriptor class provides a detailed description of a kernel invocation. It
 * contains information such as the full kernel name, unique ID, global and local invocation
 * indices, dispatch ID, execution descriptor, and GPU platform.
 */
class KernelExecDescriptor {
 public:
  KernelExecDescriptor(const gtpin::IGtKernelDispatch& dispatcher, KernelRun runGlobalIdx,
                       KernelRun runIdx = -1)
      : kernelName(dispatcher.Kernel().Name().Get()),
        kernelId(dispatcher.Kernel().Id()),
        runGlobalIdx(runGlobalIdx),
        runIdx(runIdx),
        dispatchId(dispatcher.DispatchId()) {
    dispatcher.GetExecDescriptor(gtExecDesc);
    gpuPlatform = dispatcher.Kernel().GpuPlatform();
  };

  std::string kernelName = "";  //!< full kernel name
  KernelId kernelId = -1;       //!< unique kernel ID
  KernelRun runGlobalIdx = -1;  //!< kernel run global index, global invocation
  KernelRun runIdx = -1;        //!< kernel run index, invocation
  DispatchId dispatchId = -1;
  gtpin::GtKernelExecDesc gtExecDesc;
  gtpin::GtGpuPlatform gpuPlatform;
};

}  // namespace gtpin_prof

#endif  // PTI_GTPIN_DEF_GPU_H

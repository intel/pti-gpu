#ifndef DEF_GPU_GTPIN_H
#define DEF_GPU_GTPIN_H

#include <api/gtpin_api.h>
#include <stdint.h>

#include <cstdint>
#include <iostream>

#include "pti_assert.h"

#define STR_HELPER(...) #__VA_ARGS__
#define STR(...) STR_HELPER(__VA_ARGS__)

#define GTPIN_LAST_ERROR_STR \
  (std::string("GTPin error:\n") + GTPin_GetCore()->LastError().ToString() + "\n")

#ifdef _WIN32
#define DEL '\\'
#else
#define DEL '/'
#endif

#define MAX_SIMD_WIDTH_BYTES 4
#define MAX_SIMD_WIDTH 8 * MAX_SIMD_WIDTH_BYTES
#define MAX_SIMD_WIDTH_STR STR(MAX_SIMD_WIDTH)

using namespace gtpin;

namespace gtpin_prof {

enum PROF_STATUS { PROF_STATUS_SUCCESS = 0, PROF_STATUS_ERROR, PROF_STATUS_NOTHING_TO_INSTRUMENT };

typedef uint32_t KernelRun;
typedef uint32_t KernelId;
typedef uint32_t InstructionOffset;

/**
 * @brief KernelExecDesriptor describes the kernel invocation
 */
struct KernelExecDesriptor {
  KernelExecDesriptor(std::string kernelName, KernelId id, KernelRun runGlobalIdx,
                      KernelRun runIdx = -1)
      : kernelName(kernelName), id(id), runGlobalIdx(runGlobalIdx), runIdx(runIdx){};

  std::string kernelName = "";  //!< full kernel name
  KernelId id = -1;             //!< unique ID
  KernelRun runGlobalIdx = -1;  //!< kernel run global index, global ivocation
  KernelRun runIdx = -1;        //!< kernel run index, ivocation
};

/**
 * @brief HWProfileMask is used as bitmask for profiling
 */
struct HWProfileMask {
  uint32_t slice = -1;
  uint32_t subSlice = -1;
  uint32_t dualSlice = -1;
  uint32_t EU = -1;
  uint32_t thread = -1;
};

/**
 * @brief GTPinKernelBuildDescriptor describes kernel during build process. GTPin
 * specific
 */
struct GTPinKernelBuildDescriptor {
 public:
  GTPinKernelBuildDescriptor(std::string kernelName, KernelId id)
      : kernelName(kernelName), id(id){};
  GTPinKernelBuildDescriptor(const IGtKernelInstrument& instrumentor)
      : GTPinKernelBuildDescriptor(instrumentor.Kernel().Name().Get(),
                                   instrumentor.Kernel().Id()){};

  std::string kernelName;  //!< full kernel name
  KernelId id;             //!< unique ID
};

/**
 * @brief GTPinKernelExecDesriptor describes the kernel invocation with additional
 * GTPin-specific data
 */
struct GTPinKernelExecDesriptor : public KernelExecDesriptor {
 public:
  GTPinKernelExecDesriptor(std::string kernelName, KernelId id, uint64_t dispatchId,
                           KernelRun runGlobalIdx, KernelRun runIdx = -1)
      : gtpinDispatchId(dispatchId), KernelExecDesriptor(kernelName, id, runGlobalIdx, runIdx){};

  GTPinKernelExecDesriptor(const IGtKernelDispatch& dispatcher, KernelRun runGlobalIdx,
                           KernelRun runIdx = -1)
      :  /// TODO: check that bug fixed, use next gtpinDispatchId instead of
         /// enqueue-based one gtpinDispatchId(dispatcher.DispatchId()),
        KernelExecDesriptor(dispatcher.Kernel().Name().Get(), dispatcher.Kernel().Id(),
                            runGlobalIdx, runIdx) {
    GtKernelExecDesc execDesc;
    dispatcher.GetExecDescriptor(execDesc);
    gtpinDispatchId = execDesc.l0ExecDesc.enqueue_index;
  };

  uint64_t gtpinDispatchId = -1;
};

}  // namespace gtpin_prof

#endif  // DEF_GPU_GTPIN_H

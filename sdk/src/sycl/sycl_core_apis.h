//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef PTI_SRC_SYCL_SYCL_CORE_APIS_H_
#define PTI_SRC_SYCL_SYCL_CORE_APIS_H_

#include <array>

#include "pti/pti_runtime_sycl_api_ids.h"

// Classification of UR API calls that correspond to GPU operations submitted via sycl::queue.
// Single source of truth used by:
//   - sycl_collector.h  — to route callbacks to kernel / memory / graph handling
//   - view_handler.h    — to enable/disable these APIs as a granularity class
enum class ApiType { kInvalid = 0, kKernel = 1, kMemory = 2, kGraph = 3 };

struct SyclCoreApi {
  pti_api_id_runtime_sycl id;
  ApiType type;
};

inline constexpr std::array kSyclCoreApis = {
    SyclCoreApi{pti_api_id_runtime_sycl::urEnqueueUSMFill_id, ApiType::kMemory},
    SyclCoreApi{pti_api_id_runtime_sycl::urEnqueueUSMFill2D_id, ApiType::kMemory},
    SyclCoreApi{pti_api_id_runtime_sycl::urEnqueueUSMMemcpy_id, ApiType::kMemory},
    SyclCoreApi{pti_api_id_runtime_sycl::urEnqueueUSMMemcpy2D_id, ApiType::kMemory},
    SyclCoreApi{pti_api_id_runtime_sycl::urEnqueueKernelLaunch_id, ApiType::kKernel},
    // two APIs below removed in 2025.3+ compiler runtime,
    // but we keep them around for the case when someone runs with earlier runtime
    SyclCoreApi{pti_api_id_runtime_sycl::urEnqueueKernelLaunchCustomExp_id, ApiType::kKernel},
    SyclCoreApi{pti_api_id_runtime_sycl::urEnqueueCooperativeKernelLaunchExp_id, ApiType::kKernel},
    SyclCoreApi{pti_api_id_runtime_sycl::urEnqueueKernelLaunchWithArgsExp_id, ApiType::kKernel},
    SyclCoreApi{pti_api_id_runtime_sycl::urEnqueueMemBufferFill_id, ApiType::kMemory},
    SyclCoreApi{pti_api_id_runtime_sycl::urEnqueueMemBufferRead_id, ApiType::kMemory},
    SyclCoreApi{pti_api_id_runtime_sycl::urEnqueueMemBufferWrite_id, ApiType::kMemory},
    SyclCoreApi{pti_api_id_runtime_sycl::urEnqueueMemBufferCopy_id, ApiType::kMemory},
    SyclCoreApi{pti_api_id_runtime_sycl::urUSMHostAlloc_id, ApiType::kMemory},
    SyclCoreApi{pti_api_id_runtime_sycl::urUSMSharedAlloc_id, ApiType::kMemory},
    SyclCoreApi{pti_api_id_runtime_sycl::urUSMDeviceAlloc_id, ApiType::kMemory},
    SyclCoreApi{pti_api_id_runtime_sycl::urEnqueueCommandBufferExp_id, ApiType::kGraph},
    SyclCoreApi{pti_api_id_runtime_sycl::urEnqueueGraphExp_id, ApiType::kGraph},
};

// kSyclCoreApis must stay within the compiler's full-unroll budget.
// Benchmarking shows that icpx -O2 completely unrolls the linear scan up to
// N=36; at N=37 it switches to a partial unroll and performance degrades ~5x.
// If this array grows beyond 36 entries, switch GetApiType to a sparse array
// indexed by (id - min_id) for O(1) lookup without hashing overhead.
// See: https://github.com/jfedorov/snippets/tree/main/perf_array_vs_unordered_map
static_assert(kSyclCoreApis.size() <= 36,
              "kSyclCoreApis exceeds the compiler unroll threshold (36). "
              "Switch GetApiType to a sparse array lookup for better performance. "
              "See https://github.com/jfedorov/snippets/tree/main/perf_array_vs_unordered_map");

// Returns ApiType for the given api_id, or ApiType::kInvalid if not a core API.
// Linear scan is intentional: benchmarking shows that for N <= 36 a completely
// unrolled scan over a contiguous constexpr array is faster than both
// std::unordered_map and a sparse array lookup.
inline constexpr ApiType GetApiType(pti_api_id_runtime_sycl api_id) noexcept {
  for (const auto& entry : kSyclCoreApis) {
    if (entry.id == api_id) return entry.type;
  }
  return ApiType::kInvalid;
}

#endif  // PTI_SRC_SYCL_SYCL_CORE_APIS_H_

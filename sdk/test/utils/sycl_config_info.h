#ifndef TEST_UTILS_SYCL_CONFIG_INFO_H_
#define TEST_UTILS_SYCL_CONFIG_INFO_H_

#include <level_zero/ze_api.h>

#include <sycl/ext/oneapi/backend/level_zero.hpp>
#include <sycl/sycl.hpp>

#include "ze_config_info.h"

#if defined(__SYCL_COMPILER_VERSION) && defined(__LIBSYCL_MAJOR_VERSION)
#if __SYCL_COMPILER_VERSION >= 20260724 && __LIBSYCL_MAJOR_VERSION >= 9
#define PTI_TEST_NATIVE_GRAPH_RECORDING_API_AVAILABLE
#endif
#endif

namespace pti::test::utils {
[[nodiscard]] inline bool IsIntegratedGraphics(const sycl::device& device) {
  if (!device.is_gpu()) {
    return false;
  }

  // Ideally, we want get_info<sycl::info::device::host_unified_memory> or
  // to do this via sycl but that seems to be deprecated with no replacement.
  bool result = false;
  if (device.get_backend() == sycl::backend::ext_oneapi_level_zero) {
    auto* device_handle = sycl::get_native<sycl::backend::ext_oneapi_level_zero>(device);
    result = level_zero::CheckIntegratedGraphics(device_handle);
  }

  // TODO(PTI): add supported backends as they come and move implementation to .cc

  return result;
}

[[nodiscard]] inline bool CommandListVisitAvailable() {
  return level_zero::CommandListVisitAvailable();
}

[[nodiscard]] inline bool NativeGraphApisAvailable() {
  return level_zero::NativeGraphApisAvailable();
}

}  // namespace pti::test::utils

#endif  // TEST_UTILS_SYCL_CONFIG_INFO_H_

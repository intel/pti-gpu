#ifndef TEST_UTILS_SYCL_CONFIG_INFO_H_
#define TEST_UTILS_SYCL_CONFIG_INFO_H_

#include <level_zero/ze_api.h>

#include <sycl/ext/oneapi/backend/level_zero.hpp>
#include <sycl/sycl.hpp>

#include "ze_config_info.h"

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

  // TODO: add supported backends as they come and move implementation to .cc

  return result;
}

}  // namespace pti::test::utils

#endif  // TEST_UTILS_SYCL_CONFIG_INFO_H_

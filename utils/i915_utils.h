//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_UTILS_I915_UTILS_H_
#define PTI_UTILS_I915_UTILS_H_

#if defined(__linux__)
#include <drm/i915_drm.h>
#include <xf86drm.h>

#define I915_TIMESTAMP_LOW_OFFSET 0x2358

#ifndef I915_PARAM_CS_TIMESTAMP_FREQUENCY
#define I915_PARAM_CS_TIMESTAMP_FREQUENCY 51
#endif

#endif

#include "utils.h"
#include "ze_utils.h"

namespace utils {
namespace i915 {

inline uint64_t GetGpuTimerFrequencyFromL0() {
  ze_driver_handle_t driver = nullptr;
  ze_device_handle_t device = nullptr;
  utils::ze::GetIntelDeviceAndDriver(ZE_DEVICE_TYPE_GPU, device, driver);
  PTI_ASSERT(device != nullptr);
  PTI_ASSERT(driver != nullptr);

  return static_cast<uint64_t>(NSEC_IN_SEC) /
    utils::ze::GetTimerResolution(device);
}

inline uint64_t GetGpuTimerFrequency() {
#if defined(_WIN32)

  return GetGpuTimerFrequencyFromL0();

#elif defined(__linux__)

  int fd = drmOpenWithType("i915", NULL, DRM_NODE_RENDER);
  if (fd < 0) {
    fd = drmOpenWithType("i915", NULL, DRM_NODE_PRIMARY);
  }
  PTI_ASSERT(fd >= 0);

  int32_t frequency = 0;

  drm_i915_getparam_t params = {0, };
  params.param = I915_PARAM_CS_TIMESTAMP_FREQUENCY;
  params.value = &frequency;

  int ioctl_ret = drmIoctl(fd, DRM_IOCTL_I915_GETPARAM, &params);
  drmClose(fd);

  // May not work for old Linux kernels (5.0+ is required)
  if (ioctl_ret != 0) {
    return GetGpuTimerFrequencyFromL0();
  }

  PTI_ASSERT(frequency > 0);
  return static_cast<uint64_t>(frequency);

#endif
}

} // namespace i915
} // namespace utils

#endif // PTI_UTILS_I915_UTILS_H_
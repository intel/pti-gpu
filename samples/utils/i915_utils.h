//==============================================================
// Copyright © 2020 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_SAMPLES_UTILS_I915_UTILS_H_
#define PTI_SAMPLES_UTILS_I915_UTILS_H_

#if defined(__linux__)
#include <drm/i915_drm.h>
#include <xf86drm.h>

#define I915_TIMESTAMP_LOW_OFFSET 0x2358
#endif

#include "utils.h"
#include "ze_utils.h"

namespace utils {
namespace i915 {

inline uint64_t GetGpuTimestamp() {
#if defined(_WIN32)

  return 0; // Not yet implemented

#elif defined(__linux__)

  int fd = drmOpenWithType("i915", NULL, DRM_NODE_RENDER);
  if (fd < 0) {
    fd = drmOpenWithType("i915", NULL, DRM_NODE_PRIMARY);
  }
  PTI_ASSERT(fd >= 0);

  struct drm_i915_reg_read reg_read_params = {0, };
  reg_read_params.offset = I915_TIMESTAMP_LOW_OFFSET | 1;

  int ioctl_ret = drmIoctl(fd, DRM_IOCTL_I915_REG_READ, &reg_read_params);
  PTI_ASSERT(ioctl_ret == 0);

  drmClose(fd);

  return reg_read_params.val;

#endif
}

inline uint64_t GetGpuTimerFrequency() {
#if defined(_WIN32)

  ze_driver_handle_t driver = nullptr;
  ze_device_handle_t device = nullptr;
  utils::ze::GetIntelDeviceAndDriver(ZE_DEVICE_TYPE_GPU, device, driver);
  PTI_ASSERT(device != nullptr);
  PTI_ASSERT(driver != nullptr);

  return static_cast<uint64_t>(NSEC_IN_SEC) /
    utils::ze::GetTimerResolution(device);

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
  PTI_ASSERT(ioctl_ret == 0);
  PTI_ASSERT(frequency > 0);

  drmClose(fd);

  return static_cast<uint64_t>(frequency);

#endif
}

} // namespace i915
} // namespace utils

#endif // PTI_SAMPLES_UTILS_I915_UTILS_H_
//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_PC_SAMPLING_TEST_FIXTURE_H_
#define PTI_PC_SAMPLING_TEST_FIXTURE_H_

#include <gtest/gtest.h>
#include <level_zero/ze_api.h>

#include <cstdlib>
#include <vector>

#include "pti/pti_pc_sampling.h"
#include "pti/pti_view.h"

inline constexpr size_t kBufferSize = 64 * 1024;

inline void BufferRequested(unsigned char** buf, size_t* size) {
  *buf = static_cast<unsigned char*>(std::malloc(kBufferSize));
  *size = (*buf == nullptr) ? 0 : kBufferSize;
}

inline void BufferCompleted(unsigned char* buf, size_t /*size*/, size_t /*used*/) {
  std::free(buf);
}

class PcSamplingTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_EQ(zeInit(ZE_INIT_FLAG_GPU_ONLY), ZE_RESULT_SUCCESS);
    uint32_t driver_count = 0;
    ASSERT_EQ(zeDriverGet(&driver_count, nullptr), ZE_RESULT_SUCCESS);
    if (driver_count == 0) {
      GTEST_SKIP() << "No L0 driver";
    }

    std::vector<ze_driver_handle_t> drivers(driver_count);
    ASSERT_EQ(zeDriverGet(&driver_count, drivers.data()), ZE_RESULT_SUCCESS);
    ASSERT_FALSE(drivers.empty());

    uint32_t device_count = 0;
    ASSERT_EQ(zeDeviceGet(drivers[0], &device_count, nullptr), ZE_RESULT_SUCCESS);
    ASSERT_GT(device_count, 0) << "No L0 GPU device found under driver";

    devices_.resize(device_count);
    ASSERT_EQ(zeDeviceGet(drivers[0], &device_count, devices_.data()), ZE_RESULT_SUCCESS);

    ASSERT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), PTI_SUCCESS);
    ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), PTI_SUCCESS);
  }

  void TearDown() override {
    ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL);
    ptiFlushAllViews();
  }

  std::vector<ze_device_handle_t> devices_;
};

#endif  // PTI_PC_SAMPLING_TEST_FIXTURE_H_

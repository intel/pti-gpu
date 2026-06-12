//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <gtest/gtest.h>

#include "pc_sampling/pti_pc_sampling_internal.h"

TEST(PcSamplingFeatureTest, IsConfiguredDeviceRejectsNullHandle) {
  EXPECT_FALSE(pti::pc_sampling::IsConfiguredDevice(nullptr, nullptr));
}

TEST(PcSamplingFeatureTest, IsConfiguredDeviceRejectsNullDevice) {
  _pti_pc_sampling_handle_t handle_storage;
  pti_pc_sampling_handle_t handle = &handle_storage;

  EXPECT_FALSE(pti::pc_sampling::IsConfiguredDevice(handle, nullptr));
}

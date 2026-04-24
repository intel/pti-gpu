//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "pti_memory_route.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>

#include "pti/pti_view.h"

namespace {

constexpr std::array kAllMemoryTypes = {
    pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_MEMORY,
    pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_HOST,
    pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE,
    pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_SHARED,
};

}  // namespace

TEST(PtiMemoryCommandRouteTest, GetCharReturnsExpectedLettersForEachMemoryType) {
  EXPECT_EQ(PtiMemoryCommandRoute::GetChar(pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_MEMORY), 'M');
  EXPECT_EQ(PtiMemoryCommandRoute::GetChar(pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_HOST), 'H');
  EXPECT_EQ(PtiMemoryCommandRoute::GetChar(pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE), 'D');
  EXPECT_EQ(PtiMemoryCommandRoute::GetChar(pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_SHARED), 'S');
}

TEST(PtiMemoryCommandRouteTest, DefaultRoute) {
  PtiMemoryCommandRoute default_route{};
  EXPECT_EQ(default_route.GetCompactStringForTypes(), "M2M");
  EXPECT_EQ(default_route.is_peer_2_peer, false);
  EXPECT_EQ(default_route.GetCompactStringForP2P(), "");
  EXPECT_EQ(default_route.GetMemcpyType(), pti_view_memcpy_type::PTI_VIEW_MEMCPY_TYPE_M2M);
  EXPECT_FALSE(default_route.IsMainDeviceSrc());
}

TEST(PtiMemoryCommandRouteTest, GetCompactStringForTypesFormatsRoute) {
  PtiMemoryCommandRoute device_2_host{pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE,
                                      pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_HOST, false};
  EXPECT_EQ(device_2_host.GetCompactStringForTypes(), "D2H");

  PtiMemoryCommandRoute shared_2_device{pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_SHARED,
                                        pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE, false};
  EXPECT_EQ(shared_2_device.GetCompactStringForTypes(), "S2D");

  PtiMemoryCommandRoute host_2_memory{pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_HOST,
                                      pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_MEMORY, false};
  EXPECT_EQ(host_2_memory.GetCompactStringForTypes(), "H2M");
}

TEST(PtiMemoryCommandRouteTest, GetCompactStringForP2PTogglesOnFlag) {
  PtiMemoryCommandRoute route;
  route.is_peer_2_peer = false;
  EXPECT_EQ(route.GetCompactStringForP2P(), "");
  route.is_peer_2_peer = true;
  EXPECT_EQ(route.GetCompactStringForP2P(), " - P2P");
}

TEST(PtiMemoryCommandRouteTest, GetMemcpyTypeCoverPairs) {
  PtiMemoryCommandRoute route{pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_MEMORY,
                              pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_SHARED, false};
  EXPECT_EQ(route.GetMemcpyType(), pti_view_memcpy_type::PTI_VIEW_MEMCPY_TYPE_M2S);

  route.src_type = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_HOST;
  route.dst_type = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE;
  EXPECT_EQ(route.GetMemcpyType(), pti_view_memcpy_type::PTI_VIEW_MEMCPY_TYPE_H2D);

  route.src_type = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE;
  route.dst_type = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_HOST;
  EXPECT_EQ(route.GetMemcpyType(), pti_view_memcpy_type::PTI_VIEW_MEMCPY_TYPE_D2H);

  route.src_type = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE;
  route.dst_type = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE;
  EXPECT_EQ(route.GetMemcpyType(), pti_view_memcpy_type::PTI_VIEW_MEMCPY_TYPE_D2D);

  route.src_type = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_SHARED;
  route.dst_type = pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_SHARED;
  EXPECT_EQ(route.GetMemcpyType(), pti_view_memcpy_type::PTI_VIEW_MEMCPY_TYPE_S2S);
}

TEST(PtiMemoryCommandRouteTest, IsMainDeviceSrcTrueForDeviceSrcExceptD2D) {
  PtiMemoryCommandRoute device_2_host{pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE,
                                      pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_HOST, false};
  EXPECT_TRUE(device_2_host.IsMainDeviceSrc());

  PtiMemoryCommandRoute device_2_shared{pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE,
                                        pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_SHARED, false};
  EXPECT_TRUE(device_2_shared.IsMainDeviceSrc());

  PtiMemoryCommandRoute device_2_memory_unknown{pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE,
                                                pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_MEMORY,
                                                false};
  EXPECT_TRUE(device_2_memory_unknown.IsMainDeviceSrc());

  PtiMemoryCommandRoute device_2_device{pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE,
                                        pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_DEVICE, false};
  EXPECT_FALSE(device_2_device.IsMainDeviceSrc());
}

TEST(PtiMemoryCommandRouteTest, IsMainDeviceSrcTrueForSharedSrc) {
  for (pti_view_memory_type dst : kAllMemoryTypes) {
    PtiMemoryCommandRoute route{pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_SHARED, dst, false};
    EXPECT_TRUE(route.IsMainDeviceSrc())
        << "Expected true for SHARED src with dst=" << static_cast<int>(dst);
  }
}

TEST(PtiMemoryCommandRouteTest, IsMainDeviceSrcFalseForHostOrMemorySrc) {
  for (pti_view_memory_type dst : kAllMemoryTypes) {
    PtiMemoryCommandRoute host_src{pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_HOST, dst, false};
    EXPECT_FALSE(host_src.IsMainDeviceSrc())
        << "Expected false for HOST src with dst=" << static_cast<int>(dst);

    PtiMemoryCommandRoute mem_src{pti_view_memory_type::PTI_VIEW_MEMORY_TYPE_MEMORY, dst, false};
    EXPECT_FALSE(mem_src.IsMainDeviceSrc())
        << "Expected false for MEMORY src with dst=" << static_cast<int>(dst);
  }
}

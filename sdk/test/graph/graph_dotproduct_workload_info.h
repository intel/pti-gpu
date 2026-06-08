//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef TEST_GRAPH_DOTPRODUCT_WORKLOAD_INFO_H_
#define TEST_GRAPH_DOTPRODUCT_WORKLOAD_INFO_H_

#include <cstddef>
#include <tuple>

#include "graph_dotproduct_scalars.h"

// Shape: Diamond
//
//      *
//     / \
//    *   *
//     \ /
//      *
//
// This workload is duplicated in two places (SYCL, Level Zero) to test both the L0 graph API and
// the SYCL graph API. Major changes to this workload might require changes in both places
// (sycl_graph_workloads.h, sycl_graph_fixture.cc, ze_graph_workloads.h, ze_graph_fixture.cc).
template <typename T>
struct DotProductWorkload {
  using DefaultVectorDataType = T;
  using DefaultResultDataType = T;
  static constexpr auto kDefaultAlpha = PTI_TEST_GRAPH_DOTPRODUCT_ALPHA;
  static constexpr auto kDefaultBeta = PTI_TEST_GRAPH_DOTPRODUCT_BETA;
  static constexpr auto kDefaultGamma = PTI_TEST_GRAPH_DOTPRODUCT_GAMMA;
  static constexpr std::size_t kDefaultVectorSize = 10;

  struct InitDotProductCl {
    static constexpr const char* const kKernelName = "InitDotProductVectors";
    static constexpr std::size_t kArgN = 3;
  };

  struct VecAddAlphaCl {
    static constexpr const char* const kKernelName = "VecAddAlpha";
    static constexpr std::size_t kArgN = 2;
  };

  struct VecAddGammaCl {
    static constexpr const char* const kKernelName = "VecAddGamma";
    static constexpr std::size_t kArgN = 2;
  };

  struct CalculateDotProductCl {
    static constexpr const char* const kKernelName = "CalculateDotProduct";
    static constexpr std::size_t kArgN = 4;
  };

  using KernelInfo =
      std::tuple<InitDotProductCl, VecAddAlphaCl, VecAddGammaCl, CalculateDotProductCl>;

  static constexpr std::size_t kDefaultKernelNumber = std::tuple_size_v<KernelInfo>;

  static constexpr const char* const kSpvKernelFile = "ze_graph_test_kernels.spv";

  static constexpr DefaultResultDataType Result(std::size_t vector_size) {
    constexpr auto kResultX = (static_cast<DefaultResultDataType>(kDefaultAlpha) *
                               static_cast<DefaultResultDataType>(kDefaultAlpha)) +
                              (static_cast<DefaultResultDataType>(kDefaultBeta) *
                               static_cast<DefaultResultDataType>(kDefaultBeta));
    constexpr auto kResultZ = (static_cast<DefaultResultDataType>(kDefaultGamma) *
                               static_cast<DefaultResultDataType>(kDefaultGamma)) +
                              (static_cast<DefaultResultDataType>(kDefaultBeta) *
                               static_cast<DefaultResultDataType>(kDefaultBeta));

    DefaultResultDataType result = 0;
    for (std::size_t i = 0; i < vector_size; ++i) {
      result += kResultX * kResultZ;
    }
    return result;
  }

  static constexpr DefaultResultDataType Result() { return Result(kDefaultVectorSize); }
};

#endif  // TEST_GRAPH_DOTPRODUCT_WORKLOAD_INFO_H_

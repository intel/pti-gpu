//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <gtest/gtest.h>
#include <level_zero/ze_api.h>
#include <level_zero/zes_api.h>
#include <pti/pti_view.h>

#include <cstdint>
#include <stdexcept>
#include <sycl/sycl.hpp>
#include <vector>

#include "utils/test_helpers.h"
#include "utils/ze_config_info.h"
#include "ze_utils.h"

namespace {
constexpr size_t kPtiDeviceId = 0;  // run on first device

bool ProperLoaderForZeInitDrivers() {
  auto loader_version = utils::ze::GetLoaderVersion();
  if (!loader_version) {
    return false;
  }
  return *loader_version >= pti::test::utils::level_zero::kProperLoaderVersionForZeInitDrivers;
}

bool ProperLoaderForZesInit() {
  auto loader_version = utils::ze::GetLoaderVersion();
  if (!loader_version) {
    return false;
  }
  return *loader_version >= pti::test::utils::level_zero::kProperLoaderVersionForZesInit;
}

void PtiZeInitDrivers(std::vector<ze_driver_handle_t>& drivers) {
  ze_init_driver_type_desc_t ze_init_des{};
  ze_init_des.stype = ZE_STRUCTURE_TYPE_INIT_DRIVER_TYPE_DESC;
  ze_init_des.pNext = nullptr;
  ze_init_des.flags = ZE_INIT_DRIVER_TYPE_FLAG_GPU;
  uint32_t driver_count = 0;
  ASSERT_EQ(zeInitDrivers(&driver_count, nullptr, &ze_init_des), ZE_RESULT_SUCCESS);
  drivers = std::vector<ze_driver_handle_t>(driver_count);
  ASSERT_EQ(zeInitDrivers(&driver_count, drivers.data(), &ze_init_des), ZE_RESULT_SUCCESS);
}

void ZeInitOrGetDrivers(std::vector<ze_driver_handle_t>& drivers) {
  if (ProperLoaderForZeInitDrivers()) {
    PtiZeInitDrivers(drivers);
  } else {
    drivers = utils::ze::GetDriverList();
  }
}

template <typename T>
void VecAdd(sycl::queue& q, const std::vector<T>& a_vector, const std::vector<T>& b_vector,
            std::vector<T>& sum) {
  sycl::range<1> num_items{a_vector.size()};
  sycl::buffer a_buf(a_vector);
  sycl::buffer b_buf(b_vector);
  sycl::buffer sum_buf(sum.data(), num_items);

  q.submit([&](sycl::handler& h) {
    sycl::accessor a(a_buf, h, sycl::read_only);
    sycl::accessor b(b_buf, h, sycl::read_only);
    sycl::accessor sum(sum_buf, h, sycl::write_only, sycl::no_init);
    h.parallel_for(num_items, [=](auto i) { sum[i] = a[i] + b[i]; });
  });
  q.wait_and_throw();
}

// This workload does not really matter, as long as its SYCL and launching a kernel or memory
// operation.
template <typename T>
std::vector<T> AddTwoVectorsDevice(const std::vector<T>& a_vector, const std::vector<T>& b_vector) {
  if (a_vector.size() != b_vector.size()) {
    throw std::invalid_argument("only two vectors of the same size supported");
  }
  sycl::property_list prop{sycl::property::queue::in_order()};
  auto queue = sycl::queue(sycl::gpu_selector_v, prop);
  std::vector<T> result(a_vector.size(), 0);
  VecAdd(queue, a_vector, b_vector, result);
  return result;
}

// This workload does not really matter, as long as its L0 and launching a kernel or memory
// operation.
template <typename T>
void CopyToAndFromDevice(ze_driver_handle_t driver, T& memory) {
  auto* dev = utils::ze::GetGpuDevice(kPtiDeviceId);
  ASSERT_NE(driver, nullptr);
  auto* ctx = utils::ze::GetContext(driver);

  ze_command_queue_desc_t cmd_queue_desc = {
      ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC, nullptr, 0, 0, 0, ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS,
      ZE_COMMAND_QUEUE_PRIORITY_NORMAL};

  ze_command_queue_handle_t queue = nullptr;
  ASSERT_EQ(zeCommandQueueCreate(ctx, dev, &cmd_queue_desc, &queue), ZE_RESULT_SUCCESS);

  ze_command_list_desc_t cmd_list_desc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC, nullptr, 0, 0};
  cmd_list_desc.flags |= ZE_COMMAND_LIST_FLAG_IN_ORDER;

  ze_command_list_handle_t list = nullptr;
  ASSERT_EQ(zeCommandListCreate(ctx, dev, &cmd_list_desc, &list), ZE_RESULT_SUCCESS);

  const ze_device_mem_alloc_desc_t alloc_desc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC, nullptr,
                                                 0, 0};
  auto copy_size = std::size(memory) * sizeof(typename T::value_type);
  constexpr auto kAlign = 64;
  void* device_storage = nullptr;
  ASSERT_EQ(zeMemAllocDevice(ctx, &alloc_desc, copy_size, kAlign, dev, &device_storage),
            ZE_RESULT_SUCCESS);
  ASSERT_EQ(zeCommandListAppendMemoryCopy(list, device_storage, std::data(memory), copy_size,
                                          nullptr, 0, nullptr),
            ZE_RESULT_SUCCESS);
  ASSERT_EQ(zeCommandListAppendBarrier(list, nullptr, 0, nullptr), ZE_RESULT_SUCCESS);
  ASSERT_EQ(zeCommandListAppendMemoryCopy(list, std::data(memory), device_storage, copy_size,
                                          nullptr, 0, nullptr),
            ZE_RESULT_SUCCESS);
  ASSERT_EQ(zeCommandListAppendBarrier(list, nullptr, 0, nullptr), ZE_RESULT_SUCCESS);
  ASSERT_EQ(zeCommandListClose(list), ZE_RESULT_SUCCESS);
  ASSERT_EQ(zeCommandQueueExecuteCommandLists(queue, 1, &list, nullptr), ZE_RESULT_SUCCESS);
  ASSERT_EQ(zeCommandQueueSynchronize(queue, UINT64_MAX), ZE_RESULT_SUCCESS);
}

template <typename T>
void CopyToAndFromDevice(T& memory) {
  auto* drv = utils::ze::GetGpuDriver(kPtiDeviceId);
  CopyToAndFromDevice(drv, memory);
}
}  // namespace

// These tests are better run within the context of CTest (i.e., separate processes) since ze*Init*
// is a global operation. However, they should pass standalone.
// They can be greatly simplified with the introduction of PTI_VIEW_DRIVER_API (we can test tracing
// without having to launch a kernel or memory operation. However, we want to backport this to
// PTI 0.10.
// Note about using zesInit, zesInit is only supported platforms later than PVC. Therefore, we
// cannot call it or there could be crashes in other oneAPI component. However, if the user calls
// it, it will appear as if tracing is broken because we were not able to call it pre-tracing
// enable.
class InitTestsFixture : public ::testing::Test {
 protected:
  constexpr static auto kDefaultRequestedBufferSize = 1'000;

  struct InitTestsData {
    static InitTestsData& Get() {
      static InitTestsData test_data{};
      return test_data;
    }
    void Reset() {
      kernels = 0;
      mem_copies = 0;
    }
    std::size_t kernels = 0;
    std::size_t mem_copies = 0;
  };

  static void ParseBuffer(unsigned char* buf, size_t used_bytes) {
    pti_view_record_base* ptr = nullptr;
    while (true) {
      auto buf_status = ptiViewGetNextRecord(buf, used_bytes, &ptr);
      if (buf_status == pti_result::PTI_STATUS_END_OF_BUFFER) {
        break;
      }
      if (buf_status != pti_result::PTI_SUCCESS) {
        FAIL() << "Found Error Parsing Records from PTI";
        break;
      }
      switch (ptr->_view_kind) {
        case pti_view_kind::PTI_VIEW_DEVICE_GPU_KERNEL: {
          InitTestsData::Get().kernels++;
          break;
        }
        case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_COPY: {
          InitTestsData::Get().mem_copies++;
          break;
        }
        default: {
          FAIL() << "Found Invalid PTI View Record: " << ptr->_view_kind;
          break;
        }
      }
    }
  }

  static void BufferCompleted(unsigned char* buf, size_t buf_size, size_t used_bytes) {
    if (!buf || !used_bytes || !buf_size) {
      if (used_bytes) {
        pti::test::utils::AlignedDealloc(buf);
      }
      return;
    }
    ParseBuffer(buf, used_bytes);
    pti::test::utils::AlignedDealloc(buf);
  }

  static void BufferRequested(unsigned char** buf, size_t* buf_size) {
    *buf = pti::test::utils::AlignedAlloc<unsigned char>(kDefaultRequestedBufferSize);
    if (!*buf) {
      FAIL() << "Unable to allocate buffer for PTI tracing";
    }
    *buf_size = kDefaultRequestedBufferSize;
  }

  void SetUp() override { InitTestsData::Get().Reset(); }
};

TEST_F(InitTestsFixture, CallAllTheZeInitFunctionsAndForceZesInitAfterTracingBegins) {
  utils::SetEnv("PTI_SYSMAN_ZESINIT", "1");  // should just warn if not supported. Needed because
                                             // DG2 and PVC don't support zesInit.
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), PTI_SUCCESS);
  EXPECT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_COPY), PTI_SUCCESS);
  ASSERT_EQ(zeInit(ZE_INIT_FLAG_GPU_ONLY), ZE_RESULT_SUCCESS);
  std::vector<ze_driver_handle_t> drivers;
  ZeInitOrGetDrivers(drivers);

  auto bmg_or_newer = utils::ze::ContainsDeviceWithAtLeastIpVersion(
      drivers, pti::test::utils::level_zero::kBmgIpVersion);
  if (ProperLoaderForZesInit()) {
    if (bmg_or_newer) {
      ASSERT_EQ(zesInit(0), ZE_RESULT_SUCCESS);
    } else {
      zesInit(0);  // NOT VALID
    }
  }

  constexpr std::size_t kSizeOfTestVector = 10;
  constexpr int kDefaultValue = 8;
  std::vector<int> vector_to_copy(kSizeOfTestVector, kDefaultValue);
  const auto test_vector_to_copy = vector_to_copy;

  ASSERT_GT(drivers.size(), static_cast<size_t>(0));
  CopyToAndFromDevice(drivers[kPtiDeviceId], vector_to_copy);

  ASSERT_EQ(vector_to_copy, test_vector_to_copy);

  EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY), PTI_SUCCESS);
  EXPECT_EQ(ptiFlushAllViews(), PTI_SUCCESS);
  ASSERT_GT(InitTestsData::Get().mem_copies, static_cast<std::size_t>(0));
}

TEST_F(InitTestsFixture, CallSyclAfterTracingBegins) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), PTI_SUCCESS);
  EXPECT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), PTI_SUCCESS);
  constexpr std::size_t kSizeOfTestVector = 10;
  constexpr int kDefaultValue = 1;
  constexpr int kDefaultResultValue = kDefaultValue + kDefaultValue;
  const std::vector<int> first_vec(kSizeOfTestVector, kDefaultValue);

  const auto result = AddTwoVectorsDevice(first_vec, first_vec);

  const std::vector<int> expected_result(kSizeOfTestVector, kDefaultResultValue);

  ASSERT_EQ(result, expected_result);

  EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), PTI_SUCCESS);
  EXPECT_EQ(ptiFlushAllViews(), PTI_SUCCESS);
  ASSERT_GT(InitTestsData::Get().kernels, static_cast<std::size_t>(0));
}

TEST_F(InitTestsFixture, CallTheZeInitFunctionsBesidesZesInitAfterTracingBegins) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), PTI_SUCCESS);
  EXPECT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_COPY), PTI_SUCCESS);
  ASSERT_EQ(zeInit(ZE_INIT_FLAG_GPU_ONLY), ZE_RESULT_SUCCESS);
  std::vector<ze_driver_handle_t> drivers;
  ZeInitOrGetDrivers(drivers);

  constexpr std::size_t kSizeOfTestVector = 10;
  constexpr int kDefaultValue = 8;
  std::vector<int> vector_to_copy(kSizeOfTestVector, kDefaultValue);
  const auto test_vector_to_copy = vector_to_copy;

  ASSERT_GT(drivers.size(), static_cast<size_t>(0));
  CopyToAndFromDevice(drivers[kPtiDeviceId], vector_to_copy);

  ASSERT_EQ(vector_to_copy, test_vector_to_copy);

  EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY), PTI_SUCCESS);
  EXPECT_EQ(ptiFlushAllViews(), PTI_SUCCESS);
  ASSERT_GT(InitTestsData::Get().mem_copies, static_cast<std::size_t>(0));
}

TEST_F(InitTestsFixture, CallOnlyZeInitDriversAfterTracingBegins) {
  if (!ProperLoaderForZeInitDrivers()) {
    GTEST_SKIP() << "Skipping test because zeInitDrivers is not supported";
  }
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), PTI_SUCCESS);
  EXPECT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_COPY), PTI_SUCCESS);
  std::vector<ze_driver_handle_t> drivers;
  PtiZeInitDrivers(drivers);

  constexpr std::size_t kSizeOfTestVector = 10;
  constexpr int kDefaultValue = 8;
  std::vector<int> vector_to_copy(kSizeOfTestVector, kDefaultValue);
  const auto test_vector_to_copy = vector_to_copy;

  ASSERT_GT(drivers.size(), static_cast<size_t>(0));
  CopyToAndFromDevice(drivers[kPtiDeviceId], vector_to_copy);

  ASSERT_EQ(vector_to_copy, test_vector_to_copy);

  EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY), PTI_SUCCESS);
  EXPECT_EQ(ptiFlushAllViews(), PTI_SUCCESS);
  ASSERT_GT(InitTestsData::Get().mem_copies, static_cast<std::size_t>(0));
}

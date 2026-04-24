//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef TEST_UTILS_PTI_RECORD_COLLECTION_FIXTURE_H_
#define TEST_UTILS_PTI_RECORD_COLLECTION_FIXTURE_H_

#include <gtest/gtest.h>

#include <cstddef>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "pti/pti_view.h"
#include "test_helpers.h"

namespace pti::test::utils {

class RecordCollectionFixture : public ::testing::Test {
 protected:
  RecordCollectionFixture() { record_storage_.Reset(); }

  struct CollectedRecords {
    void Reset() {
      kernel_records.clear();
      memcpy_records.clear();
      p2p_memcpy_records.clear();
      memfill_records.clear();
      api_records.clear();
      const std::lock_guard<std::mutex> lock(buffers_mtx);
      buffers.clear();
    }

    mutable std::mutex buffers_mtx;
    std::unordered_map<unsigned char*, PtiViewBuffer> buffers;
    std::vector<const pti_view_record_kernel*> kernel_records;
    std::vector<const pti_view_record_memory_copy*> memcpy_records;
    std::vector<const pti_view_record_memory_copy_p2p*> p2p_memcpy_records;
    std::vector<const pti_view_record_memory_fill*> memfill_records;
    std::vector<const pti_view_record_api*> api_records;
  };

  static void ProvideBuffer(unsigned char** buf, size_t* buf_size) {
    auto buffer = PtiViewBuffer(kRequestedBufferSize);
    if (!buffer.Valid()) {
      FAIL() << "Unable to allocate buffer for PTI tracing";
    }
    *buf = buffer.data();
    *buf_size = buffer.size();

    const std::lock_guard<std::mutex> lock(record_storage_.buffers_mtx);
    record_storage_.buffers[*buf] = std::move(buffer);
  }

  static void MarkBuffer(unsigned char* buf, size_t /*buf_size*/, size_t used_bytes) {
    const std::lock_guard<std::mutex> lock(record_storage_.buffers_mtx);
    if (auto it = record_storage_.buffers.find(buf); it != record_storage_.buffers.end()) {
      it->second.SetUsedBytes(used_bytes);
    }
  }

  static void HandleView(pti_view_record_base* view) {
    switch (view->_view_kind) {
      case PTI_VIEW_DEVICE_GPU_KERNEL:
        record_storage_.kernel_records.push_back(
            reinterpret_cast<const pti_view_record_kernel*>(view));
        break;
      case PTI_VIEW_DEVICE_GPU_MEM_COPY:
        record_storage_.memcpy_records.push_back(
            reinterpret_cast<const pti_view_record_memory_copy*>(view));
        break;
      case PTI_VIEW_DEVICE_GPU_MEM_COPY_P2P:
        record_storage_.p2p_memcpy_records.push_back(
            reinterpret_cast<const pti_view_record_memory_copy_p2p*>(view));
        break;
      case PTI_VIEW_DEVICE_GPU_MEM_FILL:
        record_storage_.memfill_records.push_back(
            reinterpret_cast<const pti_view_record_memory_fill*>(view));
        break;
      case PTI_VIEW_RUNTIME_API:
        record_storage_.api_records.push_back(reinterpret_cast<const pti_view_record_api*>(view));
        break;
      case PTI_VIEW_DRIVER_API:
        record_storage_.api_records.push_back(reinterpret_cast<const pti_view_record_api*>(view));
        break;
      default:
        break;
    }
  }

  static void ParseBuffer(unsigned char* buf, size_t used_bytes) {
    pti_view_record_base* record = nullptr;
    while (true) {
      auto status = ptiViewGetNextRecord(buf, used_bytes, &record);
      if (status == pti_result::PTI_STATUS_END_OF_BUFFER) {
        break;
      }
      if (status != pti_result::PTI_SUCCESS) {
        FAIL() << "Failed to parse PTI record";
        break;
      }
      HandleView(record);
    }
  }

  static void ParseAllBuffers() {
    const std::lock_guard<std::mutex> lock(record_storage_.buffers_mtx);
    for (auto& [_, buffer] : record_storage_.buffers) {
      ParseBuffer(buffer.data(), buffer.UsedBytes());
    }
  }

  // Intentionally left out of constructor for flexibility.
  static void InitCollection() {
    EXPECT_EQ(ptiViewSetCallbacks(ProvideBuffer, MarkBuffer), PTI_SUCCESS);
  }

  template <typename... E>
  void EnableViews(E... view_kinds) {
    ([&] { EXPECT_EQ(ptiViewEnable(view_kinds), PTI_SUCCESS); }(), ...);
    (enabled_views_.push_back(view_kinds), ...);
  }

  void FinalizeCollection() {
    for (auto view_kind : enabled_views_) {
      EXPECT_EQ(ptiViewDisable(view_kind), PTI_SUCCESS);
    }
    EXPECT_EQ(ptiFlushAllViews(), PTI_SUCCESS);
    ParseAllBuffers();
  }

  inline static CollectedRecords record_storage_{};

  constexpr static std::size_t kRequestedBufferSize = 1024 * 10;  // 10 KB should be fine. We're
                                                                  // storing the buffers anyways.
  std::vector<pti_view_kind> enabled_views_;
};

}  // namespace pti::test::utils

#endif  // TEST_UTILS_PTI_RECORD_COLLECTION_FIXTURE_H_

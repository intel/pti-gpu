//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

// CCL ITT Domain Filter Tests — explicit dlopen variant
//
// Same domain-filter semantics as ccl_itt_libs_test but the two
// helper libraries (libccladd.so, libfoomul.so) are loaded at
// runtime via dlopen inside each test. This exercises the code
// path where a profiled library is loaded after process start,
// which is the common case in real CCL usage.
//
// Tests:
//   1. FiltersCclBasic   — PTI enabled before any ITT calls.
//      Expects only CCL domain calls (Add, Sub) to be captured.
//
//   2. FiltersCclDeferred — ITT calls made before PTI is enabled.
//      Only the single Add call inside the enable/disable window
//      should be captured.

#include <dlfcn.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "pti/pti_view.h"

#ifndef CCLADD_LIB_PATH
#error "CCLADD_LIB_PATH must be defined via target_compile_definitions"
#endif

#ifndef FOOMUL_LIB_PATH
#error "FOOMUL_LIB_PATH must be defined via target_compile_definitions"
#endif

#define ASSERT_PTI_SUCCESS(X)                                                         \
  do {                                                                                \
    pti_result result = (X);                                                          \
    ASSERT_EQ(result, pti_result::PTI_SUCCESS)                                        \
        << "PTI CALL FAILED: " #X << " WITH ERROR " << ptiResultTypeToString(result); \
  } while (0)

namespace {
constexpr std::align_val_t kDefaultPtiBufferAlignment{8};

template <typename T>
T* AlignedAlloc(std::size_t size) {
  try {
    return static_cast<T*>(::operator new(size, kDefaultPtiBufferAlignment));
  } catch (const std::bad_alloc& e) {
    return nullptr;
  }
}

template <typename T>
void AlignedDealloc(T* buf_ptr) {
  ::operator delete(buf_ptr, kDefaultPtiBufferAlignment);
}
}  // namespace

using FloatBinaryFn = float (*)(float, float);

class CclDomainFilterDlopenTest : public ::testing::Test {
 protected:
  static inline std::vector<pti_view_record_comms>* comms_vector_ = nullptr;
  static inline std::mutex buffer_mutex_;

  static void BufferCompletedMultiThreaded(unsigned char* buf, std::size_t buf_size,
                                           std::size_t valid_buf_size) {
    if (!buf || !valid_buf_size || !buf_size) {
      if (buf) {
        AlignedDealloc(buf);
      }
      return;
    }

    std::lock_guard<std::mutex> lock(buffer_mutex_);

    if (!comms_vector_) {
      ADD_FAILURE() << __FUNCTION__ << " CommsVector is null";
      AlignedDealloc(buf);
      return;
    }

    pti_view_record_base* ptr = nullptr;
    pti_result result = pti_result::PTI_SUCCESS;
    bool has_error = false;

    while (pti_result::PTI_STATUS_END_OF_BUFFER !=
           (result = ptiViewGetNextRecord(buf, valid_buf_size, &ptr))) {
      if (result != pti_result::PTI_SUCCESS) {
        ADD_FAILURE() << "Error retrieving the next record: " << ptiResultTypeToString(result);
        has_error = true;
        break;
      }
      switch (ptr->_view_kind) {
        case pti_view_kind::PTI_VIEW_INVALID: {
          ADD_FAILURE() << __FUNCTION__ << " Received an invalid record";
          has_error = true;
          break;
        }
        case pti_view_kind::PTI_VIEW_COMMUNICATION: {
          comms_vector_->push_back(*reinterpret_cast<pti_view_record_comms*>(ptr));
          break;
        }
        default: {
          ADD_FAILURE() << "Unexpected record type: " << std::hex << ptr->_view_kind;
          has_error = true;
          break;
        }
      }
      if (has_error) break;
    }

    AlignedDealloc(buf);
  }

  static void ProvideBuffer(unsigned char** buf, std::size_t* buf_size) {
    *buf = AlignedAlloc<unsigned char>(1000);
    if (!*buf) {
      std::abort();
    }
    *buf_size = 1000;
  }

  inline void PtiProlog() {
    ASSERT_EQ(ptiViewSetCallbacks(CclDomainFilterDlopenTest::ProvideBuffer,
                                  CclDomainFilterDlopenTest::BufferCompletedMultiThreaded),
              pti_result::PTI_SUCCESS);
    ASSERT_EQ(ptiViewEnable(PTI_VIEW_COMMUNICATION), pti_result::PTI_SUCCESS);
  }

  inline void PtiEpilog() {
    ASSERT_PTI_SUCCESS(ptiViewDisable(PTI_VIEW_COMMUNICATION));
    ASSERT_PTI_SUCCESS(ptiFlushAllViews());
  }
};

TEST_F(CclDomainFilterDlopenTest, LoadFirstCclSoThenNonCclSo) {
  //
  // Load the CCL domain library first, then the non-CCL library.
  // The static initialization __itt_global of non-CCL is presented
  // to the itt_adapter.cc code, and ignored. oneCCL should collect
  // unperturbed.
  //
  std::vector<pti_view_record_comms> local_records_vector;
  comms_vector_ = &local_records_vector;

  void* ccladd_handle = dlopen(CCLADD_LIB_PATH, RTLD_NOW | RTLD_LOCAL);
  ASSERT_NE(ccladd_handle, nullptr) << "dlopen ccladd failed: " << dlerror();

  void* foomul_handle = dlopen(FOOMUL_LIB_PATH, RTLD_NOW | RTLD_LOCAL);
  ASSERT_NE(foomul_handle, nullptr) << "dlopen foomul failed: " << dlerror();

  auto Add = reinterpret_cast<FloatBinaryFn>(dlsym(ccladd_handle, "Add"));
  ASSERT_NE(Add, nullptr) << "dlsym Add failed: " << dlerror();
  auto Sub = reinterpret_cast<FloatBinaryFn>(dlsym(ccladd_handle, "Sub"));

  PtiProlog();

  auto result_add = Add(1.0f, 2.0f);
  auto result_sub = Sub(5.0f, 3.0f);

  ASSERT_NE(Sub, nullptr) << "dlsym Sub failed: " << dlerror();
  auto Mul = reinterpret_cast<FloatBinaryFn>(dlsym(foomul_handle, "Mul"));
  ASSERT_NE(Mul, nullptr) << "dlsym Mul failed: " << dlerror();
  auto Div = reinterpret_cast<FloatBinaryFn>(dlsym(foomul_handle, "Div"));
  ASSERT_NE(Div, nullptr) << "dlsym Div failed: " << dlerror();

  result_add = Add(1.0f, 2.0f);
  result_sub = Sub(5.0f, 3.0f);

  auto result_mul = Mul(4.0f, 5.0f);
  auto result_div = Div(10.0f, 2.0f);

  PtiEpilog();

  dlclose(foomul_handle);
  dlclose(ccladd_handle);

  ASSERT_EQ(local_records_vector.size(), 4u)
      << "Expected 2 records from CCL domain, got " << local_records_vector.size();

  std::set<std::string> captured_names;
  for (const auto& record : local_records_vector) {
    ASSERT_NE(record._name, nullptr) << "Record name is null";
    captured_names.insert(record._name);
  }

  EXPECT_TRUE(captured_names.count("Add") > 0) << "Missing 'Add' record";
  EXPECT_TRUE(captured_names.count("Sub") > 0) << "Missing 'Sub' record";

  EXPECT_FLOAT_EQ(result_add, 3.0f);
  EXPECT_FLOAT_EQ(result_sub, 2.0f);
  EXPECT_FLOAT_EQ(result_mul, 20.0f);
  EXPECT_FLOAT_EQ(result_div, 5.0f);

  for (const auto& record : local_records_vector) {
    EXPECT_GE(record._end_timestamp, record._start_timestamp)
        << "Invalid timestamps for " << record._name;
    EXPECT_GT(record._process_id, 0) << "Invalid process ID";
    EXPECT_GT(record._thread_id, 0) << "Invalid thread ID";
  }
}

TEST_F(CclDomainFilterDlopenTest, NonCclSo) {
  std::vector<pti_view_record_comms> local_records_vector;
  comms_vector_ = &local_records_vector;

  void* foomul_handle = dlopen(FOOMUL_LIB_PATH, RTLD_NOW | RTLD_LOCAL);
  ASSERT_NE(foomul_handle, nullptr) << "dlopen foomul failed: " << dlerror();

  auto Mul = reinterpret_cast<FloatBinaryFn>(dlsym(foomul_handle, "Mul"));
  ASSERT_NE(Mul, nullptr) << "dlsym Mul failed: " << dlerror();
  auto Div = reinterpret_cast<FloatBinaryFn>(dlsym(foomul_handle, "Div"));
  ASSERT_NE(Div, nullptr) << "dlsym Div failed: " << dlerror();

  auto result_mul = Mul(4.0f, 5.0f);
  auto result_div = Div(10.0f, 2.0f);

  PtiProlog();

  auto result_mul_seen = Mul(3.0f, 3.0f);

  PtiEpilog();
  auto result_div_2 = Div(20.0f, 4.0f);

  dlclose(foomul_handle);

  ASSERT_EQ(local_records_vector.size(), 0u)
      << "Expected 0 records from non-CCL domains, got " << local_records_vector.size();

  EXPECT_FLOAT_EQ(result_mul, 20.0f);
  EXPECT_FLOAT_EQ(result_mul_seen, 9.0f);
  EXPECT_FLOAT_EQ(result_div, 5.0f);
  EXPECT_FLOAT_EQ(result_div_2, 5.0f);
}

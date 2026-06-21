//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

// CCL ITT Domain Filter Tests
//
// This test suite verifies that PTI's ITT collector correctly filters
// ITT calls by domain, capturing only calls from the "oneCCL::API" domain
// while ignoring calls from other domains. The test mocks real application
// behavior where ITT notifications are coming from different libraries, each
// using different ITT domains.
//
// Test Libraries:
//   - libccladd.so: Implements Add/Sub functions using "oneCCL::API" domain
//   - libfoomul.so: Implements Mul/Div functions using "FOO1" and "FOO2" domains
//
// Tests:
//   1. FiltersCclBasic: Verifies domain filtering when PTI is enabled
//      before any ITT calls are made. Expects only CCL domain calls
//      (Add, Sub) to be captured, while FOO domain calls (Mul, Div)
//      are filtered out.
//
//   2. FiltersCclDeferred: Verifies domain filtering when ITT calls
//      are made before PTI is enabled. Only ITT calls made between
//      ptiViewEnable() and ptiViewDisable() should be captured.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "pti/pti_view.h"

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

extern "C" {
float Add(float x, float y);
float Sub(float x, float y);
float Mul(float x, float y);
float Div(float x, float y);
}

class CclDomainFilterTest : public ::testing::Test {
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
    ASSERT_EQ(ptiViewSetCallbacks(CclDomainFilterTest::ProvideBuffer,
                                  CclDomainFilterTest::BufferCompletedMultiThreaded),
              pti_result::PTI_SUCCESS);

    ASSERT_EQ(ptiViewEnable(PTI_VIEW_COMMUNICATION), pti_result::PTI_SUCCESS);
  }

  inline void PtiEpilog() {
    ASSERT_PTI_SUCCESS(ptiViewDisable(PTI_VIEW_COMMUNICATION));
    ASSERT_PTI_SUCCESS(ptiFlushAllViews());
  }
};

TEST_F(CclDomainFilterTest, FiltersCclBasic) {
  std::vector<pti_view_record_comms> local_records_vector;
  comms_vector_ = &local_records_vector;
  size_t records_to_see = 2;

  PtiProlog();

  auto result_add = Add(1.0f, 2.0f);
  auto result_sub = Sub(5.0f, 3.0f);
  auto result_mul = Mul(4.0f, 5.0f);
  auto result_div = Div(10.0f, 2.0f);

  PtiEpilog();

  ASSERT_EQ(local_records_vector.size(), records_to_see)
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

TEST_F(CclDomainFilterTest, FiltersDeferredCclFirst) {
  std::vector<pti_view_record_comms> local_records_vector;
  comms_vector_ = &local_records_vector;
  size_t records_to_see = 1;

  auto result_add = Add(1.0f, 2.0f);  // these from CCL domain
  auto result_sub = Sub(5.0f, 3.0f);
  auto result_mul = Mul(4.0f, 5.0f);  // these from FOO domain
  auto result_div = Div(10.0f, 2.0f);

  PtiProlog();

  auto result_add_seen = Add(8.0f, 2.0f);

  PtiEpilog();
  auto result_sub_2 = Sub(10.0f, 3.0f);

  ASSERT_EQ(local_records_vector.size(), records_to_see)
      << "Expected " << records_to_see << " records from CCL domain, got "
      << local_records_vector.size();

  std::set<std::string> captured_names;
  for (const auto& record : local_records_vector) {
    ASSERT_NE(record._name, nullptr) << "Record name is null";
    captured_names.insert(record._name);
  }

  EXPECT_TRUE(captured_names.count("Add") > 0) << "Missing 'Add' record";

  EXPECT_FLOAT_EQ(result_add, 3.0f);
  EXPECT_FLOAT_EQ(result_add_seen, 10.0f);
  EXPECT_FLOAT_EQ(result_sub, 2.0f);
  EXPECT_FLOAT_EQ(result_sub_2, 7.0f);
  EXPECT_FLOAT_EQ(result_mul, 20.0f);
  EXPECT_FLOAT_EQ(result_div, 5.0f);

  for (const auto& record : local_records_vector) {
    EXPECT_GE(record._end_timestamp, record._start_timestamp)
        << "Invalid timestamps for " << record._name;
    EXPECT_GT(record._process_id, 0) << "Invalid process ID";
    EXPECT_GT(record._thread_id, 0) << "Invalid thread ID";
  }
}

TEST_F(CclDomainFilterTest, FiltersDeferredCclSecond) {
  std::vector<pti_view_record_comms> local_records_vector;
  comms_vector_ = &local_records_vector;
  size_t records_to_see = 1;

  auto result_mul = Mul(4.0f, 5.0f);  // these from FOO domain
  auto result_div = Div(10.0f, 2.0f);
  auto result_add = Add(1.0f, 2.0f);  // these from CCL domain
  auto result_sub = Sub(5.0f, 3.0f);

  PtiProlog();

  auto result_add_seen = Add(8.0f, 2.0f);

  PtiEpilog();
  auto result_sub_2 = Sub(10.0f, 3.0f);

  ASSERT_EQ(local_records_vector.size(), records_to_see)
      << "Expected " << records_to_see << " records from CCL domain, got "
      << local_records_vector.size();

  std::set<std::string> captured_names;
  for (const auto& record : local_records_vector) {
    ASSERT_NE(record._name, nullptr) << "Record name is null";
    captured_names.insert(record._name);
  }

  EXPECT_TRUE(captured_names.count("Add") > 0) << "Missing 'Add' record";

  EXPECT_FLOAT_EQ(result_add, 3.0f);
  EXPECT_FLOAT_EQ(result_add_seen, 10.0f);
  EXPECT_FLOAT_EQ(result_sub, 2.0f);
  EXPECT_FLOAT_EQ(result_sub_2, 7.0f);
  EXPECT_FLOAT_EQ(result_mul, 20.0f);
  EXPECT_FLOAT_EQ(result_div, 5.0f);

  for (const auto& record : local_records_vector) {
    EXPECT_GE(record._end_timestamp, record._start_timestamp)
        << "Invalid timestamps for " << record._name;
    EXPECT_GT(record._process_id, 0) << "Invalid process ID";
    EXPECT_GT(record._thread_id, 0) << "Invalid thread ID";
  }
}

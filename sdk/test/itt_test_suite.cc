//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

// ITT tests used for CCL.

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <ittnotify.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <sycl/sycl.hpp>
#include <thread>
#include <vector>

#include "pti/pti.h"
#include "pti/pti_view.h"
#include "pti_filesystem.h"
#include "samples_utils.h"

// Test-specific assertion macro that uses GoogleTest instead of exit()
#define ASSERT_PTI_SUCCESS(X)                                                         \
  do {                                                                                \
    pti_result result = (X);                                                          \
    ASSERT_EQ(result, pti_result::PTI_SUCCESS)                                        \
        << "PTI CALL FAILED: " #X << " WITH ERROR " << ptiResultTypeToString(result); \
  } while (0)

// PTI Entry Points class for buffer management
class PtiEntryCalls {
 public:
  static unsigned char *remember_buf_;
  static std::size_t remember_buf_size_;

  static void ProvideBuffer(unsigned char **buf, std::size_t *buf_size) {
    *buf = samples_utils::AlignedAlloc<unsigned char>(1000);
    remember_buf_ = *buf;
    if (!*buf) {
      std::cerr << "Unable to allocate buffer for PTI tracing " << '\n';
      std::abort();
    }
    *buf_size = 1000;
    remember_buf_size_ = *buf_size;
  }
};

// Static member definitions
inline unsigned char *PtiEntryCalls::remember_buf_;
inline std::size_t PtiEntryCalls::remember_buf_size_;

class IttEnvVarInitializer {
  inline static bool SetIttEnvVariable() {
    std::string itt_lib_path = samples_utils::GetEnv("INTEL_LIBITTNOTIFY64");
    if (itt_lib_path.empty()) {
      std::cerr << "Warning: Failed to set INTEL_LIBITTNOTIFY64 environment variable." << std::endl;
      std::cerr << "Warning: ITT collector inactive." << std::endl;
    } else if (!pti::utils::filesystem::exists(itt_lib_path)) {
      std::cerr << "Warning: ITT library defined in INTEL_LIBITTNOTIFY64 not found at: "
                << itt_lib_path << " ITT collector inactive." << std::endl;
      std::cerr << "Warning: ITT collector inactive." << std::endl;
    } else {
      std::cout << "Using ITT library: " << itt_lib_path << std::endl;
    }
    return true;
  }

  inline static bool result_ = SetIttEnvVariable();
};

class IttTest : public ::testing::Test {
 protected:
  static inline std::vector<pti_view_record_comms> *comms_vector_ = nullptr;
  static inline constexpr std::string_view kCclDomain = "oneCCL::API";

  static void VerifyCollectionStateMachine(unsigned char *buf, std::size_t buf_size,
                                           std::size_t valid_buf_size) {
    //
    // comms_vector_ is sized to the number of records we should retrieve.
    //
    ASSERT_TRUE(comms_vector_) << __FUNCTION__ << " CommsVector is null";
    ASSERT_TRUE(buf) << __FUNCTION__ << " Buffer is null";
    ASSERT_GT(buf_size, 0) << __FUNCTION__ << " Buffer size is zero";

    pti_view_record_base *ptr = nullptr;
    int i = 0;  // Counter for pti_view_kind::PTI_VIEW_COMMUNICATION
    for (auto &record : *comms_vector_) {
      if (pti_result::PTI_STATUS_END_OF_BUFFER == ptiViewGetNextRecord(buf, valid_buf_size, &ptr)) {
        FAIL() << "Unexpected End of buffer";
        break;
      }

      switch (ptr->_view_kind) {
        case pti_view_kind::PTI_VIEW_INVALID: {
          FAIL() << __FUNCTION__ << " Received an invalid record";
          break;
        }
        case pti_view_kind::PTI_VIEW_COMMUNICATION: {
          ASSERT_LE(++i, comms_vector_->size()) << "Received more records than expected";
          record = *reinterpret_cast<pti_view_record_comms *>(ptr);
          break;
        }
        default: {
          FAIL() << "Corrupt record type." << std::hex << ptr->_view_kind;
          break;
        }
      }
    }
    ASSERT_EQ(comms_vector_->size(), i)
        << "Expecting " << comms_vector_->size() << " records. Received " << i << "\n";

    //
    // There is a special case where we wish to re-examine
    // the buffer after disabling the trace and flushing the buffer.
    // So don't release it.
    //
    if (PtiEntryCalls::remember_buf_ == nullptr) {
      ASSERT_EQ(pti_result::PTI_STATUS_END_OF_BUFFER,
                ptiViewGetNextRecord(buf, valid_buf_size, &ptr))
          << "Expected end of buffer after processing the previous number of records";
      samples_utils::AlignedDealloc(buf);
    }
  }

  inline void PtiProlog(bool forget_buffer = true) {
    ASSERT_EQ(
        ptiViewSetCallbacks(PtiEntryCalls::ProvideBuffer, IttTest::VerifyCollectionStateMachine),
        pti_result::PTI_SUCCESS);
    ASSERT_EQ(ptiViewEnable(PTI_VIEW_COMMUNICATION), pti_result::PTI_SUCCESS);

    if (forget_buffer) {
      PtiEntryCalls::remember_buf_ = nullptr;
    }
  }

  inline void PtiEpilog() {
    ASSERT_PTI_SUCCESS(ptiViewDisable(PTI_VIEW_COMMUNICATION));
    ASSERT_PTI_SUCCESS(ptiFlushAllViews());
  }
};

TEST_F(IttTest, Task_Handlecreate_Begin_End) {
  constexpr std::string_view task_name = "Task_Handlecreate_Begin_End";
  std::vector<pti_view_record_comms> local_records_vector(1);
  comms_vector_ = &local_records_vector;

  PtiProlog();

  auto domain = __itt_domain_create(kCclDomain.data());
  auto task = __itt_string_handle_create(task_name.data());
  __itt_task_begin(domain, __itt_null, __itt_null, task);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  __itt_task_end(domain);

  PtiEpilog();

  ASSERT_STREQ(local_records_vector[0]._name, task_name.data());
  ASSERT_GT(local_records_vector[0]._end_timestamp, local_records_vector[0]._start_timestamp);
}

TEST_F(IttTest, StrayDomain_Filtered) {
  constexpr std::string_view kTaskName = "StrayDomain_Task";
  std::vector<pti_view_record_comms> local_records_vector(0);
  comms_vector_ = &local_records_vector;

  PtiProlog();

  auto stray_domain = __itt_domain_create("Stray Domain");
  auto task = __itt_string_handle_create(kTaskName.data());
  __itt_task_begin(stray_domain, __itt_null, __itt_null, task);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  __itt_task_end(stray_domain);

  PtiEpilog();
}

TEST_F(IttTest, StackedTasks) {
  constexpr std::string_view kTask1Name = "task1_should_finish_last";
  constexpr std::string_view kTask2Name = "task2_should_finish_first";
  std::vector<pti_view_record_comms> local_records_vector(2);
  comms_vector_ = &local_records_vector;

  PtiProlog();

  auto domain = __itt_domain_create(kCclDomain.data());
  auto task1 = __itt_string_handle_create(kTask1Name.data());
  auto task2 = __itt_string_handle_create(kTask2Name.data());

  __itt_task_begin(domain, __itt_null, __itt_null, task1);
  __itt_task_begin(domain, __itt_null, __itt_null, task2);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  __itt_task_end(domain);
  __itt_task_end(domain);

  PtiEpilog();

  ASSERT_STREQ(local_records_vector[1]._name, kTask1Name.data());
  ASSERT_GT(local_records_vector[1]._end_timestamp, local_records_vector[0]._start_timestamp);
  ASSERT_STREQ(local_records_vector[0]._name, kTask2Name.data());
  ASSERT_GT(local_records_vector[0]._end_timestamp, local_records_vector[1]._start_timestamp);
}

TEST_F(IttTest, Task_Begin_Handlecreate_Addmetadata_End) {
  constexpr std::string_view kTaskName = "Task_Begin_Handlecreate_Addmetadata_End";
  uint64_t validate_metadata_expected_size = 0xdeadbeef;
  std::vector<pti_view_record_comms> local_records_vector(1);
  comms_vector_ = &local_records_vector;

  PtiProlog();

  auto domain = __itt_domain_create(kCclDomain.data());
  auto task = __itt_string_handle_create(kTaskName.data());
  auto handle = __itt_string_handle_create("send_size");

  __itt_task_begin(domain, __itt_null, __itt_null, task);
  __itt_metadata_add(domain, __itt_null, handle, __itt_metadata_u64, 1,
                     &validate_metadata_expected_size);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  __itt_task_end(domain);

  PtiEpilog();

  ASSERT_STREQ(local_records_vector[0]._name, kTaskName.data());
  ASSERT_GT(local_records_vector[0]._end_timestamp, local_records_vector[0]._start_timestamp);
  ASSERT_EQ(local_records_vector[0]._metadata_size, validate_metadata_expected_size);
}

TEST_F(IttTest, EnableDisableTrace) {
  constexpr std::string_view kTaskName = "EnableDisableTrace";
  std::vector<pti_view_record_comms> local_records_vector(1);
  comms_vector_ = &local_records_vector;

  PtiProlog(/* forget_buffer = */ false);

  auto domain = __itt_domain_create(kCclDomain.data());
  auto task = __itt_string_handle_create(kTaskName.data());
  __itt_task_begin(domain, __itt_null, __itt_null, task);
  __itt_task_end(domain);

  PtiEpilog();

  // Generate ITT traffic that SHOULD NOT BE RECORDED
  __itt_task_begin(domain, __itt_null, __itt_null, task);
  __itt_task_end(domain);

  //
  // If trace mode is enabled the log should record this message:
  // itt_return_early() Collector - tracing disabled, returning immediately
  //
  std::cout << "=== Reexamine the buffer. Only one record should be present ===" << std::endl;
  VerifyCollectionStateMachine(PtiEntryCalls::remember_buf_, PtiEntryCalls::remember_buf_size_,
                               PtiEntryCalls::remember_buf_size_);

  samples_utils::AlignedDealloc(PtiEntryCalls::remember_buf_);
}

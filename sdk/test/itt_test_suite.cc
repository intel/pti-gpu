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
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <sycl/sycl.hpp>
#include <thread>
#include <vector>
#ifdef __linux__
#include <unistd.h>
#endif

#include "pti/pti.h"
#include "pti/pti_view.h"
#include "pti_filesystem.h"
#include "samples_utils.h"
#include "utils.h"

// Test-specific assertion macro that uses GoogleTest instead of exit()
#define ASSERT_PTI_SUCCESS(X)                                                         \
  do {                                                                                \
    pti_result result = (X);                                                          \
    ASSERT_EQ(result, pti_result::PTI_SUCCESS)                                        \
        << "PTI CALL FAILED: " #X << " WITH ERROR " << ptiResultTypeToString(result); \
  } while (0)

class IttEnvVarInitializer {
 public:
  enum class Status { Success, LibraryNotSet, LibraryNotFound };

  IttEnvVarInitializer() : status_(initialize()) {}
  Status getStatus() const { return status_; }
  bool isValid() const { return status_ == Status::Success; }

 private:
  Status initialize() {
    const std::string itt_lib_path = samples_utils::GetEnv("INTEL_LIBITTNOTIFY64");

    if (itt_lib_path.empty()) {
      logWarning("INTEL_LIBITTNOTIFY64 environment variable not set");
      return Status::LibraryNotSet;
    }

    if (!pti::utils::filesystem::exists(itt_lib_path)) {
      logWarning("ITT library not found at: " + itt_lib_path);
      return Status::LibraryNotFound;
    }

    std::cout << "Using ITT library: " << itt_lib_path << std::endl;
    return Status::Success;
  }

  void logWarning(const std::string &message) const {
    std::cerr << "Warning: " << message << std::endl;
    std::cerr << "Warning: ITT collector inactive." << std::endl;
  }

  Status status_;
};

class IttTest : public ::testing::Test {
 protected:
  static inline std::vector<pti_view_record_comms> *comms_vector_ = nullptr;
  static inline std::mutex buffer_mutex_;
  static inline constexpr std::string_view kCclDomain = "oneCCL::API";
  static constexpr uint32_t kSleepTimeMs = 5;

  // Functions for PTI buffer management

  // Buffer completion callback. Also works for multi-threaded tests
  // Collects records with mutex protection, actual verification done in test body
  static void BufferCompletedMultiThreaded(unsigned char *buf, std::size_t buf_size,
                                           std::size_t valid_buf_size) {
    if (!buf || !valid_buf_size || !buf_size) {
      std::cerr << "Received empty buffer" << '\n';
      if (buf) {
        samples_utils::AlignedDealloc(buf);
      }
      return;
    }

    std::lock_guard<std::mutex> lock(buffer_mutex_);

    // Check comms_vector_ validity - use non-fatal ADD_FAILURE to avoid leak
    if (!comms_vector_) {
      ADD_FAILURE() << __FUNCTION__ << " CommsVector is null";
      samples_utils::AlignedDealloc(buf);
      return;
    }

    pti_view_record_base *ptr = nullptr;
    pti_result result = pti_result::PTI_SUCCESS;
    bool has_error = false;

    // Collect all records in the order they appear in the buffer
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
          comms_vector_->push_back(*reinterpret_cast<pti_view_record_comms *>(ptr));
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

    // Always deallocate buffer, even on error
    samples_utils::AlignedDealloc(buf);
  }

  static void ProvideBuffer(unsigned char **buf, std::size_t *buf_size) {
    *buf = samples_utils::AlignedAlloc<unsigned char>(1000);
    if (!*buf) {
      std::cerr << "Unable to allocate buffer for PTI tracing " << '\n';
      std::abort();
    }
    *buf_size = 1000;
  }

  // Collection Start and Stop helpers to avoid repetition in test bodies

  inline void PtiProlog() {
    IttEnvVarInitializer();
    ASSERT_EQ(ptiViewSetCallbacks(IttTest::ProvideBuffer, IttTest::BufferCompletedMultiThreaded),
              pti_result::PTI_SUCCESS);

    ASSERT_EQ(ptiViewEnable(PTI_VIEW_COMMUNICATION), pti_result::PTI_SUCCESS);
  }

  inline void PtiEpilog() {
    ASSERT_PTI_SUCCESS(ptiViewDisable(PTI_VIEW_COMMUNICATION));
    ASSERT_PTI_SUCCESS(ptiFlushAllViews());
  }
};

TEST_F(IttTest, Task_Handlecreate_Begin_End) {
  constexpr std::string_view task_name = "Task_Handlecreate_Begin_End";
  constexpr int kExpectedRecords = 1;  // Only one record should be generated for this test
  std::vector<pti_view_record_comms> local_records_vector;
  comms_vector_ = &local_records_vector;

  PtiProlog();

  auto domain = __itt_domain_create(kCclDomain.data());
  auto task = __itt_string_handle_create(task_name.data());
  __itt_task_begin(domain, __itt_null, __itt_null, task);
  std::this_thread::sleep_for(std::chrono::milliseconds(kSleepTimeMs));
  __itt_task_end(domain);

  PtiEpilog();

  ASSERT_EQ(local_records_vector.size(), kExpectedRecords)
      << "Expected " << kExpectedRecords << " record, collected " << local_records_vector.size();
  ASSERT_STREQ(local_records_vector[0]._name, task_name.data());
  ASSERT_GT(local_records_vector[0]._end_timestamp, local_records_vector[0]._start_timestamp);

  // Validate PID/TID fields
  ASSERT_EQ(local_records_vector[0]._process_id, utils::GetPid()) << "PID mismatch in ITT record";
  ASSERT_EQ(local_records_vector[0]._thread_id, utils::GetTid()) << "TID mismatch in ITT record";
}

TEST_F(IttTest, IttCallsBeforeViewInit) {
  constexpr std::string_view some_domain1 = "someDomain1";
  constexpr std::string_view some_domain2 = "someDomain2";
  constexpr std::string_view task_name1 = "IttCallsBeforeViewInit_Task1";
  constexpr std::string_view task_name2 = "IttCallsAfterViewInit_Task2";
  constexpr std::string_view task_name3 = "IttCallsAfterViewInit_Task3";

  constexpr int kExpectedRecords = 2;  // Only two records should be generated for this test
  std::vector<pti_view_record_comms> local_records_vector;

  comms_vector_ = &local_records_vector;

  auto some_domain_handle1 = __itt_domain_create(some_domain1.data());

  // This task will be handled by default itt implementation
  auto task2 = __itt_string_handle_create(task_name2.data());

  // Here ITT would call __itt_api_init - on the first task_begin
  __itt_task_begin(some_domain_handle1, __itt_null, __itt_null, task2);
  std::this_thread::sleep_for(std::chrono::milliseconds(kSleepTimeMs));
  __itt_task_end(some_domain_handle1);

  // Purposely creating CCL domain after _itt_api_init call but before ptiViewEnable
  auto ccl_domain_handle = __itt_domain_create(kCclDomain.data());

  auto task1 = __itt_string_handle_create(task_name1.data());

  __itt_task_begin(ccl_domain_handle, __itt_null, __itt_null, task1);
  std::this_thread::sleep_for(std::chrono::milliseconds(kSleepTimeMs));
  __itt_task_end(ccl_domain_handle);

  auto some_domain_handle2 = __itt_domain_create(some_domain2.data());

  PtiProlog();

  auto task3 = __itt_string_handle_create(task_name3.data());

  // Only these two tasks should be captured by PTI as they are in CCL domain
  __itt_task_begin(ccl_domain_handle, __itt_null, __itt_null, task2);
  std::this_thread::sleep_for(std::chrono::milliseconds(kSleepTimeMs));
  __itt_task_end(ccl_domain_handle);

  __itt_task_begin(ccl_domain_handle, __itt_null, __itt_null, task3);
  std::this_thread::sleep_for(std::chrono::milliseconds(kSleepTimeMs));
  __itt_task_end(ccl_domain_handle);

  // This task indeed should not appear in PTI records
  __itt_task_begin(some_domain_handle1, __itt_null, __itt_null, task3);
  std::this_thread::sleep_for(std::chrono::milliseconds(kSleepTimeMs));
  __itt_task_end(some_domain_handle1);

  PtiEpilog();

  __itt_task_begin(some_domain_handle2, __itt_null, __itt_null, task3);
  std::this_thread::sleep_for(std::chrono::milliseconds(kSleepTimeMs));
  __itt_task_end(some_domain_handle2);

  EXPECT_EQ(local_records_vector.size(), kExpectedRecords)
      << "Expected " << kExpectedRecords << " record, collected " << local_records_vector.size();
  EXPECT_STREQ(local_records_vector[0]._name, task_name2.data());
  EXPECT_GT(local_records_vector[0]._end_timestamp, local_records_vector[0]._start_timestamp);

  EXPECT_STREQ(local_records_vector[1]._name, task_name3.data());
  EXPECT_GT(local_records_vector[1]._end_timestamp, local_records_vector[1]._start_timestamp);
  // Validate PID/TID fields
  EXPECT_EQ(local_records_vector[0]._process_id, utils::GetPid()) << "PID mismatch in ITT record";
  EXPECT_EQ(local_records_vector[0]._thread_id, utils::GetTid()) << "TID mismatch in ITT record";
  EXPECT_EQ(local_records_vector[1]._process_id, utils::GetPid()) << "PID mismatch in ITT record";
  EXPECT_EQ(local_records_vector[1]._thread_id, utils::GetTid()) << "TID mismatch in ITT record";
}

TEST_F(IttTest, IttCclDomainLateCreation) {
  constexpr std::string_view some_domain1 = "someDomain1";
  constexpr std::string_view task_name1 = "IttCallsBeforeViewInit_Task1";
  constexpr std::string_view task_name2 = "IttCallsAfterViewInit_Task2";
  constexpr std::string_view task_name3 = "IttCallsAfterViewInit_Task3";

  constexpr int kExpectedRecords = 2;  // Only two records should be generated for this test
  std::vector<pti_view_record_comms> local_records_vector;

  comms_vector_ = &local_records_vector;

  auto some_domain_handle1 = __itt_domain_create(some_domain1.data());

  // This task will be handled by default itt implementation
  auto task2 = __itt_string_handle_create(task_name2.data());

  PtiProlog();

  // Here ITT would call __itt_api_init - on the first task_begin
  __itt_task_begin(some_domain_handle1, __itt_null, __itt_null, task2);
  std::this_thread::sleep_for(std::chrono::milliseconds(kSleepTimeMs));
  __itt_task_end(some_domain_handle1);

  PtiEpilog();

  // Purposely creating CCL domain outside of PTI COMMUNICATION collection
  auto ccl_domain_handle = __itt_domain_create(kCclDomain.data());

  auto task1 = __itt_string_handle_create(task_name1.data());

  __itt_task_begin(ccl_domain_handle, __itt_null, __itt_null, task1);
  std::this_thread::sleep_for(std::chrono::milliseconds(kSleepTimeMs));
  __itt_task_end(ccl_domain_handle);

  auto task3 = __itt_string_handle_create(task_name3.data());

  PtiProlog();

  // Only these two tasks should be captured by PTI as they are in CCL domain
  __itt_task_begin(ccl_domain_handle, __itt_null, __itt_null, task2);
  std::this_thread::sleep_for(std::chrono::milliseconds(kSleepTimeMs));
  __itt_task_end(ccl_domain_handle);

  __itt_task_begin(ccl_domain_handle, __itt_null, __itt_null, task3);
  std::this_thread::sleep_for(std::chrono::milliseconds(kSleepTimeMs));
  __itt_task_end(ccl_domain_handle);

  // This task indeed should not appear in PTI records
  __itt_task_begin(some_domain_handle1, __itt_null, __itt_null, task3);
  std::this_thread::sleep_for(std::chrono::milliseconds(kSleepTimeMs));
  __itt_task_end(some_domain_handle1);

  PtiEpilog();

  __itt_task_begin(some_domain_handle1, __itt_null, __itt_null, task3);
  std::this_thread::sleep_for(std::chrono::milliseconds(kSleepTimeMs));
  __itt_task_end(some_domain_handle1);

  __itt_task_begin(ccl_domain_handle, __itt_null, __itt_null, task1);
  std::this_thread::sleep_for(std::chrono::milliseconds(kSleepTimeMs));
  __itt_task_end(ccl_domain_handle);

  EXPECT_EQ(local_records_vector.size(), kExpectedRecords)
      << "Expected " << kExpectedRecords << " record, collected " << local_records_vector.size();
  EXPECT_STREQ(local_records_vector[0]._name, task_name2.data());
  EXPECT_GT(local_records_vector[0]._end_timestamp, local_records_vector[0]._start_timestamp);

  EXPECT_STREQ(local_records_vector[1]._name, task_name3.data());
  EXPECT_GT(local_records_vector[1]._end_timestamp, local_records_vector[1]._start_timestamp);
  // Validate PID/TID fields
  EXPECT_EQ(local_records_vector[0]._process_id, utils::GetPid()) << "PID mismatch in ITT record";
  EXPECT_EQ(local_records_vector[0]._thread_id, utils::GetTid()) << "TID mismatch in ITT record";
  EXPECT_EQ(local_records_vector[1]._process_id, utils::GetPid()) << "PID mismatch in ITT record";
  EXPECT_EQ(local_records_vector[1]._thread_id, utils::GetTid()) << "TID mismatch in ITT record";
}

TEST_F(IttTest, StrayDomain_Filtered) {
  constexpr std::string_view kTaskName = "StrayDomain_Task";
  constexpr int kExpectedRecords = 0;
  std::vector<pti_view_record_comms> local_records_vector;
  comms_vector_ = &local_records_vector;

  PtiProlog();

  auto stray_domain = __itt_domain_create("Stray Domain");
  auto task = __itt_string_handle_create(kTaskName.data());
  __itt_task_begin(stray_domain, __itt_null, __itt_null, task);
  std::this_thread::sleep_for(std::chrono::milliseconds(kSleepTimeMs));
  __itt_task_end(stray_domain);

  PtiEpilog();

  ASSERT_EQ(local_records_vector.size(), kExpectedRecords)
      << "Expected " << kExpectedRecords << " record, collected " << local_records_vector.size();
}

//
// Interesting fact of this test if you add breakpoints to gdb.
// The first call to __itt_domain_create will not be hit.
// The next two will.
//
TEST_F(IttTest, ThreeDomainsAdded) {
  constexpr std::string_view kTaskName = "StrayDomain_Task";
  constexpr int kExpectedRecords = 1;
  std::vector<pti_view_record_comms> local_records_vector;
  comms_vector_ = &local_records_vector;

  PtiProlog();

  auto task_stray = __itt_string_handle_create(kTaskName.data());
  auto stray_domain1 = __itt_domain_create("Stray Domain1");
  __itt_task_begin(stray_domain1, __itt_null, __itt_null, task_stray);
  std::this_thread::sleep_for(std::chrono::milliseconds(kSleepTimeMs));
  __itt_task_end(stray_domain1);

  auto stray_domain2 = __itt_domain_create("Stray Domain2");
  __itt_task_begin(stray_domain2, __itt_null, __itt_null, task_stray);
  std::this_thread::sleep_for(std::chrono::milliseconds(kSleepTimeMs));
  __itt_task_end(stray_domain2);

  auto ccl_domain = __itt_domain_create(kCclDomain.data());
  constexpr std::string_view task_name = "ThreeDomainsAdded";
  auto task = __itt_string_handle_create(task_name.data());

  __itt_task_begin(ccl_domain, __itt_null, __itt_null, task);
  std::this_thread::sleep_for(std::chrono::milliseconds(kSleepTimeMs));
  __itt_task_end(ccl_domain);

  PtiEpilog();

  ASSERT_EQ(local_records_vector.size(), kExpectedRecords)
      << "Expected " << kExpectedRecords << " record, collected " << local_records_vector.size();
}

//
// Test CCL domain when created first because it executes a different
// code path.
//
TEST_F(IttTest, ThreeDomainsAddedCclFirst) {
  constexpr std::string_view kTaskName = "CclFirst_Task";
  constexpr int kExpectedRecords = 1;
  std::vector<pti_view_record_comms> local_records_vector;
  comms_vector_ = &local_records_vector;

  PtiProlog();

  // Create CCL domain first
  auto ccl_domain = __itt_domain_create(kCclDomain.data());
  constexpr std::string_view ccl_task_name = "ThreeDomainsAddedCclFirst";
  auto ccl_task = __itt_string_handle_create(ccl_task_name.data());

  __itt_task_begin(ccl_domain, __itt_null, __itt_null, ccl_task);
  std::this_thread::sleep_for(std::chrono::milliseconds(kSleepTimeMs));
  __itt_task_end(ccl_domain);

  // Then create stray domains
  auto task_stray = __itt_string_handle_create(kTaskName.data());
  auto stray_domain1 = __itt_domain_create("Stray Domain1");
  __itt_task_begin(stray_domain1, __itt_null, __itt_null, task_stray);
  std::this_thread::sleep_for(std::chrono::milliseconds(kSleepTimeMs));
  __itt_task_end(stray_domain1);

  auto stray_domain2 = __itt_domain_create("Stray Domain2");
  __itt_task_begin(stray_domain2, __itt_null, __itt_null, task_stray);
  std::this_thread::sleep_for(std::chrono::milliseconds(kSleepTimeMs));
  __itt_task_end(stray_domain2);

  PtiEpilog();

  ASSERT_EQ(local_records_vector.size(), kExpectedRecords)
      << "Expected " << kExpectedRecords << " record, collected " << local_records_vector.size();
}

TEST_F(IttTest, StackedTasks) {
  constexpr std::string_view kTask1Name = "task1_should_finish_last";
  constexpr std::string_view kTask2Name = "task2_should_finish_first";
  constexpr int kExpectedRecords = 2;
  std::vector<pti_view_record_comms> local_records_vector;
  comms_vector_ = &local_records_vector;

  PtiProlog();

  auto domain = __itt_domain_create(kCclDomain.data());
  auto task1 = __itt_string_handle_create(kTask1Name.data());
  auto task2 = __itt_string_handle_create(kTask2Name.data());

  __itt_task_begin(domain, __itt_null, __itt_null, task1);
  __itt_task_begin(domain, __itt_null, __itt_null, task2);
  std::this_thread::sleep_for(std::chrono::milliseconds(kSleepTimeMs));
  __itt_task_end(domain);
  __itt_task_end(domain);

  PtiEpilog();

  ASSERT_EQ(local_records_vector.size(), kExpectedRecords)
      << "Expected " << kExpectedRecords << " records, collected " << local_records_vector.size();
  ASSERT_STREQ(local_records_vector[1]._name, kTask1Name.data());
  ASSERT_GT(local_records_vector[1]._end_timestamp, local_records_vector[0]._start_timestamp);
  ASSERT_STREQ(local_records_vector[0]._name, kTask2Name.data());
  ASSERT_GT(local_records_vector[0]._end_timestamp, local_records_vector[1]._start_timestamp);

  // Validate PID/TID fields - both records should have same PID/TID (same thread)
  uint32_t expected_pid = utils::GetPid();
  uint32_t expected_tid = utils::GetTid();
  for (const auto &rec : local_records_vector) {
    ASSERT_EQ(rec._process_id, expected_pid) << "PID mismatch in ITT record";
    ASSERT_EQ(rec._thread_id, expected_tid) << "TID mismatch in ITT record";
  }
}

TEST_F(IttTest, Task_Begin_Handlecreate_Addmetadata_End) {
  constexpr std::string_view kTaskName = "Task_Begin_Handlecreate_Addmetadata_End";
  uint64_t validate_metadata_expected_size = 0xdeadbeef;
  constexpr int kExpectedRecords = 1;
  std::vector<pti_view_record_comms> local_records_vector;
  comms_vector_ = &local_records_vector;

  PtiProlog();

  auto domain = __itt_domain_create(kCclDomain.data());
  auto task = __itt_string_handle_create(kTaskName.data());
  auto handle = __itt_string_handle_create("send_size");

  __itt_task_begin(domain, __itt_null, __itt_null, task);
  __itt_metadata_add(domain, __itt_null, handle, __itt_metadata_u64, 1,
                     &validate_metadata_expected_size);
  std::this_thread::sleep_for(std::chrono::milliseconds(kSleepTimeMs));
  __itt_task_end(domain);

  PtiEpilog();

  ASSERT_EQ(local_records_vector.size(), kExpectedRecords)
      << "Expected " << kExpectedRecords << " record, collected " << local_records_vector.size();
  ASSERT_STREQ(local_records_vector[0]._name, kTaskName.data());
  ASSERT_GT(local_records_vector[0]._end_timestamp, local_records_vector[0]._start_timestamp);
  ASSERT_EQ(local_records_vector[0]._metadata_size, validate_metadata_expected_size);

  // Validate PID/TID fields
  ASSERT_EQ(local_records_vector[0]._process_id, utils::GetPid()) << "PID mismatch in ITT record";
  ASSERT_EQ(local_records_vector[0]._thread_id, utils::GetTid()) << "TID mismatch in ITT record";
}

TEST_F(IttTest, EnableDisableTrace) {
  constexpr std::string_view kTaskName = "EnableDisableTrace";
  constexpr int kExpectedRecords = 1;  // Only the first task should be recorded
  std::vector<pti_view_record_comms> local_records_vector;

  comms_vector_ = &local_records_vector;

  PtiProlog();

  auto domain = __itt_domain_create(kCclDomain.data());
  auto task = __itt_string_handle_create(kTaskName.data());
  __itt_task_begin(domain, __itt_null, __itt_null, task);
  __itt_task_end(domain);

  // Split Epilog into 2 parts: 1st - disabling
  ASSERT_PTI_SUCCESS(ptiViewDisable(PTI_VIEW_COMMUNICATION));

  // Generate ITT traffic that SHOULD NOT BE RECORDED, but
  // for the __itt_string_handle that was created.
  auto no_more_itt_ops = __itt_string_handle_create("there_should_be_no_further_itt_ops");
  __itt_task_begin(domain, __itt_null, __itt_null, no_more_itt_ops);
  __itt_task_end(domain);

  // Split Epilog into 2 parts: 2nd - flushing records
  ASSERT_PTI_SUCCESS(ptiFlushAllViews());

  ASSERT_EQ(local_records_vector.size(), kExpectedRecords)
      << "Expected " << kExpectedRecords << " records, collected " << local_records_vector.size();

  // Validate PID/TID fields for the one recorded task
  ASSERT_EQ(local_records_vector[0]._process_id, utils::GetPid()) << "PID mismatch in ITT record";
  ASSERT_EQ(local_records_vector[0]._thread_id, utils::GetTid()) << "TID mismatch in ITT record";
}

TEST_F(IttTest, EnableDisableEnable) {
  constexpr std::string_view kFirstTaskName = "Task_First_Record";
  constexpr std::string_view kSecondTaskName = "Task_Second_Drop";
  constexpr std::string_view kThirdTaskName = "Task_Third_Record";
  constexpr int kExpectedRecords = 2;
  std::vector<pti_view_record_comms> local_records_vector;

  comms_vector_ = &local_records_vector;

  PtiProlog();

  auto domain = __itt_domain_create(kCclDomain.data());

  // First task to be recorded
  auto task1 = __itt_string_handle_create(kFirstTaskName.data());
  __itt_task_begin(domain, __itt_null, __itt_null, task1);
  __itt_task_end(domain);

  ASSERT_PTI_SUCCESS(ptiViewDisable(PTI_VIEW_COMMUNICATION));

  // Second task to be dropped
  auto task2 = __itt_string_handle_create(kSecondTaskName.data());
  __itt_task_begin(domain, __itt_null, __itt_null, task2);
  __itt_task_end(domain);

  ASSERT_PTI_SUCCESS(ptiViewEnable(PTI_VIEW_COMMUNICATION));

  // Third task to be recorded
  auto task3 = __itt_string_handle_create(kThirdTaskName.data());
  __itt_task_begin(domain, __itt_null, __itt_null, task3);
  __itt_task_end(domain);

  PtiEpilog();

  ASSERT_EQ(local_records_vector.size(), kExpectedRecords)
      << "Expected " << kExpectedRecords << " records, collected " << local_records_vector.size();

  // Verify specific task names are recorded in correct order
  ASSERT_STREQ(local_records_vector[0]._name, kFirstTaskName.data())
      << "First record should be " << kFirstTaskName.data();
  ASSERT_STREQ(local_records_vector[1]._name, kThirdTaskName.data())
      << "Second record should be " << kThirdTaskName.data();
}

TEST_F(IttTest, StackPurgeOnDisable) {
  constexpr std::string_view kTask1Name = "IncompleteTask1";
  constexpr std::string_view kTask2Name = "IncompleteTask2";
  std::vector<pti_view_record_comms> local_records_vector;

  comms_vector_ = &local_records_vector;

  PtiProlog();

  auto domain = __itt_domain_create(kCclDomain.data());
  auto task1 = __itt_string_handle_create(kTask1Name.data());
  auto task2 = __itt_string_handle_create(kTask2Name.data());

  __itt_task_begin(domain, __itt_null, __itt_null, task1);
  __itt_task_begin(domain, __itt_null, __itt_null, task2);

  ASSERT_PTI_SUCCESS(ptiViewDisable(PTI_VIEW_COMMUNICATION));

  __itt_task_end(domain);
  __itt_task_end(domain);

  ASSERT_PTI_SUCCESS(ptiViewEnable(PTI_VIEW_COMMUNICATION));

  constexpr std::string_view kValidTaskName = "ValidTaskAfterPurge";
  auto valid_task = __itt_string_handle_create(kValidTaskName.data());
  __itt_task_begin(domain, __itt_null, __itt_null, valid_task);
  __itt_task_end(domain);

  PtiEpilog();

  ASSERT_EQ(local_records_vector.size(), 1)
      << "Expected 1 record after stack purge, collected " << local_records_vector.size();

  ASSERT_STREQ(local_records_vector[0]._name, kValidTaskName.data())
      << "Should only record the task created after stack purge";

  ASSERT_EQ(local_records_vector[0]._process_id, utils::GetPid()) << "PID mismatch in ITT record";
  ASSERT_EQ(local_records_vector[0]._thread_id, utils::GetTid()) << "TID mismatch in ITT record";
}

TEST_F(IttTest, MultiThreaded_PidTid_Validation) {
  constexpr int kNumThreads = 4;
  constexpr int kTasksPerThread = 2;
  constexpr int kExpectedRecords = kNumThreads * kTasksPerThread;

  std::vector<pti_view_record_comms> local_records_vector;
  local_records_vector.reserve(kExpectedRecords);
  comms_vector_ = &local_records_vector;

  PtiProlog();

  auto domain = __itt_domain_create(kCclDomain.data());
  std::vector<std::thread> threads;
  std::vector<uint32_t> thread_ids(kNumThreads);

  // Launch multiple threads
  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back([&, i]() {
      thread_ids[i] = utils::GetTid();  // Capture actual TID

      for (int j = 0; j < kTasksPerThread; ++j) {
        std::string task_name = "thread_" + std::to_string(i) + "_task_" + std::to_string(j);
        auto task = __itt_string_handle_create(task_name.c_str());
        __itt_task_begin(domain, __itt_null, __itt_null, task);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        __itt_task_end(domain);
      }
    });
  }

  // Join all threads with joinability check
  for (auto &t : threads) {
    if (t.joinable()) {
      t.join();
    }
  }

  PtiEpilog();

  // Verify we collected the expected number of records
  ASSERT_EQ(local_records_vector.size(), kExpectedRecords)
      << "Expected " << kExpectedRecords << " records, collected " << local_records_vector.size();

  // Validate: All records should have same PID but different TIDs
  uint32_t expected_pid = utils::GetPid();
  std::set<uint32_t> observed_tids;

  for (const auto &rec : local_records_vector) {
    ASSERT_EQ(rec._process_id, expected_pid) << "All records should have same PID";
    ASSERT_GT(rec._thread_id, 0) << "TID should be non-zero";
    observed_tids.insert(rec._thread_id);
  }

  // Should observe multiple different thread IDs
  ASSERT_GE(observed_tids.size(), kNumThreads)
      << "Should observe at least " << kNumThreads << " different TIDs, observed "
      << observed_tids.size();

  // Verify captured TIDs match recorded TIDs
  for (uint32_t tid : thread_ids) {
    ASSERT_TRUE(observed_tids.find(tid) != observed_tids.end())
        << "Expected TID " << tid << " not found in records";
  }
}

TEST_F(IttTest, PidTid_Consistency_ThreadLocal) {
  constexpr std::string_view kTaskName = "consistency_check";
  constexpr int kIterations = 100;
  std::vector<pti_view_record_comms> local_records_vector;
  local_records_vector.reserve(kIterations);
  comms_vector_ = &local_records_vector;

  PtiProlog();

  auto domain = __itt_domain_create(kCclDomain.data());
  auto task = __itt_string_handle_create(kTaskName.data());

  // Generate many ITT tasks in same thread
  for (int i = 0; i < kIterations; ++i) {
    __itt_task_begin(domain, __itt_null, __itt_null, task);
    __itt_task_end(domain);
  }

  PtiEpilog();

  // Verify we collected all expected records
  ASSERT_EQ(local_records_vector.size(), kIterations)
      << "Expected " << kIterations << " records, collected " << local_records_vector.size();

  // All records from same thread should have identical PID/TID
  uint32_t expected_pid = local_records_vector[0]._process_id;
  uint32_t expected_tid = local_records_vector[0]._thread_id;

  for (int i = 1; i < kIterations; ++i) {
    ASSERT_EQ(local_records_vector[i]._process_id, expected_pid)
        << "PID changed unexpectedly at iteration " << i;
    ASSERT_EQ(local_records_vector[i]._thread_id, expected_tid)
        << "TID changed unexpectedly at iteration " << i;
  }

  // Validate against system calls
  ASSERT_EQ(expected_pid, utils::GetPid()) << "Cached PID doesn't match current PID";
  ASSERT_EQ(expected_tid, utils::GetTid()) << "Cached TID doesn't match current TID";
}

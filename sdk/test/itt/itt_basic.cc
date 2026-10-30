//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <ittnotify.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sycl/sycl.hpp>
#include <thread>

#include "pti/pti_view.h"
#include "samples_utils.h"

#define StopTracing()                                          \
  do {                                                         \
    PTI_CHECK_SUCCESS(ptiViewDisable(PTI_VIEW_COMMUNICATION)); \
  } while (0)

void ProvideBuffer(unsigned char **buf, std::size_t *buf_size) {
  *buf = samples_utils::AlignedAlloc<unsigned char>(1000);
  if (!*buf) {
    std::cerr << "Unable to allocate buffer for PTI tracing " << '\n';
    std::abort();
  }
  *buf_size = 1000;
}

static constexpr int kThreeRecordsExpected = 3;
static int record_count = 0;
static bool parsing_failed = false;

void ParseBuffer(unsigned char *buf, std::size_t buf_size, std::size_t valid_buf_size) {
  if (!buf || !valid_buf_size || !buf_size) {
    std::cerr << "Received empty buffer" << '\n';
    samples_utils::AlignedDealloc(buf);
    return;
  }

  pti_view_record_base *ptr = nullptr;

  for (;;) {
    auto buf_status = ptiViewGetNextRecord(buf, valid_buf_size, &ptr);
    if (buf_status == pti_result::PTI_STATUS_END_OF_BUFFER) {
      break;
    }
    if (buf_status != pti_result::PTI_SUCCESS) {
      std::cerr << "Found Error Parsing Records from PTI" << '\n';
      parsing_failed = true;
      break;
    }
    if (ptr->_view_kind == pti_view_kind::PTI_VIEW_COMMUNICATION) {
      ++record_count;
      std::cout << "Found ITT Record" << '\n';
      samples_utils::DumpRecord(reinterpret_cast<pti_view_record_comms *>(ptr));
    } else {
      std::cerr << "Unexpected record kind: " << ptr->_view_kind << '\n';
      parsing_failed = true;
    }
  }
  samples_utils::AlignedDealloc(buf);
}

// Generates three ITT records from the oneCCL::API domain; other domains are filtered.
int main() {
  const auto *unavailable_collector_flag = std::getenv("PTI_TEST_EXPECT_UNAVAILABLE_ITT_COLLECTOR");
  const bool expect_unavailable_collector =
      unavailable_collector_flag && std::strcmp(unavailable_collector_flag, "1") == 0;
  const auto expected_enable_result = expect_unavailable_collector
                                          ? pti_result::PTI_ERROR_COLLECTOR_UNAVAILABLE
                                          : pti_result::PTI_SUCCESS;
  const auto expected_records = expect_unavailable_collector ? 0 : kThreeRecordsExpected;

  PTI_CHECK_SUCCESS(ptiViewSetCallbacks(ProvideBuffer, ParseBuffer));

  std::cout << "BEFORE_COMMUNICATION_ENABLE" << std::endl;
  const auto enable_result = ptiViewEnable(PTI_VIEW_COMMUNICATION);
  std::cout << "AFTER_COMMUNICATION_ENABLE" << std::endl;
  if (enable_result != expected_enable_result) {
    std::cerr << "FAIL: Expected enable result " << ptiResultTypeToString(expected_enable_result)
              << ", but received " << ptiResultTypeToString(enable_result) << '\n';
    return EXIT_FAILURE;
  }

  //
  // Essential case
  // Notice that the itt collector only supports oneCCL::API domain
  //
  auto domain = __itt_domain_create("oneCCL::API");
  auto task = __itt_string_handle_create("main-1sec");
  __itt_task_begin(domain, __itt_null, __itt_null, task);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  __itt_task_end(domain);

  //
  // Nested tasks ended by __itt_task_end
  //
  auto task_follow = __itt_string_handle_create("main-2sec-follow");
  __itt_task_begin(domain, __itt_null, __itt_null, task);
  __itt_task_begin(domain, __itt_null, __itt_null, task_follow);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  __itt_task_end(domain);
  __itt_task_end(domain);

  auto domain1 = __itt_domain_create("Test Domain");
  auto task1 = __itt_string_handle_create("main-1sec");
  __itt_task_begin(domain1, __itt_null, __itt_null, task1);
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  __itt_task_end(domain1);

  StopTracing();
  PTI_CHECK_SUCCESS(ptiFlushAllViews());

  std::cout << "Size of pti_view_record_comms: " << sizeof(pti_view_record_comms) << '\n';

  if (parsing_failed || record_count != expected_records) {
    std::cerr << "FAIL: Expected " << expected_records << " communication records, but received "
              << record_count << '\n';
    return EXIT_FAILURE;
  }
  std::cout << "SUCCESS: Collected " << record_count << '/' << expected_records
            << " communication records" << '\n';
  return EXIT_SUCCESS;
}

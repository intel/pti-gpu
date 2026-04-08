//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <ittnotify.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sycl/sycl.hpp>
#include <thread>
#ifdef __linux__
#include <unistd.h>
#endif

#include "pti/pti_view.h"
#include "pti_filesystem.h"
#include "samples_utils.h"

class IttEnvVarInitializer {
  //
  // Workaround to prevent spurious Warning when ctest are being discovered.
  // However keeping the warning adds to usability because it diagnoses an
  // obvious reason for failure.
  //
  inline static bool IsRunningUnderCTest() {
#ifdef __linux__
    // Check if parent process is ctest
    std::ifstream cmdline("/proc/" + std::to_string(getppid()) + "/cmdline");
    std::string parent_cmd;
    if (std::getline(cmdline, parent_cmd)) {
      return parent_cmd.find("ctest") != std::string::npos;
    }
#endif
    return false;
  }

  inline static bool SetIttEnvVariable() {
    if (IsRunningUnderCTest()) {
      return true;  // Suppress all output when under CTest
    }

    const std::string itt_lib_path = samples_utils::GetEnv("INTEL_LIBITTNOTIFY64");
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

#define StartTracing()                                        \
  do {                                                        \
    PTI_CHECK_SUCCESS(ptiViewEnable(PTI_VIEW_COMMUNICATION)); \
  } while (0)

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

void ParseBuffer(unsigned char *buf, std::size_t buf_size, std::size_t valid_buf_size) {
  if (!buf || !valid_buf_size || !buf_size) {
    std::cerr << "Received empty buffer" << '\n';
    if (valid_buf_size) {
      samples_utils::AlignedDealloc(buf);
    }
    return;
  }

  pti_view_record_base *ptr = nullptr;

  for (record_count = 0;; record_count++) {
    auto buf_status = ptiViewGetNextRecord(buf, valid_buf_size, &ptr);
    if (buf_status == pti_result::PTI_STATUS_END_OF_BUFFER) {
      if (record_count != kThreeRecordsExpected) {
        std::cerr << "FAIL: Expected " << kThreeRecordsExpected << " records, but received "
                  << record_count << '\n';
      } else {
        std::cout << "SUCCESS: Reached End of buffer after " << record_count << '/'
                  << kThreeRecordsExpected << " records " << '\n';
      }
      break;
    }
    if (buf_status != pti_result::PTI_SUCCESS) {
      std::cerr << "Found Error Parsing Records from PTI" << '\n';
      break;
    }
    switch (ptr->_view_kind) {
      case pti_view_kind::PTI_VIEW_INVALID: {
        std::cout << "Found Invalid Record" << '\n';
        break;
      }
      case pti_view_kind::PTI_VIEW_COMMUNICATION: {
        std::cout << "---------------------------------------------------"
                     "-----------------------------"
                  << '\n';
        std::cout << "Found ITT Record" << '\n';
        samples_utils::DumpRecord(reinterpret_cast<pti_view_record_comms *>(ptr));
        break;
      }
      default: {
        std::cout << "---------------------------------------------------"
                     "-----------------------------"
                  << '\n';
        std::cerr << std::hex;
        std::cerr << "This shouldn't happen " << ptr->_view_kind << '\n';
        std::cerr << std::dec;
        break;
      }
    }
  }
  samples_utils::AlignedDealloc(buf);
}

//
// We should generate 3 IIT records by this test.
//
int main() {
  PTI_CHECK_SUCCESS(ptiViewSetCallbacks(ProvideBuffer, ParseBuffer));

  StartTracing();

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

  return record_count == kThreeRecordsExpected ? 0 : 1;
}

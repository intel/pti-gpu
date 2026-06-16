/*
 Copyright 2016-2020 Intel Corporation
 
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at
 
     http://www.apache.org/licenses/LICENSE-2.0
 
 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/
#include "sycl_base.hpp"

using namespace std;
using namespace sycl;

//
// Start of ITT tracing functions
//
#if defined(USE_PTI_VIEW)
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string_view>
#include <vector>
#include <mutex>
#include "pti_filesystem.h"

#include "pti/pti_view.h"
#include "samples_utils.h"

// Global vector to store communication records
std::vector<pti_view_record_comms> comms_vector;
std::mutex buffer_mutex;

// A lower bound of records to expect
constexpr size_t kExpectedCommsRecords = 1;
constexpr size_t kBufferSize = 1000;

void StartTracing(){
  PTI_CHECK_SUCCESS(ptiViewEnable(PTI_VIEW_COMMUNICATION));
  PTI_CHECK_SUCCESS(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL));
}

void StopTracing() {
  PTI_CHECK_SUCCESS(ptiViewDisable(PTI_VIEW_COMMUNICATION));
  PTI_CHECK_SUCCESS(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL));
}

void ProvideBuffer(unsigned char **buf, std::size_t *buf_size) {
  *buf = samples_utils::AlignedAlloc<unsigned char>(kBufferSize);
  if (!*buf) {
    std::cerr << "Unable to allocate buffer for PTI tracing " << '\n';
    std::abort();
  }
  *buf_size = kBufferSize;
}

void ParseBuffer(unsigned char *buf, std::size_t buf_size, std::size_t valid_buf_size) {
  if (!buf || !valid_buf_size || !buf_size) {
    std::cerr << "Received empty buffer" << '\n';
    if (buf) {
      samples_utils::AlignedDealloc(buf);
    }
    return;
  }

  std::lock_guard<std::mutex> lock(buffer_mutex);

  pti_view_record_base *ptr = nullptr;
  pti_result result = pti_result::PTI_SUCCESS;
  bool has_error = false;

  // Collect all records in the order they appear in the buffer
  while (pti_result::PTI_STATUS_END_OF_BUFFER !=
         (result = ptiViewGetNextRecord(buf, valid_buf_size, &ptr))) {
    if (result != pti_result::PTI_SUCCESS) {
      std::cerr << "Error retrieving the next record: " << result << std::endl;
      has_error = true;
      break;
    }
    switch (ptr->_view_kind) {
      case pti_view_kind::PTI_VIEW_INVALID: {
         std::cout << "Found Invalid Record" << '\n';
         has_error = true;
         break;
      }
      case pti_view_kind::PTI_VIEW_COMMUNICATION: {
        // Capture the record into the vector
        comms_vector.push_back(*reinterpret_cast<pti_view_record_comms *>(ptr));
        break;
      }
      default: {
        std::cout << "---------------------------------------------------"
                     "-----------------------------"
                  << '\n';
        std::cerr << "Unexpected record type: " << ptr->_view_kind << '\n';
        break;
      }
    }
    if (has_error) break;
  }

  // Always deallocate buffer, even on error
  samples_utils::AlignedDealloc(buf);
}

void DumpCollectedRecords() {
  std::lock_guard<std::mutex> lock(buffer_mutex);

  std::cout << "\n=================================================\n";
  std::cout << "DEBUG: Dumping collected communication records\n";
  std::cout << "Total records collected: " << comms_vector.size() << "\n";
  std::cout << "=================================================\n";

  for (size_t i = 0; i < comms_vector.size(); ++i) {
    std::cout << "\nRecord #" << (i + 1) << ":" << "\n";
    std::cout << "---------------------------------------------------\n";
    samples_utils::DumpRecord(&comms_vector[i]);
  }

  if (comms_vector.empty()) {
    std::cout << "No communication records were collected.\n";
  }
  std::cout << "=================================================\n\n";
}
#endif

//
// End of ITT tracing functions
//

int main(int argc, char *argv[]) {
    if (argc < 2) {
        cout << "usage ./sycl_allreduce_test [device]\n";
        cout << "device could be 'cpu' or 'gpu'\n";
        cout << "example: ./sycl_allreduce_test cpu\n";
        exit(1);
    }

    string device_type = argv[1];

    if (device_type != "cpu" && device_type != "gpu") {
        cout << "error: Invalid device '" << device_type << "'.\n";
        cout << "device must be either 'cpu' or 'gpu'.\n";
        exit(1);
    }

#if defined(USE_PTI_VIEW)
    std::string itt_lib_path = samples_utils::GetEnv("INTEL_LIBITTNOTIFY64");
    if (itt_lib_path.empty()) {
        std::cerr << "Warning: INTEL_LIBITTNOTIFY64 environment variable not set." << std::endl;
        std::cerr << "Warning: ITT collector inactive." << std::endl;
    } else if (!pti::utils::filesystem::exists(itt_lib_path)) {
        std::cerr << "Warning: ITT library defined in INTEL_LIBITTNOTIFY64 not found at: " << itt_lib_path << " ITT collector inactive." << std::endl;
        std::cerr << "Warning: ITT collector inactive." << std::endl;
    } else {
        std::cout << "Using ITT library: " << itt_lib_path << std::endl;
    }

    PTI_CHECK_SUCCESS(ptiViewSetCallbacks(ProvideBuffer, ParseBuffer));
    StartTracing();
#endif

    size_t count = 10 * 1024 * 1024;

    int size = 0;
    int rank = 0;

    ccl::init();

    MPI_Init(NULL, NULL);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    atexit(mpi_finalize);

    test_args args(argc, argv, rank);

    if (args.count != args.DEFAULT_COUNT) {
        count = args.count;
    }

    sycl::queue q;
    if (!create_test_sycl_queue(device_type, rank, q, args))
        return -1;

    /* create kvs */
    ccl::shared_ptr_class<ccl::kvs> kvs;
    ccl::kvs::address_type main_addr;
    if (rank == 0) {
        kvs = ccl::create_main_kvs();
        main_addr = kvs->get_address();
        MPI_Bcast((void *)main_addr.data(), main_addr.size(), MPI_BYTE, 0, MPI_COMM_WORLD);
    }
    else {
        MPI_Bcast((void *)main_addr.data(), main_addr.size(), MPI_BYTE, 0, MPI_COMM_WORLD);
        kvs = ccl::create_kvs(main_addr);
    }

    /* create communicator */
    auto dev = ccl::create_device(q.get_device());
    auto ctx = ccl::create_context(q.get_context());
    auto comm = ccl::create_communicator(size, rank, dev, ctx, kvs);

    /* create stream */
    auto stream = ccl::create_stream(q);

    /* create buffers */
    sycl::buffer<int> send_buf(count);
    sycl::buffer<int> recv_buf(count);

    {
        /* open buffers and initialize them on the host side */
        sycl::host_accessor send_buf_acc(send_buf, sycl::write_only);
        sycl::host_accessor recv_buf_acc(recv_buf, sycl::write_only);
        for (size_t i = 0; i < count; i++) {
            send_buf_acc[i] = rank;
            recv_buf_acc[i] = -1;
        }
    }

    /* open send_buf and modify it on the device side */
    q.submit([&](auto &h) {
        sycl::accessor send_buf_acc(send_buf, h, sycl::write_only);
        h.parallel_for(count, [=](auto id) {
            send_buf_acc[id] += 1;
        });
    });

    /* do not wait completion of kernel, dependency will be resolved by sycl::buffer */

    /* invoke allreduce */
    ccl::allreduce(send_buf, recv_buf, count, ccl::reduction::sum, comm, stream).wait();

    /* open recv_buf and check its correctness on the device side */
    q.submit([&](auto &h) {
        sycl::accessor recv_buf_acc(recv_buf, h, sycl::write_only);
        h.parallel_for(count, [=](auto id) {
            if (recv_buf_acc[id] != size * (size + 1) / 2) {
                recv_buf_acc[id] = -1;
            }
        });
    });

    if (!handle_exception(q))
        return -1;

    /* print out the result of the test on the host side */
    {
        sycl::host_accessor recv_buf_acc(recv_buf, sycl::read_only);
        size_t i;
        for (i = 0; i < count; i++) {
            if (recv_buf_acc[i] == -1) {
                std::cout << "FAILED\n";
                break;
            }
        }
        if (i == count) {
            std::cout << "PASSED\n";
        }
    }

#if defined(USE_PTI_VIEW)
    StopTracing();
    PTI_CHECK_SUCCESS(ptiFlushAllViews());
    DumpCollectedRecords();

    for (auto it = comms_vector.begin(); it != comms_vector.end(); ++it) {
        if (it->_end_timestamp <= it->_start_timestamp) {
            auto record_index = std::distance(comms_vector.begin(), it);
            std::cerr << "ERROR: Record " << record_index << " has invalid timestamps: "
                      << "end (" << it->_end_timestamp << ") <= start ("
                      << it->_start_timestamp << ")" << std::endl;
            return -1;
        }
    }

    if (comms_vector.size() < kExpectedCommsRecords) {
        std::cerr << "ERROR: Expected at least " << kExpectedCommsRecords
                  << " records, but collected " << comms_vector.size() << std::endl;
        return -1;
    }
#endif

    return 0;
}

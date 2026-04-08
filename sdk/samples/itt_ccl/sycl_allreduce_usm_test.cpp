//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
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
constexpr size_t kExpectedCommsRecords = 2;

void StartTracing(){
  PTI_CHECK_SUCCESS(ptiViewEnable(PTI_VIEW_COMMUNICATION));
}

void StopTracing() {
  PTI_CHECK_SUCCESS(ptiViewDisable(PTI_VIEW_COMMUNICATION));
}

void ProvideBuffer(unsigned char **buf, std::size_t *buf_size) {
  *buf = samples_utils::AlignedAlloc<unsigned char>(1000);
  if (!*buf) {
    std::cerr << "Unable to allocate buffer for PTI tracing " << '\n';
    std::abort();
  }
  *buf_size = 1000;
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
        has_error = true;
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

/*
 * Example for user-defined reduction operation.
 * Final result is compared with a regular sum operation.
 */
int custom_sum_reduction(queue &q,
                         ccl::communicator &comm,
                         ccl::stream &stream,
                         buf_allocator<int> &allocator,
                         sycl::usm::alloc usm_alloc_type,
                         [[maybe_unused]] int size,
                         int rank,
                         size_t count) {
    /* create buffers */
    auto send_buf = allocator.allocate(count, usm_alloc_type);
    auto recv_buf = allocator.allocate(count, usm_alloc_type);

    /* create reference buffers */
    auto send_buf_ref = allocator.allocate(count, usm_alloc_type);
    auto recv_buf_ref = allocator.allocate(count, usm_alloc_type);

    /* create custom device side scalar multiplier */
    int scalar_value = rank + 1;
    auto custom_scalar = allocator.allocate(1, usm_alloc_type);

    /* open buffers (user and reference) and modify them on the device side */
    auto e = q.submit([&](auto &h) {
        h.parallel_for(count, [=](auto id) {
            send_buf[id] = rank + 1 + id;
            recv_buf[id] = -1;

            /* reference values multiplied by local scalar */
            send_buf_ref[id] = send_buf[id] * scalar_value;
            recv_buf_ref[id] = recv_buf[id];
        });
    });

    /* open scalar buffer and modify it on the device side */
    e = q.submit([&](sycl::handler &h) {
        h.depends_on(e);
        h.single_task([=]() {
            custom_scalar[0] = scalar_value;
        });
    });

    /* the scalar is in device-visible memory and will be dereferenced,
     * while the collective is running.*/
    ccl::reduction custom_op;
    ccl::reduction_create_pre_mul_sum(&custom_op,
                                      custom_scalar,
                                      ccl::datatype::int32,
                                      ccl::scalar_residence_type::scalar_device,
                                      comm);

    /* do not wait completion of kernel and provide it as dependency for operation */
    vector<ccl::event> deps;
    deps.push_back(ccl::create_event(e));

    /* invoke reference allreduce with sum reduction */
    auto attr = ccl::create_operation_attr<ccl::allreduce_attr>();
    ccl::allreduce(send_buf_ref,
                   recv_buf_ref,
                   count,
                   ccl::datatype::int32,
                   ccl::reduction::sum,
                   comm,
                   stream,
                   attr,
                   deps)
        .wait();

    /* invoke allreduce with user-defined reduction */
    ccl::allreduce(
        send_buf, recv_buf, count, ccl::datatype::int32, custom_op, comm, stream, attr, deps)
        .wait();

    /* open recv_buf and check its correctness on the device side */
    sycl::buffer<int> check_buf(count);
    sycl::buffer<int> result_buf(count);
    sycl::buffer<int> expected_buf(count);
    q.submit([&](auto &h) {
        sycl::accessor check_buf_acc(check_buf, h, sycl::write_only);
        sycl::accessor result_buf_acc(result_buf, h, sycl::write_only);
        sycl::accessor expected_buf_acc(expected_buf, h, sycl::write_only);
        h.parallel_for(count, [=](auto id) {
            if (recv_buf[id] != recv_buf_ref[id]) {
                check_buf_acc[id] = -1;
            }
            else {
                check_buf_acc[id] = 0;
            }
            /* debug info */
            result_buf_acc[id] = recv_buf[id];
            expected_buf_acc[id] = recv_buf_ref[id];
        });
    });

    if (!handle_exception(q))
        return -1;

    /* print out the result of the test on the host side */
    {
        sycl::host_accessor result_buf_acc(result_buf, sycl::read_only);
        sycl::host_accessor check_buf_acc(check_buf, sycl::read_only);
        sycl::host_accessor expected_buf_acc(expected_buf, sycl::read_only);
        size_t i;
        for (i = 0; i < count; i++) {
            if (check_buf_acc[i] == -1) {
                std::cout << "CUSTOM FAILED at index " << i << ", value: " << result_buf_acc[i]
                          << ", expected: " << expected_buf_acc[i] << "\n";
                break;
            }
        }
        if (i == count) {
            std::cout << "PASSED\n";
        }
    }

    /* destroy custom operation */
    ccl::reduction_destroy(custom_op, comm);

    return 0;
}

int sum_reduction(queue &q,
                  ccl::communicator &comm,
                  ccl::stream &stream,
                  buf_allocator<int> &allocator,
                  sycl::usm::alloc usm_alloc_type,
                  int size,
                  int rank,
                  size_t count) {
    /* create buffers */
    auto send_buf = allocator.allocate(count, usm_alloc_type);
    auto recv_buf = allocator.allocate(count, usm_alloc_type);

    /* open buffers and modify them on the device side */
    auto e = q.submit([&](auto &h) {
        h.parallel_for(count, [=](auto id) {
            send_buf[id] = rank + id + 1;
            recv_buf[id] = -1;
        });
    });

    int check_sum = 0;
    for (int i = 1; i <= size; ++i) {
        check_sum += i;
    }

    /* do not wait completion of kernel and provide it as dependency for operation */
    vector<ccl::event> deps;
    deps.push_back(ccl::create_event(e));

    /* invoke allreduce */
    auto attr = ccl::create_operation_attr<ccl::allreduce_attr>();
    ccl::allreduce(send_buf,
                   recv_buf,
                   count,
                   ccl::datatype::int32,
                   ccl::reduction::sum,
                   comm,
                   stream,
                   attr,
                   deps)
        .wait();

    /* open recv_buf and check its correctness on the device side */
    sycl::buffer<int> check_buf(count);
    q.submit([&](auto &h) {
        sycl::accessor check_buf_acc(check_buf, h, sycl::write_only);
        h.parallel_for(count, [=](auto id) {
            if (recv_buf[id] != static_cast<int>(check_sum + size * id)) {
                check_buf_acc[id] = -1;
            }
            else {
                check_buf_acc[id] = 0;
            }
        });
    });

    if (!handle_exception(q))
        return -1;

    /* print out the result of the test on the host side */
    {
        sycl::host_accessor check_buf_acc(check_buf, sycl::read_only);
        size_t i;
        for (i = 0; i < count; i++) {
            if (check_buf_acc[i] == -1) {
                std::cout << "FAILED\n";
                break;
            }
        }
        if (i == count) {
            std::cout << "PASSED\n";
        }
    }

    return 0;
}

int average_reduction(queue &q,
                      ccl::communicator &comm,
                      ccl::stream &stream,
                      buf_allocator<int> &allocator,
                      sycl::usm::alloc usm_alloc_type,
                      int size,
                      int rank,
                      size_t count) {
    /* create buffers */
    auto send_buf = allocator.allocate(count, usm_alloc_type);
    auto recv_buf = allocator.allocate(count, usm_alloc_type);

    /* open buffers and modify them on the device side */
    /* Average allreduce calculation is the same as the sum example above,
     * except the division by the number of nodes (size).
     *
     * Note that the sum is not divisible by the size for even-sized number
     * of nodes and integer division is applied (fractional part is truncated)
     * as seen in the example below.
     *
     * Example of send/recv/expected allreduce average initial data and results.
     * RANK 0:
     * * SEND          -> (1) (2) (3) (4) - initial data
     * * RECV/EXPECTED -> (2) (3) (4) (5)
     * RANK 1:
     * * SEND          -> (2) (3) (4) (5) - initial data
     * * RECV/EXPECTED -> (2) (3) (4) (5)
     * RANK 2:
     * * SEND          -> (3) (4) (5) (6) - initial data
     * * RECV/EXPECTED -> (2) (3) (4) (5)
     * RANK 3:
     * * SEND          -> (4) (5) (6) (7) - initial data
     * * RECV/EXPECTED -> (2) (3) (4) (5)
    */

    auto e = q.submit([&](auto &h) {
        h.parallel_for(count, [=](auto id) {
            send_buf[id] = rank + id + 1;
            recv_buf[id] = -1;
        });
    });

    int check_sum = 0;
    for (int i = 1; i <= size; ++i) {
        check_sum += i;
    }

    /* do not wait completion of kernel and provide it as dependency for operation */
    vector<ccl::event> deps;
    deps.push_back(ccl::create_event(e));

    /* invoke allreduce */
    auto attr = ccl::create_operation_attr<ccl::allreduce_attr>();
    ccl::allreduce(send_buf,
                   recv_buf,
                   count,
                   ccl::datatype::int32,
                   ccl::reduction::avg,
                   comm,
                   stream,
                   attr,
                   deps)
        .wait();

    /* open recv_buf and check its correctness on the device side */
    sycl::buffer<int> check_buf(count);
    q.submit([&](auto &h) {
        sycl::accessor check_buf_acc(check_buf, h, sycl::write_only);
        h.parallel_for(count, [=](auto id) {
            if (recv_buf[id] != static_cast<int>(check_sum / size + id)) {
                check_buf_acc[id] = -1;
            }
            else {
                check_buf_acc[id] = 0;
            }
        });
    });

    if (!handle_exception(q))
        return -1;

    /* print out the result of the test on the host side */
    {
        sycl::host_accessor check_buf_acc(check_buf, sycl::read_only);
        size_t i;
        for (i = 0; i < count; i++) {
            if (check_buf_acc[i] == -1) {
                std::cout << "FAILED\n";
                break;
            }
        }
        if (i == count) {
            std::cout << "PASSED\n";
        }
    }

    return 0;
}

int min_reduction(queue &q,
                  ccl::communicator &comm,
                  ccl::stream &stream,
                  buf_allocator<int> &allocator,
                  sycl::usm::alloc usm_alloc_type,
                  [[maybe_unused]] int size,
                  int rank,
                  size_t count) {
    /* create buffers */
    auto send_buf = allocator.allocate(count, usm_alloc_type);
    auto recv_buf = allocator.allocate(count, usm_alloc_type);

    /* open buffers and modify them on the device side */
    auto e = q.submit([&](auto &h) {
        h.parallel_for(count, [=](auto id) {
            send_buf[id] = rank + id + 1;
            recv_buf[id] = -1;
        });
    });

    /* do not wait completion of kernel and provide it as dependency for operation */
    vector<ccl::event> deps;
    deps.push_back(ccl::create_event(e));

    /* invoke allreduce */
    auto attr = ccl::create_operation_attr<ccl::allreduce_attr>();
    ccl::allreduce(send_buf,
                   recv_buf,
                   count,
                   ccl::datatype::int32,
                   ccl::reduction::min,
                   comm,
                   stream,
                   attr,
                   deps)
        .wait();

    /* open recv_buf and check its correctness on the device side */
    sycl::buffer<int> check_buf(count);
    q.submit([&](auto &h) {
        sycl::accessor check_buf_acc(check_buf, h, sycl::write_only);
        h.parallel_for(count, [=](auto id) {
            if (recv_buf[id] != static_cast<int>(id + 1)) {
                check_buf_acc[id] = -1;
            }
            else {
                check_buf_acc[id] = 0;
            }
        });
    });

    if (!handle_exception(q))
        return -1;

    /* print out the result of the test on the host side */
    {
        sycl::host_accessor check_buf_acc(check_buf, sycl::read_only);
        size_t i;
        for (i = 0; i < count; i++) {
            if (check_buf_acc[i] == -1) {
                std::cout << "FAILED\n";
                break;
            }
        }
        if (i == count) {
            std::cout << "PASSED\n";
        }
    }

    return 0;
}

int max_reduction(queue &q,
                  ccl::communicator &comm,
                  ccl::stream &stream,
                  buf_allocator<int> &allocator,
                  sycl::usm::alloc usm_alloc_type,
                  int size,
                  int rank,
                  size_t count) {
    /* create buffers */
    auto send_buf = allocator.allocate(count, usm_alloc_type);
    auto recv_buf = allocator.allocate(count, usm_alloc_type);

    /* open buffers and modify them on the device side */
    auto e = q.submit([&](auto &h) {
        h.parallel_for(count, [=](auto id) {
            send_buf[id] = rank + id + 1;
            recv_buf[id] = -1;
        });
    });

    /* do not wait completion of kernel and provide it as dependency for operation */
    vector<ccl::event> deps;
    deps.push_back(ccl::create_event(e));

    /* invoke allreduce */
    auto attr = ccl::create_operation_attr<ccl::allreduce_attr>();
    ccl::allreduce(send_buf,
                   recv_buf,
                   count,
                   ccl::datatype::int32,
                   ccl::reduction::max,
                   comm,
                   stream,
                   attr,
                   deps)
        .wait();

    /* open recv_buf and check its correctness on the device side */
    sycl::buffer<int> check_buf(count);
    q.submit([&](auto &h) {
        sycl::accessor check_buf_acc(check_buf, h, sycl::write_only);
        h.parallel_for(count, [=](auto id) {
            if (recv_buf[id] != static_cast<int>(size + id)) {
                check_buf_acc[id] = -1;
            }
            else {
                check_buf_acc[id] = 0;
            }
        });
    });

    if (!handle_exception(q))
        return -1;

    /* print out the result of the test on the host side */
    {
        sycl::host_accessor check_buf_acc(check_buf, sycl::read_only);
        size_t i;
        for (i = 0; i < count; i++) {
            if (check_buf_acc[i] == -1) {
                std::cout << "FAILED\n";
                break;
            }
        }
        if (i == count) {
            std::cout << "PASSED\n";
        }
    }

    return 0;
}

int prod_reduction(queue &q,
                   ccl::communicator &comm,
                   ccl::stream &stream,
                   buf_allocator<int> &allocator,
                   sycl::usm::alloc usm_alloc_type,
                   int size,
                   int rank,
                   size_t count) {
    /* create buffers */
    auto send_buf = allocator.allocate(count, usm_alloc_type);
    auto recv_buf = allocator.allocate(count, usm_alloc_type);

    /* open buffers and modify them on the device side */
    auto e = q.submit([&](auto &h) {
        h.parallel_for(count, [=](auto id) {
            send_buf[id] = rank + id + 1;
            recv_buf[id] = -1;
        });
    });

    /* do not wait completion of kernel and provide it as dependency for operation */
    vector<ccl::event> deps;
    deps.push_back(ccl::create_event(e));

    /* invoke allreduce */
    auto attr = ccl::create_operation_attr<ccl::allreduce_attr>();
    ccl::allreduce(send_buf,
                   recv_buf,
                   count,
                   ccl::datatype::int32,
                   ccl::reduction::prod,
                   comm,
                   stream,
                   attr,
                   deps)
        .wait();

    /* open recv_buf and check its correctness on the device side */
    sycl::buffer<int> check_buf(count);
    q.submit([&](auto &h) {
        sycl::accessor check_buf_acc(check_buf, h, sycl::write_only);
        h.parallel_for(count, [=](auto id) {
            auto combination = [](int size, int id) -> int {
                int result = 1;
                for (int i = 0; i < size; ++i) {
                    result *= size + id - i;
                }
                return result;
            };

            if (recv_buf[id] != static_cast<int>(combination(size, id))) {
                check_buf_acc[id] = -1;
            }
            else {
                check_buf_acc[id] = 0;
            }
        });
    });

    if (!handle_exception(q))
        return -1;

    /* print out the result of the test on the host side */

    {
        sycl::host_accessor check_buf_acc(check_buf, sycl::read_only);
        size_t i;
        for (i = 0; i < count; i++) {
            if (check_buf_acc[i] == -1) {
                std::cout << "FAILED\n";
                break;
            }
        }
        if (i == count) {
            std::cout << "PASSED\n";
        }
    }

    return 0;
}

int main(int argc, char *argv[]) {
    if (!check_example_args(argc, argv))
        exit(1);

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

    string device_type = argv[1];
    string alloc_type = argv[2];

    sycl::queue q;
    if (!create_test_sycl_queue(device_type, rank, q, args))
        return -1;

    buf_allocator<int> allocator(q);

    auto usm_alloc_type = usm_alloc_type_from_string(alloc_type);

    if (!check_sycl_usm(q, usm_alloc_type)) {
        return -1;
    }

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

    /* check skip conditions */
    bool skip_case = false;
    const char *esimd_env = std::getenv("CCL_SYCL_ESIMD");
    if (esimd_env && std::string(esimd_env) == "1") {
        skip_case = true;
    }

    /* run examples */
    int ret = 0;

    ret = sum_reduction(q, comm, stream, allocator, usm_alloc_type, size, rank, count);
    if (ret == -1)
        return -1;

    if (!skip_case) {
        ret = prod_reduction(q, comm, stream, allocator, usm_alloc_type, size, rank, count);
        if (ret == -1)
            return -1;

        ret = min_reduction(q, comm, stream, allocator, usm_alloc_type, size, rank, count);
        if (ret == -1)
            return -1;

        ret = max_reduction(q, comm, stream, allocator, usm_alloc_type, size, rank, count);
        if (ret == -1)
            return -1;
    }

    try {
        ret = average_reduction(q, comm, stream, allocator, usm_alloc_type, size, rank, count);
    }
    catch (ccl::exception &e) {
        std::string exception_msg(e.what());
        std::string check_msg("average operation");
        if (exception_msg.find(check_msg) != std::string::npos) {
            std::cout << "SKIP: average operation is not supported for the scheduler path."
                      << std::endl;
            ret = 0;
        }
        else {
            std::cout << e.what() << std::endl;
            std::cout << "FAILED\n";
            return -1;
        }
    }
    catch (...) {
        std::cout << "FAILED\n";
        return -1;
    }
    if (ret == -1)
        return -1;

    if (!skip_case) {
        try {
            ret =
                custom_sum_reduction(q, comm, stream, allocator, usm_alloc_type, size, rank, count);
        }
        catch (ccl::exception &e) {
            std::string exception_msg(e.what());
            std::string check_msg("user-defined reduction operation");
            if (exception_msg.find(check_msg) != std::string::npos) {
                std::cout
                    << "SKIP: user-defined reduction operation is not supported for the scheduler or scale-out path."
                    << std::endl;
                ret = 0;
            }
            else {
                std::cout << e.what() << std::endl;
                std::cout << "FAILED\n";
                return -1;
            }
        }
        catch (...) {
            std::cout << "FAILED\n";
            return -1;
        }
        if (ret == -1)
            return -1;
    }

#if defined(USE_PTI_VIEW)
    // PTI Tracing finalization
    StopTracing();
    PTI_CHECK_SUCCESS(ptiFlushAllViews());
    DumpCollectedRecords();

    // Verify that every record in comms_vector has valid timestamps
    for (auto it = comms_vector.begin(); it != comms_vector.end(); ++it) {
        if (it->_end_timestamp <= it->_start_timestamp) {
            auto record_index = std::distance(comms_vector.begin(), it);
            std::cerr << "ERROR: Record " << record_index << " has invalid timestamps: "
                      << "end (" << it->_end_timestamp << ") <= start ("
                      << it->_start_timestamp << ")" << std::endl;
            return -1;
        }
    }

    // Verify we have collected records. We cannot fix it
    // because depending on the mpiexec invocation this may change
    if (comms_vector.size() <  kExpectedCommsRecords) {
        std::cerr << "ERROR: Expected at least " << kExpectedCommsRecords
                  << " records, but collected " << comms_vector.size() << std::endl;
        return -1;
    }

    return 0;
#else
    return 0;
#endif
}

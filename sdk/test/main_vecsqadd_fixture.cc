//==============================================================
// Based on Vector Add example from OneAPI samples
//==============================================================
// Copyright © Intel Corporation
// SPDX-License-Identifier: MIT
// =============================================================
#include <gtest/gtest.h>
#include <level_zero/ze_api.h>

#include <iostream>
#include <string>
#include <sycl/ext/oneapi/backend/level_zero.hpp>
#include <sycl/sycl.hpp>
#include <vector>

#include "pti/pti_view.h"

using namespace sycl;

namespace {

enum class TestType {
  RUN_ALL = 0,
  OVERFLOW_STRESS = 1,
  EXTERNAL_CORR_ID = 2,
};

constexpr uint32_t stress_loop_counter = 5;

bool matched_sq_corr_ids = false;
bool matched_add_corr_ids = false;
bool timestamps_monotonic = true;
constexpr size_t vector_size = 5000;
typedef std::vector<double> DoubleVector;
uint64_t sycl_kernel_corr_id[3];
uint64_t sycl_kernel_start_time[3];
uint64_t kernel_corr_id[3];
uint64_t kernel_append_time[3];
uint64_t sycl_idx = 0;
uint64_t kernel_idx = 0;
uint64_t a_append_timestamp = 0;
uint64_t eid = 11;  // external correlation id base.

//************************************
// Vector square in SYCL on device: modifies each input vector
//************************************
void vecSq(queue &q, const DoubleVector &a_vector, const DoubleVector &b_vector) {
  ptiViewPushExternalCorrelationId(pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_3,
                                   eid + 20);
  range<1> num_items{a_vector.size()};
  buffer a_buf(a_vector);
  buffer b_buf(b_vector);

  q.submit([&](handler &h) {
    accessor a(a_buf, h, read_write);
    accessor b(b_buf, h, read_write);
    h.parallel_for(num_items, [=](auto i) {
      a[i] = a[i] * a[i];
      b[i] = b[i] * b[i];
    });
  });
  q.wait();
  ptiViewPopExternalCorrelationId(pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_3, &eid);
}

void vecPassThroughToVecSq(queue &q, const DoubleVector &a_vector, const DoubleVector &b_vector) {
  // This external id(21) is ignored due to overriding push in the vecSq call it preceeds.
  ptiViewPushExternalCorrelationId(pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_3,
                                   eid + 10);
  vecSq(q, a_vector, b_vector);
  ptiViewPopExternalCorrelationId(pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_3, &eid);
};

//************************************
// Vector add in SYCL on device: returns sum in 4th parameter "sq_add".
//************************************
void vecAdd(queue &q, const DoubleVector &a_vector, const DoubleVector &b_vector,
            DoubleVector &sq_add) {
  range<1> num_items{a_vector.size()};
  buffer a_buf(a_vector);
  buffer b_buf(b_vector);
  buffer sum_buf(sq_add.data(), num_items);

  q.submit([&](handler &h) {
    accessor a(a_buf, h, read_only);
    accessor b(b_buf, h, read_only);
    accessor sum(sum_buf, h, write_only, no_init);
    h.parallel_for(num_items, [=](auto i) { sum[i] = a[i] + b[i]; });
  });
  q.wait();
}

void print_results(const DoubleVector &sq_add, int n) {
  double sum = 0;
  for (int i = 0; i < n; i++) sum += sq_add[i];
  printf("final result: %f\n", sum / n);
}

void StartTracing() {
  assert(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL) == pti_result::PTI_SUCCESS);
  assert(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_COPY) == pti_result::PTI_SUCCESS);
  assert(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_FILL) == pti_result::PTI_SUCCESS);
  assert(ptiViewEnable(PTI_VIEW_SYCL_RUNTIME_CALLS) == pti_result::PTI_SUCCESS);
  assert(ptiViewEnable(PTI_VIEW_EXTERNAL_CORRELATION) == pti_result::PTI_SUCCESS);
}

void StopTracing() {
  ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL);
  ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY);
  ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_FILL);
  ptiViewDisable(PTI_VIEW_SYCL_RUNTIME_CALLS);
  ptiViewDisable(PTI_VIEW_EXTERNAL_CORRELATION);
}

static void BufferRequested(unsigned char **buf, size_t *buf_size) {
  *buf_size = sizeof(pti_view_record_kernel);
  void *ptr = ::operator new(*buf_size);
  ptr = std::align(8, sizeof(unsigned char), ptr, *buf_size);
  *buf = reinterpret_cast<unsigned char *>(ptr);
  if (!*buf) {
    std::abort();
  }
  return;
}

static void BufferCompleted(unsigned char *buf, size_t buf_size, size_t valid_buf_size) {
  if (!buf_size || !valid_buf_size || !buf_size) {
    std::cerr << "Received empty buffer" << '\n';
    if (valid_buf_size) {
      ::operator delete(buf);
    }
    return;
  }
  pti_view_record_base *ptr = nullptr;
  while (true) {
    auto buf_status = ptiViewGetNextRecord(buf, valid_buf_size, &ptr);
    if (buf_status == pti_result::PTI_STATUS_END_OF_BUFFER) {
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
      case pti_view_kind::PTI_VIEW_EXTERNAL_CORRELATION: {
        [[maybe_unused]] pti_view_record_external_correlation *a_ext_rec =
            reinterpret_cast<pti_view_record_external_correlation *>(ptr);
        break;
      }
      case pti_view_kind::PTI_VIEW_COLLECTION_OVERHEAD: {
        [[maybe_unused]] pti_view_record_overhead *record =
            reinterpret_cast<pti_view_record_overhead *>(ptr);
        break;
      }
      case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_COPY: {
        break;
      }
      case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_FILL: {
        break;
      }
      case pti_view_kind::PTI_VIEW_SYCL_RUNTIME_CALLS: {
        pti_view_record_sycl_runtime *a_sycl_rec =
            reinterpret_cast<pti_view_record_sycl_runtime *>(ptr);
        std::string function_name = a_sycl_rec->_name;
        if ((sycl_idx < 2) && (function_name.find("piEnqueueKernelLaunch") != std::string::npos)) {
          sycl_kernel_corr_id[sycl_idx] = a_sycl_rec->_correlation_id;
          sycl_kernel_start_time[sycl_idx] = a_sycl_rec->_start_timestamp;
          sycl_idx++;
        };
        break;
      }
      case pti_view_kind::PTI_VIEW_DEVICE_GPU_KERNEL: {
        pti_view_record_kernel *a_kernel_rec = reinterpret_cast<pti_view_record_kernel *>(ptr);
        std::string kernel_name = a_kernel_rec->_name;
        if ((kernel_idx < 2) && (kernel_name.find("vecSq(") != std::string::npos)) {
          kernel_corr_id[kernel_idx] = a_kernel_rec->_correlation_id;
          kernel_append_time[kernel_idx] = a_kernel_rec->_append_timestamp;
          kernel_idx++;
        }
        if ((kernel_idx < 2) && (kernel_name.find("vecAdd(") != std::string::npos)) {
          kernel_corr_id[kernel_idx] = a_kernel_rec->_correlation_id;
          kernel_append_time[kernel_idx] = a_kernel_rec->_append_timestamp;
          kernel_idx++;
        }
        std::cout << "KernelTimestamp for vecAdd: " << a_kernel_rec->_append_timestamp << "\n";
        timestamps_monotonic =
            timestamps_monotonic && (a_append_timestamp < a_kernel_rec->_append_timestamp);
        a_append_timestamp = a_kernel_rec->_append_timestamp;
        break;
      }
      default: {
        std::cerr << "This shouldn't happen" << '\n';
        break;
      }
    }
  }
  ::operator delete(buf);
};

void RunExternalCorrIdTest(queue &q, const DoubleVector &a, const DoubleVector &b,
                           const DoubleVector &c, const DoubleVector &d, DoubleVector &sq_add,
                           DoubleVector &sq_add2) {
  StartTracing();
  vecPassThroughToVecSq(q, a, b);
  StopTracing();
  vecAdd(q, a, b, sq_add);
  print_results(sq_add, vector_size);

  vecAdd(q, a, b, sq_add);
  print_results(sq_add, vector_size);

  StartTracing();
  vecAdd(q, c, d, sq_add2);
  StopTracing();
  print_results(sq_add2, 2 * vector_size);
};

void RunOverflowStressTest(queue &q, [[maybe_unused]] const DoubleVector &a,
                           [[maybe_unused]] const DoubleVector &b, const DoubleVector &c,
                           const DoubleVector &d, [[maybe_unused]] DoubleVector &sq_add,
                           DoubleVector &sq_add2) {
  StartTracing();
  // stress_loop_counter value of 17500 works to drive the collection time to 24hours+.
  for (unsigned int i = 0; i < stress_loop_counter; i++) {
    vecAdd(q, c, d, sq_add2);
    sleep(5);
  };
  StopTracing();
};

void RunVecsqadd(TestType a_test_type) {
  DoubleVector a, b, c, d, sq_add, sq_add2;
  ptiViewPushExternalCorrelationId(pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_3, eid);
  auto dev = sycl::device(sycl::gpu_selector_v);

  a.resize(vector_size);
  b.resize(vector_size);
  c.resize(2 * vector_size);
  d.resize(2 * vector_size);
  sq_add.resize(vector_size);
  sq_add2.resize(2 * vector_size);

  for (size_t i = 0; i < vector_size; i++) {
    a[i] = sin(i);
    b[i] = cos(i);
    c[2 * i] = sin(i) * sin(i);
    c[2 * i + 1] = sin(i);
    d[2 * i] = cos(i) * cos(i);
    d[2 * i + 1] = cos(i);
  }

  auto d_selector{gpu_selector_v};
  queue q(d_selector, NULL);

  // Start Tests by Type

  if (a_test_type == TestType::RUN_ALL) {
    RunExternalCorrIdTest(q, a, b, c, d, sq_add, sq_add2);
    RunOverflowStressTest(q, a, b, c, d, sq_add, sq_add2);
  };

  if (a_test_type == TestType::EXTERNAL_CORR_ID)
    RunExternalCorrIdTest(q, a, b, c, d, sq_add, sq_add2);
  if (a_test_type == TestType::OVERFLOW_STRESS)
    RunOverflowStressTest(q, a, b, c, d, sq_add, sq_add2);

  a.clear();
  b.clear();
  sq_add.clear();
  c.clear();
  d.clear();
  sq_add2.clear();

  ptiViewPopExternalCorrelationId(pti_view_external_kind::PTI_VIEW_EXTERNAL_KIND_CUSTOM_3, &eid);
  ptiFlushAllViews();
}

}  // namespace

class VecsqaddFixtureTest : public ::testing::Test {
 protected:
  VecsqaddFixtureTest() {
    // Setup work for each test
  }

  ~VecsqaddFixtureTest() override {
    // Cleanup work for each test
  }

  void SetUp() override {
    matched_sq_corr_ids = false;
    matched_add_corr_ids = false;
    timestamps_monotonic = true;
    a_append_timestamp = 0;
  }

  void TearDown() override {
    // Called right before destructor after each test
  }
};

TEST_F(VecsqaddFixtureTest, CorrelationIdsMatchForSq) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunVecsqadd(TestType::EXTERNAL_CORR_ID);
  if ((sycl_kernel_corr_id[0] == kernel_corr_id[0]) &&
      (sycl_kernel_start_time[0] < kernel_append_time[0]))
    matched_sq_corr_ids = true;
  EXPECT_EQ(matched_sq_corr_ids, true);
}

TEST_F(VecsqaddFixtureTest, CorrelationIdsMatchForAdd) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunVecsqadd(TestType::EXTERNAL_CORR_ID);
  if ((sycl_kernel_corr_id[1] == kernel_corr_id[1]) &&
      (sycl_kernel_start_time[1] < kernel_append_time[1]))
    matched_add_corr_ids = true;
  EXPECT_EQ(matched_add_corr_ids, true);
}

TEST_F(VecsqaddFixtureTest, TimestampWrapAroundOnOverflow) {
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunVecsqadd(TestType::OVERFLOW_STRESS);
  EXPECT_EQ(timestamps_monotonic, true);
}

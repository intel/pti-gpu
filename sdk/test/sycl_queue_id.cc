#include <gtest/gtest.h>

#include <array>
#include <cstring>
#include <sycl/sycl.hpp>
#include <thread>
#include <vector>

#include "pti/pti_view.h"
#include "samples_utils.h"
#include "utils.h"

inline constexpr auto kAValue = 0.128f;

namespace {
constexpr int kVectorSize = 1024;
bool queue_id_kernel_records = false;
bool queue_id_memcpy_records = false;
bool queue_id_memfill_records = false;
std::set<uint64_t> mt_q_ids;
constexpr uint32_t kThreadCount = 5;
uint64_t queue_id_K1 = -1;
uint64_t queue_id_K2 = 0;
uint64_t queue_id = -1;
bool use_same_q = false;
bool use_same_k = false;
[[maybe_unused]] bool use_stacked_q = false;
[[maybe_unused]] bool templated_run = false;
inline constexpr uint64_t kMaxQueueId = static_cast<uint64_t>(-1);
}  // namespace

void StartTracing() {
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_COPY), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_COPY_P2P), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_MEM_FILL), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_SYCL_RUNTIME_CALLS), pti_result::PTI_SUCCESS);
}

void StopTracing() {
  EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
  EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY), pti_result::PTI_SUCCESS);
  EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_COPY_P2P), pti_result::PTI_SUCCESS);
  EXPECT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_MEM_FILL), pti_result::PTI_SUCCESS);
  EXPECT_EQ(ptiViewDisable(PTI_VIEW_SYCL_RUNTIME_CALLS), pti_result::PTI_SUCCESS);
}

static void BufferRequested(unsigned char **buf, size_t *buf_size) {
  *buf_size = sizeof(pti_view_record_kernel);
  void *ptr = ::operator new(*buf_size);
  ptr = std::align(8, sizeof(unsigned char), ptr, *buf_size);
  *buf = static_cast<unsigned char *>(ptr);
}

static void BufferCompleted(unsigned char *buf, size_t buf_size, size_t used_bytes) {
  if (!buf || !used_bytes || !buf_size) {
    std::cerr << "Received empty buffer" << '\n';
    ::operator delete(buf);
    return;
  }

  pti_view_record_base *ptr = nullptr;
  while (true) {
    auto buf_status = ptiViewGetNextRecord(buf, used_bytes, &ptr);
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
        [[maybe_unused]] pti_view_record_external_correlation *aExtRec =
            reinterpret_cast<pti_view_record_external_correlation *>(ptr);
        break;
      }
      case pti_view_kind::PTI_VIEW_COLLECTION_OVERHEAD: {
        [[maybe_unused]] pti_view_record_overhead *record =
            reinterpret_cast<pti_view_record_overhead *>(ptr);
        break;
      }
      case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_COPY: {
        [[maybe_unused]] pti_view_record_memory_copy *rec =
            reinterpret_cast<pti_view_record_memory_copy *>(ptr);
        if (rec->_sycl_queue_id != kMaxQueueId) queue_id_memcpy_records = true;
        break;
      }
      case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_COPY_P2P: {
        [[maybe_unused]] pti_view_record_memory_copy_p2p *rec =
            reinterpret_cast<pti_view_record_memory_copy_p2p *>(ptr);
        break;
      }
      case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_FILL: {
        [[maybe_unused]] pti_view_record_memory_fill *rec =
            reinterpret_cast<pti_view_record_memory_fill *>(ptr);
        if (rec->_sycl_queue_id != kMaxQueueId) queue_id_memfill_records = true;
        break;
      }
      case pti_view_kind::PTI_VIEW_SYCL_RUNTIME_CALLS: {
        [[maybe_unused]] std::string function_name =
            reinterpret_cast<pti_view_record_sycl_runtime *>(ptr)->_name;
        break;
      }
      case pti_view_kind::PTI_VIEW_DEVICE_GPU_KERNEL: {
        [[maybe_unused]] pti_view_record_kernel *rec =
            reinterpret_cast<pti_view_record_kernel *>(ptr);
        if (rec->_sycl_queue_id != kMaxQueueId) queue_id_kernel_records = true;
        queue_id = rec->_sycl_queue_id;
        mt_q_ids.insert(queue_id);
        break;
      }
      default: {
        std::cerr << "This shouldn't happen" << '\n';
        break;
      }
    }
  }
  ::operator delete(buf);
}

template <typename T>
static void InitKernel(sycl::queue queue, std::vector<float> &a, unsigned size) {
  [[maybe_unused]] T dummy_variable = 1;
  PTI_ASSERT(size > 0);
  PTI_ASSERT(a.size() == size * size);
  try {
    sycl::buffer<float, 1> a_buf(a.data(), a.size());
    sycl::range<1> num_items{a.size()};
    sycl::event event = queue.submit([&](sycl::handler &cgh) {
      auto a_acc = a_buf.get_access<sycl::access::mode::write>(cgh);
      cgh.parallel_for(num_items, [=](auto i) { a_acc[i] = i; });
    });
    queue.wait_and_throw();
  } catch (const sycl::exception &e) {
    std::cout << "[ERROR] " << e.what() << std::endl;
    throw;
  }
}

static void InitKernelA(sycl::queue queue, std::vector<float> &a, unsigned size) {
  PTI_ASSERT(size > 0);
  PTI_ASSERT(a.size() == size * size);
  try {
    sycl::buffer<float, 1> a_buf(a.data(), a.size());
    sycl::range<1> num_items{a.size()};
    sycl::event event = queue.submit([&](sycl::handler &cgh) {
      auto a_acc = a_buf.get_access<sycl::access::mode::write>(cgh);
      cgh.parallel_for(num_items, [=](auto i) { a_acc[i] = i; });
    });
    queue.wait_and_throw();
  } catch (const sycl::exception &e) {
    std::cout << "[ERROR] " << e.what() << std::endl;
    throw;
  }
}

static void InitKernelAStackedQ(std::vector<float> &a, unsigned size) {
  PTI_ASSERT(size > 0);
  PTI_ASSERT(a.size() == size * size);
  sycl::device dev;
  dev = sycl::device(sycl::gpu_selector_v);

  sycl::property_list prop_list{sycl::property::queue::in_order()};
  sycl::queue q(dev, sycl::async_handler{}, prop_list);

  try {
    sycl::buffer<float, 1> a_buf(a.data(), a.size());
    sycl::range<1> num_items{a.size()};
    sycl::event event = q.submit([&](sycl::handler &cgh) {
      auto a_acc = a_buf.get_access<sycl::access::mode::write>(cgh);
      cgh.parallel_for(num_items, [=](auto i) { a_acc[i] = i; });
    });
    q.wait_and_throw();
  } catch (const sycl::exception &e) {
    std::cout << "[ERROR] " << e.what() << std::endl;
    throw;
  }
}

static void InitKernelB(sycl::queue queue, std::vector<float> &a, unsigned size) {
  PTI_ASSERT(size > 0);
  PTI_ASSERT(a.size() == size * size);
  try {
    sycl::buffer<float, 1> a_buf(a.data(), a.size());
    sycl::range<1> num_items{a.size()};
    sycl::event event = queue.submit([&](sycl::handler &cgh) {
      auto a_acc = a_buf.get_access<sycl::access::mode::write>(cgh);
      cgh.parallel_for(num_items, [=](auto i) { a_acc[i] = i; });
    });
    queue.wait_and_throw();
  } catch (const sycl::exception &e) {
    std::cout << "[ERROR] " << e.what() << std::endl;
    throw;
  }
}

static void InitKernelC(sycl::queue queue, std::vector<float> &a, unsigned size) {
  PTI_ASSERT(size > 0);
  PTI_ASSERT(a.size() == size * size);
  try {
    sycl::buffer<float, 1> a_buf(a.data(), a.size());
    sycl::range<1> num_items{a.size()};
    sycl::event event = queue.submit([&](sycl::handler &cgh) {
      auto a_acc = a_buf.get_access<sycl::access::mode::write>(cgh);
      cgh.parallel_for(num_items, [=](auto i) { a_acc[i] = i; });
    });
    queue.wait_and_throw();
  } catch (const sycl::exception &e) {
    std::cout << "[ERROR] " << e.what() << std::endl;
    throw;
  }
}

static void InitKernelD(sycl::queue queue, std::vector<float> &a, unsigned size) {
  PTI_ASSERT(size > 0);
  PTI_ASSERT(a.size() == size * size);
  try {
    sycl::buffer<float, 1> a_buf(a.data(), a.size());
    sycl::range<1> num_items{a.size()};
    sycl::event event = queue.submit([&](sycl::handler &cgh) {
      auto a_acc = a_buf.get_access<sycl::access::mode::write>(cgh);
      cgh.parallel_for(num_items, [=](auto i) { a_acc[i] = i; });
    });
    queue.wait_and_throw();
  } catch (const sycl::exception &e) {
    std::cout << "[ERROR] " << e.what() << std::endl;
    throw;
  }
}

static void InitKernelE(sycl::queue queue, std::vector<float> &a, unsigned size) {
  PTI_ASSERT(size > 0);
  PTI_ASSERT(a.size() == size * size);
  try {
    sycl::buffer<float, 1> a_buf(a.data(), a.size());
    sycl::range<1> num_items{a.size()};
    sycl::event event = queue.submit([&](sycl::handler &cgh) {
      auto a_acc = a_buf.get_access<sycl::access::mode::write>(cgh);
      cgh.parallel_for(num_items, [=](auto i) { a_acc[i] = i; });
    });
    queue.wait_and_throw();
  } catch (const sycl::exception &e) {
    std::cout << "[ERROR] " << e.what() << std::endl;
    throw;
  }
}

static void InitKernelF(sycl::queue queue, std::vector<float> &a, unsigned size) {
  PTI_ASSERT(size > 0);
  PTI_ASSERT(a.size() == size * size);
  try {
    sycl::buffer<float, 1> a_buf(a.data(), a.size());
    sycl::range<1> num_items{a.size()};
    sycl::event event = queue.submit([&](sycl::handler &cgh) {
      auto a_acc = a_buf.get_access<sycl::access::mode::write>(cgh);
      cgh.parallel_for(num_items, [=](auto i) { a_acc[i] = i; });
    });
    queue.wait_and_throw();
  } catch (const sycl::exception &e) {
    std::cout << "[ERROR] " << e.what() << std::endl;
    throw;
  }
}

//************************************
// Vector square in SYCL on device: modifies each input vector
//************************************
template <typename T>
void VecSq(sycl::queue &q, const std::vector<T> &a_vector, const std::vector<T> &b_vector) {
  sycl::range<1> num_items{a_vector.size()};
  sycl::buffer a_buf(a_vector);
  sycl::buffer b_buf(b_vector);

  q.submit([&](sycl::handler &h) {
    sycl::accessor a(a_buf, h, sycl::read_write);
    sycl::accessor b(b_buf, h, sycl::read_write);
    h.parallel_for(num_items, [=](auto i) {
      a[i] = a[i] * a[i];
      b[i] = b[i] * b[i];
    });
  });
  q.wait();
}

template <typename T>
void VecSqStackedQ(const std::vector<T> &a_vector, const std::vector<T> &b_vector) {
  sycl::range<1> num_items{a_vector.size()};
  sycl::buffer a_buf(a_vector);
  sycl::buffer b_buf(b_vector);

  sycl::device dev;
  dev = sycl::device(sycl::gpu_selector_v);

  sycl::property_list prop_list{sycl::property::queue::in_order()};
  sycl::queue q(dev, sycl::async_handler{}, prop_list);

  q.submit([&](sycl::handler &h) {
    sycl::accessor a(a_buf, h, sycl::read_write);
    sycl::accessor b(b_buf, h, sycl::read_write);
    h.parallel_for(num_items, [=](auto i) {
      a[i] = a[i] * a[i];
      b[i] = b[i] * b[i];
    });
  });
  q.wait();
}

template <typename T>
void SyclQueueIdMtTestsRouted(std::vector<sycl::queue> &queues, bool stacked_q) {
  std::vector<T> a(kVectorSize);
  std::vector<T> b(kVectorSize);
  auto thread_function = [](sycl::queue &q, const auto &a, const auto &b) { VecSq(q, a, b); };
  auto thread_function_stacked_q = [](const auto &a, const auto &b) { VecSqStackedQ(a, b); };

  std::vector<std::thread> the_threads;
  for (uint32_t i = 0; i < kThreadCount; i++) {
    if (stacked_q) {
      std::thread t = std::thread(thread_function_stacked_q, a, b);
      the_threads.push_back(std::move(t));
    } else {
      std::thread t = std::thread(thread_function, std::ref(queues[i]), a, b);
      the_threads.push_back(std::move(t));
    }
  }

  for (auto &th : the_threads) {
    if (th.joinable()) {
      th.join();
    }
  }
}

int RunSyclQueueIdMtTests(bool stacked_q = false) {
  int exit_code = EXIT_SUCCESS;

  StartTracing();
  sycl::device dev;
  dev = sycl::device(sycl::gpu_selector_v);

  sycl::property_list prop_list{sycl::property::queue::enable_profiling(),
                                sycl::property::queue::in_order()};

  std::vector<sycl::queue> mt_queues;
  unsigned int queue_size = 5;

  for (uint32_t i = 0; i < queue_size; i++) {
    mt_queues.push_back(sycl::queue(dev, sycl::async_handler{}, prop_list));
  }

  auto devA = static_cast<float *>(malloc_device(kVectorSize * sizeof(float), mt_queues[2]));
  mt_queues[2]
      .memset(devA, 0, kVectorSize * sizeof(float))
      .wait();  // force a memfill pti record to test.

  if (dev.has(sycl::aspect::fp64)) {
    SyclQueueIdMtTestsRouted<double>(mt_queues, stacked_q);
  } else {
    SyclQueueIdMtTestsRouted<float>(mt_queues, stacked_q);
  }

  return exit_code;
}

int RunSyclQueueIdTests(bool use_same_q = false, bool use_same_k = false,
                        bool templated_run = false, bool use_stacked_q = false) {
  int exit_code = EXIT_SUCCESS;
  unsigned size = 1024;
  std::vector<float> a(size * size, kAValue);

  if (use_stacked_q) {
    StartTracing();
    for (unsigned i = 0; i < kThreadCount; i++) {
      InitKernelAStackedQ(a, size);
    }
    StopTracing();
    return exit_code;
  }
  StartTracing();
  sycl::device dev;
  dev = sycl::device(sycl::gpu_selector_v);

  sycl::property_list prop_list{sycl::property::queue::enable_profiling(),
                                sycl::property::queue::in_order()};

  std::vector<sycl::queue> queues;
  std::vector<sycl::queue> mt_queues;
  unsigned int queue_size = 5;

  for (uint32_t i = 0; i < queue_size; i++) {
    queues.push_back(sycl::queue(dev, sycl::async_handler{}, prop_list));
  }

  auto devA = static_cast<float *>(malloc_device(size * sizeof(float), queues[2]));
  queues[2].memset(devA, 0, size * sizeof(float)).wait();  // force a memfill pti record to test.
  try {
    if (!templated_run) {
      if (use_same_q && use_same_k) {
        // same kernel, same Q
        InitKernelA(queues[1], a, size);
        queue_id_K1 = queue_id;
        InitKernelA(queues[1], a, size);
        queue_id_K2 = queue_id;
      } else if (use_same_q && !use_same_k) {  // Do not use KernelA or queue2 in below paths
        // different kernel, same Q
        InitKernelB(queues[1], a, size);
        queue_id_K1 = queue_id;
        InitKernelC(queues[1], a, size);
        queue_id_K2 = queue_id;
      } else if (!use_same_q && use_same_k) {  // Do not use KernelA/B/C or queue2/3 in below paths
        // same kernel, different Q
        InitKernelD(queues[1], a, size);
        queue_id_K1 = queue_id;
        InitKernelD(queues[2], a, size);
        queue_id_K2 = queue_id;
      } else if (!use_same_q &&
                 !use_same_k) {  // Do not use KernelA/B/C/D or queue2/3/4/5 in below paths
        // different kernel, different Q
        InitKernelE(queues[1], a, size);
        queue_id_K1 = queue_id;
        InitKernelF(queues[2], a, size);
        queue_id_K2 = queue_id;
      }
    } else {
      if (use_same_q && use_same_k) {
        InitKernel<uint32_t>(queues[1], a, size);
        queue_id_K1 = queue_id;
        InitKernel<uint32_t>(queues[1], a, size);
        queue_id_K2 = queue_id;
      } else if (!use_same_q && use_same_k) {
        InitKernel<uint64_t>(queues[1], a, size);
        queue_id_K1 = queue_id;
        InitKernel<uint64_t>(queues[2], a, size);
        queue_id_K2 = queue_id;
      } else if (!use_same_q && !use_same_k) {
        InitKernel<double>(queues[1], a, size);
        queue_id_K1 = queue_id;
        InitKernel<float>(queues[2], a, size);
        queue_id_K2 = queue_id;
      } else if (use_same_q && !use_same_k) {
        InitKernel<char>(queues[1], a, size);
        queue_id_K1 = queue_id;
        InitKernel<uint8_t>(queues[1], a, size);
        queue_id_K2 = queue_id;
      }
    }
    StopTracing();
  } catch (const sycl::exception &e) {
    std::cerr << "Error: Exception while executing SYCL " << e.what() << '\n';
    std::cerr << "\tError code: " << e.code().value() << "\n\tCategory: " << e.category().name()
              << "\n\tMessage: " << e.code().message() << '\n';
    exit_code = EXIT_FAILURE;
  } catch (const std::exception &e) {
    std::cerr << "Error: Exception caught " << e.what() << '\n';
    exit_code = EXIT_FAILURE;
  } catch (...) {
    std::cerr << "Error: Unknown exception caught." << '\n';
    exit_code = EXIT_FAILURE;
  }

  return exit_code;
}

class SyclQueueIdFixtureTest : public ::testing::TestWithParam<bool> {
 protected:
  void SetUp() override {
    queue_id_kernel_records = false;
    queue_id_memcpy_records = false;
    queue_id_memfill_records = false;
    mt_q_ids.clear();
    queue_id_K1 = -1;
    queue_id_K2 = 0;
    queue_id = -1;
  }

  void TearDown() override {}
};

//
// Test sequence is important due to queue_id being bound to kernel location (and NOT the queue
// itself).
//
TEST_P(SyclQueueIdFixtureTest, SameQSameKernelSameQID) {
  bool do_immediate = GetParam();
  utils::SetEnv("SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS", do_immediate ? "1" : "0");
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunSyclQueueIdTests(use_same_q = true, use_same_k = true, templated_run = false);
  ASSERT_NE(queue_id_K1, kMaxQueueId);
  ASSERT_NE(queue_id_K2, 0ULL);
  ASSERT_EQ(queue_id_K1, queue_id_K2);
}

TEST_P(SyclQueueIdFixtureTest, SameQDifferentKernelSameQID) {
  bool do_immediate = GetParam();
  utils::SetEnv("SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS", do_immediate ? "1" : "0");
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunSyclQueueIdTests(use_same_q = true, use_same_k = false, templated_run = false);
  ASSERT_NE(queue_id_K1, kMaxQueueId);
  ASSERT_NE(queue_id_K2, 0ULL);
  ASSERT_EQ(queue_id_K1, queue_id_K2);
}

#if __INTEL_LLVM_COMPILER >= 20240101 || __LIBSYCL_MAJOR_VERSION >= 8
TEST_P(SyclQueueIdFixtureTest, DifferentQSameKernelDifferentQID) {
  bool do_immediate = GetParam();
  utils::SetEnv("SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS", do_immediate ? "1" : "0");
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunSyclQueueIdTests(use_same_q = false, use_same_k = true, templated_run = false);
  ASSERT_NE(queue_id_K1, kMaxQueueId);
  ASSERT_NE(queue_id_K2, 0ULL);
  ASSERT_NE(queue_id_K1, queue_id_K2);
}

TEST_P(SyclQueueIdFixtureTest, DifferentQDifferentKernelDifferentQID) {
  bool do_immediate = GetParam();
  utils::SetEnv("SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS", do_immediate ? "1" : "0");
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunSyclQueueIdTests(use_same_q = false, use_same_k = false, templated_run = false);
  ASSERT_NE(queue_id_K1, kMaxQueueId);
  ASSERT_NE(queue_id_K2, 0ULL);
  ASSERT_NE(queue_id_K1, queue_id_K2);
}
#endif

TEST_P(SyclQueueIdFixtureTest, SameQSameTemplatedKernelSameQID) {
  bool do_immediate = GetParam();
  utils::SetEnv("SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS", do_immediate ? "1" : "0");
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunSyclQueueIdTests(use_same_q = true, use_same_k = true, templated_run = true);
  ASSERT_NE(queue_id_K1, kMaxQueueId);
  ASSERT_NE(queue_id_K2, 0ULL);
  ASSERT_EQ(queue_id_K1, queue_id_K2);
}

TEST_P(SyclQueueIdFixtureTest, SameQDifferentTemplatedKernelSameQID) {
  bool do_immediate = GetParam();
  utils::SetEnv("SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS", do_immediate ? "1" : "0");
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunSyclQueueIdTests(use_same_q = true, use_same_k = false, templated_run = true);
  ASSERT_NE(queue_id_K1, kMaxQueueId);
#if __INTEL_LLVM_COMPILER >= 20240101 || __LIBSYCL_MAJOR_VERSION >= 8
  ASSERT_NE(queue_id_K1, (kMaxQueueId - 1));
#endif
  ASSERT_NE(queue_id_K2, 0ULL);
  ASSERT_EQ(queue_id_K1, queue_id_K2);
}

#if __INTEL_LLVM_COMPILER >= 20240101 || __LIBSYCL_MAJOR_VERSION >= 8
TEST_P(SyclQueueIdFixtureTest, DifferentQSameTemplatedKernelDifferentQID) {
  bool do_immediate = GetParam();
  utils::SetEnv("SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS", do_immediate ? "1" : "0");
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunSyclQueueIdTests(use_same_q = false, use_same_k = true, templated_run = true);
  ASSERT_NE(queue_id_K1, kMaxQueueId);
  ASSERT_NE(queue_id_K2, 0ULL);
  ASSERT_NE(queue_id_K1, queue_id_K2);
}

TEST_P(SyclQueueIdFixtureTest, DifferentQDifferentTemplatedKernelDifferentQID) {
  bool do_immediate = GetParam();
  utils::SetEnv("SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS", do_immediate ? "1" : "0");
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunSyclQueueIdTests(use_same_q = false, use_same_k = false, templated_run = true);
  ASSERT_NE(queue_id_K1, kMaxQueueId);
  ASSERT_NE(queue_id_K2, 0ULL);
  ASSERT_NE(queue_id_K1, queue_id_K2);
}
#else
TEST_P(SyclQueueIdFixtureTest, InvalidQueueIDGeneratedAndUsed) {
  bool do_immediate = GetParam();
  utils::SetEnv("SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS", do_immediate ? "1" : "0");
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunSyclQueueIdTests(use_same_q = false, use_same_k = false, templated_run = true);
  ASSERT_EQ(queue_id_K1, (kMaxQueueId - 1));
}
#endif

TEST_P(SyclQueueIdFixtureTest, QueueIDPresentInAllRecords) {
  bool do_immediate = GetParam();
  utils::SetEnv("SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS", do_immediate ? "1" : "0");
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunSyclQueueIdTests(use_same_q = false, use_same_k = false, templated_run = false);
  ASSERT_EQ(queue_id_kernel_records, true);
  ASSERT_EQ(queue_id_memcpy_records, true);
  ASSERT_EQ(queue_id_memfill_records, true);
}

#if __INTEL_LLVM_COMPILER >= 20240101 || __LIBSYCL_MAJOR_VERSION >= 8
TEST_P(SyclQueueIdFixtureTest, STQueueIDsUniqueInLoopInstancesStackedQ) {
  bool do_immediate = GetParam();
  utils::SetEnv("SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS", do_immediate ? "1" : "0");
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunSyclQueueIdTests(use_same_q = false, use_same_k = false, templated_run = false,
                      use_stacked_q = true);
  ASSERT_EQ(mt_q_ids.size(), kThreadCount);
}
TEST_P(SyclQueueIdFixtureTest, MTQueueIDsUniqueInAllThreads) {
  bool do_immediate = GetParam();
  utils::SetEnv("SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS", do_immediate ? "1" : "0");
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunSyclQueueIdMtTests();
  ASSERT_EQ(mt_q_ids.size(), kThreadCount);
}

TEST_P(SyclQueueIdFixtureTest, MTQueueIDsUniqueInAllThreadsStackedQ) {
  bool do_immediate = GetParam();
  utils::SetEnv("SYCL_PI_LEVEL_ZERO_USE_IMMEDIATE_COMMANDLISTS", do_immediate ? "1" : "0");
  EXPECT_EQ(ptiViewSetCallbacks(BufferRequested, BufferCompleted), pti_result::PTI_SUCCESS);
  RunSyclQueueIdMtTests(use_stacked_q = true);
  ASSERT_EQ(mt_q_ids.size(), kThreadCount);
}
#endif

INSTANTIATE_TEST_SUITE_P(SyclQueueIdTests, SyclQueueIdFixtureTest, ::testing::Values(false, true));

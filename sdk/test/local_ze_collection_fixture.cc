#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <level_zero/ze_api.h>

#include <chrono>
#include <stdexcept>
#include <thread>

#include "pti/pti_view.h"
#include "utils.h"
#include "utils/gtest_helpers.h"
#include "utils/test_helpers.h"
#include "utils/ze_config_info.h"
#include "ze_utils.h"

namespace {
constexpr size_t kPtiDeviceId = 0;  // run on first device
constexpr auto kAlign = 64;
constexpr auto kAValue = 0.128f;
constexpr auto kBValue = 0.256f;
constexpr auto kMaxEps = 1.0e-4f;
constexpr uint32_t kDefaultEventWaitTimeMs = 5000;

// Prevent Test from hanging indefinitely
ze_result_t SpinBlockEvent(ze_event_handle_t event, uint32_t milliseconds) {
  ze_result_t result = ZE_RESULT_NOT_READY;
  auto start = std::chrono::high_resolution_clock::now();
  while ((result = zeEventQueryStatus(event)) == ZE_RESULT_NOT_READY) {
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
    if (elapsed > milliseconds) {
      return ZE_RESULT_NOT_READY;
    }
    std::this_thread::yield();
  }
  return result;
}

ze_result_t SpinBlockEvent(ze_event_handle_t event) {
  return SpinBlockEvent(event, kDefaultEventWaitTimeMs);
}

float Check(const std::vector<float>& result, float value) {
  if (value <= kMaxEps) {
    throw std::invalid_argument("Value must be greater than max epsilon");
  }

  float eps = 0.0f;
  for (auto result_val : result) {
    eps += std::fabs((result_val - value) / value);
  }
  return eps / static_cast<float>(std::size(result));
}
}  // namespace

class LocalModeZeGemmTest : public testing::Test {
 protected:
  LocalModeZeGemmTest()
      : spv_binary_(utils::LoadBinaryFile(utils::GetExecutablePath() + kKernelFile)) {
    a_vector_ = std::vector<float>(size_ * size_, kAValue);
    b_vector_ = std::vector<float>(size_ * size_, kBValue);
    result_vector_ = std::vector<float>(size_ * size_, 0);
  }

  void EnableView(pti_view_kind view) {
    EXPECT_EQ(ptiViewEnable(view), PTI_SUCCESS);
    enabled_views_.push_back(view);
  }

  void DisableAndFlushAllViews() {
    for (auto view : enabled_views_) {
      EXPECT_EQ(ptiViewDisable(view), PTI_SUCCESS);
    }
    enabled_views_.clear();
    EXPECT_EQ(ptiFlushAllViews(), PTI_SUCCESS);
  }

  void InitializeDrivers() {
    auto status = zeInit(ZE_INIT_FLAG_GPU_ONLY);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);
    drv_ = utils::ze::GetGpuDriver(kPtiDeviceId);
    dev_ = utils::ze::GetGpuDevice(kPtiDeviceId);
    ASSERT_NE(drv_, nullptr);
    ctx_ = utils::ze::GetContext(drv_);
  }

  void InitializeEvents(uint32_t event_count) {
    ze_event_pool_desc_t event_pool_desc = {
        ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr,
        ZE_EVENT_POOL_FLAG_HOST_VISIBLE,  // all events in pool are visible to Host
        event_count                       // count
    };

    if (event_timestamps_enabled_) {
      event_pool_desc.flags |= ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
    }

    ASSERT_NE(ctx_, nullptr);
    ASSERT_NE(dev_, nullptr);
    auto status = zeEventPoolCreate(ctx_, &event_pool_desc, 1, &dev_, &evt_pl_);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);
    ze_event_desc_t event_desc = {ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, 0,
                                  ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST};

    ASSERT_NE(evt_pl_, nullptr);
    evts_.resize(event_count);
    for (uint32_t i = 0; i < event_count; ++i) {
      event_desc.index = i;
      ASSERT_EQ(zeEventCreate(evt_pl_, &event_desc, &evts_[i]), ZE_RESULT_SUCCESS);
    }
  }

  void InitializeQueue() {
    ze_command_queue_desc_t cmd_queue_desc = {
        ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC, nullptr, 0, 0, 0, ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS,
        ZE_COMMAND_QUEUE_PRIORITY_NORMAL};

    auto status = zeCommandQueueCreate(ctx_, dev_, &cmd_queue_desc, &cmd_q_);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);

    ze_command_list_desc_t cmd_list_desc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC, nullptr, 0, 0};

    if (kInorderQueue) {
      cmd_list_desc.flags |= ZE_COMMAND_LIST_FLAG_IN_ORDER;
    }

    status = zeCommandListCreate(ctx_, dev_, &cmd_list_desc, &cmd_list_);

    ASSERT_EQ(status, ZE_RESULT_SUCCESS);
  }

  void InitializeLists(bool sychronous) {
    ze_command_queue_desc_t cmd_queue_desc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC,
                                              nullptr,
                                              0,
                                              0,
                                              ZE_COMMAND_QUEUE_FLAG_IN_ORDER,
                                              ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS,
                                              ZE_COMMAND_QUEUE_PRIORITY_NORMAL};
    if (sychronous) {
      cmd_queue_desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    }
    uint32_t compute_queue_ordinal = 0;
    uint32_t copy_queue_ordinal = 0;
    ASSERT_EQ(pti::test::utils::level_zero::GetGroupOrdinals(dev_, compute_queue_ordinal,
                                                             copy_queue_ordinal),
              0);
    cmd_queue_desc.ordinal = compute_queue_ordinal;
    ASSERT_EQ(zeCommandListCreateImmediate(ctx_, dev_, &cmd_queue_desc, &compute_cmd_list_),
              ZE_RESULT_SUCCESS);
    cmd_queue_desc.ordinal = copy_queue_ordinal;
    ASSERT_EQ(zeCommandListCreateImmediate(ctx_, dev_, &cmd_queue_desc, &copy_cmd_list_),
              ZE_RESULT_SUCCESS);
  }

  void SetKernelGroupSize() {
    ASSERT_NE(knl_, nullptr);
    auto status = zeKernelSuggestGroupSize(knl_, size_, size_, 1, std::data(group_size_),
                                           std::data(group_size_) + 1, std::data(group_size_) + 2);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);

    if ((size_ % group_size_[0]) != 0 || (size_ % group_size_[1]) != 0) {
      FAIL() << "Non-uniform group size";
    }
    status = zeKernelSetGroupSize(knl_, group_size_[0], group_size_[1], group_size_[2]);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);
  }

  void* AllocateDeviceBuffer(size_t size, size_t alignment) {
    void* storage = nullptr;
    const ze_device_mem_alloc_desc_t alloc_desc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC, nullptr,
                                                   0, 0};
    if (!ctx_) {
      return nullptr;
    }
    if (!dev_) {
      return nullptr;
    }

    auto status = zeMemAllocDevice(ctx_, &alloc_desc, size, alignment, dev_, &storage);

    if (status != ZE_RESULT_SUCCESS) {
      return nullptr;
    }

    return storage;
  }

  template <typename T>
  void AppendCopyToDevice(void* dev, const T& host_container) {
    ASSERT_NE(cmd_list_, nullptr);
    ASSERT_NE(dev, nullptr);
    auto status = zeCommandListAppendMemoryCopy(
        cmd_list_, dev, std::data(host_container),
        std::size(host_container) * sizeof(typename T::value_type), nullptr, 0, nullptr);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);
  }

  template <typename T>
  void AppendCopyFromDevice(T& host_container, const void* dev) {
    ASSERT_NE(cmd_list_, nullptr);
    ASSERT_NE(dev, nullptr);
    auto status = zeCommandListAppendMemoryCopy(
        cmd_list_, std::data(host_container), dev,
        std::size(host_container) * sizeof(typename T::value_type), nullptr, 0, nullptr);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);
  }

  void AppendBarrier() {
    ASSERT_NE(cmd_list_, nullptr);
    auto status = zeCommandListAppendBarrier(cmd_list_, nullptr, 0, nullptr);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);
  }

  void SetKernelGroupCount() {
    ASSERT_NE(group_size_[0], 0U);
    ASSERT_NE(group_size_[1], 0U);
    dim_ = {size_ / group_size_[0], size_ / group_size_[1], 1};
  }

  void AppendGemmKernel() {
    SetKernelGroupCount();
    ASSERT_NE(cmd_list_, nullptr);
    auto status = zeCommandListAppendLaunchKernel(cmd_list_, knl_, &dim_, evts_[0], 0, nullptr);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);
  }

  void ValidateGemmKernel() const {
    float expected_result = kAValue * kBValue * static_cast<float>(size_);
    auto eps = Check(result_vector_, expected_result);
    ASSERT_LE(eps, kMaxEps);
  }

  void PrepareCommandList() {
    AppendCopyToDevice(a_buf_, a_vector_);
    AppendCopyToDevice(b_buf_, b_vector_);
    AppendBarrier();
    AppendGemmKernel();
    AppendBarrier();
    AppendCopyFromDevice(result_vector_, result_buf_);
    AppendBarrier();
    ASSERT_NE(cmd_list_, nullptr);
    auto status = zeCommandListClose(cmd_list_);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);
  }

  void AllocateGemmDeviceBuffers() {
    a_buf_ = AllocateDeviceBuffer(size_ * size_ * sizeof(typename decltype(a_vector_)::value_type),
                                  kAlign);
    ASSERT_NE(a_buf_, nullptr);

    b_buf_ = AllocateDeviceBuffer(size_ * size_ * sizeof(typename decltype(b_vector_)::value_type),
                                  kAlign);
    ASSERT_NE(b_buf_, nullptr);

    result_buf_ = AllocateDeviceBuffer(
        size_ * size_ * sizeof(typename decltype(result_vector_)::value_type), kAlign);
    ASSERT_NE(result_buf_, nullptr);
  }

  void SetKernelArguments() {
    AllocateGemmDeviceBuffers();
    ASSERT_NE(knl_, nullptr);
    auto status = zeKernelSetArgumentValue(knl_, 0, sizeof(a_buf_), &a_buf_);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);
    status = zeKernelSetArgumentValue(knl_, 1, sizeof(b_buf_), &b_buf_);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);
    status = zeKernelSetArgumentValue(knl_, 2, sizeof(result_buf_), &result_buf_);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);
    status = zeKernelSetArgumentValue(knl_, 3, sizeof(size_), &size_);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);
  }

  void CreateKernel() {
    const ze_module_desc_t module_desc = {ZE_STRUCTURE_TYPE_MODULE_DESC,
                                          nullptr,
                                          ZE_MODULE_FORMAT_IL_SPIRV,
                                          std::size(spv_binary_),
                                          std::data(spv_binary_),
                                          nullptr,
                                          nullptr};

    auto status = zeModuleCreate(ctx_, dev_, &module_desc, &mdl_, nullptr);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);

    ASSERT_NE(mdl_, nullptr);
    const ze_kernel_desc_t kernel_desc = {ZE_STRUCTURE_TYPE_KERNEL_DESC, nullptr, 0, kKernelName};
    status = zeKernelCreate(mdl_, &kernel_desc, &knl_);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);
  }

  void SetUp() override {
    if (ptiViewGPULocalAvailable() != pti_result::PTI_SUCCESS) {
      GTEST_SKIP() << "GPULocal is not available. Skipping Test Suite";
    }
    ASSERT_NE(std::size(spv_binary_), 0ULL);

    ASSERT_EQ(ptiViewSetCallbacks(ProvideBuffer, ParseBuffer), pti_result::PTI_SUCCESS);

    LocalModeZeGemmTestData::Instance().Reset();
  }

  void TearDown() override {
    // Workaround for driver reusing handles:
    // The collector's internal state does not reset between tests. Also, unless tracing is enabled,
    // we do not properly track command list destruction.
    // Therefore, this is a workaround to enable tracing and ensure any internal state tracking the
    // queues is properly reset. For example, a handle being tracked as an "immediate" command list
    // could become a regular command list in the next test, and if we do not reset the state.
    // TODO: fix in the collector.

    EnableView(PTI_VIEW_DEVICE_GPU_KERNEL);

    // TODO: RAII-ify this
    if (result_buf_) {
      EXPECT_EQ(zeMemFree(ctx_, result_buf_), ZE_RESULT_SUCCESS);
    }

    if (b_buf_) {
      EXPECT_EQ(zeMemFree(ctx_, b_buf_), ZE_RESULT_SUCCESS);
    }

    if (a_buf_) {
      EXPECT_EQ(zeMemFree(ctx_, a_buf_), ZE_RESULT_SUCCESS);
    }

    if (cmd_list_) {
      EXPECT_EQ(zeCommandListDestroy(cmd_list_), ZE_RESULT_SUCCESS);
    }

    if (compute_cmd_list_) {
      EXPECT_EQ(zeCommandListDestroy(compute_cmd_list_), ZE_RESULT_SUCCESS);
    }

    if (copy_cmd_list_) {
      EXPECT_EQ(zeCommandListDestroy(copy_cmd_list_), ZE_RESULT_SUCCESS);
    }

    if (cmd_q_) {
      EXPECT_EQ(zeCommandQueueDestroy(cmd_q_), ZE_RESULT_SUCCESS);
    }

    for (auto* evt : evts_) {
      if (evt) {
        EXPECT_EQ(zeEventDestroy(evt), ZE_RESULT_SUCCESS);
      }
    }

    if (evt_pl_) {
      EXPECT_EQ(zeEventPoolDestroy(evt_pl_), ZE_RESULT_SUCCESS);
    }

    if (knl_) {
      EXPECT_EQ(zeKernelDestroy(knl_), ZE_RESULT_SUCCESS);
    }

    if (mdl_) {
      EXPECT_EQ(zeModuleDestroy(mdl_), ZE_RESULT_SUCCESS);
    }

    if (ctx_) {
      EXPECT_EQ(zeContextDestroy(ctx_), ZE_RESULT_SUCCESS);
    }
    DisableAndFlushAllViews();
  }

  static void ProvideBuffer(unsigned char** buf, size_t* buf_size) {
    *buf = pti::test::utils::AlignedAlloc<unsigned char>(kRequestedBufferSize);
    if (!*buf) {
      FAIL() << "Unable to allocate buffer for PTI tracing";
    }
    *buf_size = kRequestedBufferSize;
  }

  static void ParseBuffer(unsigned char* buf, size_t buf_size, size_t used_bytes) {
    if (!buf || !used_bytes || !buf_size) {
      std::cerr << "Received empty buffer" << '\n';
      if (used_bytes) {
        pti::test::utils::AlignedDealloc(buf);
      }
      return;
    }
    pti_view_record_base* ptr = nullptr;
    while (true) {
      auto buf_status = ptiViewGetNextRecord(buf, used_bytes, &ptr);
      if (buf_status == pti_result::PTI_STATUS_END_OF_BUFFER) {
        break;
      }
      if (buf_status != pti_result::PTI_SUCCESS) {
        FAIL() << "Found Error Parsing Records from PTI";
        break;
      }
      switch (ptr->_view_kind) {
        case pti_view_kind::PTI_VIEW_INVALID: {
          FAIL() << "Found Invalid PTI View Record";
          break;
        }
        case pti_view_kind::PTI_VIEW_DRIVER_API: {
          LocalModeZeGemmTestData::Instance().num_ze_records++;
          break;
        }
        case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_COPY: {
          LocalModeZeGemmTestData::Instance().num_mem_copies++;
          break;
        }
        case pti_view_kind::PTI_VIEW_DEVICE_GPU_KERNEL: {
          LocalModeZeGemmTestData::Instance().num_kernels++;
          auto* kernel_record = reinterpret_cast<pti_view_record_kernel*>(ptr);
          EXPECT_THAT(kernel_record->_name, ::testing::StrEq(kKernelName));
          break;
        }
        default: {
          break;
        }
      }
    }
    pti::test::utils::AlignedDealloc(buf);
  }
  static constexpr auto kRequestedBufferSize = 1'000;
  static constexpr auto kInorderQueue = true;
  static constexpr const char* const kKernelName = "GEMM";
  static constexpr const char* const kKernelFile = "gemm.spv";

  struct LocalModeZeGemmTestData {
    static LocalModeZeGemmTestData& Instance() {
      static LocalModeZeGemmTestData test_data{};
      return test_data;
    }

    void Reset() {
      num_ze_records = 0;
      num_kernels = 0;
      num_mem_copies = 0;
    }

    size_t num_ze_records = 0;
    size_t num_kernels = 0;
    size_t num_mem_copies = 0;
  };

  std::vector<pti_view_kind> enabled_views_;
  bool event_timestamps_enabled_ = false;
  std::vector<uint8_t> spv_binary_;
  unsigned int size_ = 1024;
  std::vector<float> a_vector_;
  std::vector<float> b_vector_;
  std::vector<float> result_vector_;
  ze_driver_handle_t drv_ = nullptr;
  ze_device_handle_t dev_ = nullptr;
  ze_context_handle_t ctx_ = nullptr;
  ze_module_handle_t mdl_ = nullptr;
  std::array<uint32_t, 3> group_size_ = {0};
  ze_kernel_handle_t knl_ = nullptr;
  ze_event_pool_handle_t evt_pl_ = nullptr;
  std::vector<ze_event_handle_t> evts_;
  ze_command_queue_handle_t cmd_q_ = nullptr;
  ze_command_list_handle_t cmd_list_ = nullptr;
  ze_command_list_handle_t copy_cmd_list_ = nullptr;
  ze_command_list_handle_t compute_cmd_list_ = nullptr;
  void* a_buf_ = nullptr;
  void* b_buf_ = nullptr;
  void* result_buf_ = nullptr;
  ze_group_count_t dim_ = {0, 0, 0};
};

TEST_F(LocalModeZeGemmTest, TestStartTracingExecuteCommandQueue) {
  // Leaving out of Constructor / SetUp for now to allow extending to more test
  // cases.
  InitializeDrivers();
  InitializeEvents(1);
  InitializeQueue();
  CreateKernel();
  SetKernelGroupSize();
  SetKernelArguments();
  PrepareCommandList();

  EnableView(PTI_VIEW_DEVICE_GPU_KERNEL);
  EnableView(PTI_VIEW_DRIVER_API);

  auto status = zeCommandQueueExecuteCommandLists(cmd_q_, 1, &cmd_list_, nullptr);
  ASSERT_EQ(status, ZE_RESULT_SUCCESS);
  status = zeCommandQueueSynchronize(cmd_q_, UINT64_MAX);
  ASSERT_EQ(status, ZE_RESULT_SUCCESS);

  DisableAndFlushAllViews();

  EXPECT_EQ(LocalModeZeGemmTestData::Instance().num_ze_records, static_cast<size_t>(2));
  EXPECT_EQ(LocalModeZeGemmTestData::Instance().num_kernels, static_cast<size_t>(0));

  ValidateGemmKernel();
}

TEST_F(LocalModeZeGemmTest, TestStartTracingPrepareCommandList) {
  // Leaving out of Constructor / SetUp for now to allow extending to more test
  // cases.
  InitializeDrivers();
  InitializeEvents(1);
  InitializeQueue();
  CreateKernel();
  SetKernelGroupSize();
  SetKernelArguments();
  EnableView(PTI_VIEW_DEVICE_GPU_KERNEL);
  PrepareCommandList();

  auto status = zeCommandQueueExecuteCommandLists(cmd_q_, 1, &cmd_list_, nullptr);
  ASSERT_EQ(status, ZE_RESULT_SUCCESS);
  status = zeCommandQueueSynchronize(cmd_q_, UINT64_MAX);
  ASSERT_EQ(status, ZE_RESULT_SUCCESS);

  DisableAndFlushAllViews();

  EXPECT_EQ(LocalModeZeGemmTestData::Instance().num_kernels, static_cast<size_t>(1));

  ValidateGemmKernel();
}

TEST_F(LocalModeZeGemmTest, TestAsynchInorderQueueImplementationWithImmediateCommandLists) {
  constexpr auto kNumberOfEventsRequired = 4;
  InitializeDrivers();
  InitializeEvents(kNumberOfEventsRequired);
  InitializeLists(false);
  CreateKernel();
  SetKernelGroupSize();
  SetKernelArguments();
  SetKernelGroupCount();
  EnableView(PTI_VIEW_DEVICE_GPU_KERNEL);
  EnableView(PTI_VIEW_DEVICE_GPU_MEM_COPY);

  auto* memcpy_signal_1 = evts_[0];
  auto* memcpy_signal_2 = evts_[1];
  auto* kernel_signal = evts_[2];
  auto* memcpy_signal_3 = evts_[3];

  ASSERT_EQ(zeCommandListAppendMemoryCopy(copy_cmd_list_, a_buf_, std::data(a_vector_),
                                          std::size(a_vector_) * sizeof(float), memcpy_signal_1, 0,
                                          nullptr),
            ZE_RESULT_SUCCESS);
  ASSERT_EQ(zeCommandListAppendMemoryCopy(copy_cmd_list_, b_buf_, std::data(b_vector_),
                                          std::size(b_vector_) * sizeof(float), memcpy_signal_2, 1,
                                          &memcpy_signal_1),
            ZE_RESULT_SUCCESS);

  ASSERT_EQ(zeCommandListAppendLaunchKernel(compute_cmd_list_, knl_, &dim_, kernel_signal, 2,
                                            evts_.data()),
            ZE_RESULT_SUCCESS);

  ASSERT_EQ(zeCommandListAppendMemoryCopy(copy_cmd_list_, std::data(result_vector_), result_buf_,
                                          std::size(result_vector_) * sizeof(float),
                                          memcpy_signal_3, 1, &kernel_signal),
            ZE_RESULT_SUCCESS);

  ASSERT_EQ(zeEventHostSynchronize(memcpy_signal_3, ((std::numeric_limits<uint64_t>::max)() - 1)),
            ZE_RESULT_SUCCESS);

  DisableAndFlushAllViews();

  EXPECT_EQ(LocalModeZeGemmTestData::Instance().num_kernels, static_cast<size_t>(1));
  EXPECT_EQ(LocalModeZeGemmTestData::Instance().num_mem_copies, static_cast<size_t>(3));

  ValidateGemmKernel();
}

TEST_F(LocalModeZeGemmTest, TestAsynchInorderQueueImplementationWithImmediateCommandListsAndReset) {
  constexpr auto kNumberOfEventsRequired = 3;
  InitializeDrivers();
  InitializeEvents(kNumberOfEventsRequired);
  InitializeLists(true);  // ensure we're OK regarding synchronization.
  CreateKernel();
  SetKernelGroupSize();
  SetKernelArguments();
  SetKernelGroupCount();
  EnableView(PTI_VIEW_DEVICE_GPU_KERNEL);
  EnableView(PTI_VIEW_DEVICE_GPU_MEM_COPY);

  auto* memcpy_signal_1 = evts_[0];
  auto* memcpy_signal_2 = evts_[1];
  auto* kernel_signal = evts_[2];

  ASSERT_EQ(zeCommandListAppendMemoryCopy(copy_cmd_list_, a_buf_, std::data(a_vector_),
                                          std::size(a_vector_) * sizeof(float), memcpy_signal_1, 0,
                                          nullptr),
            ZE_RESULT_SUCCESS);

  ASSERT_EQ(zeCommandListAppendMemoryCopy(copy_cmd_list_, b_buf_, std::data(b_vector_),
                                          std::size(b_vector_) * sizeof(float), memcpy_signal_2, 1,
                                          &memcpy_signal_1),
            ZE_RESULT_SUCCESS);

  ASSERT_EQ(zeCommandListAppendLaunchKernel(compute_cmd_list_, knl_, &dim_, kernel_signal, 2,
                                            evts_.data()),
            ZE_RESULT_SUCCESS);

  ASSERT_EQ(zeEventHostReset(memcpy_signal_1), ZE_RESULT_SUCCESS);

  ASSERT_EQ(zeCommandListAppendMemoryCopy(copy_cmd_list_, std::data(result_vector_), result_buf_,
                                          std::size(result_vector_) * sizeof(float),
                                          memcpy_signal_1, 1, &kernel_signal),
            ZE_RESULT_SUCCESS);

  ASSERT_EQ(zeEventHostSynchronize(memcpy_signal_1, ((std::numeric_limits<uint64_t>::max)() - 1)),
            ZE_RESULT_SUCCESS);

  DisableAndFlushAllViews();

  EXPECT_EQ(LocalModeZeGemmTestData::Instance().num_kernels, static_cast<size_t>(1));
  EXPECT_EQ(LocalModeZeGemmTestData::Instance().num_mem_copies, static_cast<size_t>(3));

  ValidateGemmKernel();
}

TEST_F(LocalModeZeGemmTest,
       TestInorderQueueImplementationWithCommandListImmediateAppendCommandListsExp) {
  InitializeDrivers();
  InitializeEvents(4);
  InitializeQueue();
  InitializeLists(false);
  CreateKernel();
  SetKernelGroupSize();
  SetKernelArguments();
  SetKernelGroupCount();
  EnableView(PTI_VIEW_DEVICE_GPU_KERNEL);
  EnableView(PTI_VIEW_DEVICE_GPU_MEM_COPY);
  PrepareCommandList();
  ZE_ASSERT_SUCCESS_BUT_SKIP_UNSUPPORTED(zeCommandListImmediateAppendCommandListsExp(
      compute_cmd_list_, 1, &cmd_list_, evts_[1], 0, nullptr));

  ASSERT_EQ(zeEventHostSynchronize(evts_[1], ((std::numeric_limits<uint64_t>::max)() - 1)),
            ZE_RESULT_SUCCESS);

  DisableAndFlushAllViews();
  EXPECT_EQ(LocalModeZeGemmTestData::Instance().num_kernels, static_cast<size_t>(1));
  EXPECT_EQ(LocalModeZeGemmTestData::Instance().num_mem_copies, static_cast<size_t>(3));

  ValidateGemmKernel();
}

TEST_F(LocalModeZeGemmTest,
       TestAsynchInorderQueueImplementationWithImmediateCommandListsAndEventDestroy) {
  constexpr auto kNumberOfEventsRequired = 4;
  InitializeDrivers();
  InitializeEvents(kNumberOfEventsRequired);
  InitializeLists(true);  // ensure we're OK regarding synchronization.
  CreateKernel();
  SetKernelGroupSize();
  SetKernelArguments();
  SetKernelGroupCount();
  EnableView(PTI_VIEW_DEVICE_GPU_KERNEL);
  EnableView(PTI_VIEW_DEVICE_GPU_MEM_COPY);

  auto* memcpy_signal_1 = evts_[0];
  auto* memcpy_signal_2 = evts_[1];
  auto* kernel_signal = evts_[2];
  auto* memcpy_signal_3 = evts_[3];

  ASSERT_EQ(zeCommandListAppendMemoryCopy(copy_cmd_list_, a_buf_, std::data(a_vector_),
                                          std::size(a_vector_) * sizeof(float), memcpy_signal_1, 0,
                                          nullptr),
            ZE_RESULT_SUCCESS);

  ASSERT_EQ(zeCommandListAppendMemoryCopy(copy_cmd_list_, b_buf_, std::data(b_vector_),
                                          std::size(b_vector_) * sizeof(float), memcpy_signal_2, 1,
                                          &memcpy_signal_1),
            ZE_RESULT_SUCCESS);

  ASSERT_EQ(zeCommandListAppendLaunchKernel(compute_cmd_list_, knl_, &dim_, kernel_signal, 1,
                                            &memcpy_signal_2),
            ZE_RESULT_SUCCESS);

  // 2 waits for 1, so no need to wait for 1.
  ASSERT_EQ(SpinBlockEvent(memcpy_signal_2), ZE_RESULT_SUCCESS)
      << "Timeout waiting for event to be signaled";

  ASSERT_EQ(zeEventDestroy(memcpy_signal_1), ZE_RESULT_SUCCESS);
  evts_[0] = nullptr;  // prevent clean up segv

  ASSERT_EQ(zeCommandListReset(copy_cmd_list_), ZE_RESULT_SUCCESS);

  ASSERT_EQ(zeCommandListAppendMemoryCopy(copy_cmd_list_, std::data(result_vector_), result_buf_,
                                          std::size(result_vector_) * sizeof(float),
                                          memcpy_signal_3, 1, &kernel_signal),
            ZE_RESULT_SUCCESS);

  ASSERT_EQ(zeEventHostSynchronize(memcpy_signal_3, ((std::numeric_limits<uint64_t>::max)() - 1)),
            ZE_RESULT_SUCCESS);

  DisableAndFlushAllViews();

  EXPECT_EQ(LocalModeZeGemmTestData::Instance().num_kernels, static_cast<size_t>(1));
  EXPECT_EQ(LocalModeZeGemmTestData::Instance().num_mem_copies, static_cast<size_t>(3));

  ValidateGemmKernel();
}

TEST_F(LocalModeZeGemmTest,
       TestAsynchInorderQueueImplementationWithImmediateCommandListsSpinBlock) {
  constexpr auto kNumberOfEventsRequired = 4;
  InitializeDrivers();
  InitializeEvents(kNumberOfEventsRequired);
  InitializeLists(false);
  CreateKernel();
  SetKernelGroupSize();
  SetKernelArguments();
  SetKernelGroupCount();
  EnableView(PTI_VIEW_DEVICE_GPU_KERNEL);
  EnableView(PTI_VIEW_DEVICE_GPU_MEM_COPY);

  auto* memcpy_signal_1 = evts_[0];
  auto* memcpy_signal_2 = evts_[1];
  auto* kernel_signal = evts_[2];
  auto* memcpy_signal_3 = evts_[3];

  ASSERT_EQ(zeCommandListAppendMemoryCopy(copy_cmd_list_, a_buf_, std::data(a_vector_),
                                          std::size(a_vector_) * sizeof(float), memcpy_signal_1, 0,
                                          nullptr),
            ZE_RESULT_SUCCESS);

  ASSERT_EQ(SpinBlockEvent(memcpy_signal_1), ZE_RESULT_SUCCESS)
      << "Timeout waiting for event to be signaled";

  ASSERT_EQ(zeCommandListAppendMemoryCopy(copy_cmd_list_, b_buf_, std::data(b_vector_),
                                          std::size(b_vector_) * sizeof(float), memcpy_signal_2, 0,
                                          nullptr),
            ZE_RESULT_SUCCESS);

  ASSERT_EQ(SpinBlockEvent(memcpy_signal_2), ZE_RESULT_SUCCESS)
      << "Timeout waiting for event to be signaled";

  ASSERT_EQ(
      zeCommandListAppendLaunchKernel(compute_cmd_list_, knl_, &dim_, kernel_signal, 0, nullptr),
      ZE_RESULT_SUCCESS);

  // TODO: Investate why this doesn't work in FULL mode (i.e., PTI_COLLECTION_MODE=0)
  // ASSERT_EQ(SpinBlockEvent(kernel_signal), ZE_RESULT_SUCCESS);

  ASSERT_EQ(zeCommandListAppendMemoryCopy(copy_cmd_list_, std::data(result_vector_), result_buf_,
                                          std::size(result_vector_) * sizeof(float),
                                          memcpy_signal_3, 1, &kernel_signal),
            ZE_RESULT_SUCCESS);

  ASSERT_EQ(SpinBlockEvent(memcpy_signal_3), ZE_RESULT_SUCCESS)
      << "Timeout waiting for event to be signaled";

  DisableAndFlushAllViews();

  EXPECT_EQ(LocalModeZeGemmTestData::Instance().num_kernels, static_cast<size_t>(1));
  EXPECT_EQ(LocalModeZeGemmTestData::Instance().num_mem_copies, static_cast<size_t>(3));

  ValidateGemmKernel();
}

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <level_zero/ze_api.h>

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>

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
constexpr auto kDefaultWaitTime = std::chrono::milliseconds(5'000);
constexpr uint32_t kDefaultEventWaitTimeMs = kDefaultWaitTime.count();
constexpr uint64_t kDefaultEventWaitTimeNs =
    std::chrono::duration_cast<std::chrono::nanoseconds>(kDefaultWaitTime).count();

template <typename T>
std::size_t ContainerBytes(const T& container) {
  return std::size(container) * sizeof(typename T::value_type);
}

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

// Used with macros at the moment - avoid compiler warnings.
[[maybe_unused]] ze_result_t HostBlockEvent(ze_event_handle_t event, uint64_t block_time_ns) {
  return zeEventHostSynchronize(event, block_time_ns);
}

[[maybe_unused]] ze_result_t HostBlockEvent(ze_event_handle_t event) {
  return HostBlockEvent(event, kDefaultEventWaitTimeNs);
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
    ASSERT_NE(drv_, nullptr);
    auto props = utils::ze::GetDriverProperties(drv_);
    command_list_visit_supported_ =
        ::utils::ze::IsDriverExtensionSupported(drv_, ZE_COMMAND_VISIT_EXT_NAME);
    if (props) {
      if (props->driverVersion <=
          pti::test::utils::level_zero::kDriverVersionWithBrokenVisitorExtension) {
        command_list_visit_supported_ = false;
      }
    }
    dev_ = utils::ze::GetGpuDevice(kPtiDeviceId);
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

    if (command_list_visit_supported_) {
      cmd_list_desc.flags |= ZE_COMMAND_LIST_FLAG_ENABLE_COMMAND_VISIT;
    }

    status = zeCommandListCreate(ctx_, dev_, &cmd_list_desc, &cmd_list_);

    ASSERT_EQ(status, ZE_RESULT_SUCCESS);
  }

  void InitializeLists(bool synchronous) {
    ze_command_queue_desc_t cmd_queue_desc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC,
                                              nullptr,
                                              0,
                                              0,
                                              ZE_COMMAND_QUEUE_FLAG_IN_ORDER,
                                              ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS,
                                              ZE_COMMAND_QUEUE_PRIORITY_NORMAL};
    if (synchronous) {
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
    auto status =
        zeCommandListAppendMemoryCopy(cmd_list_, dev, std::data(host_container),
                                      ContainerBytes(host_container), nullptr, 0, nullptr);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);
  }

  template <typename T>
  void AppendCopyFromDevice(T& host_container, const void* dev) {
    ASSERT_NE(cmd_list_, nullptr);
    ASSERT_NE(dev, nullptr);
    auto status =
        zeCommandListAppendMemoryCopy(cmd_list_, std::data(host_container), dev,
                                      ContainerBytes(host_container), nullptr, 0, nullptr);
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

  static void ValidateTimestamps() {
    constexpr uint64_t kZero = 0;
    EXPECT_THAT(LocalModeZeGemmTestData::Instance().start_end_timestamps,
                ::testing::Each(
                    ::testing::AllOf(::testing::Pair(::testing::Ne(kZero), ::testing::Ne(kZero)),
                                     ::testing::Args<0, 1>(::testing::Lt()))));
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
    a_buf_ = AllocateDeviceBuffer(ContainerBytes(a_vector_), kAlign);
    ASSERT_NE(a_buf_, nullptr);

    b_buf_ = AllocateDeviceBuffer(ContainerBytes(b_vector_), kAlign);
    ASSERT_NE(b_buf_, nullptr);

    result_buf_ = AllocateDeviceBuffer(ContainerBytes(result_vector_), kAlign);
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
    const ze_module_desc_t module_desc = {
        ZE_STRUCTURE_TYPE_MODULE_DESC,
        nullptr,
        ZE_MODULE_FORMAT_IL_SPIRV,
        std::size(spv_binary_),
        std::data(spv_binary_),
        nullptr,
        nullptr,
    };

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

  template <typename RecordT>
  static void AddTimestamps(const RecordT* record) {
    if (!record) {
      return;
    }
    LocalModeZeGemmTestData::Instance().start_end_timestamps.push_back(
        std::make_pair(record->_start_timestamp, record->_end_timestamp));
  }

  static void ParseBuffer(unsigned char* buf, size_t buf_size, size_t used_bytes) {
    if (!buf || !used_bytes || !buf_size) {
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
          const auto* const mem_copy_record =
              reinterpret_cast<pti_view_record_memory_copy_type const*>(ptr);
          LocalModeZeGemmTestData::Instance().mem_copy_device_handles.push_back(
              mem_copy_record->_device_handle);
          LocalModeZeGemmTestData::Instance().mem_copy_names.emplace_back(mem_copy_record->_name);
          AddTimestamps(mem_copy_record);
          break;
        }
        case pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_FILL: {
          LocalModeZeGemmTestData::Instance().num_mem_fills++;
          const auto* const mem_fill_record =
              reinterpret_cast<pti_view_record_memory_fill_type const*>(ptr);
          LocalModeZeGemmTestData::Instance().mem_fill_device_handles.push_back(
              mem_fill_record->_device_handle);
          AddTimestamps(mem_fill_record);
          break;
        }
        case pti_view_kind::PTI_VIEW_DEVICE_SYNCHRONIZATION: {
          LocalModeZeGemmTestData::Instance().num_syncs++;
          break;
        }
        case pti_view_kind::PTI_VIEW_DEVICE_GPU_KERNEL: {
          LocalModeZeGemmTestData::Instance().num_kernels++;
          const auto* const kernel_record =
              reinterpret_cast<pti_view_record_kernel_type const*>(ptr);
          EXPECT_THAT(kernel_record->_name, ::testing::StrEq(kKernelName));
          AddTimestamps(kernel_record);
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
      num_mem_fills = 0;
      num_syncs = 0;
      mem_copy_device_handles.clear();
      mem_copy_names.clear();
      mem_fill_device_handles.clear();
      start_end_timestamps.clear();
    }

    size_t num_ze_records = 0;
    size_t num_kernels = 0;
    size_t num_mem_copies = 0;
    size_t num_mem_fills = 0;
    size_t num_syncs = 0;
    std::vector<pti_device_handle_t> mem_copy_device_handles;
    std::vector<std::string> mem_copy_names;
    std::vector<pti_device_handle_t> mem_fill_device_handles;
    std::vector<std::pair<uint64_t, uint64_t>> start_end_timestamps;
  };

  std::vector<pti_view_kind> enabled_views_;
  bool event_timestamps_enabled_ = false;
  std::vector<uint8_t> spv_binary_;
  unsigned int size_ = 1024;
  std::vector<float> a_vector_;
  std::vector<float> b_vector_;
  std::vector<float> result_vector_;
  bool command_list_visit_supported_ = false;
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

  if (command_list_visit_supported_) {
    EXPECT_EQ(LocalModeZeGemmTestData::Instance().num_kernels, static_cast<size_t>(1));
    SUCCEED()
        << "Command List Visiting is supported, expected 2 driver API records and 1 kernel record.";
  } else {
    EXPECT_EQ(LocalModeZeGemmTestData::Instance().num_kernels, static_cast<size_t>(0));
  }

  ValidateGemmKernel();
}

TEST_F(LocalModeZeGemmTest, TestStartTracingExecuteCommandQueueWithoutVisitorExtensionEnabled) {
  // Leaving out of Constructor / SetUp for now to allow extending to more test
  // cases.
  InitializeDrivers();
  command_list_visit_supported_ = false;
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

TEST_F(LocalModeZeGemmTest, TestExecuteCommandListResetWhileTracingStopped) {
  InitializeDrivers();
  if (!command_list_visit_supported_) {
    GTEST_SKIP() << "Command list visiting is not supported. Skipping test.";
  }
  InitializeEvents(1);
  InitializeQueue();
  CreateKernel();
  SetKernelGroupSize();
  SetKernelArguments();
  PrepareCommandList();

  EnableView(PTI_VIEW_DEVICE_GPU_KERNEL);
  ASSERT_EQ(zeCommandQueueExecuteCommandLists(cmd_q_, 1, &cmd_list_, nullptr), ZE_RESULT_SUCCESS);
  ASSERT_EQ(zeCommandQueueSynchronize(cmd_q_, UINT64_MAX), ZE_RESULT_SUCCESS);
  DisableAndFlushAllViews();
  ValidateGemmKernel();

  // Keep the first result buffer alive so the new recording copies to a different address.
  std::vector<float> first_result(size_ * size_, 0);
  result_vector_.swap(first_result);
  ASSERT_EQ(zeCommandListReset(cmd_list_), ZE_RESULT_SUCCESS);
  ASSERT_EQ(zeEventHostReset(evts_[0]), ZE_RESULT_SUCCESS);
  PrepareCommandList();

  EnableView(PTI_VIEW_DEVICE_GPU_KERNEL);
  ASSERT_EQ(zeCommandQueueExecuteCommandLists(cmd_q_, 1, &cmd_list_, nullptr), ZE_RESULT_SUCCESS);
  ASSERT_EQ(zeCommandQueueSynchronize(cmd_q_, UINT64_MAX), ZE_RESULT_SUCCESS);
  DisableAndFlushAllViews();

  EXPECT_EQ(LocalModeZeGemmTestData::Instance().num_kernels, static_cast<size_t>(2));
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
                                          ContainerBytes(a_vector_), memcpy_signal_1, 0, nullptr),
            ZE_RESULT_SUCCESS);
  ASSERT_EQ(zeCommandListAppendMemoryCopy(copy_cmd_list_, b_buf_, std::data(b_vector_),
                                          ContainerBytes(b_vector_), memcpy_signal_2, 1,
                                          &memcpy_signal_1),
            ZE_RESULT_SUCCESS);

  ASSERT_EQ(zeCommandListAppendLaunchKernel(compute_cmd_list_, knl_, &dim_, kernel_signal, 2,
                                            evts_.data()),
            ZE_RESULT_SUCCESS);

  ASSERT_EQ(zeCommandListAppendMemoryCopy(copy_cmd_list_, std::data(result_vector_), result_buf_,
                                          ContainerBytes(result_vector_), memcpy_signal_3, 1,
                                          &kernel_signal),
            ZE_RESULT_SUCCESS);

  ASSERT_EQ(zeEventHostSynchronize(memcpy_signal_3, ((std::numeric_limits<uint64_t>::max)() - 1)),
            ZE_RESULT_SUCCESS);

  DisableAndFlushAllViews();

  EXPECT_EQ(LocalModeZeGemmTestData::Instance().num_kernels, static_cast<size_t>(1));
  EXPECT_EQ(LocalModeZeGemmTestData::Instance().num_mem_copies, static_cast<size_t>(3));

  ValidateGemmKernel();
}

TEST_F(
    LocalModeZeGemmTest,
    TestAsynchInorderQueueImplementationWithImmediateCommandListsAndZeCommandListAppendLaunchKernelWithParameters) {
#if !defined(_WIN32)
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
                                          ContainerBytes(a_vector_), memcpy_signal_1, 0, nullptr),
            ZE_RESULT_SUCCESS);
  ASSERT_EQ(zeCommandListAppendMemoryCopy(copy_cmd_list_, b_buf_, std::data(b_vector_),
                                          ContainerBytes(b_vector_), memcpy_signal_2, 1,
                                          &memcpy_signal_1),
            ZE_RESULT_SUCCESS);

  ze_command_list_append_launch_kernel_param_cooperative_desc_t launch_params = {
      ZE_STRUCTURE_TYPE_COMMAND_LIST_APPEND_PARAM_COOPERATIVE_DESC,
      nullptr,
      static_cast<ze_bool_t>(0),
  };

  ZE_ASSERT_SUCCESS_BUT_SKIP_UNSUPPORTED(zeCommandListAppendLaunchKernelWithParameters(
      compute_cmd_list_, knl_, &dim_, static_cast<const void*>(&launch_params), kernel_signal, 2,
      evts_.data()));

  ASSERT_EQ(zeCommandListAppendMemoryCopy(copy_cmd_list_, std::data(result_vector_), result_buf_,
                                          ContainerBytes(result_vector_), memcpy_signal_3, 1,
                                          &kernel_signal),
            ZE_RESULT_SUCCESS);

  ASSERT_EQ(zeEventHostSynchronize(memcpy_signal_3, ((std::numeric_limits<uint64_t>::max)() - 1)),
            ZE_RESULT_SUCCESS);

  DisableAndFlushAllViews();

  EXPECT_EQ(LocalModeZeGemmTestData::Instance().num_kernels, static_cast<size_t>(1));
  EXPECT_EQ(LocalModeZeGemmTestData::Instance().num_mem_copies, static_cast<size_t>(3));

  ValidateGemmKernel();
#else
  GTEST_SKIP() << "zeCommandListAppendLaunchKernelWithParameters is not supported";
#endif
}

TEST_F(
    LocalModeZeGemmTest,
    TestAsynchInorderQueueImplementationWithImmediateCommandListsAndZeCommandListAppendLaunchKernelWithArguments) {
#if !defined(_WIN32)
  constexpr auto kNumberOfEventsRequired = 4;
  InitializeDrivers();
  InitializeEvents(kNumberOfEventsRequired);
  InitializeLists(false);
  AllocateGemmDeviceBuffers();
  CreateKernel();
  EnableView(PTI_VIEW_DEVICE_GPU_KERNEL);
  EnableView(PTI_VIEW_DEVICE_GPU_MEM_COPY);

  auto* memcpy_signal_1 = evts_[0];
  auto* memcpy_signal_2 = evts_[1];
  auto* kernel_signal = evts_[2];
  auto* memcpy_signal_3 = evts_[3];

  ASSERT_EQ(zeCommandListAppendMemoryCopy(copy_cmd_list_, a_buf_, std::data(a_vector_),
                                          ContainerBytes(a_vector_), memcpy_signal_1, 0, nullptr),
            ZE_RESULT_SUCCESS);
  ASSERT_EQ(zeCommandListAppendMemoryCopy(copy_cmd_list_, b_buf_, std::data(b_vector_),
                                          ContainerBytes(b_vector_), memcpy_signal_2, 1,
                                          &memcpy_signal_1),
            ZE_RESULT_SUCCESS);

  ASSERT_EQ(zeKernelSuggestGroupSize(knl_, size_, size_, 1, std::data(group_size_),
                                     std::data(group_size_) + 1, std::data(group_size_) + 2),
            ZE_RESULT_SUCCESS);

  std::array<void*, 4> kernel_args = {&a_buf_, &b_buf_, &result_buf_, &size_};
  ze_group_size_t suggested_group_size = {group_size_[0], group_size_[1], group_size_[2]};
  SetKernelGroupCount();
  ZE_ASSERT_SUCCESS_BUT_SKIP_UNSUPPORTED(zeCommandListAppendLaunchKernelWithArguments(
      compute_cmd_list_, knl_, dim_, suggested_group_size, kernel_args.data(), nullptr,
      kernel_signal, 2, evts_.data()));

  ASSERT_EQ(zeCommandListAppendMemoryCopy(copy_cmd_list_, std::data(result_vector_), result_buf_,
                                          ContainerBytes(result_vector_), memcpy_signal_3, 1,
                                          &kernel_signal),
            ZE_RESULT_SUCCESS);

  ASSERT_EQ(zeEventHostSynchronize(memcpy_signal_3, ((std::numeric_limits<uint64_t>::max)() - 1)),
            ZE_RESULT_SUCCESS);

  DisableAndFlushAllViews();

  EXPECT_EQ(LocalModeZeGemmTestData::Instance().num_kernels, static_cast<size_t>(1));
  EXPECT_EQ(LocalModeZeGemmTestData::Instance().num_mem_copies, static_cast<size_t>(3));

  ValidateGemmKernel();
#else
  GTEST_SKIP() << "zeCommandListAppendLaunchKernelWithArguments is not supported";
#endif
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
                                          ContainerBytes(a_vector_), memcpy_signal_1, 0, nullptr),
            ZE_RESULT_SUCCESS);

  ASSERT_EQ(zeCommandListAppendMemoryCopy(copy_cmd_list_, b_buf_, std::data(b_vector_),
                                          ContainerBytes(b_vector_), memcpy_signal_2, 1,
                                          &memcpy_signal_1),
            ZE_RESULT_SUCCESS);

  ASSERT_EQ(zeCommandListAppendLaunchKernel(compute_cmd_list_, knl_, &dim_, kernel_signal, 2,
                                            evts_.data()),
            ZE_RESULT_SUCCESS);

  ASSERT_EQ(zeEventHostReset(memcpy_signal_1), ZE_RESULT_SUCCESS);

  ASSERT_EQ(zeCommandListAppendMemoryCopy(copy_cmd_list_, std::data(result_vector_), result_buf_,
                                          ContainerBytes(result_vector_), memcpy_signal_1, 1,
                                          &kernel_signal),
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
                                          ContainerBytes(a_vector_), memcpy_signal_1, 0, nullptr),
            ZE_RESULT_SUCCESS);

  ASSERT_EQ(zeCommandListAppendMemoryCopy(copy_cmd_list_, b_buf_, std::data(b_vector_),
                                          ContainerBytes(b_vector_), memcpy_signal_2, 1,
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
                                          ContainerBytes(result_vector_), memcpy_signal_3, 1,
                                          &kernel_signal),
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
                                          ContainerBytes(a_vector_), memcpy_signal_1, 0, nullptr),
            ZE_RESULT_SUCCESS);

  ASSERT_EQ(SpinBlockEvent(memcpy_signal_1), ZE_RESULT_SUCCESS)
      << "Timeout waiting for event to be signaled";

  ASSERT_EQ(zeCommandListAppendMemoryCopy(copy_cmd_list_, b_buf_, std::data(b_vector_),
                                          ContainerBytes(b_vector_), memcpy_signal_2, 0, nullptr),
            ZE_RESULT_SUCCESS);

  ASSERT_EQ(SpinBlockEvent(memcpy_signal_2), ZE_RESULT_SUCCESS)
      << "Timeout waiting for event to be signaled";

  ASSERT_EQ(
      zeCommandListAppendLaunchKernel(compute_cmd_list_, knl_, &dim_, kernel_signal, 0, nullptr),
      ZE_RESULT_SUCCESS);

  // TODO(PTI): Investigate why this doesn't work in FULL mode (i.e., PTI_COLLECTION_MODE=0)
  // ASSERT_EQ(SpinBlockEvent(kernel_signal), ZE_RESULT_SUCCESS);

  ASSERT_EQ(zeCommandListAppendMemoryCopy(copy_cmd_list_, std::data(result_vector_), result_buf_,
                                          ContainerBytes(result_vector_), memcpy_signal_3, 1,
                                          &kernel_signal),
            ZE_RESULT_SUCCESS);

  ASSERT_EQ(SpinBlockEvent(memcpy_signal_3), ZE_RESULT_SUCCESS)
      << "Timeout waiting for event to be signaled";

  DisableAndFlushAllViews();

  EXPECT_EQ(LocalModeZeGemmTestData::Instance().num_kernels, static_cast<size_t>(1));
  EXPECT_EQ(LocalModeZeGemmTestData::Instance().num_mem_copies, static_cast<size_t>(3));

  ValidateGemmKernel();
}

TEST_F(LocalModeZeGemmTest,
       TestAsynchInorderQueueImplementationWithImmediateCommandListsDeviceSynch) {
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
  EnableView(PTI_VIEW_DEVICE_SYNCHRONIZATION);

  auto* memcpy_signal_1 = evts_[0];
  auto* memcpy_signal_2 = evts_[1];
  auto* kernel_signal = evts_[2];
  auto* memcpy_signal_3 = evts_[3];

  ASSERT_EQ(zeCommandListAppendMemoryCopy(copy_cmd_list_, a_buf_, std::data(a_vector_),
                                          ContainerBytes(a_vector_), memcpy_signal_1, 0, nullptr),
            ZE_RESULT_SUCCESS);

  ZE_ASSERT_SUCCESS_BUT_SKIP_UNSUPPORTED(zeDeviceSynchronize(dev_));

  ASSERT_EQ(zeCommandListAppendMemoryCopy(copy_cmd_list_, b_buf_, std::data(b_vector_),
                                          ContainerBytes(b_vector_), memcpy_signal_2, 0, nullptr),
            ZE_RESULT_SUCCESS);

  ZE_ASSERT_SUCCESS_BUT_SKIP_UNSUPPORTED(zeDeviceSynchronize(dev_));

  ASSERT_EQ(
      zeCommandListAppendLaunchKernel(compute_cmd_list_, knl_, &dim_, kernel_signal, 0, nullptr),
      ZE_RESULT_SUCCESS);

  ZE_ASSERT_SUCCESS_BUT_SKIP_UNSUPPORTED(zeDeviceSynchronize(dev_));

  ASSERT_EQ(zeCommandListAppendMemoryCopy(copy_cmd_list_, std::data(result_vector_), result_buf_,
                                          ContainerBytes(result_vector_), memcpy_signal_3, 1,
                                          &kernel_signal),
            ZE_RESULT_SUCCESS);

  ZE_ASSERT_SUCCESS_BUT_SKIP_UNSUPPORTED(zeDeviceSynchronize(dev_));

  DisableAndFlushAllViews();

  EXPECT_EQ(LocalModeZeGemmTestData::Instance().num_kernels, static_cast<size_t>(1));
  EXPECT_EQ(LocalModeZeGemmTestData::Instance().num_mem_copies, static_cast<size_t>(3));
  EXPECT_EQ(LocalModeZeGemmTestData::Instance().num_syncs, static_cast<size_t>(4));

  ValidateGemmKernel();
}

TEST_F(
    LocalModeZeGemmTest,
    TestAsynchInorderQueueImplementationWithImmediateCommandListsAndWithParametersMemoryOperations) {
#if !defined(_WIN32)
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
  EnableView(PTI_VIEW_DEVICE_GPU_MEM_FILL);

  auto* memcpy_signal_1 = evts_.at(0);
  auto* memcpy_signal_2 = evts_.at(1);
  auto* kernel_signal = evts_.at(2);
  auto* memcpy_signal_3 = evts_.at(3);

  constexpr float kFillValue = 0.0f;
  ZE_ASSERT_SUCCESS_BUT_SKIP_UNSUPPORTED(zeCommandListAppendMemoryFillWithParameters(
      compute_cmd_list_, result_buf_, &kFillValue, sizeof(kFillValue),
      ContainerBytes(result_vector_), nullptr, nullptr, 0, nullptr));

  ZE_ASSERT_SUCCESS_BUT_SKIP_UNSUPPORTED(zeCommandListAppendMemoryCopyWithParameters(
      copy_cmd_list_, a_buf_, std::data(a_vector_), ContainerBytes(a_vector_), nullptr,
      memcpy_signal_1, 0, nullptr));
  ASSERT_EQ(zeCommandListAppendMemoryCopyWithParameters(
                copy_cmd_list_, b_buf_, std::data(b_vector_), ContainerBytes(b_vector_), nullptr,
                memcpy_signal_2, 1, &memcpy_signal_1),
            ZE_RESULT_SUCCESS);

  ASSERT_EQ(zeCommandListAppendLaunchKernel(compute_cmd_list_, knl_, &dim_, kernel_signal, 2,
                                            evts_.data()),
            ZE_RESULT_SUCCESS);

  ASSERT_EQ(zeCommandListAppendMemoryCopyWithParameters(
                copy_cmd_list_, std::data(result_vector_), result_buf_,
                ContainerBytes(result_vector_), nullptr, memcpy_signal_3, 1, &kernel_signal),
            ZE_RESULT_SUCCESS);

  ASSERT_EQ(HostBlockEvent(memcpy_signal_3), ZE_RESULT_SUCCESS)
      << "Timeout waiting for event to be signaled";

  DisableAndFlushAllViews();

  EXPECT_EQ(LocalModeZeGemmTestData::Instance().num_kernels, static_cast<size_t>(1));
  EXPECT_EQ(LocalModeZeGemmTestData::Instance().num_mem_copies, static_cast<size_t>(3));
  EXPECT_EQ(LocalModeZeGemmTestData::Instance().num_mem_fills, static_cast<size_t>(1));
  EXPECT_THAT(LocalModeZeGemmTestData::Instance().mem_copy_device_handles,
              ::testing::Each(static_cast<pti_device_handle_t>(dev_)));
  EXPECT_THAT(LocalModeZeGemmTestData::Instance().mem_fill_device_handles,
              ::testing::Each(static_cast<pti_device_handle_t>(dev_)));
  ValidateTimestamps();

  ValidateGemmKernel();
#else
  GTEST_SKIP() << "zeCommandListAppendMemory{Copy,Fill}WithParameters is not supported";
#endif
}

TEST_F(LocalModeZeGemmTest,
       TestStartTracingImmediateAppendCommandListsWithParametersOfWithParametersMemoryOperations) {
#if !defined(_WIN32)
  InitializeDrivers();
  InitializeEvents(2);
  InitializeQueue();
  InitializeLists(false);
  CreateKernel();
  SetKernelGroupSize();
  SetKernelArguments();

  constexpr float kFillValue = 0.0f;
  ZE_ASSERT_SUCCESS_BUT_SKIP_UNSUPPORTED(zeCommandListAppendMemoryFillWithParameters(
      cmd_list_, result_buf_, &kFillValue, sizeof(kFillValue), ContainerBytes(result_vector_),
      nullptr, nullptr, 0, nullptr));
  ZE_ASSERT_SUCCESS_BUT_SKIP_UNSUPPORTED(zeCommandListAppendMemoryCopyWithParameters(
      cmd_list_, a_buf_, std::data(a_vector_), ContainerBytes(a_vector_), nullptr, nullptr, 0,
      nullptr));
  ASSERT_EQ(zeCommandListAppendMemoryCopyWithParameters(cmd_list_, b_buf_, std::data(b_vector_),
                                                        ContainerBytes(b_vector_), nullptr, nullptr,
                                                        0, nullptr),
            ZE_RESULT_SUCCESS);
  AppendBarrier();
  AppendGemmKernel();
  AppendBarrier();
  ASSERT_EQ(zeCommandListAppendMemoryCopyWithParameters(cmd_list_, std::data(result_vector_),
                                                        result_buf_, ContainerBytes(result_vector_),
                                                        nullptr, nullptr, 0, nullptr),
            ZE_RESULT_SUCCESS);
  AppendBarrier();
  ASSERT_EQ(zeCommandListClose(cmd_list_), ZE_RESULT_SUCCESS);

  EnableView(PTI_VIEW_DEVICE_GPU_KERNEL);
  EnableView(PTI_VIEW_DEVICE_GPU_MEM_COPY);
  EnableView(PTI_VIEW_DEVICE_GPU_MEM_FILL);

  ZE_ASSERT_SUCCESS_BUT_SKIP_UNSUPPORTED(zeCommandListImmediateAppendCommandListsWithParameters(
      compute_cmd_list_, 1, &cmd_list_, nullptr, evts_.at(1), 0, nullptr));

  ASSERT_EQ(HostBlockEvent(evts_.at(1)), ZE_RESULT_SUCCESS)
      << "Timeout waiting for event to be signaled";

  DisableAndFlushAllViews();

  const size_t expected_ops = command_list_visit_supported_ ? 1 : 0;
  EXPECT_EQ(LocalModeZeGemmTestData::Instance().num_kernels, expected_ops);
  EXPECT_EQ(LocalModeZeGemmTestData::Instance().num_mem_copies, 3 * expected_ops);
  EXPECT_EQ(LocalModeZeGemmTestData::Instance().num_mem_fills, expected_ops);
  EXPECT_THAT(LocalModeZeGemmTestData::Instance().mem_copy_device_handles,
              ::testing::Each(static_cast<pti_device_handle_t>(dev_)));
  EXPECT_THAT(LocalModeZeGemmTestData::Instance().mem_fill_device_handles,
              ::testing::Each(static_cast<pti_device_handle_t>(dev_)));
  ValidateTimestamps();

  ValidateGemmKernel();
#else
  GTEST_SKIP() << "zeCommandListImmediateAppendCommandListsWithParameters is not supported";
#endif
}

// Test for crashes when copying to and from images and checking that the events signal correctly.
// Note: this isn't image support - just a test to ensure no crash or hang.
TEST_F(LocalModeZeGemmTest, TestImageCopiesSignalTheirEvents) {
  InitializeDrivers();
  InitializeEvents(2);
  InitializeLists(false);

  ze_image_desc_t image_desc{};
  image_desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
  image_desc.type = ZE_IMAGE_TYPE_1D;
  image_desc.format = {
      ZE_IMAGE_FORMAT_LAYOUT_32, ZE_IMAGE_FORMAT_TYPE_FLOAT, ZE_IMAGE_FORMAT_SWIZZLE_R,
      ZE_IMAGE_FORMAT_SWIZZLE_0, ZE_IMAGE_FORMAT_SWIZZLE_0,  ZE_IMAGE_FORMAT_SWIZZLE_1,
  };
  image_desc.width = 1;
  image_desc.height = 1;
  image_desc.depth = 1;
  ze_image_handle_t image = nullptr;
  const auto status = zeImageCreate(ctx_, dev_, &image_desc, &image);
  if (status == ZE_RESULT_ERROR_UNSUPPORTED_FEATURE) {
    GTEST_SKIP() << "Images are not supported";
  }
  ASSERT_EQ(status, ZE_RESULT_SUCCESS);
  std::unique_ptr<std::remove_pointer_t<ze_image_handle_t>, decltype(&zeImageDestroy)> image_guard(
      image, &zeImageDestroy);

  EnableView(PTI_VIEW_DEVICE_GPU_MEM_COPY);

  constexpr float kPixel = 2.0f;
  float source = kPixel;
  float destination = 0.0f;
  ASSERT_EQ(zeCommandListAppendImageCopyFromMemory(compute_cmd_list_, image, &source, nullptr,
                                                   evts_.at(0), 0, nullptr),
            ZE_RESULT_SUCCESS);
  ASSERT_EQ(zeCommandListAppendImageCopyToMemory(compute_cmd_list_, &destination, image, nullptr,
                                                 evts_.at(1), 0, nullptr),
            ZE_RESULT_SUCCESS);
  EXPECT_EQ(HostBlockEvent(evts_.at(0)), ZE_RESULT_SUCCESS);
  EXPECT_EQ(HostBlockEvent(evts_.at(1)), ZE_RESULT_SUCCESS);
  EXPECT_EQ(zeImageDestroy(image_guard.release()), ZE_RESULT_SUCCESS);

  DisableAndFlushAllViews();
  EXPECT_EQ(destination, kPixel);
  EXPECT_THAT(LocalModeZeGemmTestData::Instance().mem_copy_device_handles,
              ::testing::Each(static_cast<pti_device_handle_t>(dev_)));
  EXPECT_THAT(LocalModeZeGemmTestData::Instance().mem_copy_names,
              ::testing::UnorderedElementsAre(
                  ::testing::StartsWith("zeCommandListAppendImageCopyFromMemory("),
                  ::testing::StartsWith("zeCommandListAppendImageCopyToMemory(")));
}

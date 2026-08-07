//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <gtest/gtest.h>
#include <level_zero/ze_api.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "graph_dotproduct_workload_info.h"
#include "graph_record_validation.h"
#include "pti/pti_view.h"
#include "utils/gtest_helpers.h"
#include "utils/pti_record_collection_fixture.h"
#include "utils/utils.h"
#include "utils/ze_symbol_loader.h"
#include "utils/ze_utils.h"
#include "ze_graph_workloads.h"

class ZeGraphTestSuite : public pti::test::utils::RecordCollectionFixture {
 protected:
  struct ZeGraphDestroy {
    ZeGraphTestSuite* test_suite = nullptr;
    void operator()(ze_graph_handle_t graph) const {
      if (graph) {
        test_suite->ze_graph_destroy_ext_(graph);
      }
    }
  };

  struct ZeExecutableGraphDestroy {
    ZeGraphTestSuite* test_suite = nullptr;
    void operator()(ze_executable_graph_handle_t exec_graph) const {
      if (exec_graph) {
        test_suite->ze_executable_graph_destroy_ext_(exec_graph);
      }
    }
  };

  static constexpr std::uint32_t kMaxEventsRequired = 100;
  static constexpr std::size_t kPtiDeviceId = 0;

  using Workload = DotProductWorkload<float>;
  using ZeGraph = std::unique_ptr<std::remove_pointer_t<ze_graph_handle_t>, ZeGraphDestroy>;
  using ZeExecutableGraph = std::unique_ptr<std::remove_pointer_t<ze_executable_graph_handle_t>,
                                            ZeExecutableGraphDestroy>;

  ZeGraphTestSuite()
      : spirv_binary_(
            utils::LoadBinaryFile(utils::GetExecutablePath() + Workload::kSpvKernelFile)) {}

  void SetUp() override {
    if (!loader_.Loaded()) {
      GTEST_SKIP() << "Level Zero loader not found. Skipping test.";
    }

    InitializeDriver();
    if (drv_ == nullptr || dev_ == nullptr) {
      GTEST_SKIP() << "No Level Zero GPU device available. Skipping ZeGraph test suite.";
    }

    ctx_ = utils::ze::GetContext(drv_);
    ASSERT_NE(ctx_, nullptr);

    auto [success, message] = LoadExtensions();
    if (!success) {
      GTEST_SKIP() << message;
    }

    ASSERT_NO_FATAL_FAILURE(CreateEventPool());
    lists_ = CreateDotProductLists(ctx_, dev_);
    ASSERT_NO_FATAL_FAILURE(CreateModule());
    kernels_ = CreateDotProductKernels(module_);
  }

  void TearDown() override {
    // TODO(PTI): Finish RAII-ifying all resources and simplify this function.
    kernels_ = DotProductKernels{};
    lists_ = DotProductLists{};
    for (auto* event : events_) {
      if (event) {
        zeEventDestroy(event);
      }
    }
    events_.clear();
    if (event_pool_) {
      zeEventPoolDestroy(event_pool_);
    }
    if (module_) {
      EXPECT_EQ(zeModuleDestroy(module_), ZE_RESULT_SUCCESS);
    }
    if (ctx_) {
      EXPECT_EQ(zeContextDestroy(ctx_), ZE_RESULT_SUCCESS);
    }
  }

  void InitializeDriver() {
    ASSERT_EQ(zeInit(ZE_INIT_FLAG_GPU_ONLY), ZE_RESULT_SUCCESS);
    drv_ = utils::ze::GetGpuDriver(kPtiDeviceId);
    dev_ = utils::ze::GetGpuDevice(kPtiDeviceId);
  }

  void CreateModule() {
    ASSERT_NE(ctx_, nullptr);
    ASSERT_NE(dev_, nullptr);
    ASSERT_NE(std::size(spirv_binary_), 0ULL);

    const ze_module_desc_t module_desc = {ZE_STRUCTURE_TYPE_MODULE_DESC,
                                          nullptr,
                                          ZE_MODULE_FORMAT_IL_SPIRV,
                                          std::size(spirv_binary_),
                                          std::data(spirv_binary_),
                                          nullptr,
                                          nullptr};
    ASSERT_EQ(zeModuleCreate(ctx_, dev_, &module_desc, &module_, nullptr), ZE_RESULT_SUCCESS);
    ASSERT_NE(module_, nullptr);
  }

  void CreateEventPool() {
    ASSERT_NE(ctx_, nullptr);
    ASSERT_NE(dev_, nullptr);

    const ze_event_pool_desc_t event_pool_desc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr,
                                                  ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
                                                  kMaxEventsRequired};
    ASSERT_EQ(zeEventPoolCreate(ctx_, &event_pool_desc, 1, &dev_, &event_pool_), ZE_RESULT_SUCCESS);
    ASSERT_NE(event_pool_, nullptr);
  }

  ze_event_handle_t GetEvent(ze_event_scope_flags_t signal_scope) {
    if (!event_pool_ || next_event_index_ >= kMaxEventsRequired) {
      return nullptr;
    }
    ze_event_handle_t event = nullptr;
    ze_event_desc_t event_desc = {ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, next_event_index_,
                                  signal_scope, ZE_EVENT_SCOPE_FLAG_SUBDEVICE};
    if (zeEventCreate(event_pool_, &event_desc, &event) != ZE_RESULT_SUCCESS || !event) {
      return nullptr;
    }
    ++next_event_index_;
    events_.push_back(event);
    return event;
  }

  ze_event_handle_t GetEvent() { return GetEvent(ZE_EVENT_SCOPE_FLAG_SUBDEVICE); }

  template <typename T>
  auto CreateDotProductVectors(size_t vector_size) {
    return ::CreateDotProductVectors<T>(ctx_, dev_, vector_size);
  }

  ZeGraph CreateGraph() {
    ze_graph_handle_t graph = nullptr;
    if (ze_graph_create_ext_(ctx_, nullptr, &graph) != ZE_RESULT_SUCCESS || !graph) {
      throw std::runtime_error("Failed to create graph");
    }
    return ZeGraph{graph, ZeGraphDestroy{this}};
  }

  template <typename Func, typename... Args>
  std::pair<ZeGraph, ZeExecutableGraph> CaptureGraph(ze_command_list_handle_t primary_list,
                                                     Func record_func, Args&&... args) {
    auto graph = CreateGraph();
    if (ze_command_list_begin_capture_into_graph_ext_(primary_list, graph.get(), nullptr) !=
        ZE_RESULT_SUCCESS) {
      throw std::runtime_error("Failed to begin graph capture");
    }

    record_func(std::forward<Args>(args)...);

    auto* graph_ptr = graph.get();
    if (ze_command_list_end_graph_capture_ext_(primary_list, nullptr, &graph_ptr) !=
        ZE_RESULT_SUCCESS) {
      throw std::runtime_error("Failed to end graph capture");
    }

    ze_executable_graph_handle_t exec_graph = nullptr;
    if (ze_graph_instantiate_ext_(graph.get(), nullptr, &exec_graph) != ZE_RESULT_SUCCESS) {
      throw std::runtime_error("Failed to instantiate graph");
    }
    return {std::move(graph), ZeExecutableGraph{exec_graph, ZeExecutableGraphDestroy{this}}};
  }

  void ExecuteGraph(ze_command_list_handle_t command_list, ze_executable_graph_handle_t exec_graph,
                    ze_event_handle_t signal_event = nullptr, uint32_t num_wait_events = 0,
                    ze_event_handle_t* wait_events = nullptr) {
    ASSERT_EQ(ze_command_list_append_graph_ext_(command_list, exec_graph, nullptr, signal_event,
                                                num_wait_events, wait_events),
              ZE_RESULT_SUCCESS);
  }

  static void WaitForGraphExecution(ze_event_handle_t event) {
    ASSERT_EQ(zeEventHostSynchronize(event, (std::numeric_limits<uint64_t>::max)()),
              ZE_RESULT_SUCCESS);
  }

  auto& GetDotProductLists() { return lists_; }

  std::pair<bool, std::string> LoadExtensions() {
    if (!drv_) {
      return {false, "Driver handle is null"};
    }

    if (!utils::ze::IsDriverExtensionSupported(drv_, ZE_RECORD_REPLAY_GRAPH_EXT_NAME)) {
      return {false, "Level Zero " + std::string(ZE_RECORD_REPLAY_GRAPH_EXT_NAME) +
                         " extension not supported by the driver"};
    }

#define PTI_TEST_ZE_GET_SYMBOL(name) loader_.Get<decltype(&name)>(#name)
    ze_graph_create_ext_ = PTI_TEST_ZE_GET_SYMBOL(zeGraphCreateExt);
    ze_graph_destroy_ext_ = PTI_TEST_ZE_GET_SYMBOL(zeGraphDestroyExt);
    ze_graph_instantiate_ext_ = PTI_TEST_ZE_GET_SYMBOL(zeGraphInstantiateExt);
    ze_graph_is_empty_ext_ = PTI_TEST_ZE_GET_SYMBOL(zeGraphIsEmptyExt);
    ze_graph_dump_contents_ext_ = PTI_TEST_ZE_GET_SYMBOL(zeGraphDumpContentsExt);
    ze_graph_set_destruction_callback_ext_ =
        PTI_TEST_ZE_GET_SYMBOL(zeGraphSetDestructionCallbackExt);
    ze_command_list_begin_graph_capture_ext_ =
        PTI_TEST_ZE_GET_SYMBOL(zeCommandListBeginGraphCaptureExt);
    ze_command_list_begin_capture_into_graph_ext_ =
        PTI_TEST_ZE_GET_SYMBOL(zeCommandListBeginCaptureIntoGraphExt);
    ze_command_list_end_graph_capture_ext_ =
        PTI_TEST_ZE_GET_SYMBOL(zeCommandListEndGraphCaptureExt);
    ze_command_list_is_graph_capture_enabled_ext_ =
        PTI_TEST_ZE_GET_SYMBOL(zeCommandListIsGraphCaptureEnabledExt);
    ze_command_list_append_graph_ext_ = PTI_TEST_ZE_GET_SYMBOL(zeCommandListAppendGraphExt);
    ze_command_list_get_graph_ext_ = PTI_TEST_ZE_GET_SYMBOL(zeCommandListGetGraphExt);
    ze_executable_graph_destroy_ext_ = PTI_TEST_ZE_GET_SYMBOL(zeExecutableGraphDestroyExt);
#undef PTI_TEST_ZE_GET_SYMBOL

    if (!ze_graph_create_ext_ || !ze_graph_destroy_ext_ || !ze_graph_instantiate_ext_ ||
        !ze_graph_is_empty_ext_ || !ze_graph_dump_contents_ext_ ||
        !ze_graph_set_destruction_callback_ext_ || !ze_command_list_begin_graph_capture_ext_ ||
        !ze_command_list_begin_capture_into_graph_ext_ || !ze_command_list_end_graph_capture_ext_ ||
        !ze_command_list_is_graph_capture_enabled_ext_ || !ze_command_list_append_graph_ext_ ||
        !ze_command_list_get_graph_ext_ || !ze_executable_graph_destroy_ext_) {
      return {false,
              "Level Zero loader does not export the record and replay graph entry points. "
              "Skipping ZeGraph test suite."};
    }
    return {true, ""};
  }

  pti::test::utils::level_zero::ZeSymbolLoader loader_{};

  std::vector<std::uint8_t> spirv_binary_;

  ze_driver_handle_t drv_ = nullptr;
  ze_device_handle_t dev_ = nullptr;
  ze_context_handle_t ctx_ = nullptr;
  ze_module_handle_t module_ = nullptr;
  ze_event_pool_handle_t event_pool_ = nullptr;
  std::uint32_t next_event_index_ = 0;
  std::vector<ze_event_handle_t> events_;

  DotProductLists lists_;
  DotProductKernels kernels_;

  decltype(&zeGraphCreateExt) ze_graph_create_ext_ = nullptr;
  decltype(&zeGraphDestroyExt) ze_graph_destroy_ext_ = nullptr;
  decltype(&zeGraphInstantiateExt) ze_graph_instantiate_ext_ = nullptr;
  decltype(&zeGraphIsEmptyExt) ze_graph_is_empty_ext_ = nullptr;
  decltype(&zeGraphDumpContentsExt) ze_graph_dump_contents_ext_ = nullptr;
  decltype(&zeGraphSetDestructionCallbackExt) ze_graph_set_destruction_callback_ext_ = nullptr;
  decltype(&zeCommandListBeginGraphCaptureExt) ze_command_list_begin_graph_capture_ext_ = nullptr;
  decltype(&zeCommandListBeginCaptureIntoGraphExt) ze_command_list_begin_capture_into_graph_ext_ =
      nullptr;
  decltype(&zeCommandListEndGraphCaptureExt) ze_command_list_end_graph_capture_ext_ = nullptr;
  decltype(&zeCommandListIsGraphCaptureEnabledExt) ze_command_list_is_graph_capture_enabled_ext_ =
      nullptr;
  decltype(&zeCommandListAppendGraphExt) ze_command_list_append_graph_ext_ = nullptr;
  decltype(&zeCommandListGetGraphExt) ze_command_list_get_graph_ext_ = nullptr;
  decltype(&zeExecutableGraphDestroyExt) ze_executable_graph_destroy_ext_ = nullptr;
};

TEST_F(ZeGraphTestSuite, TestZeUsmGraphExecutionTracingGraphCreation) {
  constexpr std::size_t kExpectedNumberOfDriverCalls =
      10;  // 1 graph creation + 2 graph recording + 1 graph instantiation + 1 graph execution + 4
           // kernel launches + 1 synch
  auto vectors =
      CreateDotProductVectors<Workload::DefaultVectorDataType>(Workload::kDefaultVectorSize);
  auto& [primary, fork_one, fork_two] = GetDotProductLists();

  std::uint32_t vec_size = static_cast<std::uint32_t>(Workload::kDefaultVectorSize);

  auto* vector_init_event = GetEvent();
  ASSERT_NE(vector_init_event, nullptr);
  auto* vec_add1_event = GetEvent();
  ASSERT_NE(vec_add1_event, nullptr);
  auto* vec_add2_event = GetEvent();
  ASSERT_NE(vec_add2_event, nullptr);
  auto* graph_finish = GetEvent(ZE_EVENT_SCOPE_FLAG_HOST);
  ASSERT_NE(graph_finish, nullptr);

  // Start tracing before graph is created and captured.
  ASSERT_EQ(ptiViewSetCallbacks(ProvideBuffer, MarkBuffer), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_DRIVER_API), pti_result::PTI_SUCCESS);

  auto [graph, exec_graph] = CaptureGraph(
      primary.get(), RecordDotProductGraph, primary.get(), fork_one.get(), fork_two.get(),
      vector_init_event, vec_add1_event, vec_add2_event, kernels_, vectors, &vec_size);

  ASSERT_NO_FATAL_FAILURE(ExecuteGraph(primary.get(), exec_graph.get(), graph_finish));
  WaitForGraphExecution(graph_finish);

  EXPECT_FLOAT_EQ(*std::get<0>(vectors), Workload::Result());
  ASSERT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewDisable(PTI_VIEW_DRIVER_API), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiFlushAllViews(), pti_result::PTI_SUCCESS);
  ParseAllBuffers();
  // Graph creation captured, so records expected for all kernels in the graph.
  EXPECT_EQ(std::size(record_storage_.kernel_records), std::size_t{Workload::kDefaultKernelNumber});
  EXPECT_GE(std::size(record_storage_.api_records), kExpectedNumberOfDriverCalls);
  ValidateViewTimestamps(record_storage_.kernel_records);
}

TEST_F(ZeGraphTestSuite, TestZeUsmGraphExecutionWithoutTracingGraphCreation) {
  auto vectors =
      CreateDotProductVectors<Workload::DefaultVectorDataType>(Workload::kDefaultVectorSize);
  auto& [primary, fork_one, fork_two] = GetDotProductLists();

  std::uint32_t vec_size = static_cast<std::uint32_t>(Workload::kDefaultVectorSize);

  auto* vector_init_event = GetEvent();
  ASSERT_NE(vector_init_event, nullptr);
  auto* vec_add1_event = GetEvent();
  ASSERT_NE(vec_add1_event, nullptr);
  auto* vec_add2_event = GetEvent();
  ASSERT_NE(vec_add2_event, nullptr);
  auto* graph_finish = GetEvent(ZE_EVENT_SCOPE_FLAG_HOST);
  ASSERT_NE(graph_finish, nullptr);

  auto [graph, exec_graph] = CaptureGraph(
      primary.get(), RecordDotProductGraph, primary.get(), fork_one.get(), fork_two.get(),
      vector_init_event, vec_add1_event, vec_add2_event, kernels_, vectors, &vec_size);

  // Start tracing after graph is captured.
  ASSERT_EQ(ptiViewSetCallbacks(ProvideBuffer, MarkBuffer), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);

  ASSERT_NO_FATAL_FAILURE(ExecuteGraph(primary.get(), exec_graph.get(), graph_finish));
  WaitForGraphExecution(graph_finish);

  EXPECT_FLOAT_EQ(*std::get<0>(vectors), Workload::Result());
  ASSERT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiFlushAllViews(), pti_result::PTI_SUCCESS);
  ParseAllBuffers();
  // Graph creation not captured, so no records expected.
  EXPECT_EQ(std::size(record_storage_.kernel_records), std::size_t{4});
  ValidateViewTimestamps(record_storage_.kernel_records);
}

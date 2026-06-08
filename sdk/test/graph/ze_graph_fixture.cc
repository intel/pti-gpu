//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <gtest/gtest.h>
#include <level_zero/driver_experimental/zex_graph.h>
#include <level_zero/ze_api.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <vector>

#include "graph_dotproduct_workload_info.h"
#include "graph_record_validation.h"
#include "pti/pti_view.h"
#include "utils/pti_record_collection_fixture.h"
#include "utils/utils.h"
#include "utils/ze_utils.h"
#include "ze_graph_workloads.h"

class ZeGraphTestSuite : public pti::test::utils::RecordCollectionFixture {
 protected:
  // TODO(PTI): Move these into a common test utility header either when more tests are added that
  // need them or when graph extensions are supported by the Level Zero loader.
  struct ZeGraphDestroy {
    ZeGraphTestSuite* test_suite = nullptr;
    void operator()(ze_graph_handle_t graph) const {
      if (graph) {
        test_suite->ze_graph_destroy_exp_(graph);
      }
    }
  };

  struct ZeExecutableGraphDestroy {
    ZeGraphTestSuite* test_suite = nullptr;
    void operator()(ze_executable_graph_handle_t exec_graph) const {
      if (exec_graph) {
        test_suite->ze_executable_graph_destroy_exp_(exec_graph);
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
    constexpr static const char* const kUnsupportedGraphMessage =
        "Required Level Zero graph extensions not supported. Skipping ZeGraph test suite.";

#if !defined(ZE_RECORD_REPLAY_GRAPH_EXP_NAME)
    GTEST_SKIP() << kUnsupportedGraphMessage;
#endif

    InitializeDriver();
    if (drv_ == nullptr || dev_ == nullptr) {
      GTEST_SKIP() << "No Level Zero GPU device available. Skipping ZeGraph test suite.";
    }

    ctx_ = utils::ze::GetContext(drv_);
    ASSERT_NE(ctx_, nullptr);

    LoadExtensions();

    if (!graph_supported_) {
      GTEST_SKIP() << kUnsupportedGraphMessage;
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
    if (ze_graph_create_exp_(ctx_, &graph, nullptr) != ZE_RESULT_SUCCESS || !graph) {
      throw std::runtime_error("Failed to create graph");
    }
    return ZeGraph{graph, ZeGraphDestroy{this}};
  }

  template <typename Func, typename... Args>
  std::pair<ZeGraph, ZeExecutableGraph> CaptureGraph(ze_command_list_handle_t primary_list,
                                                     Func record_func, Args&&... args) {
    auto graph = CreateGraph();
    if (ze_command_list_begin_capture_into_graph_exp_(primary_list, graph.get(), nullptr) !=
        ZE_RESULT_SUCCESS) {
      throw std::runtime_error("Failed to begin graph capture");
    }

    record_func(std::forward<Args>(args)...);

    auto* graph_ptr = graph.get();
    if (ze_command_list_end_graph_capture_exp_(primary_list, &graph_ptr, nullptr) !=
        ZE_RESULT_SUCCESS) {
      throw std::runtime_error("Failed to end graph capture");
    }

    ze_executable_graph_handle_t exec_graph = nullptr;
    if (ze_command_list_instantiate_graph_exp_(graph.get(), &exec_graph, nullptr) !=
        ZE_RESULT_SUCCESS) {
      throw std::runtime_error("Failed to instantiate graph");
    }
    return {std::move(graph), ZeExecutableGraph{exec_graph, ZeExecutableGraphDestroy{this}}};
  }

  void ExecuteGraph(ze_command_list_handle_t command_list, ze_executable_graph_handle_t exec_graph,
                    ze_event_handle_t signal_event = nullptr, uint32_t num_wait_events = 0,
                    ze_event_handle_t* wait_events = nullptr) {
    ASSERT_EQ(ze_command_list_append_graph_exp_(command_list, exec_graph, nullptr, signal_event,
                                                num_wait_events, wait_events),
              ZE_RESULT_SUCCESS);
  }

  static void WaitForGraphExecution(ze_event_handle_t event) {
    ASSERT_EQ(zeEventHostSynchronize(event, (std::numeric_limits<uint64_t>::max)()),
              ZE_RESULT_SUCCESS);
  }

  auto& GetDotProductLists() { return lists_; }

  template <typename FnPtr>
  bool LoadExtensionFunction(std::string_view fn_name, FnPtr& fn_ptr) {
    auto* ptr = utils::ze::GetExtensionFunctionAddr(drv_, fn_name.data());
    if (!ptr) {
      return false;
    }
    fn_ptr = reinterpret_cast<FnPtr>(ptr);
    return true;
  }

  void LoadExtensions() {
    ASSERT_NE(drv_, nullptr);

    if (!utils::ze::IsDriverExtensionSupported(drv_, ZE_RECORD_REPLAY_GRAPH_EXP_NAME)) {
      return;
    }
    graph_supported_ =
        LoadExtensionFunction("zeGraphCreateExp", ze_graph_create_exp_) &&
        LoadExtensionFunction("zeCommandListBeginGraphCaptureExp",
                              ze_command_list_begin_graph_capture_exp_) &&
        LoadExtensionFunction("zeCommandListBeginCaptureIntoGraphExp",
                              ze_command_list_begin_capture_into_graph_exp_) &&
        LoadExtensionFunction("zeCommandListEndGraphCaptureExp",
                              ze_command_list_end_graph_capture_exp_) &&
        LoadExtensionFunction("zeCommandListInstantiateGraphExp",
                              ze_command_list_instantiate_graph_exp_) &&
        LoadExtensionFunction("zeCommandListAppendGraphExp", ze_command_list_append_graph_exp_) &&
        LoadExtensionFunction("zeGraphDestroyExp", ze_graph_destroy_exp_) &&
        LoadExtensionFunction("zeExecutableGraphDestroyExp", ze_executable_graph_destroy_exp_) &&
        LoadExtensionFunction("zeCommandListIsGraphCaptureEnabledExp",
                              ze_command_list_is_graph_capture_enabled_exp_) &&
        LoadExtensionFunction("zeGraphIsEmptyExp", ze_graph_is_empty_exp_) &&
        LoadExtensionFunction("zeGraphDumpContentsExp", ze_graph_dump_contents_exp_) &&
        LoadExtensionFunction("zeCommandListGetGraphExp", ze_command_list_get_graph_exp_) &&
        LoadExtensionFunction("zeGraphSetDestructionCallbackExp",
                              ze_graph_set_destruction_callback_exp_);
  }

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

  bool graph_supported_ = false;
  decltype(&zeGraphCreateExp) ze_graph_create_exp_ = nullptr;
  decltype(&zeCommandListBeginGraphCaptureExp) ze_command_list_begin_graph_capture_exp_ = nullptr;
  decltype(&zeCommandListBeginCaptureIntoGraphExp) ze_command_list_begin_capture_into_graph_exp_ =
      nullptr;
  decltype(&zeCommandListEndGraphCaptureExp) ze_command_list_end_graph_capture_exp_ = nullptr;
  decltype(&zeCommandListInstantiateGraphExp) ze_command_list_instantiate_graph_exp_ = nullptr;
  decltype(&zeCommandListAppendGraphExp) ze_command_list_append_graph_exp_ = nullptr;
  decltype(&zeGraphDestroyExp) ze_graph_destroy_exp_ = nullptr;
  decltype(&zeExecutableGraphDestroyExp) ze_executable_graph_destroy_exp_ = nullptr;
  decltype(&zeCommandListIsGraphCaptureEnabledExp) ze_command_list_is_graph_capture_enabled_exp_ =
      nullptr;
  decltype(&zeGraphIsEmptyExp) ze_graph_is_empty_exp_ = nullptr;
  decltype(&zeGraphDumpContentsExp) ze_graph_dump_contents_exp_ = nullptr;
  decltype(&zeCommandListGetGraphExp) ze_command_list_get_graph_exp_ = nullptr;
  decltype(&zeGraphSetDestructionCallbackExp) ze_graph_set_destruction_callback_exp_ = nullptr;
};

TEST_F(ZeGraphTestSuite, TestZeUsmGraphExecutionTracingGraphCreation) {
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

  auto [graph, exec_graph] = CaptureGraph(
      primary.get(), RecordDotProductGraph, primary.get(), fork_one.get(), fork_two.get(),
      vector_init_event, vec_add1_event, vec_add2_event, kernels_, vectors, &vec_size);

  ASSERT_NO_FATAL_FAILURE(ExecuteGraph(primary.get(), exec_graph.get(), graph_finish));
  WaitForGraphExecution(graph_finish);

  EXPECT_FLOAT_EQ(*std::get<0>(vectors), Workload::Result());
  ASSERT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiFlushAllViews(), pti_result::PTI_SUCCESS);
  ParseAllBuffers();
  // Graph creation captured, so records expected for all kernels in the graph.
  EXPECT_EQ(std::size(record_storage_.kernel_records), std::size_t{Workload::kDefaultKernelNumber});
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
  EXPECT_EQ(std::size(record_storage_.kernel_records), std::size_t{0});
  ValidateViewTimestamps(record_storage_.kernel_records);
}

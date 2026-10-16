//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <gtest/gtest.h>
#include <level_zero/ze_api.h>

#include <array>
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

namespace {

template <typename T>
inline void LeakHandle(T handle) {
  static std::vector<typename T::pointer> leaked_handles;
  leaked_handles.push_back(handle.release());
}

}  // namespace

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
    DestroyDriverResources();
    if (ctx_) {
      EXPECT_EQ(zeContextDestroy(ctx_), ZE_RESULT_SUCCESS);
    }
  }

  void DestroyDriverResources() {
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
      event_pool_ = nullptr;
    }
    if (module_) {
      EXPECT_EQ(zeModuleDestroy(module_), ZE_RESULT_SUCCESS);
      module_ = nullptr;
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

  ZeGraph CreateGraph(ze_context_handle_t ctx) {
    ze_graph_handle_t graph = nullptr;
    if (ze_graph_create_ext_(ctx, nullptr, &graph) != ZE_RESULT_SUCCESS || !graph) {
      throw std::runtime_error("Failed to create graph");
    }
    return ZeGraph{graph, ZeGraphDestroy{this}};
  }

  template <typename Func, typename... Args>
  std::pair<ZeGraph, ZeExecutableGraph> CaptureGraph(ze_command_list_handle_t primary_list,
                                                     Func record_func, Args&&... args) {
    return CaptureGraphInContext(ctx_, primary_list, record_func, std::forward<Args>(args)...);
  }

  template <typename Func, typename... Args>
  std::pair<ZeGraph, ZeExecutableGraph> CaptureGraphInContext(ze_context_handle_t ctx,
                                                              ze_command_list_handle_t primary_list,
                                                              Func record_func, Args&&... args) {
    auto graph = CreateGraph(ctx);
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
  EXPECT_EQ(std::size(record_storage_.kernel_records), std::size_t{4});
  ValidateViewTimestamps(record_storage_.kernel_records);
}

TEST_F(ZeGraphTestSuite, TestZeUsmGraphExecutionTraceWithExecutableGraphDestroy) {
  auto vectors =
      CreateDotProductVectors<Workload::DefaultVectorDataType>(Workload::kDefaultVectorSize);
  auto& [primary, fork_one, fork_two] = GetDotProductLists();

  auto vec_size = static_cast<std::uint32_t>(Workload::kDefaultVectorSize);

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

  exec_graph.reset();  // Destroy the executable graph while tracing is enabled.

  EXPECT_FLOAT_EQ(*std::get<0>(vectors), Workload::Result());
  ASSERT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiFlushAllViews(), pti_result::PTI_SUCCESS);
  ParseAllBuffers();
  EXPECT_EQ(std::size(record_storage_.kernel_records), std::size_t{4});
  ValidateViewTimestamps(record_storage_.kernel_records);
}

TEST_F(ZeGraphTestSuite, TestZeUsmGraphExecutionTraceWithContextDestroy) {
  auto vectors =
      CreateDotProductVectors<Workload::DefaultVectorDataType>(Workload::kDefaultVectorSize);
  auto& [primary, fork_one, fork_two] = GetDotProductLists();

  auto vec_size = static_cast<std::uint32_t>(Workload::kDefaultVectorSize);

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

  ASSERT_EQ(ptiViewSetCallbacks(ProvideBuffer, MarkBuffer), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);

  ASSERT_NO_FATAL_FAILURE(ExecuteGraph(primary.get(), exec_graph.get(), graph_finish));
  WaitForGraphExecution(graph_finish);

  EXPECT_FLOAT_EQ(*std::get<0>(vectors), Workload::Result());

  // Before the context is destroyed, the resources derived from that context must be destroyed or
  // else the test is not valid or the source of the crash will not be from the graph test.
  // Purposely leak the graph and executable graph to test context destroy behavior.
  LeakHandle(std::move(exec_graph));
  LeakHandle(std::move(graph));

  vectors = {};
  DestroyDriverResources();
  if (ctx_) {
    const auto destroy_result = zeContextDestroy(ctx_);
    ctx_ = nullptr;
    ASSERT_EQ(destroy_result, ZE_RESULT_SUCCESS);
  }

  ASSERT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiFlushAllViews(), pti_result::PTI_SUCCESS);
  ParseAllBuffers();
  EXPECT_EQ(std::size(record_storage_.kernel_records), std::size_t{4});
  ValidateViewTimestamps(record_storage_.kernel_records);
}

// Ensures we're handling state correctly when a graph is destroyed between graph executions within
// a workload. The graph must have completed: the executable graph must not be executing when
// destroyed.
TEST_F(ZeGraphTestSuite, TestZeUsmGraphExecutionTraceWithExecutableGraphHandleDestroyMidExecution) {
  auto first_vectors =
      CreateDotProductVectors<Workload::DefaultVectorDataType>(Workload::kDefaultVectorSize);
  auto second_vectors =
      CreateDotProductVectors<Workload::DefaultVectorDataType>(Workload::kDefaultVectorSize);
  auto& [primary, fork_one, fork_two] = GetDotProductLists();

  auto vec_size = static_cast<std::uint32_t>(Workload::kDefaultVectorSize);

  ASSERT_EQ(ptiViewSetCallbacks(ProvideBuffer, MarkBuffer), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);

  auto [first_graph, first_exec_graph] = CaptureGraph(
      primary.get(), RecordDotProductGraph, primary.get(), fork_one.get(), fork_two.get(),
      GetEvent(), GetEvent(), GetEvent(), kernels_, first_vectors, &vec_size);
  auto* first_graph_finish = GetEvent(ZE_EVENT_SCOPE_FLAG_HOST);
  ASSERT_NE(first_graph_finish, nullptr);
  ASSERT_NO_FATAL_FAILURE(ExecuteGraph(primary.get(), first_exec_graph.get(), first_graph_finish));
  WaitForGraphExecution(first_graph_finish);
  EXPECT_FLOAT_EQ(*std::get<0>(first_vectors), Workload::Result());

  first_exec_graph.reset();
  first_graph.reset();

  auto [second_graph, second_exec_graph] = CaptureGraph(
      primary.get(), RecordDotProductGraph, primary.get(), fork_one.get(), fork_two.get(),
      GetEvent(), GetEvent(), GetEvent(), kernels_, second_vectors, &vec_size);

  *std::get<0>(second_vectors) = 0;
  auto* second_graph_finish = GetEvent(ZE_EVENT_SCOPE_FLAG_HOST);
  ASSERT_NE(second_graph_finish, nullptr);
  ASSERT_NO_FATAL_FAILURE(
      ExecuteGraph(primary.get(), second_exec_graph.get(), second_graph_finish));
  WaitForGraphExecution(second_graph_finish);
  EXPECT_FLOAT_EQ(*std::get<0>(second_vectors), Workload::Result());

  ASSERT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiFlushAllViews(), pti_result::PTI_SUCCESS);
  ParseAllBuffers();
  EXPECT_EQ(std::size(record_storage_.kernel_records), std::size_t{8});
  ValidateViewTimestamps(record_storage_.kernel_records);
}

// Ensures destroying an executable graph that was never appended while tracing (so it was never
// stored or instrumented) does not touch the graph storage.
TEST_F(ZeGraphTestSuite, TestZeUsmExecutableGraphDestroyWithoutTracedExecution) {
  auto vectors =
      CreateDotProductVectors<Workload::DefaultVectorDataType>(Workload::kDefaultVectorSize);
  auto& [primary, fork_one, fork_two] = GetDotProductLists();

  auto vec_size = static_cast<std::uint32_t>(Workload::kDefaultVectorSize);

  auto [graph, exec_graph] = CaptureGraph(primary.get(), RecordDotProductGraph, primary.get(),
                                          fork_one.get(), fork_two.get(), GetEvent(), GetEvent(),
                                          GetEvent(), kernels_, vectors, &vec_size);

  ASSERT_EQ(ptiViewSetCallbacks(ProvideBuffer, MarkBuffer), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);

  exec_graph.reset();
  graph.reset();

  ASSERT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiFlushAllViews(), pti_result::PTI_SUCCESS);
  ParseAllBuffers();
  EXPECT_EQ(std::size(record_storage_.kernel_records), std::size_t{0});
}

// Ensures destroying a context only releases the graphs created from that context.
TEST_F(ZeGraphTestSuite, TestZeUsmGraphExecutionTraceWithOtherContextDestroy) {
  // Create a graph as normal
  auto vectors =
      CreateDotProductVectors<Workload::DefaultVectorDataType>(Workload::kDefaultVectorSize);
  auto& [primary, fork_one, fork_two] = GetDotProductLists();

  auto vec_size = static_cast<std::uint32_t>(Workload::kDefaultVectorSize);

  ASSERT_EQ(ptiViewSetCallbacks(ProvideBuffer, MarkBuffer), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);

  auto [graph, exec_graph] = CaptureGraph(primary.get(), RecordDotProductGraph, primary.get(),
                                          fork_one.get(), fork_two.get(), GetEvent(), GetEvent(),
                                          GetEvent(), kernels_, vectors, &vec_size);
  auto* first_graph_finish = GetEvent(ZE_EVENT_SCOPE_FLAG_HOST);
  ASSERT_NE(first_graph_finish, nullptr);
  // Execute graph once (as normal)
  ASSERT_NO_FATAL_FAILURE(ExecuteGraph(primary.get(), exec_graph.get(), first_graph_finish));
  WaitForGraphExecution(first_graph_finish);
  EXPECT_FLOAT_EQ(*std::get<0>(vectors), Workload::Result());

  // Create a new context and graph with that new context (will need to re-create other driver
  // resources with that new context handle)
  auto* other_ctx = utils::ze::GetContext(drv_);
  ASSERT_NE(other_ctx, nullptr);

  const ze_module_desc_t module_desc = {ZE_STRUCTURE_TYPE_MODULE_DESC,
                                        nullptr,
                                        ZE_MODULE_FORMAT_IL_SPIRV,
                                        std::size(spirv_binary_),
                                        std::data(spirv_binary_),
                                        nullptr,
                                        nullptr};
  ze_module_handle_t other_module = nullptr;
  ASSERT_EQ(zeModuleCreate(other_ctx, dev_, &module_desc, &other_module, nullptr),
            ZE_RESULT_SUCCESS);

  std::array<ze_event_handle_t, 4> other_events{};
  constexpr auto kOtherEventCount = static_cast<std::uint32_t>(other_events.size());
  constexpr auto kHostFlagEventIndex = kOtherEventCount - 1;
  const ze_event_pool_desc_t event_pool_desc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr,
                                                ZE_EVENT_POOL_FLAG_HOST_VISIBLE, kOtherEventCount};
  ze_event_pool_handle_t other_event_pool = nullptr;
  ASSERT_EQ(zeEventPoolCreate(other_ctx, &event_pool_desc, 1, &dev_, &other_event_pool),
            ZE_RESULT_SUCCESS);
  for (std::uint32_t i = 0; i < kOtherEventCount; ++i) {
    const auto signal_scope =
        (i == kHostFlagEventIndex) ? ZE_EVENT_SCOPE_FLAG_HOST : ZE_EVENT_SCOPE_FLAG_SUBDEVICE;
    const ze_event_desc_t event_desc = {ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, i, signal_scope,
                                        ZE_EVENT_SCOPE_FLAG_SUBDEVICE};
    ASSERT_EQ(zeEventCreate(other_event_pool, &event_desc, &other_events.at(i)), ZE_RESULT_SUCCESS);
  }

  {
    // Run the workload with a new context.
    auto other_lists = CreateDotProductLists(other_ctx, dev_);
    auto other_kernels = CreateDotProductKernels(other_module);
    auto other_vectors = ::CreateDotProductVectors<Workload::DefaultVectorDataType>(
        other_ctx, dev_, Workload::kDefaultVectorSize);
    auto& [other_primary, other_fork_one, other_fork_two] = other_lists;

    auto [other_graph, other_exec_graph] = CaptureGraphInContext(
        other_ctx, other_primary.get(), RecordDotProductGraph, other_primary.get(),
        other_fork_one.get(), other_fork_two.get(), other_events[0], other_events[1],
        other_events[2], other_kernels, other_vectors, &vec_size);
    ASSERT_NO_FATAL_FAILURE(
        ExecuteGraph(other_primary.get(), other_exec_graph.get(), other_events[3]));
    WaitForGraphExecution(other_events[3]);
    EXPECT_FLOAT_EQ(*std::get<0>(other_vectors), Workload::Result());
    LeakHandle(std::move(other_exec_graph));
    LeakHandle(std::move(other_graph));
  }

  for (auto* event : other_events) {
    EXPECT_EQ(zeEventDestroy(event), ZE_RESULT_SUCCESS);
  }
  EXPECT_EQ(zeEventPoolDestroy(other_event_pool), ZE_RESULT_SUCCESS);
  EXPECT_EQ(zeModuleDestroy(other_module), ZE_RESULT_SUCCESS);
  ASSERT_EQ(zeContextDestroy(other_ctx), ZE_RESULT_SUCCESS);

  *std::get<0>(vectors) = 0;
  auto* second_graph_finish = GetEvent(ZE_EVENT_SCOPE_FLAG_HOST);
  ASSERT_NE(second_graph_finish, nullptr);
  ASSERT_NO_FATAL_FAILURE(ExecuteGraph(primary.get(), exec_graph.get(), second_graph_finish));
  WaitForGraphExecution(second_graph_finish);
  EXPECT_FLOAT_EQ(*std::get<0>(vectors), Workload::Result());

  ASSERT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiFlushAllViews(), pti_result::PTI_SUCCESS);
  ParseAllBuffers();
  // Two executions of the graph in this context and one in the destroyed context.
  EXPECT_EQ(std::size(record_storage_.kernel_records), std::size_t{12});
  ValidateViewTimestamps(record_storage_.kernel_records);
}

// Excercise start/stop between replays.
TEST_F(ZeGraphTestSuite, TestZeUsmGraphExecutionTraceAcrossStopAndStartTracing) {
  constexpr std::size_t kNumberOfStartStopTracingCycles = 2;
  auto vectors =
      CreateDotProductVectors<Workload::DefaultVectorDataType>(Workload::kDefaultVectorSize);
  auto& [primary, fork_one, fork_two] = GetDotProductLists();

  auto vec_size = static_cast<std::uint32_t>(Workload::kDefaultVectorSize);

  auto [graph, exec_graph] = CaptureGraph(primary.get(), RecordDotProductGraph, primary.get(),
                                          fork_one.get(), fork_two.get(), GetEvent(), GetEvent(),
                                          GetEvent(), kernels_, vectors, &vec_size);

  ASSERT_EQ(ptiViewSetCallbacks(ProvideBuffer, MarkBuffer), pti_result::PTI_SUCCESS);
  for (std::size_t i = 0; i < kNumberOfStartStopTracingCycles; ++i) {
    *std::get<0>(vectors) = 0;
    auto* graph_finish = GetEvent(ZE_EVENT_SCOPE_FLAG_HOST);
    ASSERT_NE(graph_finish, nullptr);
    ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(ExecuteGraph(primary.get(), exec_graph.get(), graph_finish));
    WaitForGraphExecution(graph_finish);
    ASSERT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
    EXPECT_FLOAT_EQ(*std::get<0>(vectors), Workload::Result());
  }

  ASSERT_EQ(ptiFlushAllViews(), pti_result::PTI_SUCCESS);
  ParseAllBuffers();
  EXPECT_EQ(std::size(record_storage_.kernel_records),
            std::size_t{4} * kNumberOfStartStopTracingCycles);
  ValidateViewTimestamps(record_storage_.kernel_records);
}

TEST_F(ZeGraphTestSuite, TestZeUsmGraphExecutionTraceWithGraphDestroyedWhileTracingStopped) {
  auto first_vectors =
      CreateDotProductVectors<Workload::DefaultVectorDataType>(Workload::kDefaultVectorSize);
  auto second_vectors =
      CreateDotProductVectors<Workload::DefaultVectorDataType>(Workload::kDefaultVectorSize);
  auto& [primary, fork_one, fork_two] = GetDotProductLists();

  auto vec_size = static_cast<std::uint32_t>(Workload::kDefaultVectorSize);

  ASSERT_EQ(ptiViewSetCallbacks(ProvideBuffer, MarkBuffer), pti_result::PTI_SUCCESS);
  {
    auto [first_graph, first_exec_graph] = CaptureGraph(
        primary.get(), RecordDotProductGraph, primary.get(), fork_one.get(), fork_two.get(),
        GetEvent(), GetEvent(), GetEvent(), kernels_, first_vectors, &vec_size);
    auto* first_graph_finish = GetEvent(ZE_EVENT_SCOPE_FLAG_HOST);
    ASSERT_NE(first_graph_finish, nullptr);
    ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
    ASSERT_NO_FATAL_FAILURE(
        ExecuteGraph(primary.get(), first_exec_graph.get(), first_graph_finish));
    WaitForGraphExecution(first_graph_finish);
    ASSERT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
    EXPECT_FLOAT_EQ(*std::get<0>(first_vectors), Workload::Result());
    // Tracing is stopped and the graph is destroyed.
  }

  auto [second_graph, second_exec_graph] = CaptureGraph(
      primary.get(), RecordDotProductGraph, primary.get(), fork_one.get(), fork_two.get(),
      GetEvent(), GetEvent(), GetEvent(), kernels_, second_vectors, &vec_size);
  *std::get<0>(second_vectors) = 0;
  auto* second_graph_finish = GetEvent(ZE_EVENT_SCOPE_FLAG_HOST);
  ASSERT_NE(second_graph_finish, nullptr);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
  ASSERT_NO_FATAL_FAILURE(
      ExecuteGraph(primary.get(), second_exec_graph.get(), second_graph_finish));
  WaitForGraphExecution(second_graph_finish);
  EXPECT_FLOAT_EQ(*std::get<0>(second_vectors), Workload::Result());

  ASSERT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiFlushAllViews(), pti_result::PTI_SUCCESS);
  ParseAllBuffers();
  EXPECT_EQ(std::size(record_storage_.kernel_records), std::size_t{8});
  ValidateViewTimestamps(record_storage_.kernel_records);
}

// Stop tracing after the graph has been executed but before the graph event was synchronized.
TEST_F(ZeGraphTestSuite, TestZeUsmGraphExecutionStopTracingJustBeforeGraphSynchronization) {
  auto vectors =
      CreateDotProductVectors<Workload::DefaultVectorDataType>(Workload::kDefaultVectorSize);
  auto& [primary, fork_one, fork_two] = GetDotProductLists();

  auto vec_size = static_cast<std::uint32_t>(Workload::kDefaultVectorSize);

  auto [graph, exec_graph] = CaptureGraph(primary.get(), RecordDotProductGraph, primary.get(),
                                          fork_one.get(), fork_two.get(), GetEvent(), GetEvent(),
                                          GetEvent(), kernels_, vectors, &vec_size);
  auto* graph_start = GetEvent(ZE_EVENT_SCOPE_FLAG_HOST);
  ASSERT_NE(graph_start, nullptr);
  auto* graph_finish = GetEvent(ZE_EVENT_SCOPE_FLAG_HOST);
  ASSERT_NE(graph_finish, nullptr);

  ASSERT_EQ(ptiViewSetCallbacks(ProvideBuffer, MarkBuffer), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
  ASSERT_NO_FATAL_FAILURE(
      ExecuteGraph(primary.get(), exec_graph.get(), graph_finish, 1, &graph_start));
  ASSERT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);

  ASSERT_EQ(zeEventHostSignal(graph_start), ZE_RESULT_SUCCESS);
  WaitForGraphExecution(graph_finish);
  EXPECT_FLOAT_EQ(*std::get<0>(vectors), Workload::Result());

  *std::get<0>(vectors) = 0;
  auto* second_graph_finish = GetEvent(ZE_EVENT_SCOPE_FLAG_HOST);
  ASSERT_NE(second_graph_finish, nullptr);
  ASSERT_EQ(ptiViewEnable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
  ASSERT_NO_FATAL_FAILURE(ExecuteGraph(primary.get(), exec_graph.get(), second_graph_finish));
  WaitForGraphExecution(second_graph_finish);
  EXPECT_FLOAT_EQ(*std::get<0>(vectors), Workload::Result());

  ASSERT_EQ(ptiViewDisable(PTI_VIEW_DEVICE_GPU_KERNEL), pti_result::PTI_SUCCESS);
  ASSERT_EQ(ptiFlushAllViews(), pti_result::PTI_SUCCESS);
  ParseAllBuffers();
  // The pending execution's records are delivered once tracing starts again.
  EXPECT_EQ(std::size(record_storage_.kernel_records), std::size_t{8});
  ValidateViewTimestamps(record_storage_.kernel_records);
}

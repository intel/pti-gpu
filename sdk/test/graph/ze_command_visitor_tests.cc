#include <gtest/gtest.h>
#include <level_zero/driver_experimental/zex_visit.h>
#include <level_zero/ze_api.h>

#include <memory>
#include <stdexcept>
#include <type_traits>

#include "lz_api_tracing_api_loader.h"
#include "utils.h"
#include "utils/gtest_helpers.h"
#include "ze_command_visitor.h"
#include "ze_driver_init.h"
#include "ze_event_managers.h"
#include "ze_gpu_command.h"
#include "ze_utils.h"

namespace {
constexpr uint32_t kDriverVersionWithBrokenVisitorExtension = 17012470;
}  // namespace

class ZeCommandVisitorTestSuite : public testing::Test {
 protected:
  static constexpr size_t kPtiDeviceId = 0;
  static constexpr const char* const kKernelName = "GEMM";
  static constexpr const char* const kKernelFile = "gemm.spv";
  static constexpr uint32_t kSize = 1024;
  static constexpr size_t kAlign = 64;
  using CommandListUniquePtr = std::unique_ptr<std::remove_pointer_t<ze_command_list_handle_t>,
                                               void (*)(ze_command_list_handle_t)>;

  ZeCommandVisitorTestSuite()
      : spv_binary_(utils::LoadBinaryFile(utils::GetExecutablePath() + kKernelFile)) {}

  void SetUp() override {
    if (!driver_init_.Success()) {
      GTEST_SKIP() << "Driver initialization failed, skipping test.";
    }
    for (auto* driver : driver_init_.Drivers()) {
      auto extension = driver_init_.GetExtension<ZeExts::Visit>(driver);
      if (extension) {
        driver_ = driver;
        driver_props_ = utils::ze::GetDriverProperties(driver_);
        visit_extension_ = *extension;
        break;
      }
    }
    if (!visit_extension_.ze_command_list_visit) {
      GTEST_SKIP() << "Command list visit extension not supported, skipping test.";
    }
    auto drivers = std::vector<ze_driver_handle_t>{driver_};
    device_ = utils::ze::GetGpuDevice(drivers, kPtiDeviceId);
    if (!device_) {
      GTEST_SKIP() << "No GPU device found, skipping test.";
    }
    context_ = utils::ze::GetContext(driver_);
  }

  void CreateKernel() {
    const ze_module_desc_t module_desc = {ZE_STRUCTURE_TYPE_MODULE_DESC,
                                          nullptr,
                                          ZE_MODULE_FORMAT_IL_SPIRV,
                                          std::size(spv_binary_),
                                          std::data(spv_binary_),
                                          nullptr,
                                          nullptr};

    auto status = zeModuleCreate(context_, device_, &module_desc, &mdl_, nullptr);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);

    ASSERT_NE(mdl_, nullptr);
    const ze_kernel_desc_t kernel_desc = {ZE_STRUCTURE_TYPE_KERNEL_DESC, nullptr, 0, kKernelName};
    status = zeKernelCreate(mdl_, &kernel_desc, &knl_);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);
  }

  void SetKernelGroupSize() {
    ASSERT_NE(knl_, nullptr);
    auto status = zeKernelSuggestGroupSize(knl_, kSize, kSize, 1, std::data(group_size_),
                                           std::data(group_size_) + 1, std::data(group_size_) + 2);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);

    if ((kSize % group_size_[0]) != 0 || (kSize % group_size_[1]) != 0) {
      FAIL() << "Non-uniform group size";
    }
    status = zeKernelSetGroupSize(knl_, group_size_[0], group_size_[1], group_size_[2]);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);
  }

  void SetKernelGroupCount() {
    ASSERT_NE(group_size_[0], 0U);
    ASSERT_NE(group_size_[1], 0U);
    dim_ = {kSize / group_size_[0], kSize / group_size_[1], 1};
  }

  void* AllocateDeviceBuffer(size_t size, size_t alignment) {
    void* storage = nullptr;
    const ze_device_mem_alloc_desc_t alloc_desc = {ZE_STRUCTURE_TYPE_DEVICE_MEM_ALLOC_DESC, nullptr,
                                                   0, 0};
    if (!context_ || !device_) {
      return nullptr;
    }
    if (zeMemAllocDevice(context_, &alloc_desc, size, alignment, device_, &storage) !=
        ZE_RESULT_SUCCESS) {
      return nullptr;
    }
    return storage;
  }

  void AllocateGemmDeviceBuffers() {
    const size_t buffer_size = static_cast<size_t>(kSize) * kSize * sizeof(float);
    a_buf_ = AllocateDeviceBuffer(buffer_size, kAlign);
    ASSERT_NE(a_buf_, nullptr);
    b_buf_ = AllocateDeviceBuffer(buffer_size, kAlign);
    ASSERT_NE(b_buf_, nullptr);
    result_buf_ = AllocateDeviceBuffer(buffer_size, kAlign);
    ASSERT_NE(result_buf_, nullptr);
  }

  void TearDown() override {
    if (result_buf_) {
      EXPECT_EQ(zeMemFree(context_, result_buf_), ZE_RESULT_SUCCESS);
    }

    if (b_buf_) {
      EXPECT_EQ(zeMemFree(context_, b_buf_), ZE_RESULT_SUCCESS);
    }

    if (a_buf_) {
      EXPECT_EQ(zeMemFree(context_, a_buf_), ZE_RESULT_SUCCESS);
    }

    if (knl_) {
      EXPECT_EQ(zeKernelDestroy(knl_), ZE_RESULT_SUCCESS);
    }

    if (mdl_) {
      EXPECT_EQ(zeModuleDestroy(mdl_), ZE_RESULT_SUCCESS);
    }
    if (context_) {
      zeContextDestroy(context_);
    }
  }

  CommandListUniquePtr CreateCommandList() {
    ze_command_list_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
    desc.flags |= ZE_COMMAND_LIST_FLAG_ENABLE_CMD_VISITING;
    desc.pNext = nullptr;
    desc.commandQueueGroupOrdinal = 0;
    ze_command_list_handle_t command_list = nullptr;
    ze_result_t result = zeCommandListCreate(context_, device_, &desc, &command_list);
    if (result != ZE_RESULT_SUCCESS) {
      throw std::runtime_error("Failed to create command list");
    }
    return CommandListUniquePtr(command_list, [](auto cmdlist) {
      if (cmdlist) {
        zeCommandListDestroy(cmdlist);
      }
    });
  }

  static void ResetCommandList(CommandListUniquePtr& command_list) {
    if (command_list) {
      auto result = zeCommandListReset(command_list.get());
      if (result != ZE_RESULT_SUCCESS) {
        throw std::runtime_error("Failed to reset command list");
      }
    }
  }

  std::vector<uint8_t> spv_binary_;
  ZeDriverInit driver_init_{};
  ZeEventPoolManager event_pool_manager_{};
  ze_driver_handle_t driver_ = nullptr;
  std::optional<ze_driver_properties_t> driver_props_ = std::nullopt;
  ZeExts::Visit visit_extension_{};
  ze_device_handle_t device_ = nullptr;
  ze_context_handle_t context_ = nullptr;
  ze_module_handle_t mdl_ = nullptr;
  ze_kernel_handle_t knl_ = nullptr;
  std::array<uint32_t, 3> group_size_ = {0};
  ze_group_count_t dim_ = {0, 0, 0};
  void* a_buf_ = nullptr;
  void* b_buf_ = nullptr;
  void* result_buf_ = nullptr;
};

TEST_F(ZeCommandVisitorTestSuite, ConstructCommandVisitorWithNullEventPool) {
  // We need an event pool to construct the visitor to ensure proper swap event management.
  EXPECT_THROW(ZeCommandVisitor visitor(visit_extension_, nullptr);, std::invalid_argument);
}

TEST_F(ZeCommandVisitorTestSuite, ConstructCommandVisitorWithValidEventPool) {
  // We need an event pool to construct the visitor to ensure proper swap event management.
  EXPECT_NO_THROW(ZeCommandVisitor visitor(visit_extension_, &event_pool_manager_););
}

TEST_F(ZeCommandVisitorTestSuite, VisitEmptyCommandList) {
  auto command_list = CreateCommandList();
  auto command_list_instrumented = CreateCommandList();
  ZeCommandVisitor visitor(visit_extension_, &event_pool_manager_);
  ZeDeviceDescriptor device_desc{};
  ZeCommandListInfo command_list_info{};
  auto [commands, result] = visitor.Visit(device_desc, command_list_info, command_list.get(),
                                          command_list_instrumented.get());
  EXPECT_EQ(result, ZE_RESULT_SUCCESS);
  EXPECT_TRUE(commands.empty());
}

TEST_F(ZeCommandVisitorTestSuite, VisitOpenCommandListWithASingleCommand) {
  auto command_list = CreateCommandList();
  auto command_list_instrumented = CreateCommandList();
  ZeCommandVisitor visitor(visit_extension_, &event_pool_manager_);
  ZeDeviceDescriptor device_desc{};
  ZeCommandListInfo command_list_info{};
  command_list_info.closed = false;
  command_list_info.context = context_;
  command_list_info.device = device_;
  auto append_result = zeCommandListAppendBarrier(command_list.get(), nullptr, 0, nullptr);
  ASSERT_EQ(append_result, ZE_RESULT_SUCCESS);
  auto [commands, result] = visitor.Visit(device_desc, command_list_info, command_list.get(),
                                          command_list_instrumented.get());
  EXPECT_EQ(result, ZE_RESULT_SUCCESS);
  EXPECT_EQ(commands.size(), 1);
  EXPECT_EQ(commands[0].get()->props.type, KernelCommandType::kCommand);
}

TEST_F(ZeCommandVisitorTestSuite, VisitClosedCommandListWithASingleCommand) {
  auto command_list = CreateCommandList();
  auto command_list_instrumented = CreateCommandList();
  ZeCommandVisitor visitor(visit_extension_, &event_pool_manager_);
  ZeDeviceDescriptor device_desc{};
  ZeCommandListInfo command_list_info{};
  command_list_info.context = context_;
  command_list_info.device = device_;
  auto append_result = zeCommandListAppendBarrier(command_list.get(), nullptr, 0, nullptr);
  ASSERT_EQ(append_result, ZE_RESULT_SUCCESS);
  auto close_result = zeCommandListClose(command_list.get());
  ASSERT_EQ(close_result, ZE_RESULT_SUCCESS);
  command_list_info.closed = true;
  auto [commands, result] = visitor.Visit(device_desc, command_list_info, command_list.get(),
                                          command_list_instrumented.get());
  EXPECT_EQ(result, ZE_RESULT_SUCCESS);
  EXPECT_EQ(commands.size(), 1);
  EXPECT_EQ(commands[0].get()->props.type, KernelCommandType::kCommand);
}

TEST_F(ZeCommandVisitorTestSuite, VisitSkipsCommandWhenLoaderSymbolMissing) {
  if (driver_props_) {
    // zeCommandListAppendLaunchKernelWithArguments callback inside the driver has the wrong
    // signature. This is fixed in later driver versions.
    if (driver_props_->driverVersion <= kDriverVersionWithBrokenVisitorExtension) {
      GTEST_SKIP() << "Driver version " << kDriverVersionWithBrokenVisitorExtension
                   << " or lower has a broken (or non existent) command list visitor extension, "
                      "skipping test.";
    }
  }
  auto& loader = pti::PtiLzTracerLoader::Instance();
  auto saved = loader.zeCommandListAppendLaunchKernelWithArguments_;
  ASSERT_NE(saved, nullptr) << "Loader barrier symbol should be resolved before corruption";
  loader.zeCommandListAppendLaunchKernelWithArguments_ = nullptr;
  // Since the loader is global, ensure its restored after test runs.
  struct GuardLzLoader {
    using SavedSymbolType = decltype(saved);
    pti::PtiLzTracerLoader* lz_loader;
    SavedSymbolType saved_symbol;
    GuardLzLoader(pti::PtiLzTracerLoader* loader, SavedSymbolType symbol)
        : lz_loader(loader), saved_symbol(symbol) {}
    GuardLzLoader(const GuardLzLoader&) = delete;
    GuardLzLoader& operator=(const GuardLzLoader&) = delete;
    GuardLzLoader(GuardLzLoader&&) noexcept = delete;
    GuardLzLoader& operator=(GuardLzLoader&&) noexcept = delete;
    ~GuardLzLoader() { lz_loader->zeCommandListAppendLaunchKernelWithArguments_ = saved_symbol; }
  };

  GuardLzLoader restore_symbol{&loader, saved};

  auto command_list = CreateCommandList();
  auto command_list_instrumented = CreateCommandList();
  CreateKernel();
  SetKernelGroupSize();
  SetKernelGroupCount();
  AllocateGemmDeviceBuffers();

  ZeCommandVisitor visitor(visit_extension_, &event_pool_manager_);
  ZeDeviceDescriptor device_desc{};
  ZeCommandListInfo command_list_info{};
  command_list_info.context = context_;
  command_list_info.device = device_;
  const ze_group_size_t group_size{group_size_[0], group_size_[1], group_size_[2]};
  auto kernel_size = kSize;
  std::array<void*, 4> kernel_args = {&a_buf_, &b_buf_, &result_buf_, &kernel_size};
  auto append_result = zeCommandListAppendLaunchKernelWithArguments(
      command_list.get(), knl_, dim_, group_size, kernel_args.data(), nullptr, nullptr, 0, nullptr);
  ZE_ASSERT_SUCCESS_BUT_SKIP_UNSUPPORTED(append_result);
  EXPECT_EQ(append_result, ZE_RESULT_SUCCESS);
  auto close_result = zeCommandListClose(command_list.get());
  ASSERT_EQ(close_result, ZE_RESULT_SUCCESS);
  command_list_info.closed = true;

  auto [commands, result] = visitor.Visit(device_desc, command_list_info, command_list.get(),
                                          command_list_instrumented.get());

  // We're missing a symbol from the loader; therefore, we cannot rebuild the user's command list.
  EXPECT_TRUE(commands.empty());
  ASSERT_NE(result, ZE_RESULT_SUCCESS);
}

// This test is actually a regression test for a known issue in the driver where the command list
// visitor extension does not handle CommandListReset.
TEST_F(ZeCommandVisitorTestSuite, VisitClosedCommandListWithASingleCommandThenResetAndTryAgain) {
  if (driver_props_) {
    // This is fixed in later driver versions.
    if (driver_props_->driverVersion <= kDriverVersionWithBrokenVisitorExtension) {
      GTEST_SKIP() << "Driver version " << kDriverVersionWithBrokenVisitorExtension
                   << " or lower has a broken (or non existent) command list visitor extension, "
                      "skipping test.";
    }
  }
  auto command_list = CreateCommandList();
  auto command_list_instrumented = CreateCommandList();
  ZeCommandVisitor visitor(visit_extension_, &event_pool_manager_);
  ZeDeviceDescriptor device_desc{};
  ZeCommandListInfo command_list_info{};
  command_list_info.context = context_;
  command_list_info.device = device_;
  auto append_result = zeCommandListAppendBarrier(command_list.get(), nullptr, 0, nullptr);
  ASSERT_EQ(append_result, ZE_RESULT_SUCCESS);
  auto close_result = zeCommandListClose(command_list.get());
  ASSERT_EQ(close_result, ZE_RESULT_SUCCESS);
  command_list_info.closed = true;
  auto [commands, result] = visitor.Visit(device_desc, command_list_info, command_list.get(),
                                          command_list_instrumented.get());
  EXPECT_EQ(result, ZE_RESULT_SUCCESS);
  EXPECT_EQ(commands.size(), 1);
  EXPECT_EQ(commands[0].get()->props.type, KernelCommandType::kCommand);
  ResetCommandList(command_list);
  append_result = zeCommandListAppendBarrier(command_list.get(), nullptr, 0, nullptr);
  ASSERT_EQ(append_result, ZE_RESULT_SUCCESS);
  close_result = zeCommandListClose(command_list.get());
  ASSERT_EQ(close_result, ZE_RESULT_SUCCESS);
  auto [commands2, result2] = visitor.Visit(device_desc, command_list_info, command_list.get(),
                                            command_list_instrumented.get());
  EXPECT_EQ(result2, ZE_RESULT_SUCCESS);
  EXPECT_EQ(commands2.size(), 1);
  EXPECT_EQ(commands2[0].get()->props.type, KernelCommandType::kCommand);
}

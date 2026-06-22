#include <gtest/gtest.h>
#include <level_zero/ze_api.h>

#include <memory>
#include <stdexcept>
#include <type_traits>

#include "utils.h"
#include "ze_command_visitor.h"
#include "ze_driver_init.h"
#include "ze_event_managers.h"
#include "ze_gpu_command.h"
#include "ze_utils.h"

class ZeCommandVisitorTestSuite : public testing::Test {
 protected:
  static constexpr size_t kPtiDeviceId = 0;
  using CommandListUniquePtr = std::unique_ptr<std::remove_pointer_t<ze_command_list_handle_t>,
                                               void (*)(ze_command_list_handle_t)>;

  ZeCommandVisitorTestSuite() = default;

  void SetUp() override {
    if (!driver_init_.Success()) {
      GTEST_SKIP() << "Driver initialization failed, skipping test.";
    }
    for (auto* driver : driver_init_.Drivers()) {
      auto extension = driver_init_.GetExtension<ZeExts::Visit>(driver);
      if (extension) {
        driver_ = driver;
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

  void TearDown() override {
    if (context_) {
      zeContextDestroy(context_);
    }
  }

  CommandListUniquePtr CreateCommandList() {
    ze_command_list_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC;
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

  ZeDriverInit driver_init_{};
  ZeEventPoolManager event_pool_manager_{};
  ze_driver_handle_t driver_ = nullptr;
  ZeExts::Visit visit_extension_{};
  ze_device_handle_t device_ = nullptr;
  ze_context_handle_t context_ = nullptr;
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

TEST_F(ZeCommandVisitorTestSuite, VisitClosedCommandListWithASingleCommandThenResetAndTryAgain) {
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

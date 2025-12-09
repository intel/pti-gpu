//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <fmt/format.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <level_zero/ze_api.h>

#include <tuple>
#include <vector>

#include "utils.h"
#include "ze_event_managers.h"
#include "ze_utils.h"

namespace {
constexpr size_t kPtiDeviceId = 0;  // run on first device
}  // namespace

class ZeEventPoolTestSuite : public testing::Test {
 protected:
  ZeEventPoolTestSuite() = default;

  void SetUp() override {
    auto status = zeInit(ZE_INIT_FLAG_GPU_ONLY);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);
    drv_ = utils::ze::GetGpuDriver(kPtiDeviceId);
    dev_ = utils::ze::GetGpuDevice(kPtiDeviceId);
    ASSERT_NE(drv_, nullptr);
    ctx_ = utils::ze::GetContext(drv_);
    ASSERT_NE(ctx_, nullptr);
  }

  void TearDown() override {
    if (ctx_ != nullptr) {
      EXPECT_EQ(zeContextDestroy(ctx_), ZE_RESULT_SUCCESS);
    }
  }

  ze_driver_handle_t drv_ = nullptr;
  ze_device_handle_t dev_ = nullptr;
  ze_context_handle_t ctx_ = nullptr;
};

class ZeEventPoolManagerTestSuite
    : public ZeEventPoolTestSuite,
      public testing::WithParamInterface<std::tuple<uint32_t, uint32_t>> {
 protected:
  void SetUp() override {
    ZeEventPoolTestSuite::SetUp();
    auto [num_contexts, num_events] = GetParam();
    for (uint32_t i = 0; i < num_contexts; ++i) {
      ze_context_handle_t ctx = utils::ze::GetContext(drv_);
      ASSERT_NE(ctx, nullptr);
      ctx_list_.push_back(ctx);
    }
  }
  void TearDown() override {
    ZeEventPoolTestSuite::TearDown();
    for (auto* ctx : ctx_list_) {
      if (ctx) {
        EXPECT_EQ(zeContextDestroy(ctx), ZE_RESULT_SUCCESS);
      }
    }
  }

  std::vector<ze_context_handle_t> ctx_list_;
};

TEST_F(ZeEventPoolTestSuite, TestConstructionWithOneEvent) { ZeEventPool event_pool(ctx_, 1U); }

TEST_F(ZeEventPoolTestSuite, TestConstructionWithManyEvents) {
  constexpr uint32_t kManyEventCount = 1024;
  ZeEventPool event_pool(ctx_, kManyEventCount);
}

TEST_F(ZeEventPoolTestSuite,
       TestCheckingIfAOneEventEventPoolHasEventsLeftWithoutAcquiringTheEvent) {
  ZeEventPool event_pool(ctx_, 1U);
  EXPECT_FALSE(event_pool.IsExhausted());
}

TEST_F(ZeEventPoolTestSuite, TestCheckingIfAOneEventPoolHasEventsLeftWithAcquiringTheEvent) {
  ZeEventPool event_pool(ctx_, 1U);
  auto my_event = event_pool.AcquireEvent();
  EXPECT_TRUE(event_pool.IsExhausted());
}

TEST_F(ZeEventPoolTestSuite,
       TestCheckingIfAManyEventEventPoolHasEventsLeftWithoutAcquiringAnyEvents) {
  constexpr uint32_t kManyEventCount = 1024;
  ZeEventPool event_pool(ctx_, kManyEventCount);
  EXPECT_FALSE(event_pool.IsExhausted());
}

TEST_F(ZeEventPoolTestSuite,
       TestCheckingIfAManyEventEventPoolHasEventsLeftWithAfterAcquiringOneEventButNotAllOfThem) {
  constexpr uint32_t kManyEventCount = 1024;
  ZeEventPool event_pool(ctx_, kManyEventCount);
  auto my_event = event_pool.AcquireEvent();
  EXPECT_FALSE(event_pool.IsExhausted());
}

TEST_F(ZeEventPoolTestSuite,
       TestCheckingIfAManyEventEventPoolHasEventsLeftWithAfterAcquiringManyEventsButNotAllOfThem) {
  constexpr uint32_t kManyEventCount = 1024;
  ZeEventPool event_pool(ctx_, kManyEventCount);
  auto my_event1 = event_pool.AcquireEvent();
  auto my_event2 = event_pool.AcquireEvent();
  auto my_event3 = event_pool.AcquireEvent();
  auto my_event4 = event_pool.AcquireEvent();
  auto my_event5 = event_pool.AcquireEvent();
  EXPECT_FALSE(event_pool.IsExhausted());
}

TEST_F(ZeEventPoolTestSuite, TestCheckingIfAManyEventEventPoolHasEventsLeftAfterAcquireAllOfThem) {
  constexpr uint32_t kManyEventCount = 5;
  ZeEventPool event_pool(ctx_, kManyEventCount);
  auto my_event1 = event_pool.AcquireEvent();
  auto my_event2 = event_pool.AcquireEvent();
  auto my_event3 = event_pool.AcquireEvent();
  auto my_event4 = event_pool.AcquireEvent();
  auto my_event5 = event_pool.AcquireEvent();
  EXPECT_TRUE(event_pool.IsExhausted());
}

TEST_F(
    ZeEventPoolTestSuite,
    TestCheckingIfAManyEventEventPoolHasEventsLeftAfterAcquireAllOfThemHoweverLetThemGoOutOfScope) {
  constexpr uint32_t kManyEventCount = 5;
  ZeEventPool event_pool(ctx_, kManyEventCount);
  for (uint32_t i = 0; i < kManyEventCount; ++i) {
    auto my_event = event_pool.AcquireEvent();
  }
  EXPECT_FALSE(event_pool.IsExhausted());
}

TEST_F(
    ZeEventPoolTestSuite,
    TestCheckingIfAManyEventEventPoolHasEventsLeftAfterAcquireAllOfThemHoweverLetThemGoOutOfScopeMultipleTimes) {
  constexpr uint32_t kManyEventCount = 5;
  constexpr uint32_t kWrapArounds = 2;

  ZeEventPool event_pool(ctx_, kManyEventCount);
  for (uint32_t i = 0; i < kManyEventCount * kWrapArounds; ++i) {
    auto my_event = event_pool.AcquireEvent();
  }

  EXPECT_FALSE(event_pool.IsExhausted());
}

TEST_F(ZeEventPoolTestSuite,
       TestCheckingIfAnEventAcquiredFromTheEventPoolIsNotNullIfPoolHasManyEvents) {
  constexpr uint32_t kManyEventCount = 5;
  ZeEventPool event_pool(ctx_, kManyEventCount);
  auto my_event = event_pool.AcquireEvent();
  EXPECT_NE(my_event.Get(), nullptr);
}

TEST_F(ZeEventPoolTestSuite, TestCheckingIfAnEventAcquiredFromTheEventPoolIsNullIfPoolHasOneEvent) {
  ZeEventPool event_pool(ctx_, 1U);
  auto my_event = event_pool.AcquireEvent();
  EXPECT_NE(my_event.Get(), nullptr);
}

TEST_F(ZeEventPoolTestSuite,
       TestCheckingIfMultipleEventAcquiredFromTheEventPoolAreNullIfPoolHasNotEnoughEvents) {
  ZeEventPool event_pool(ctx_, 1U);
  auto my_event = event_pool.AcquireEvent();
  EXPECT_NE(my_event.Get(), nullptr);
  auto my_event1 = event_pool.AcquireEvent();
  EXPECT_EQ(my_event1.Get(), nullptr);
}

TEST_F(ZeEventPoolTestSuite, TestCheckingIfAnEventAcquiredFromAnUnusedEventPoolIsNotReady) {
  ZeEventPool event_pool(ctx_, 1U);
  auto my_event = event_pool.AcquireEvent();
  EXPECT_FALSE(my_event.Ready());
}

TEST_F(ZeEventPoolTestSuite,
       TestCheckingIfAnEventAcquiredFromAnUnusedEventPoolIsReadyAfterHostSignal) {
  ZeEventPool event_pool(ctx_, 1U);
  auto my_event = event_pool.AcquireEvent();
  ASSERT_EQ(zeEventHostSignal(my_event.Get()), ZE_RESULT_SUCCESS);
  EXPECT_TRUE(my_event.Ready());
}

TEST_F(ZeEventPoolTestSuite, TestCheckingIfEventsAcquiredFromAnUnusedEventPoolAreNotReady) {
  constexpr uint32_t kManyEventCount = 2;
  constexpr uint32_t kWrapArounds = 10;

  ZeEventPool event_pool(ctx_, kManyEventCount);
  for (uint32_t i = 0; i < kManyEventCount * kWrapArounds; ++i) {
    auto my_event = event_pool.AcquireEvent();
    EXPECT_FALSE(my_event.Ready());
  }
}

TEST_F(ZeEventPoolTestSuite, TestConstructingSingleEventUnmanagedByAnEventPool) {
  ZeEventPool event_pool(ctx_, 1U);
  ZeEventView<ZeEventPool> my_event(event_pool.AcquireEvent());
  EXPECT_FALSE(my_event.Empty());
}

TEST_P(ZeEventPoolManagerTestSuite, TestCheckingIfEventsAcquiredFromUnusedEventManagerAreNotReady) {
  auto [num_ctx, num_events] = GetParam();
  ZeEventPoolManager event_manager;
  for (auto* ctx : ctx_list_) {
    for (uint32_t i = 0; i < num_events; ++i) {
      auto my_event = event_manager.AcquireEvent(ctx);
      EXPECT_FALSE(my_event.Ready());
    }
    event_manager.Clear(ctx);
  }
}

INSTANTIATE_TEST_SUITE_P(
    ZeEventPoolManagerTests, ZeEventPoolManagerTestSuite,
    ::testing::Combine(::testing::Values(1, 4, 8, 256), ::testing::Values(1, 16, 32, 1024)),
    [](const testing::TestParamInfo<ZeEventPoolManagerTestSuite::ParamType>& info) {
      return fmt::format("Ctx_{}_Events_{}", std::get<0>(info.param), std::get<1>(info.param));
    });

// Tests for pool reuse functionality
TEST_F(ZeEventPoolTestSuite, TestIsCompletelyAvailableOnNewPool) {
  ZeEventPool event_pool(ctx_, 5U);
  EXPECT_TRUE(event_pool.IsCompletelyAvailable());
}

TEST_F(ZeEventPoolTestSuite, TestIsCompletelyAvailableAfterAcquiringOneEvent) {
  ZeEventPool event_pool(ctx_, 5U);
  auto my_event = event_pool.AcquireEvent();
  EXPECT_FALSE(event_pool.IsCompletelyAvailable());
}

TEST_F(ZeEventPoolTestSuite, TestAssignmentResourceManagement) {
  ZeEventPool event_pool(ctx_, 5U);
  {
    auto my_event = event_pool.AcquireEvent();
    auto my_event2 = event_pool.AcquireEvent();
    my_event2 = std::move(my_event);
  }
  EXPECT_TRUE(event_pool.IsCompletelyAvailable());
}

TEST_F(ZeEventPoolTestSuite, TestIsCompletelyAvailableAfterAcquiringAndReturningOneEvent) {
  ZeEventPool event_pool(ctx_, 5U);
  {
    auto my_event = event_pool.AcquireEvent();
    EXPECT_FALSE(event_pool.IsCompletelyAvailable());
  }  // Event goes out of scope, gets returned via destructor
  EXPECT_TRUE(event_pool.IsCompletelyAvailable());
}

TEST_F(ZeEventPoolTestSuite, TestIsCompletelyAvailableAfterAcquiringAllEvents) {
  constexpr uint32_t kPoolSize = 5;
  ZeEventPool event_pool(ctx_, kPoolSize);
  std::vector<ZeEventView<ZeEventPool>> events;
  for (uint32_t i = 0; i < kPoolSize; ++i) {
    events.push_back(event_pool.AcquireEvent());
  }
  EXPECT_FALSE(event_pool.IsCompletelyAvailable());
}

TEST_F(ZeEventPoolTestSuite, TestIsCompletelyAvailableAfterAcquiringAndReturningAllEvents) {
  constexpr uint32_t kPoolSize = 5;
  ZeEventPool event_pool(ctx_, kPoolSize);
  {
    std::vector<ZeEventView<ZeEventPool>> events;
    for (uint32_t i = 0; i < kPoolSize; ++i) {
      events.push_back(event_pool.AcquireEvent());
    }
    EXPECT_FALSE(event_pool.IsCompletelyAvailable());
  }  // All events go out of scope
  EXPECT_TRUE(event_pool.IsCompletelyAvailable());
}

TEST_F(ZeEventPoolTestSuite, TestIsCompletelyAvailableAfterPartialReturn) {
  constexpr uint32_t kPoolSize = 5;
  ZeEventPool event_pool(ctx_, kPoolSize);
  std::vector<ZeEventView<ZeEventPool>> events;
  for (uint32_t i = 0; i < kPoolSize; ++i) {
    events.push_back(event_pool.AcquireEvent());
  }

  // Return some but not all events
  events.pop_back();
  events.pop_back();
  EXPECT_FALSE(event_pool.IsCompletelyAvailable());
}

TEST_F(ZeEventPoolTestSuite, TestPoolReuseAfterAllEventsReturned) {
  constexpr uint32_t kPoolSize = 3;
  ZeEventPool event_pool(ctx_, kPoolSize);

  // First round: acquire all events
  {
    std::vector<ZeEventView<ZeEventPool>> events;
    for (uint32_t i = 0; i < kPoolSize; ++i) {
      events.push_back(event_pool.AcquireEvent());
    }
    EXPECT_TRUE(event_pool.IsExhausted());
  }  // All events returned

  EXPECT_TRUE(event_pool.IsCompletelyAvailable());
  EXPECT_FALSE(event_pool.IsExhausted());

  // Second round: should be able to acquire events again
  std::vector<ZeEventView<ZeEventPool>> new_events;
  for (uint32_t i = 0; i < kPoolSize; ++i) {
    new_events.push_back(event_pool.AcquireEvent());
    EXPECT_NE(new_events.back().Get(), nullptr);
  }
  EXPECT_TRUE(event_pool.IsExhausted());
  EXPECT_FALSE(event_pool.IsCompletelyAvailable());
}

TEST_F(ZeEventPoolTestSuite, TestPoolReuseMultipleCycles) {
  constexpr uint32_t kPoolSize = 3;
  constexpr uint32_t kCycles = 5;
  ZeEventPool event_pool(ctx_, kPoolSize);

  for (uint32_t cycle = 0; cycle < kCycles; ++cycle) {
    EXPECT_TRUE(event_pool.IsCompletelyAvailable());
    {
      std::vector<ZeEventView<ZeEventPool>> events;
      for (uint32_t i = 0; i < kPoolSize; ++i) {
        events.push_back(event_pool.AcquireEvent());
        EXPECT_NE(events.back().Get(), nullptr);
      }
      EXPECT_FALSE(event_pool.IsCompletelyAvailable());
    }
    EXPECT_TRUE(event_pool.IsCompletelyAvailable());
  }
}

TEST_F(ZeEventPoolTestSuite, TestManagerReusesPoolsAcrossCycles) {
  constexpr uint32_t kPoolSize = ZeEventPoolManager::kDefaultPoolSize;
  ZeEventPoolManager manager;

  // First cycle: acquire all events from first pool
  {
    std::vector<ZeEventView<ZeEventPool>> events;
    for (uint32_t i = 0; i < kPoolSize; ++i) {
      events.push_back(manager.AcquireEvent(ctx_));
      EXPECT_NE(events.back().Get(), nullptr);
    }
  }  // All events returned

  // Second cycle: should reuse the same pool instead of creating a new one
  // We can verify this indirectly by checking that events are acquired successfully
  std::vector<ZeEventView<ZeEventPool>> new_events;
  for (uint32_t i = 0; i < kPoolSize; ++i) {
    new_events.push_back(manager.AcquireEvent(ctx_));
    EXPECT_NE(new_events.back().Get(), nullptr);
  }
}

TEST_F(ZeEventPoolTestSuite, TestManagerCreatesSecondPoolWhenFirstIsExhausted) {
  constexpr uint32_t kPoolSize = ZeEventPoolManager::kDefaultPoolSize;
  ZeEventPoolManager manager;

  // Acquire more than one pool's worth of events
  std::vector<ZeEventView<ZeEventPool>> events;
  for (uint32_t i = 0; i < kPoolSize + 10; ++i) {
    events.push_back(manager.AcquireEvent(ctx_));
    EXPECT_NE(events.back().Get(), nullptr);
  }
}

TEST_F(ZeEventPoolTestSuite, TestManagerReusesPoolAfterPartialUsage) {
  constexpr uint32_t kAcquireCount = 10;
  ZeEventPoolManager manager;

  // Acquire some events and let them return
  {
    std::vector<ZeEventView<ZeEventPool>> events;
    for (uint32_t i = 0; i < kAcquireCount; ++i) {
      events.push_back(manager.AcquireEvent(ctx_));
    }
  }  // Events returned

  // Acquire more events - should reuse the same pool
  std::vector<ZeEventView<ZeEventPool>> new_events;
  for (uint32_t i = 0; i < kAcquireCount; ++i) {
    new_events.push_back(manager.AcquireEvent(ctx_));
    EXPECT_NE(new_events.back().Get(), nullptr);
  }
}

TEST_F(ZeEventPoolTestSuite, TestManagerReusesPoolAfterMultiplePoolsCreated) {
  constexpr uint32_t kAcquireCount = (ZeEventPoolManager::kDefaultPoolSize * 2) + 10;
  ZeEventPoolManager manager;

  std::vector<ZeEventView<ZeEventPool>> events;
  events.reserve(kAcquireCount);
  for (uint32_t i = 0; i < kAcquireCount; ++i) {
    events.push_back(manager.AcquireEvent(ctx_));
  }

  for (uint32_t i = 0; i < ZeEventPoolManager::kDefaultPoolSize; ++i) {
    events.at(i) = ZeEventView<ZeEventPool>(nullptr);  // Release a pool
  }

  // Acquire more events - should reuse the first pool
  std::vector<ZeEventView<ZeEventPool>> new_events;
  for (uint32_t i = 0; i < kAcquireCount; ++i) {
    new_events.push_back(manager.AcquireEvent(ctx_));
    EXPECT_NE(new_events.back().Get(), nullptr);
  }
}

TEST_F(ZeEventPoolTestSuite, TestEventsReturnedOutOfOrderReverse) {
  constexpr uint32_t kPoolSize = 3;
  ZeEventPool event_pool(ctx_, kPoolSize);

  auto event0 = event_pool.AcquireEvent();
  auto event1 = event_pool.AcquireEvent();
  auto event2 = event_pool.AcquireEvent();

  EXPECT_NE(event0.Get(), nullptr);
  EXPECT_NE(event1.Get(), nullptr);
  EXPECT_NE(event2.Get(), nullptr);

  event2 = ZeEventView<ZeEventPool>(nullptr);
  event1 = ZeEventView<ZeEventPool>(nullptr);
  event0 = ZeEventView<ZeEventPool>(nullptr);

  EXPECT_TRUE(event_pool.IsCompletelyAvailable());
  EXPECT_FALSE(event_pool.IsExhausted());
}

TEST_F(ZeEventPoolTestSuite, TestEventsReturnedOutOfOrderMiddleFirst) {
  constexpr uint32_t kPoolSize = 5;
  ZeEventPool event_pool(ctx_, kPoolSize);

  // Acquire all events
  auto event0 = event_pool.AcquireEvent();
  auto event1 = event_pool.AcquireEvent();
  auto middle_event2 = event_pool.AcquireEvent();
  auto event3 = event_pool.AcquireEvent();
  auto event4 = event_pool.AcquireEvent();

  EXPECT_TRUE(event_pool.IsExhausted());

  middle_event2 = ZeEventView<ZeEventPool>(nullptr);
  EXPECT_FALSE(event_pool.IsCompletelyAvailable());

  event4 = ZeEventView<ZeEventPool>(nullptr);
  event0 = ZeEventView<ZeEventPool>(nullptr);
  event3 = ZeEventView<ZeEventPool>(nullptr);
  event1 = ZeEventView<ZeEventPool>(nullptr);

  EXPECT_TRUE(event_pool.IsCompletelyAvailable());
}

TEST_F(ZeEventPoolTestSuite, TestEventsReturnedOutOfOrderThenReacquiredWithSameIndices) {
  constexpr uint32_t kPoolSize = 4;
  ZeEventPool event_pool(ctx_, kPoolSize);

  auto event0 = event_pool.AcquireEvent();
  auto event1 = event_pool.AcquireEvent();
  auto event2 = event_pool.AcquireEvent();
  auto event3 = event_pool.AcquireEvent();

  auto idx0 = event0.Idx();
  auto idx1 = event1.Idx();
  auto idx2 = event2.Idx();
  auto idx3 = event3.Idx();

  EXPECT_TRUE(event_pool.IsExhausted());

  event3 = ZeEventView<ZeEventPool>(nullptr);
  event1 = ZeEventView<ZeEventPool>(nullptr);
  event0 = ZeEventView<ZeEventPool>(nullptr);
  event2 = ZeEventView<ZeEventPool>(nullptr);

  EXPECT_TRUE(event_pool.IsCompletelyAvailable());

  auto new_event0 = event_pool.AcquireEvent();
  auto new_event1 = event_pool.AcquireEvent();
  auto new_event2 = event_pool.AcquireEvent();
  auto new_event3 = event_pool.AcquireEvent();

  // Events should be acquired in the same order.
  EXPECT_EQ(new_event0.Idx(), idx0);
  EXPECT_EQ(new_event1.Idx(), idx1);
  EXPECT_EQ(new_event2.Idx(), idx2);
  EXPECT_EQ(new_event3.Idx(), idx3);

  EXPECT_TRUE(event_pool.IsExhausted());
}

TEST_F(ZeEventPoolTestSuite, TestPartialOutOfOrderReturnAllowsPartialReacquire) {
  constexpr uint32_t kPoolSize = 5;
  ZeEventPool event_pool(ctx_, kPoolSize);

  auto event0 = event_pool.AcquireEvent();
  auto middle_event1 = event_pool.AcquireEvent();
  auto middle_event2 = event_pool.AcquireEvent();

  middle_event1 = ZeEventView<ZeEventPool>(nullptr);
  middle_event2 = ZeEventView<ZeEventPool>(nullptr);

  EXPECT_FALSE(event_pool.IsExhausted());

  auto event3 = event_pool.AcquireEvent();
  auto event4 = event_pool.AcquireEvent();

  // 1 and 2 are available. However, next 'free' event is calculated on the next acquired event.
  EXPECT_TRUE(event_pool.IsExhausted());

  event0 = ZeEventView<ZeEventPool>(nullptr);
  event3 = ZeEventView<ZeEventPool>(nullptr);
  event4 = ZeEventView<ZeEventPool>(nullptr);

  EXPECT_TRUE(event_pool.IsCompletelyAvailable());
}

TEST_F(ZeEventPoolTestSuite, TestOutOfOrderReturnWithWrapAround) {
  constexpr uint32_t kPoolSize = 3;
  ZeEventPool event_pool(ctx_, kPoolSize);

  std::vector<ZeEventView<ZeEventPool>> first_round;
  for (uint32_t i = 0; i < kPoolSize; ++i) {
    first_round.push_back(event_pool.AcquireEvent());
  }
  EXPECT_TRUE(event_pool.IsExhausted());

  for (uint32_t i = kPoolSize; i > 0; --i) {
    first_round.at(i - 1) = ZeEventView<ZeEventPool>(nullptr);
  }
  EXPECT_TRUE(event_pool.IsCompletelyAvailable());

  std::vector<ZeEventView<ZeEventPool>> second_round;
  for (uint32_t i = 0; i < kPoolSize; ++i) {
    second_round.push_back(event_pool.AcquireEvent());
    EXPECT_NE(second_round.back().Get(), nullptr);
  }
  EXPECT_TRUE(event_pool.IsExhausted());

  second_round.at(1) = ZeEventView<ZeEventPool>(nullptr);
  second_round.at(0) = ZeEventView<ZeEventPool>(nullptr);
  second_round.at(2) = ZeEventView<ZeEventPool>(nullptr);

  EXPECT_TRUE(event_pool.IsCompletelyAvailable());

  for (uint32_t i = 0; i < kPoolSize; ++i) {
    auto event = event_pool.AcquireEvent();
    EXPECT_NE(event.Get(), nullptr);
  }
}

TEST_F(ZeEventPoolTestSuite, TestManagerHandlesOutOfOrderReturnsAcrossMultiplePools) {
  constexpr uint32_t kPoolSize = ZeEventPoolManager::kDefaultPoolSize;
  constexpr uint32_t kOverflowSize = kPoolSize + 10;
  ZeEventPoolManager manager;

  std::vector<ZeEventView<ZeEventPool>> events;
  for (uint32_t i = 0; i < kOverflowSize; ++i) {
    events.push_back(manager.AcquireEvent(ctx_));
    EXPECT_NE(events.back().Get(), nullptr);
  }

  // Return events out of order:
  // (1) Return events from latter pool(s). Based on overflow.
  // (2) Return events from first pool.
  for (uint32_t i = kPoolSize; i < kOverflowSize; ++i) {
    events.at(i) = ZeEventView<ZeEventPool>(nullptr);
  }

  for (uint32_t i = kPoolSize; i > 0; --i) {
    events.at(i - 1) = ZeEventView<ZeEventPool>(nullptr);
  }

  std::vector<ZeEventView<ZeEventPool>> new_events;
  for (uint32_t i = 0; i < kOverflowSize; ++i) {
    new_events.push_back(manager.AcquireEvent(ctx_));
    EXPECT_NE(new_events.back().Get(), nullptr);
  }
}

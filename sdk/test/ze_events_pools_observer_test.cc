#include <fmt/format.h>
#include <gtest/gtest.h>
#include <level_zero/ze_api.h>

#include <atomic>
#include <shared_mutex>
#include <thread>
#include <vector>

#include "levelzero/ze_events_and_pools_observer.h"
#include "utils/ze_config_info.h"
#include "ze_utils.h"

namespace {
constexpr size_t kPtiDeviceId = 0;  // run on first device
constexpr int kNumThreads = 8;
constexpr int kPoolsPerThread = 10;
constexpr uint32_t kEventsPerPool = 5;
}  // namespace

class ZeEventPoolsObserverTest : public testing::Test {
 protected:
  void SetUp() override {
    // Initialize Level Zero
    auto status = zeInit(ZE_INIT_FLAG_GPU_ONLY);
    ASSERT_EQ(status, ZE_RESULT_SUCCESS);

    drv_ = utils::ze::GetGpuDriver(kPtiDeviceId);
    ASSERT_NE(drv_, nullptr);

    dev_ = utils::ze::GetGpuDevice(kPtiDeviceId);
    ASSERT_NE(dev_, nullptr);

    ctx1_ = utils::ze::GetContext(drv_);
    ASSERT_NE(ctx1_, nullptr);

    ctx2_ = utils::ze::GetContext(drv_);
    ASSERT_NE(ctx2_, nullptr);
  }

  void TearDown() override {
    // Destroy all events first (must destroy before their pools)
    for (auto event : events_) {
      if (event) {
        zeEventDestroy(event);
      }
    }
    events_.clear();

    // Destroy all event pools
    for (auto pool : event_pools_) {
      if (pool) {
        zeEventPoolDestroy(pool);
      }
    }
    event_pools_.clear();

    if (ctx1_) {
      zeContextDestroy(ctx1_);
    }
    if (ctx2_) {
      zeContextDestroy(ctx2_);
    }
  }

  ze_event_pool_handle_t CreateEventPool(ze_context_handle_t context, ze_event_pool_flags_t flags,
                                         uint32_t count = 5,
                                         EventPoolType type = EventPoolType::kRegular) {
    ze_event_pool_desc_t pool_desc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr, flags, count};

    // If counter-based, add the experimental descriptor
    ze_event_pool_counter_based_exp_desc_t counter_desc = {};
    if (type == EventPoolType::kCounterBased) {
      counter_desc.stype = ZE_STRUCTURE_TYPE_COUNTER_BASED_EVENT_POOL_EXP_DESC;
      counter_desc.pNext = nullptr;
      counter_desc.flags = ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE;
      pool_desc.pNext = &counter_desc;
    }

    ze_event_pool_handle_t pool = nullptr;
    auto status = zeEventPoolCreate(context, &pool_desc, 1, &dev_, &pool);
    EXPECT_EQ(status, ZE_RESULT_SUCCESS);

    if (pool) {
      event_pools_.push_back(pool);
    }
    return pool;
  }

  ze_event_handle_t CreateEvent(ze_event_pool_handle_t pool, uint32_t index = 0) {
    ze_event_desc_t event_desc = {ZE_STRUCTURE_TYPE_EVENT_DESC, nullptr, index,
                                  ZE_EVENT_SCOPE_FLAG_HOST, ZE_EVENT_SCOPE_FLAG_HOST};

    ze_event_handle_t event = nullptr;
    auto status = zeEventCreate(pool, &event_desc, &event);
    EXPECT_EQ(status, ZE_RESULT_SUCCESS);

    if (event) {
      events_.push_back(event);
    }
    return event;
  }

  ze_driver_handle_t drv_ = nullptr;
  ze_device_handle_t dev_ = nullptr;
  ze_context_handle_t ctx1_ = nullptr;
  ze_context_handle_t ctx2_ = nullptr;
  std::vector<ze_event_pool_handle_t> event_pools_;
  std::vector<ze_event_handle_t> events_;
};

// ============================================================================
// Tests for basic pool operations
// ============================================================================
TEST_F(ZeEventPoolsObserverTest, PoolsBasicTest) {
  Level0Wrapper l0_wrapper;
  ZeEventPoolsObserver observer(l0_wrapper);

  //
  // Add regular and mocked counter-based pools
  // add them to Observer and check the returned properties
  //
  auto pool1 = CreateEventPool(ctx1_, ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
  ASSERT_NE(pool1, nullptr);

  auto added1 =
      observer.Add(pool1, ctx1_, ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP, EventPoolType::kRegular);
  EXPECT_TRUE(added1) << "First add should return true";

  ze_event_pool_flags_t flags =
      ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP | ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
  auto pool2 = CreateEventPool(ctx1_, flags, kEventsPerPool, EventPoolType::kCounterBased);
  ASSERT_NE(pool2, nullptr);

  auto added2 = observer.Add(pool2, ctx1_, flags, EventPoolType::kCounterBased);
  EXPECT_TRUE(added2) << "First add should return true";

  // Query and verify properties
  auto props = observer.GetPoolProperties(pool1);
  ASSERT_TRUE(props.has_value()) << "Should find existing pool";
  EXPECT_EQ(props->type, EventPoolType::kRegular) << "Pool should be regular (not counter-based)";
  EXPECT_TRUE(props->flags & ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP)
      << "Pool should have timestamp flag";
  EXPECT_FALSE(props->flags & ZE_EVENT_POOL_FLAG_HOST_VISIBLE)
      << "Pool should not have host visible flag";

  auto props2 = observer.GetPoolProperties(pool2);
  ASSERT_TRUE(props2.has_value());
  EXPECT_EQ(props2->type, EventPoolType::kCounterBased) << "Pool should be counter-based";
  EXPECT_TRUE(props2->flags & ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
  EXPECT_TRUE(props2->flags & ZE_EVENT_POOL_FLAG_HOST_VISIBLE);

  // Create event from counter-based pool
  auto counter_event = CreateEvent(pool2);
  ASSERT_NE(counter_event, nullptr);

  // Verify event inherits counter-based property
  auto event_props = observer.GetEventProperties(counter_event);
  ASSERT_TRUE(event_props.has_value());
  EXPECT_EQ(event_props->type, EventPoolType::kCounterBased)
      << "Event from counter-based pool should be counter-based";

  //
  // Add Existing Pool
  //
  auto added2_again =
      observer.Add(pool2, ctx1_, ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP, EventPoolType::kRegular);
  EXPECT_FALSE(added2_again) << "Second add should return false";

  // Verify that properties stay as when added first time
  auto props3 = observer.GetPoolProperties(pool2);
  EXPECT_TRUE(props3.has_value());
  EXPECT_EQ(props3->type, props2->type);
  EXPECT_EQ(props3->flags, props2->flags);

  //
  // Query Non-Existent Pool
  //
  // Create a pool but don't add it to observer
  auto untracked_pool = CreateEventPool(ctx1_, ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
  ASSERT_NE(untracked_pool, nullptr);

  //  Query event from untracked pool
  auto untracked_event = CreateEvent(untracked_pool);
  ASSERT_NE(untracked_event, nullptr);
  auto untracked_event_props = observer.GetEventProperties(untracked_event);
  EXPECT_FALSE(untracked_event_props.has_value()) << "Event's pool not in cache";

  //
  // ClearPool
  //
  // Add some pools
  auto pool_cleanup1 = CreateEventPool(ctx1_, 0);
  auto pool_cleanup2 = CreateEventPool(ctx1_, ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
  observer.Add(pool_cleanup1, ctx1_, 0, EventPoolType::kRegular);
  observer.Add(pool_cleanup2, ctx1_, ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP, EventPoolType::kRegular);

  // Verify they exist and information stored
  auto cleanup_props1 = observer.GetPoolProperties(pool_cleanup1);
  auto cleanup_props2 = observer.GetPoolProperties(pool_cleanup2);
  EXPECT_TRUE(cleanup_props1.has_value());
  EXPECT_TRUE(cleanup_props2.has_value());
  EXPECT_FALSE(cleanup_props1->flags & ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
  EXPECT_TRUE(cleanup_props2->flags & ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
  EXPECT_TRUE(cleanup_props1->type == EventPoolType::kRegular);
  EXPECT_TRUE(cleanup_props2->type == EventPoolType::kRegular);

  // Clean Pool in Observer
  observer.ClearPool(pool_cleanup2);

  // pool_cleanup2 is gone from observer, it does not know its types
  auto after_cleanup1 = observer.GetPoolProperties(pool_cleanup1);
  auto after_cleanup2 = observer.GetPoolProperties(pool_cleanup2);
  EXPECT_TRUE(after_cleanup1.has_value());
  EXPECT_FALSE(after_cleanup2.has_value());
  EXPECT_FALSE(after_cleanup1->flags & ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
  EXPECT_TRUE(after_cleanup1->type == EventPoolType::kRegular);
}

// ============================================================================
// Event-based queries and CleanContext
// ============================================================================
TEST_F(ZeEventPoolsObserverTest, EventsTest) {
  Level0Wrapper l0_wrapper;
  ZeEventPoolsObserver observer(l0_wrapper);

  // Regular and counter-based event pool and events
  auto regular_pool =
      CreateEventPool(ctx1_, ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP | ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
                      5, EventPoolType::kRegular);
  ASSERT_NE(regular_pool, nullptr);

  observer.Add(regular_pool, ctx1_,
               ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP | ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
               EventPoolType::kRegular);

  auto counter_pool = CreateEventPool(ctx2_, ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP, 5,
                                      EventPoolType::kCounterBased);  // true = counter-based
  ASSERT_NE(counter_pool, nullptr);

  observer.Add(counter_pool, ctx2_, ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP,
               EventPoolType::kCounterBased);

  auto regular_event = CreateEvent(regular_pool);
  ASSERT_NE(regular_event, nullptr);
  auto counter_event = CreateEvent(counter_pool);
  ASSERT_NE(counter_event, nullptr);

  // Query event properties
  auto regular_event_props = observer.GetEventProperties(regular_event);
  ASSERT_TRUE(regular_event_props.has_value());
  EXPECT_EQ(regular_event_props->type, EventPoolType::kRegular)
      << "Regular event should NOT be counter-based";
  EXPECT_TRUE(regular_event_props->flags & ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP)
      << "Regular event should inherit pool's timestamp flag";
  EXPECT_TRUE(regular_event_props->flags & ZE_EVENT_POOL_FLAG_HOST_VISIBLE)
      << "Regular event should inherit pool's host-visible flag";

  observer.ClearContext(ctx1_);
  // After clearing context, regular event's pool should be gone,
  // but counter-based still available

  auto counter_event_props = observer.GetEventProperties(counter_event);
  ASSERT_TRUE(counter_event_props.has_value());
  EXPECT_EQ(counter_event_props->type, EventPoolType::kCounterBased)
      << "Event from counter-based pool should be counter-based";
  EXPECT_TRUE(counter_event_props->flags & ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP)
      << "Counter event should inherit pool's timestamp flag";
  EXPECT_FALSE(counter_event_props->flags & ZE_EVENT_POOL_FLAG_HOST_VISIBLE)
      << "Counter event pool didn't have host-visible flag";

  regular_event_props = observer.GetEventProperties(regular_event);
  ASSERT_FALSE(regular_event_props.has_value());
}

// ============================================================================
// Parameterized test for multiple flag combinations
// ============================================================================
struct FlagTestCase {
  ze_event_pool_flags_t flags;
  EventPoolType type;
  bool expect_timestamp;
  bool expect_host_visible;
};

class FlagCombinationPoolsObserverTest : public ZeEventPoolsObserverTest,
                                         public testing::WithParamInterface<FlagTestCase> {};

TEST_P(FlagCombinationPoolsObserverTest, FlagCombinations) {
  Level0Wrapper l0_wrapper;
  ZeEventPoolsObserver observer(l0_wrapper);

  const auto& tc = GetParam();

  auto pool = CreateEventPool(ctx1_, tc.flags, kEventsPerPool, tc.type);
  ASSERT_NE(pool, nullptr);

  observer.Add(pool, ctx1_, tc.flags, tc.type);

  auto tc_props = observer.GetPoolProperties(pool);
  ASSERT_TRUE(tc_props.has_value());
  EventPoolType expected_type = tc.type;
  EXPECT_EQ(tc_props->type, expected_type);
  EXPECT_EQ(static_cast<bool>(tc_props->flags & ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP),
            tc.expect_timestamp);
  EXPECT_EQ(static_cast<bool>(tc_props->flags & ZE_EVENT_POOL_FLAG_HOST_VISIBLE),
            tc.expect_host_visible);

  // Test event from this pool
  auto event = CreateEvent(pool);
  ASSERT_NE(event, nullptr);

  auto tc_event_props = observer.GetEventProperties(event);
  ASSERT_TRUE(tc_event_props.has_value());
  EXPECT_EQ(tc_event_props->type, expected_type)
      << "Event should inherit pool's counter-based property";
  EXPECT_EQ(static_cast<bool>(tc_event_props->flags & ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP),
            tc.expect_timestamp)
      << "Event should inherit pool's timestamp flag";
  EXPECT_EQ(static_cast<bool>(tc_event_props->flags & ZE_EVENT_POOL_FLAG_HOST_VISIBLE),
            tc.expect_host_visible)
      << "Event should inherit pool's host-visible flag";
}

INSTANTIATE_TEST_SUITE_P(
    FlagCombinations, FlagCombinationPoolsObserverTest,
    ::testing::Values(
        // Regular pools
        FlagTestCase{0, EventPoolType::kRegular, false, false},
        FlagTestCase{ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP, EventPoolType::kRegular, true, false},
        FlagTestCase{ZE_EVENT_POOL_FLAG_HOST_VISIBLE, EventPoolType::kRegular, false, true},
        FlagTestCase{ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP | ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
                     EventPoolType::kRegular, true, true},
        // Counter-based pools
        FlagTestCase{ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP, EventPoolType::kCounterBased, true,
                     false},
        FlagTestCase{ZE_EVENT_POOL_FLAG_HOST_VISIBLE, EventPoolType::kCounterBased, false, true},
        FlagTestCase{ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP | ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
                     EventPoolType::kCounterBased, true, true}),

    [](const testing::TestParamInfo<FlagCombinationPoolsObserverTest::ParamType>& info) {
      std::string name = "Flags";
      if (info.param.flags & ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP) {
        name += "_Timestamp";
      }
      if (info.param.flags & ZE_EVENT_POOL_FLAG_HOST_VISIBLE) {
        name += "_HostVisible";
      }
      if (info.param.type == EventPoolType::kCounterBased) {
        name += "_CounterBased";
      } else {
        name += "_Regular";
      }
      return name;
    });

// ============================================================================
// Tests for thread safety
// ============================================================================
TEST_F(ZeEventPoolsObserverTest, PoolMultithreadedTest) {
  Level0Wrapper l0_wrapper;
  ZeEventPoolsObserver observer(l0_wrapper);

  std::atomic<int> ready{0};
  std::vector<std::thread> threads;
  std::vector<std::vector<ze_event_pool_handle_t>> event_pools_per_thread(kNumThreads);

  for (int t = 0; t < kNumThreads; ++t) {
    threads.emplace_back([&, t]() {
      ready++;
      while (ready.load() < kNumThreads) {
      }  // Synchronize start

      for (int i = 0; i < kPoolsPerThread; ++i) {
        ze_event_pool_flags_t flags = (i % 2 == 0) ? ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP : 0;
        bool is_counter = (i % 3 == 0);

        ze_event_pool_desc_t pool_desc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC, nullptr, flags, 10};

        ze_event_pool_handle_t pool = nullptr;
        auto status = zeEventPoolCreate(ctx1_, &pool_desc, 1, &dev_, &pool);
        if (status == ZE_RESULT_SUCCESS && pool) {
          event_pools_per_thread[t].push_back(pool);
          observer.Add(pool, ctx1_, flags,
                       (is_counter ? EventPoolType::kCounterBased : EventPoolType::kRegular));
        }
      }
    });
  }

  for (auto& th : threads) {
    if (th.joinable()) {
      th.join();
    }
  }

  // Verify all pools were added correctly
  for (int t = 0; t < kNumThreads; ++t) {
    for (size_t i = 0; i < event_pools_per_thread[t].size(); ++i) {
      auto pool = event_pools_per_thread[t][i];
      auto pool_props = observer.GetPoolProperties(pool);
      ASSERT_TRUE(pool_props.has_value()) << "Pool should exist after concurrent adds";
      EventPoolType expected_pool_type =
          (i % 3 == 0) ? EventPoolType::kCounterBased : EventPoolType::kRegular;
      EXPECT_EQ(pool_props->type, expected_pool_type);
      ze_event_pool_flags_t expected_pool_flags =
          (i % 2 == 0) ? ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP : 0;
      EXPECT_EQ(pool_props->flags, expected_pool_flags);
    }

    // Clean up pools
    for (auto pool : event_pools_per_thread[t]) {
      zeEventPoolDestroy(pool);
    }
  }

  observer.ClearAll();
  ready.store(0);
  threads.clear();

  // Concurrent Reads and Writes
  constexpr int kNumReaderThreads = 4;
  constexpr int kNumWriterThreads = 4;
  constexpr int kOperationsPerThread = 500;
  constexpr int kNumPools = 20;

  std::vector<ze_event_pool_handle_t> shared_pools;
  for (int i = 0; i < kNumPools; ++i) {
    ze_context_handle_t context = (i % 2 == 0) ? ctx1_ : ctx2_;
    auto pool = CreateEventPool(context, ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
    if (pool) {
      shared_pools.push_back(pool);
      observer.Add(pool, context, ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP, EventPoolType::kRegular);
    }
  }

  ASSERT_EQ(shared_pools.size(), kNumPools) << "Failed to create all pools for concurrent test";

  // Writer threads
  for (int t = 0; t < kNumWriterThreads; ++t) {
    threads.emplace_back([&]() {
      ready++;
      while (ready.load() < (kNumReaderThreads + kNumWriterThreads)) {
      }

      for (int i = 0; i < kOperationsPerThread; ++i) {
        auto pool = shared_pools[i % shared_pools.size()];
        if (i % 2) {
          auto res = observer.Add(pool, ctx1_, ZE_EVENT_POOL_FLAG_HOST_VISIBLE,
                                  EventPoolType::kCounterBased);
          EXPECT_FALSE(res);
        } else {
          auto res = observer.Add(pool, ctx1_, ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP,
                                  EventPoolType::kRegular);
          EXPECT_FALSE(res);
        }
      }
    });
  }

  // Reader threads
  for (int t = 0; t < kNumReaderThreads; ++t) {
    threads.emplace_back([&]() {
      ready++;
      while (ready.load() < (kNumReaderThreads + kNumWriterThreads)) {
      }

      for (int i = 0; i < kOperationsPerThread; ++i) {
        auto pool = shared_pools[i % shared_pools.size()];

        // Query properties - should not crash even under contention
        auto read_props = observer.GetPoolProperties(pool);

        // Should always find it since it was pre-populated
        EXPECT_TRUE(read_props.has_value());

        // Verify invariants based on what writers set
        if (read_props.has_value()) {
          EXPECT_EQ(read_props->type, EventPoolType::kRegular)
              << "Pool type should stay to regular";
        }
      }
    });
  }

  for (auto& th : threads) {
    if (th.joinable()) {
      th.join();
    }
  }

}  // namespace

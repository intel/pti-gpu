//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "ze_kernel_name_cache.h"

#include <gtest/gtest.h>
#include <level_zero/ze_api.h>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

namespace {

// Mock kernel name fetcher for testing
// Tracks call count to verify cache hits vs misses
struct CountingFetcher {
  mutable std::atomic<int> call_count{0};

  std::string operator()(ze_kernel_handle_t kernel, bool demangle) const {
    ++call_count;
    // Generate predictable names based on kernel handle and demangle flag
    std::string base_name = "TestKernel_" + std::to_string(reinterpret_cast<uintptr_t>(kernel));
    return demangle ? ("demangled_" + base_name) : base_name;
  }
};

}  // namespace

class ZeKernelNameCacheTest : public ::testing::Test {
 protected:
  // Helper to create fake kernel handles for testing
  static ze_kernel_handle_t MakeKernel(uintptr_t id) {
    return reinterpret_cast<ze_kernel_handle_t>(id);
  }
};

// =============================================================================
// Basic Functionality Tests
// =============================================================================

TEST_F(ZeKernelNameCacheTest, CacheHitForSameKernel) {
  CountingFetcher fetcher;
  ZeKernelNameCache<CountingFetcher&> cache(fetcher);

  auto kernel = MakeKernel(0x100);

  // First call - cache miss
  std::string name1 = cache.GetKernelName(kernel, false);
  EXPECT_EQ(fetcher.call_count, 1);

  // Second call - cache hit (no new fetch)
  std::string name2 = cache.GetKernelName(kernel, false);
  EXPECT_EQ(fetcher.call_count, 1);  // Still 1!

  EXPECT_EQ(name1, name2);
  EXPECT_EQ(name1, "TestKernel_256");  // 0x100 = 256
}

TEST_F(ZeKernelNameCacheTest, CacheMissForDifferentKernels) {
  CountingFetcher fetcher;
  ZeKernelNameCache<CountingFetcher&> cache(fetcher);

  auto kernel1 = MakeKernel(0x100);
  auto kernel2 = MakeKernel(0x200);
  auto kernel3 = MakeKernel(0x300);

  // Each kernel causes a cache miss
  cache.GetKernelName(kernel1, false);
  EXPECT_EQ(fetcher.call_count, 1);

  cache.GetKernelName(kernel2, false);
  EXPECT_EQ(fetcher.call_count, 2);

  cache.GetKernelName(kernel3, false);
  EXPECT_EQ(fetcher.call_count, 3);

  // Accessing them again should hit the cache
  cache.GetKernelName(kernel1, false);
  cache.GetKernelName(kernel2, false);
  cache.GetKernelName(kernel3, false);
  EXPECT_EQ(fetcher.call_count, 3);  // Still 3!
}

TEST_F(ZeKernelNameCacheTest, MangledAndDemangledCachedSeparately) {
  CountingFetcher fetcher;
  ZeKernelNameCache<CountingFetcher&> cache(fetcher);

  auto kernel = MakeKernel(0x100);

  // Fetch mangled version
  std::string mangled = cache.GetKernelName(kernel, false);
  EXPECT_EQ(fetcher.call_count, 1);
  EXPECT_EQ(mangled, "TestKernel_256");

  // Fetch demangled version - separate cache entry, causes new fetch
  std::string demangled = cache.GetKernelName(kernel, true);
  EXPECT_EQ(fetcher.call_count, 2);
  EXPECT_EQ(demangled, "demangled_TestKernel_256");

  // Fetch mangled again - cached
  std::string mangled2 = cache.GetKernelName(kernel, false);
  EXPECT_EQ(fetcher.call_count, 2);  // Still 2
  EXPECT_EQ(mangled2, mangled);

  // Fetch demangled again - cached
  std::string demangled2 = cache.GetKernelName(kernel, true);
  EXPECT_EQ(fetcher.call_count, 2);  // Still 2
  EXPECT_EQ(demangled2, demangled);
}

TEST_F(ZeKernelNameCacheTest, RepeatedAccessesSameResult) {
  CountingFetcher fetcher;
  ZeKernelNameCache<CountingFetcher&> cache(fetcher);

  auto kernel = MakeKernel(0x100);

  // Call multiple times
  std::string name1 = cache.GetKernelName(kernel, false);
  std::string name2 = cache.GetKernelName(kernel, false);
  std::string name3 = cache.GetKernelName(kernel, false);
  std::string name4 = cache.GetKernelName(kernel, false);
  std::string name5 = cache.GetKernelName(kernel, false);

  // All should be identical
  EXPECT_EQ(name1, name2);
  EXPECT_EQ(name2, name3);
  EXPECT_EQ(name3, name4);
  EXPECT_EQ(name4, name5);

  // Only one fetch should have occurred
  EXPECT_EQ(fetcher.call_count, 1);
}

// =============================================================================
// Lifecycle Management Tests
// =============================================================================

TEST_F(ZeKernelNameCacheTest, RemoveKernelInvalidatesEntry) {
  CountingFetcher fetcher;
  ZeKernelNameCache<CountingFetcher&> cache(fetcher);

  auto kernel = MakeKernel(0x100);

  // Populate cache
  cache.GetKernelName(kernel, false);
  EXPECT_EQ(fetcher.call_count, 1);

  // Remove from cache
  cache.RemoveKernel(kernel);

  // Next access causes re-fetch
  cache.GetKernelName(kernel, false);
  EXPECT_EQ(fetcher.call_count, 2);
}

TEST_F(ZeKernelNameCacheTest, RemoveNonExistentKernelDoesNotCrash) {
  CountingFetcher fetcher;
  ZeKernelNameCache<CountingFetcher&> cache(fetcher);

  auto kernel = MakeKernel(0x100);

  // Remove kernel that was never added - should not crash
  EXPECT_NO_THROW(cache.RemoveKernel(kernel));
}

TEST_F(ZeKernelNameCacheTest, ClearRemovesAllEntries) {
  CountingFetcher fetcher;
  ZeKernelNameCache<CountingFetcher&> cache(fetcher);

  auto kernel1 = MakeKernel(0x100);
  auto kernel2 = MakeKernel(0x200);
  auto kernel3 = MakeKernel(0x300);

  // Populate cache
  cache.GetKernelName(kernel1, false);
  cache.GetKernelName(kernel2, false);
  cache.GetKernelName(kernel3, false);
  EXPECT_EQ(fetcher.call_count, 3);

  // Clear cache
  cache.Clear();

  // All accesses cause re-fetch
  cache.GetKernelName(kernel1, false);
  EXPECT_EQ(fetcher.call_count, 4);

  cache.GetKernelName(kernel2, false);
  EXPECT_EQ(fetcher.call_count, 5);

  cache.GetKernelName(kernel3, false);
  EXPECT_EQ(fetcher.call_count, 6);
}

TEST_F(ZeKernelNameCacheTest, MultipleKernelsIndependentLifecycles) {
  CountingFetcher fetcher;
  ZeKernelNameCache<CountingFetcher&> cache(fetcher);

  auto kernel1 = MakeKernel(0x100);
  auto kernel2 = MakeKernel(0x200);
  auto kernel3 = MakeKernel(0x300);

  // Populate cache
  cache.GetKernelName(kernel1, false);
  cache.GetKernelName(kernel2, false);
  cache.GetKernelName(kernel3, false);
  EXPECT_EQ(fetcher.call_count, 3);

  // Remove only kernel2
  cache.RemoveKernel(kernel2);

  // kernel1 and kernel3 should still be cached
  cache.GetKernelName(kernel1, false);
  EXPECT_EQ(fetcher.call_count, 3);  // No new fetch

  cache.GetKernelName(kernel3, false);
  EXPECT_EQ(fetcher.call_count, 3);  // No new fetch

  // kernel2 should cause re-fetch
  cache.GetKernelName(kernel2, false);
  EXPECT_EQ(fetcher.call_count, 4);
}

// =============================================================================
// Thread Safety Tests
// =============================================================================

TEST_F(ZeKernelNameCacheTest, ConcurrentReadsSameKernel) {
  CountingFetcher fetcher;
  ZeKernelNameCache<CountingFetcher&> cache(fetcher);

  auto kernel = MakeKernel(0x100);

  constexpr int kNumThreads = 10;
  std::vector<std::thread> threads;
  std::atomic<int> success_count{0};
  std::vector<std::string> results(kNumThreads);

  // Launch threads that all try to read the same kernel
  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back([&, i]() {
      results[i] = cache.GetKernelName(kernel, false);
      if (results[i] == "TestKernel_256") {
        ++success_count;
      }
    });
  }

  // Wait for all threads
  for (auto& t : threads) {
    t.join();
  }

  // All threads should succeed
  EXPECT_EQ(success_count, kNumThreads);

  // All results should be identical
  for (int i = 1; i < kNumThreads; ++i) {
    EXPECT_EQ(results[i], results[0]);
  }
  // Only one fetch should have occurred
  EXPECT_LE(fetcher.call_count, 1);
}

TEST_F(ZeKernelNameCacheTest, ConcurrentWritesDifferentKernels) {
  CountingFetcher fetcher;
  ZeKernelNameCache<CountingFetcher&> cache(fetcher);

  constexpr int kNumThreads = 10;
  std::vector<std::thread> threads;

  // Each thread accesses a different kernel
  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back([&, i]() {
      auto kernel = MakeKernel(0x100 + i);
      std::string name = cache.GetKernelName(kernel, false);
      EXPECT_FALSE(name.empty());
    });
  }

  // Wait for all threads
  for (auto& t : threads) {
    t.join();
  }

  // Each kernel should have been fetched exactly once
  EXPECT_EQ(fetcher.call_count, kNumThreads);

  // Accessing all kernels again should hit cache
  int initial_count = fetcher.call_count;
  for (int i = 0; i < kNumThreads; ++i) {
    auto kernel = MakeKernel(0x100 + i);
    cache.GetKernelName(kernel, false);
  }
  EXPECT_EQ(fetcher.call_count, initial_count);  // No new fetches
}

TEST_F(ZeKernelNameCacheTest, ConcurrentMixedOperations) {
  CountingFetcher fetcher;
  ZeKernelNameCache<CountingFetcher&> cache(fetcher);

  constexpr int kNumThreads = 12;
  std::vector<std::thread> threads;
  std::atomic<bool> start{false};

  auto kernel1 = MakeKernel(0x100);
  auto kernel2 = MakeKernel(0x200);
  auto kernel3 = MakeKernel(0x300);

  // Mix of operations: read, write, remove
  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back([&, i]() {
      // Wait for all threads to be ready
      while (!start.load()) {
        std::this_thread::yield();
      }

      if (i % 3 == 0) {
        // Read kernel1
        cache.GetKernelName(kernel1, false);
      } else if (i % 3 == 1) {
        // Read kernel2
        cache.GetKernelName(kernel2, false);
      } else {
        // Read kernel3
        cache.GetKernelName(kernel3, false);
      }
    });
  }

  // Start all threads simultaneously
  start = true;

  // Wait for completion
  for (auto& t : threads) {
    t.join();
  }

  // Cache should still be consistent
  EXPECT_NO_THROW(cache.GetKernelName(kernel1, false));
  EXPECT_NO_THROW(cache.GetKernelName(kernel2, false));
  EXPECT_NO_THROW(cache.GetKernelName(kernel3, false));
}

TEST_F(ZeKernelNameCacheTest, StressTestManyKernels) {
  CountingFetcher fetcher;
  ZeKernelNameCache<CountingFetcher&> cache(fetcher);

  constexpr int kNumKernels = 1000;
  constexpr int kNumThreads = 8;
  std::vector<std::thread> threads;

  // Each thread accesses a different range of kernels
  for (int t = 0; t < kNumThreads; ++t) {
    threads.emplace_back([&, t]() {
      int start = t * (kNumKernels / kNumThreads);
      int end = (t + 1) * (kNumKernels / kNumThreads);

      for (int i = start; i < end; ++i) {
        auto kernel = MakeKernel(0x1000 + i);
        std::string name = cache.GetKernelName(kernel, false);
        EXPECT_FALSE(name.empty());
      }
    });
  }

  // Wait for all threads
  for (auto& t : threads) {
    t.join();
  }

  // Each kernel should have been fetched exactly once
  EXPECT_EQ(fetcher.call_count, kNumKernels);

  // Test cache hit for random kernels
  int initial_count = fetcher.call_count;
  cache.GetKernelName(MakeKernel(0x1000), false);
  cache.GetKernelName(MakeKernel(0x1100), false);
  cache.GetKernelName(MakeKernel(0x1200), false);
  EXPECT_EQ(fetcher.call_count, initial_count);  // All cached
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_F(ZeKernelNameCacheTest, EmptyCacheOperations) {
  CountingFetcher fetcher;
  ZeKernelNameCache<CountingFetcher&> cache(fetcher);

  auto kernel = MakeKernel(0x100);

  // Clear on empty cache should not crash
  EXPECT_NO_THROW(cache.Clear());

  // Remove on empty cache should not crash
  EXPECT_NO_THROW(cache.RemoveKernel(kernel));
}

TEST_F(ZeKernelNameCacheTest, SameKernelMangledThenDemangled) {
  CountingFetcher fetcher;
  ZeKernelNameCache<CountingFetcher&> cache(fetcher);

  auto kernel = MakeKernel(0x100);

  // Access in different order
  std::string mangled = cache.GetKernelName(kernel, false);
  EXPECT_EQ(fetcher.call_count, 1);

  std::string demangled = cache.GetKernelName(kernel, true);
  EXPECT_EQ(fetcher.call_count, 2);

  // Both should remain cached
  std::string mangled2 = cache.GetKernelName(kernel, false);
  EXPECT_EQ(fetcher.call_count, 2);
  EXPECT_EQ(mangled2, mangled);

  std::string demangled2 = cache.GetKernelName(kernel, true);
  EXPECT_EQ(fetcher.call_count, 2);
  EXPECT_EQ(demangled2, demangled);
}

TEST_F(ZeKernelNameCacheTest, RapidAddRemoveCycles) {
  CountingFetcher fetcher;
  ZeKernelNameCache<CountingFetcher&> cache(fetcher);

  auto kernel = MakeKernel(0x100);

  constexpr int kCycles = 100;
  for (int i = 0; i < kCycles; ++i) {
    cache.GetKernelName(kernel, false);
    cache.RemoveKernel(kernel);
  }

  // Should have fetched kCycles times (once per cycle)
  EXPECT_EQ(fetcher.call_count, kCycles);
}

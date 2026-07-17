//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include "pti_string_pool.h"

#include <gtest/gtest.h>

#include <atomic>
#include <string>
#include <thread>
#include <vector>

namespace {

// Get deduplicates (same string -> same pointer, different -> different) and
// returns content matching the input (incl. empty); Size counts unique strings
// only and Contains reflects membership.
TEST(StringPoolTest, BasicContract) {
  StringPool pool;

  EXPECT_EQ(pool.Size(), 0u);
  EXPECT_FALSE(pool.Contains("kernelA"));

  const char* a1 = pool.Get("kernelA");
  const char* a2 = pool.Get("kernelA");  // duplicate must not grow the pool
  const char* b = pool.Get("kernelB");
  const char* empty = pool.Get("");

  EXPECT_EQ(a1, a2) << "repeated string should return the same pointer";
  EXPECT_NE(a1, b) << "different strings should return different pointers";

  EXPECT_STREQ(a1, "kernelA");
  EXPECT_STREQ(b, "kernelB");
  EXPECT_STREQ(empty, "");

  EXPECT_EQ(pool.Size(), 3u);  // "kernelA", "kernelB", ""
  EXPECT_TRUE(pool.Contains("kernelA"));
  EXPECT_TRUE(pool.Contains("kernelB"));
  EXPECT_FALSE(pool.Contains("kernelC"));
}

// The core lifetime guarantee: a pointer handed out earlier stays valid and
// keeps its content even after many more insertions force the underlying
// container to rehash.
TEST(StringPoolTest, PointersStableAcrossRehash) {
  StringPool pool;

  const char* first = pool.Get("stable_name");

  // Insert far more entries than the default bucket count to trigger rehashing.
  for (int i = 0; i < 10000; ++i) {
    pool.Get("filler_" + std::to_string(i));
  }

  EXPECT_EQ(pool.Get("stable_name"), first)
      << "pointer changed after rehash; existing pointers must stay valid";
  EXPECT_STREQ(first, "stable_name");
}

// Drives shared and exclusive locking under contention: every thread inserts
// its own unique keys (write path / emplace) while all threads also race to
// emplace and re-read a set of shared keys. Verifies content correctness, an
// exact unique count (no lost or duplicated insertions), and that every thread
// observes the same pointer for a given shared key.
TEST(StringPoolTest, ConcurrentAccessUnderContention) {
  StringPool pool;

  constexpr int kThreads = 8;
  constexpr int kKeysPerThread = 1000;

  std::vector<std::vector<const char*>> shared_pointers(kThreads,
                                                        std::vector<const char*>(kKeysPerThread));
  std::vector<std::thread> threads;
  std::atomic<int> ready{0};

  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([&, t]() {
      ready++;
      while (ready.load() < kThreads) {
        std::this_thread::yield();
      }
      for (int i = 0; i < kKeysPerThread; ++i) {
        const std::string unique_key = "t" + std::to_string(t) + "_k" + std::to_string(i);
        EXPECT_STREQ(pool.Get(unique_key), unique_key.c_str());

        const std::string shared_key = "shared_k" + std::to_string(i);
        shared_pointers[t][i] = pool.Get(shared_key);
        EXPECT_STREQ(shared_pointers[t][i], shared_key.c_str());
      }
    });
  }
  for (auto& th : threads) {
    th.join();
  }

  EXPECT_EQ(pool.Size(), static_cast<size_t>(kThreads * kKeysPerThread + kKeysPerThread));

  // Every thread must have observed the same pointer for each shared key.
  for (int i = 0; i < kKeysPerThread; ++i) {
    for (int t = 1; t < kThreads; ++t) {
      EXPECT_EQ(shared_pointers[t][i], shared_pointers[0][i])
          << "threads observed inconsistent pointers for a shared key";
    }
  }
}

}  // namespace

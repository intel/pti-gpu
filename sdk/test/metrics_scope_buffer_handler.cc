//==============================================================
// Copyright © Intel Corporation
// SPDX-License-Identifier: MIT
// =============================================================
// Unit tests for PtiMetricsScopeBufferHandler.
// White-box tests against the templated handler via MockBuffer.
// No SYCL, no GPU, no PTI runtime needed.
//==============================================================

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include "metrics/pti_metrics_scope_buffer_handler.h"
#include "pti/pti.h"

// Mock buffer for testing without real GPU resources
struct MockBuffer {
  size_t capacity;
  pti_device_handle_t device_handle;
  uint64_t buffer_id;
  size_t record_count = 0;
  bool is_finalized = false;
  std::unique_ptr<uint8_t[]> raw_buffer_;

  MockBuffer(size_t cap, pti_device_handle_t dev, uint64_t id)
      : capacity(cap),
        device_handle(dev),
        buffer_id(id),
        raw_buffer_(std::make_unique<uint8_t[]>(cap)) {}

  MockBuffer(const MockBuffer&) = delete;
  MockBuffer& operator=(const MockBuffer&) = delete;

  void* GetRawBuffer() const { return raw_buffer_.get(); }
  size_t GetCapacity() const { return capacity; }
  pti_device_handle_t GetDeviceHandle() const { return device_handle; }
  uint64_t GetBufferId() const { return buffer_id; }
  size_t GetRecordCount() const { return record_count; }
  bool IsFinalized() const { return is_finalized; }
  void Finalize() { is_finalized = true; }

  void AddMockRecord() { record_count++; }
};

/**
 * @brief Test fixture for BufferHandler Tests
 */
class BufferHandlerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    buffer_manager_ = std::make_unique<PtiMetricsScopeBufferHandler<MockBuffer>>();
    device1_ = reinterpret_cast<pti_device_handle_t>(0x1000);
    device2_ = reinterpret_cast<pti_device_handle_t>(0x2000);
    device3_ = reinterpret_cast<pti_device_handle_t>(0x3000);
  }

  void TearDown() override { buffer_manager_.reset(); }

  std::unique_ptr<PtiMetricsScopeBufferHandler<MockBuffer>> buffer_manager_;
  pti_device_handle_t device1_;
  pti_device_handle_t device2_;
  pti_device_handle_t device3_;
};

TEST_F(BufferHandlerTest, RegisterDevice_SingleDevice) {
  buffer_manager_->RegisterDevice(device1_);

  pti_result result = buffer_manager_->CreateBuffer(1024, device1_, 0);
  EXPECT_EQ(result, PTI_SUCCESS);

  auto* buffer = buffer_manager_->GetCurrentBuffer(device1_);
  EXPECT_NE(buffer, nullptr);
  EXPECT_EQ(buffer->GetDeviceHandle(), device1_);
}

TEST_F(BufferHandlerTest, RegisterDevice_MultipleDevices) {
  buffer_manager_->RegisterDevice(device1_);
  buffer_manager_->RegisterDevice(device2_);
  buffer_manager_->RegisterDevice(device3_);

  EXPECT_EQ(buffer_manager_->CreateBuffer(1024, device1_, 0), PTI_SUCCESS);
  EXPECT_EQ(buffer_manager_->CreateBuffer(2048, device2_, 0), PTI_SUCCESS);
  EXPECT_EQ(buffer_manager_->CreateBuffer(4096, device3_, 0), PTI_SUCCESS);

  EXPECT_NE(buffer_manager_->GetCurrentBuffer(device1_), nullptr);
  EXPECT_NE(buffer_manager_->GetCurrentBuffer(device2_), nullptr);
  EXPECT_NE(buffer_manager_->GetCurrentBuffer(device3_), nullptr);
}

TEST_F(BufferHandlerTest, DuplicateDeviceRegistration) {
  buffer_manager_->RegisterDevice(device1_);
  buffer_manager_->RegisterDevice(device1_);
  buffer_manager_->RegisterDevice(device1_);

  EXPECT_EQ(PTI_SUCCESS, buffer_manager_->CreateBuffer(1024, device1_, 0));
  buffer_manager_->GetCurrentBuffer(device1_)->AddMockRecord();
  buffer_manager_->FinalizeCurrentBuffer(device1_);

  EXPECT_EQ(1, buffer_manager_->GetBufferCount());

  int visible_count = 0;
  for (size_t i = 0; i < 10; i++) {
    if (buffer_manager_->GetBuffer(i) != nullptr) {
      visible_count++;
    }
  }

  EXPECT_EQ(1, visible_count) << "Duplicate registration caused buffer to appear " << visible_count
                              << " times!";
}

TEST_F(BufferHandlerTest, CreateBuffer_UnregisteredDevice) {
  // No RegisterDevice() call before CreateBuffer - REJECT
  EXPECT_EQ(buffer_manager_->CreateBuffer(1024, device1_, 0), PTI_ERROR_BAD_ARGUMENT);

  // No buffer should have been created
  EXPECT_EQ(buffer_manager_->GetCurrentBuffer(device1_), nullptr);
  EXPECT_EQ(buffer_manager_->GetBufferCount(), 0);
  EXPECT_EQ(buffer_manager_->GetBuffer(0), nullptr);

  // After registering, CreateBuffer succeeds
  buffer_manager_->RegisterDevice(device1_);
  EXPECT_EQ(buffer_manager_->CreateBuffer(1024, device1_, 0), PTI_SUCCESS);
  EXPECT_NE(buffer_manager_->GetCurrentBuffer(device1_), nullptr);
}

TEST_F(BufferHandlerTest, CreateBuffer_Success) {
  buffer_manager_->RegisterDevice(device1_);

  pti_result result = buffer_manager_->CreateBuffer(1024, device1_, 42);
  EXPECT_EQ(result, PTI_SUCCESS);

  auto* buffer = buffer_manager_->GetCurrentBuffer(device1_);
  ASSERT_NE(buffer, nullptr);
  EXPECT_EQ(buffer->GetBufferId(), 42);
  EXPECT_EQ(buffer->GetCapacity(), 1024);
}

TEST_F(BufferHandlerTest, CreateBufferRejectsWhenNotFull) {
  buffer_manager_->RegisterDevice(device1_);

  EXPECT_EQ(PTI_SUCCESS, buffer_manager_->CreateBuffer(1000, device1_, 0));

  MockBuffer* buf = buffer_manager_->GetCurrentBuffer(device1_);
  buf->AddMockRecord();
  ASSERT_GT(buf->GetRecordCount(), 0u);

  pti_result result = buffer_manager_->CreateBuffer(1000, device1_, 1);
  EXPECT_EQ(PTI_WARN_BUFFER_NOT_FINALIZED, result);

  EXPECT_EQ(0, buffer_manager_->GetCurrentBuffer(device1_)->GetBufferId());
}

TEST_F(BufferHandlerTest, CreateBufferReplacesEmptyCurrentBuffer) {
  buffer_manager_->RegisterDevice(device1_);

  EXPECT_EQ(PTI_SUCCESS, buffer_manager_->CreateBuffer(1000, device1_, 0));
  ASSERT_EQ(0u, buffer_manager_->GetCurrentBuffer(device1_)->GetRecordCount());

  EXPECT_EQ(PTI_SUCCESS, buffer_manager_->CreateBuffer(1000, device1_, 1));
  EXPECT_EQ(1, buffer_manager_->GetCurrentBuffer(device1_)->GetBufferId());
  EXPECT_EQ(0u, buffer_manager_->GetBufferCount());
}

TEST_F(BufferHandlerTest, CreateBufferSucceedsAfterFinalize) {
  buffer_manager_->RegisterDevice(device1_);

  EXPECT_EQ(PTI_SUCCESS, buffer_manager_->CreateBuffer(1000, device1_, 0));
  MockBuffer* buf = buffer_manager_->GetCurrentBuffer(device1_);
  buf->AddMockRecord();

  // Caller explicitly finalizes before rotating.
  EXPECT_EQ(PTI_SUCCESS, buffer_manager_->FinalizeCurrentBuffer(device1_));

  EXPECT_EQ(PTI_SUCCESS, buffer_manager_->CreateBuffer(1000, device1_, 1));

  EXPECT_EQ(1, buffer_manager_->GetBufferCount());
  EXPECT_EQ(1, buffer_manager_->GetCurrentBuffer(device1_)->GetBufferId());
}

TEST_F(BufferHandlerTest, GetBufferCount_EmptyManager) {
  EXPECT_EQ(buffer_manager_->GetBufferCount(), 0);
}

TEST_F(BufferHandlerTest, GetBufferCount_CurrentBuffersWithData) {
  buffer_manager_->RegisterDevice(device1_);
  buffer_manager_->RegisterDevice(device2_);

  buffer_manager_->CreateBuffer(1024, device1_, 0);
  buffer_manager_->CreateBuffer(1024, device2_, 0);

  // Buffers without records should not count
  EXPECT_EQ(buffer_manager_->GetBufferCount(), 0);

  // Add records
  buffer_manager_->GetCurrentBuffer(device1_)->AddMockRecord();
  EXPECT_EQ(buffer_manager_->GetBufferCount(), 1);

  buffer_manager_->GetCurrentBuffer(device2_)->AddMockRecord();
  EXPECT_EQ(buffer_manager_->GetBufferCount(), 2);
}

TEST_F(BufferHandlerTest, GetBufferCount_WithFinalizedBuffers) {
  buffer_manager_->RegisterDevice(device1_);
  buffer_manager_->CreateBuffer(1024, device1_, 0);

  auto* buffer = buffer_manager_->GetCurrentBuffer(device1_);
  buffer->AddMockRecord();

  EXPECT_EQ(buffer_manager_->GetBufferCount(), 1);

  buffer_manager_->FinalizeCurrentBuffer(device1_);

  EXPECT_EQ(buffer_manager_->GetBufferCount(), 1);
}

TEST_F(BufferHandlerTest, GetBufferCount_MixedCurrentAndFinalized) {
  buffer_manager_->RegisterDevice(device1_);
  buffer_manager_->RegisterDevice(device2_);

  // Device 1: Create and finalize
  buffer_manager_->CreateBuffer(1024, device1_, 0);
  buffer_manager_->GetCurrentBuffer(device1_)->AddMockRecord();
  buffer_manager_->FinalizeCurrentBuffer(device1_);

  // Device 2: Create and keep current
  buffer_manager_->CreateBuffer(1024, device2_, 0);
  buffer_manager_->GetCurrentBuffer(device2_)->AddMockRecord();

  EXPECT_EQ(buffer_manager_->GetBufferCount(), 2);
}

TEST_F(BufferHandlerTest, GetBuffer_InvalidIndex_ReturnsNull) {
  buffer_manager_->RegisterDevice(device1_);
  buffer_manager_->CreateBuffer(1024, device1_, 0);
  buffer_manager_->GetCurrentBuffer(device1_)->AddMockRecord();

  // Only 1 buffer exists
  EXPECT_NE(buffer_manager_->GetBuffer(0), nullptr);
  EXPECT_EQ(buffer_manager_->GetBuffer(1), nullptr);
  EXPECT_EQ(buffer_manager_->GetBuffer(999), nullptr);
}

TEST_F(BufferHandlerTest, GetBuffer_ByGlobalIndex) {
  buffer_manager_->RegisterDevice(device1_);
  buffer_manager_->RegisterDevice(device2_);

  buffer_manager_->CreateBuffer(1024, device1_, 10);
  buffer_manager_->CreateBuffer(2048, device2_, 20);

  buffer_manager_->GetCurrentBuffer(device1_)->AddMockRecord();
  buffer_manager_->GetCurrentBuffer(device2_)->AddMockRecord();

  auto* buffer0 = buffer_manager_->GetBuffer(0);
  auto* buffer1 = buffer_manager_->GetBuffer(1);

  ASSERT_NE(buffer0, nullptr);
  ASSERT_NE(buffer1, nullptr);

  // Verify we got different buffers
  EXPECT_NE(buffer0, buffer1);
}

TEST_F(BufferHandlerTest, GetBuffer_CurrentBufferWithoutData_NotCounted) {
  // Test that current buffer with 0 records is correctly excluded from count and indexing

  buffer_manager_->RegisterDevice(device1_);
  buffer_manager_->RegisterDevice(device2_);

  // Device1: finalized buffer with data
  buffer_manager_->CreateBuffer(1024, device1_, 0);
  buffer_manager_->GetCurrentBuffer(device1_)->AddMockRecord();
  buffer_manager_->FinalizeCurrentBuffer(device1_);

  // Device1: current buffer with NO data (record_count = 0)
  buffer_manager_->CreateBuffer(1024, device1_, 1);
  auto* dev1_empty = buffer_manager_->GetCurrentBuffer(device1_);
  EXPECT_EQ(dev1_empty->GetRecordCount(), 0);

  // Device2: current buffer WITH data
  buffer_manager_->CreateBuffer(1024, device2_, 0);
  auto* dev2_buffer = buffer_manager_->GetCurrentBuffer(device2_);
  dev2_buffer->AddMockRecord();

  // Count should be 2 (dev1 finalized + dev2 current)
  EXPECT_EQ(buffer_manager_->GetBufferCount(), 2);

  // Index 0 should be device1 finalized
  auto* buffer_at_0 = buffer_manager_->GetBuffer(0);
  ASSERT_NE(buffer_at_0, nullptr);
  EXPECT_EQ(buffer_at_0->GetDeviceHandle(), device1_);

  // Index 1 should SKIP device1's empty current and go to device2
  auto* buffer_at_1 = buffer_manager_->GetBuffer(1);
  EXPECT_EQ(buffer_at_1, dev2_buffer);

  // Index 2 should be out of bounds
  EXPECT_EQ(buffer_manager_->GetBuffer(2), nullptr);
}

TEST_F(BufferHandlerTest, GetBuffer_DeviceWithOnlyFinalizedBuffers_SkipsEmptyCurrent) {
  // When a device has finalized buffers but no current buffer, GetBuffer()
  // must skip the non-existent current slot without breaking global indexing.

  buffer_manager_->RegisterDevice(device1_);
  buffer_manager_->RegisterDevice(device2_);

  // Device1: 2 finalized buffers, NO current buffer (or empty current)
  buffer_manager_->CreateBuffer(1024, device1_, 0);
  buffer_manager_->GetCurrentBuffer(device1_)->AddMockRecord();
  buffer_manager_->FinalizeCurrentBuffer(device1_);

  buffer_manager_->CreateBuffer(1024, device1_, 1);
  buffer_manager_->GetCurrentBuffer(device1_)->AddMockRecord();
  buffer_manager_->FinalizeCurrentBuffer(device1_);
  // After finalize, device1 has no current buffer

  // Device2: 1 current buffer with data
  buffer_manager_->CreateBuffer(1024, device2_, 0);
  auto* dev2_buffer = buffer_manager_->GetCurrentBuffer(device2_);
  dev2_buffer->AddMockRecord();

  // Total should be 3: device1 (2 finalized + 0 current), device2 (0 finalized + 1 current)
  EXPECT_EQ(buffer_manager_->GetBufferCount(), 3);

  // Indices should be: 0=dev1 finalized, 1=dev1 finalized, 2=dev2 current
  auto* buffer_at_0 = buffer_manager_->GetBuffer(0);
  ASSERT_NE(buffer_at_0, nullptr);
  EXPECT_EQ(buffer_at_0->GetDeviceHandle(), device1_);

  auto* buffer_at_1 = buffer_manager_->GetBuffer(1);
  ASSERT_NE(buffer_at_1, nullptr);
  EXPECT_EQ(buffer_at_1->GetDeviceHandle(), device1_);

  // GetBuffer should calculate index 2 correctly after skipping device1's empty current.
  auto* buffer_at_2 = buffer_manager_->GetBuffer(2);
  EXPECT_EQ(buffer_at_2, dev2_buffer);  // Should be device2's buffer
  ASSERT_NE(buffer_at_2, nullptr);
  EXPECT_EQ(buffer_at_2->GetDeviceHandle(), device2_);
}

TEST_F(BufferHandlerTest, AsymmetricBufferIndexCalculationEdgeCases) {
  buffer_manager_->RegisterDevice(device1_);
  buffer_manager_->RegisterDevice(device2_);
  buffer_manager_->RegisterDevice(device3_);

  // Device A: 100 buffers
  for (int i = 0; i < 100; i++) {
    ASSERT_EQ(PTI_SUCCESS, buffer_manager_->CreateBuffer(1024, device1_, i));
    buffer_manager_->GetCurrentBuffer(device1_)->AddMockRecord();
    buffer_manager_->FinalizeCurrentBuffer(device1_);
  }

  // Device B: 0 buffers (skip entirely)

  // Device C: 50 buffers
  for (int i = 0; i < 50; i++) {
    ASSERT_EQ(PTI_SUCCESS, buffer_manager_->CreateBuffer(1024, device3_, i));
    buffer_manager_->GetCurrentBuffer(device3_)->AddMockRecord();
    buffer_manager_->FinalizeCurrentBuffer(device3_);
  }

  EXPECT_EQ(150, buffer_manager_->GetBufferCount());

  // Verify index calculation
  // Indices 0-99: Device A
  for (int i = 0; i < 100; i++) {
    MockBuffer* buf = buffer_manager_->GetBuffer(i);
    ASSERT_NE(nullptr, buf) << "Failed at index " << i;
    EXPECT_EQ(device1_, buf->GetDeviceHandle()) << "Wrong device at index " << i;
    EXPECT_EQ(i, buf->GetBufferId()) << "Wrong buffer_id at index " << i;
  }

  // Indices 100-149: Device C (skipping device B entirely)
  for (int i = 0; i < 50; i++) {
    size_t global_idx = 100 + i;
    MockBuffer* buf = buffer_manager_->GetBuffer(global_idx);
    ASSERT_NE(nullptr, buf) << "Failed at index " << global_idx;
    EXPECT_EQ(device3_, buf->GetDeviceHandle()) << "Wrong device at index " << global_idx;
    EXPECT_EQ(i, buf->GetBufferId()) << "Wrong buffer_id at index " << global_idx;
  }

  // Index 150: out of range
  EXPECT_EQ(nullptr, buffer_manager_->GetBuffer(150));

  std::cout << "Asymmetric buffer distribution (100-0-50) handled correctly." << std::endl;
}

TEST_F(BufferHandlerTest, InterleavedCurrentAndFinalizedBuffers) {
  buffer_manager_->RegisterDevice(device1_);
  buffer_manager_->RegisterDevice(device2_);
  buffer_manager_->RegisterDevice(device3_);

  // Device A: 2 finalized, 1 current
  ASSERT_EQ(PTI_SUCCESS, buffer_manager_->CreateBuffer(1024, device1_, 0));
  buffer_manager_->GetCurrentBuffer(device1_)->AddMockRecord();
  buffer_manager_->FinalizeCurrentBuffer(device1_);

  ASSERT_EQ(PTI_SUCCESS, buffer_manager_->CreateBuffer(1024, device1_, 1));
  buffer_manager_->GetCurrentBuffer(device1_)->AddMockRecord();
  buffer_manager_->FinalizeCurrentBuffer(device1_);

  ASSERT_EQ(PTI_SUCCESS, buffer_manager_->CreateBuffer(1024, device1_, 2));
  buffer_manager_->GetCurrentBuffer(device1_)->AddMockRecord();
  // Keep as current

  // Device B: 1 finalized, NO current
  ASSERT_EQ(PTI_SUCCESS, buffer_manager_->CreateBuffer(1024, device2_, 0));
  buffer_manager_->GetCurrentBuffer(device2_)->AddMockRecord();
  buffer_manager_->FinalizeCurrentBuffer(device2_);

  // Device C: NO finalized, 1 current
  ASSERT_EQ(PTI_SUCCESS, buffer_manager_->CreateBuffer(1024, device3_, 0));
  buffer_manager_->GetCurrentBuffer(device3_)->AddMockRecord();
  // Keep as current

  // Total: 2+1 (A) + 1 (B) + 1 (C) = 5
  EXPECT_EQ(5, buffer_manager_->GetBufferCount());

  // Verify global indexing
  // 0-2: Device A (finalized[0], finalized[1], current)
  EXPECT_EQ(device1_, buffer_manager_->GetBuffer(0)->GetDeviceHandle());
  EXPECT_EQ(0, buffer_manager_->GetBuffer(0)->GetBufferId());

  EXPECT_EQ(device1_, buffer_manager_->GetBuffer(1)->GetDeviceHandle());
  EXPECT_EQ(1, buffer_manager_->GetBuffer(1)->GetBufferId());

  EXPECT_EQ(device1_, buffer_manager_->GetBuffer(2)->GetDeviceHandle());
  EXPECT_EQ(2, buffer_manager_->GetBuffer(2)->GetBufferId());

  // 3: Device B (finalized[0])
  EXPECT_EQ(device2_, buffer_manager_->GetBuffer(3)->GetDeviceHandle());
  EXPECT_EQ(0, buffer_manager_->GetBuffer(3)->GetBufferId());

  // 4: Device C (current)
  EXPECT_EQ(device3_, buffer_manager_->GetBuffer(4)->GetDeviceHandle());
  EXPECT_EQ(0, buffer_manager_->GetBuffer(4)->GetBufferId());

  EXPECT_EQ(nullptr, buffer_manager_->GetBuffer(5));
}

TEST_F(BufferHandlerTest, FinalizeCurrentBuffer_NoCurrentBuffer_ReturnsSuccess) {
  buffer_manager_->RegisterDevice(device1_);

  pti_result result = buffer_manager_->FinalizeCurrentBuffer(device1_);
  EXPECT_EQ(result, PTI_SUCCESS);
}

TEST_F(BufferHandlerTest, FinalizeCurrentBuffer_MovesToFinalizedList) {
  buffer_manager_->RegisterDevice(device1_);
  buffer_manager_->CreateBuffer(1024, device1_, 0);

  auto* buffer = buffer_manager_->GetCurrentBuffer(device1_);
  buffer->AddMockRecord();
  void* raw_ptr = buffer->GetRawBuffer();

  // Finalize
  pti_result result = buffer_manager_->FinalizeCurrentBuffer(device1_);
  EXPECT_EQ(result, PTI_SUCCESS);

  // Current buffer should be cleared
  EXPECT_EQ(buffer_manager_->GetCurrentBuffer(device1_), nullptr);

  // Should find in finalized buffers
  auto* found = buffer_manager_->FindBufferByRawPointer(raw_ptr);
  ASSERT_NE(found, nullptr);
  EXPECT_TRUE(found->IsFinalized());
}

TEST_F(BufferHandlerTest, EmptyBuffer_DiscardedOnFinalized) {
  buffer_manager_->RegisterDevice(device1_);

  EXPECT_EQ(PTI_SUCCESS, buffer_manager_->CreateBuffer(1024, device1_, 0));
  EXPECT_EQ(PTI_SUCCESS, buffer_manager_->FinalizeCurrentBuffer(device1_));
  EXPECT_EQ(0, buffer_manager_->GetBufferCount());
  EXPECT_EQ(nullptr, buffer_manager_->GetCurrentBuffer(device1_));
}

TEST_F(BufferHandlerTest, FinalizeFromMainThread_FinalizesWorkerThreadBuffers) {
  // At end-of-collection, the API thread (e.g., main) calls
  // FinalizeAllCurrentBuffers(). All threads' current buffers for that device
  // should be finalized — not just the calling thread's slot.

  buffer_manager_->RegisterDevice(device1_);

  // Worker thread creates a buffer with data
  std::thread worker([this]() {
    ASSERT_EQ(PTI_SUCCESS, buffer_manager_->CreateBuffer(1024, device1_, 100));
    auto* buf = buffer_manager_->GetCurrentBuffer(device1_);
    ASSERT_NE(nullptr, buf);
    buf->AddMockRecord();
  });
  worker.join();

  ASSERT_EQ(1, buffer_manager_->GetBufferCount());

  // Main thread (this thread) calls Finalize
  EXPECT_EQ(PTI_SUCCESS, buffer_manager_->FinalizeAllCurrentBuffers());

  auto* buf = buffer_manager_->GetBuffer(0);
  ASSERT_NE(nullptr, buf);
  EXPECT_TRUE(buf->IsFinalized()) << "Worker thread's current buffer was not finalized when "
                                     "stop-collection ran on the main thread.";
}

TEST_F(BufferHandlerTest, FinalizeCurrentBuffer_AcrossManyThreads) {
  buffer_manager_->RegisterDevice(device1_);

  constexpr int kNumThreads = 100;
  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back([this, i]() {
      buffer_manager_->CreateBuffer(1024, device1_, i);
      buffer_manager_->GetCurrentBuffer(device1_)->AddMockRecord();
      buffer_manager_->FinalizeCurrentBuffer(device1_);
    });
  }
  for (auto& t : threads) t.join();

  EXPECT_EQ(static_cast<size_t>(kNumThreads), buffer_manager_->GetBufferCount());
}

TEST_F(BufferHandlerTest, FinalizeAllCurrentBuffers_AcrossManyThreads) {
  buffer_manager_->RegisterDevice(device1_);

  constexpr int kNumThreads = 50;
  std::vector<std::thread> threads;
  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back([this, i]() {
      buffer_manager_->CreateBuffer(1024, device1_, i);
      buffer_manager_->GetCurrentBuffer(device1_)->AddMockRecord();
    });
  }
  for (auto& t : threads) t.join();

  // All threads have non-finalized current buffers with data.
  EXPECT_EQ(static_cast<size_t>(kNumThreads), buffer_manager_->GetBufferCount());

  buffer_manager_->FinalizeAllCurrentBuffers();

  // After sweep, all buffers are finalized but still counted.
  EXPECT_EQ(static_cast<size_t>(kNumThreads), buffer_manager_->GetBufferCount());
}

TEST_F(BufferHandlerTest, FindBufferByRawPointer_FindsCurrentBuffer) {
  buffer_manager_->RegisterDevice(device1_);
  buffer_manager_->CreateBuffer(1024, device1_, 0);

  auto* buffer = buffer_manager_->GetCurrentBuffer(device1_);
  void* raw_ptr = buffer->GetRawBuffer();

  auto* found = buffer_manager_->FindBufferByRawPointer(raw_ptr);
  EXPECT_EQ(found, buffer);
}

TEST_F(BufferHandlerTest, FindBufferByRawPointer_FindsFinalizedBuffer) {
  buffer_manager_->RegisterDevice(device1_);
  buffer_manager_->CreateBuffer(1024, device1_, 0);

  auto* buffer = buffer_manager_->GetCurrentBuffer(device1_);
  buffer->AddMockRecord();
  void* raw_ptr = buffer->GetRawBuffer();

  buffer_manager_->FinalizeCurrentBuffer(device1_);

  auto* found = buffer_manager_->FindBufferByRawPointer(raw_ptr);
  EXPECT_NE(found, nullptr);
  EXPECT_EQ(found->GetRawBuffer(), raw_ptr);
}

TEST_F(BufferHandlerTest, Multithread_ConcurrentRegisterDevice) {
  // Test concurrent device registration from multiple threads
  // Should not crash or corrupt device_order_

  constexpr int kNumThreads = 4;
  std::vector<std::thread> threads;
  std::atomic<int> ready_count{0};
  std::atomic<bool> start_flag{false};

  std::vector<pti_device_handle_t> devices;
  for (int i = 0; i < kNumThreads; ++i) {
    devices.push_back(reinterpret_cast<pti_device_handle_t>(0x1000 + i * 0x1000));
  }

  // Launch threads that register devices concurrently
  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back([this, &ready_count, &start_flag, device = devices[i]]() {
      ready_count++;
      while (!start_flag.load()) {
        std::this_thread::yield();
      }
      buffer_manager_->RegisterDevice(device);
    });
  }

  while (ready_count.load() < kNumThreads) {
    std::this_thread::yield();
  }
  start_flag.store(true);

  for (auto& t : threads) {
    t.join();
  }

  // Verify all devices were registered (no data corruption)
  // Each device should be able to create a buffer
  for (const auto& device : devices) {
    EXPECT_EQ(buffer_manager_->CreateBuffer(1024, device, 0), PTI_SUCCESS);
    EXPECT_NE(buffer_manager_->GetCurrentBuffer(device), nullptr);
  }
}

TEST_F(BufferHandlerTest, Multithread_ConcurrentCreateBufferDifferentDevices) {
  // Test concurrent buffer creation for different devices
  // Should handle parallel writes to different map entries

  constexpr int kNumThreads = 3;
  constexpr int kBuffersPerThread = 10;

  buffer_manager_->RegisterDevice(device1_);
  buffer_manager_->RegisterDevice(device2_);
  buffer_manager_->RegisterDevice(device3_);

  std::vector<pti_device_handle_t> devices = {device1_, device2_, device3_};
  std::vector<std::thread> threads;
  std::atomic<bool> start_flag{false};
  std::atomic<int> ready_count{0};

  // Each thread works with its own device
  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back([this, &start_flag, &ready_count, device = devices[i]]() {
      ready_count++;
      while (!start_flag.load()) {
        std::this_thread::yield();
      }

      // Create multiple buffers, adding records and finalizing
      for (int j = 0; j < kBuffersPerThread; ++j) {
        buffer_manager_->CreateBuffer(1024, device, j);
        auto* buffer = buffer_manager_->GetCurrentBuffer(device);
        if (buffer) {
          buffer->AddMockRecord();
          buffer_manager_->FinalizeCurrentBuffer(device);
        }
      }
    });
  }

  while (ready_count.load() < kNumThreads) {
    std::this_thread::yield();
  }
  start_flag.store(true);

  for (auto& t : threads) {
    t.join();
  }

  // Verify: Should have kBuffersPerThread * kNumThreads finalized buffers
  size_t expected_count = kBuffersPerThread * kNumThreads;
  EXPECT_EQ(buffer_manager_->GetBufferCount(), expected_count);
}

TEST_F(BufferHandlerTest, Multithread_ConcurrentCreateSameDevice_RaceCondition) {
  // Multiple threads creating buffers for the SAME device
  // Expected: Each (device, thread) pair has its own current buffer slot.
  //           Concurrent CreateBuffer() from N threads on the same device should produce
  //           N independent current buffers — no overwrites, no losses.

  constexpr int kNumThreads = 10;
  buffer_manager_->RegisterDevice(device1_);

  std::vector<std::thread> threads;
  std::atomic<bool> start_flag{false};
  std::atomic<int> ready_count{0};
  std::atomic<int> success_count{0};
  std::atomic<int> got_own_buffer{0};

  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back([this, &start_flag, &ready_count, &success_count, &got_own_buffer, i]() {
      ready_count++;
      while (!start_flag.load()) {
        std::this_thread::yield();
      }

      // All threads attempt CreateBuffer simultaneously
      pti_result result = buffer_manager_->CreateBuffer(1024, device1_, i);
      if (result == PTI_SUCCESS) {
        success_count++;
        auto* buf = buffer_manager_->GetCurrentBuffer(device1_);
        if (buf && buf->GetBufferId() == static_cast<uint64_t>(i)) {
          got_own_buffer++;
        }
      }
    });
  }

  while (ready_count.load() < kNumThreads) {
    std::this_thread::yield();
  }
  start_flag.store(true);

  for (auto& t : threads) {
    t.join();
  }

  // All N CreateBuffer calls should succeed
  EXPECT_EQ(success_count.load(), kNumThreads);

  // Each thread saw its own buffer — no cross-thread interference
  EXPECT_EQ(got_own_buffer.load(), kNumThreads);

  // Main thread never called CreateBuffer → its slot is empty
  EXPECT_EQ(buffer_manager_->GetCurrentBuffer(device1_), nullptr);
}

TEST_F(BufferHandlerTest, ConcurrentCreateBufferZeroLoss) {
  // N threads concurrently create + finalize a buffer on the same device.
  // All N must succeed (no losses from per-(device, thread) slot collisions).
  buffer_manager_->RegisterDevice(device1_);

  constexpr int kNumThreads = 10;
  std::atomic<int> successful_creates{0};

  auto worker = [this, &successful_creates](int thread_id) {
    if (buffer_manager_->CreateBuffer(1000, device1_, thread_id) == PTI_SUCCESS) {
      MockBuffer* buf = buffer_manager_->GetCurrentBuffer(device1_);
      if (buf) {
        buf->AddMockRecord();
        buffer_manager_->FinalizeCurrentBuffer(device1_);
        successful_creates++;
      }
    }
  };

  std::vector<std::thread> threads;
  for (int i = 0; i < kNumThreads; i++) {
    threads.emplace_back(worker, i);
  }

  for (auto& t : threads) {
    t.join();
  }

  EXPECT_EQ(kNumThreads, successful_creates.load());
  EXPECT_EQ(kNumThreads, buffer_manager_->GetBufferCount());
}

TEST_F(BufferHandlerTest, ThreadPerDeviceShouldNotInterfere) {
  constexpr int kNumDevices = 4;
  constexpr int kBuffersPerDevice = 50;

  std::vector<pti_device_handle_t> devices;
  for (int i = 0; i < kNumDevices; i++) {
    pti_device_handle_t device = reinterpret_cast<pti_device_handle_t>(0x1000 + i * 0x1000);
    devices.push_back(device);
    buffer_manager_->RegisterDevice(device);
  }

  std::atomic<int> total_created{0};
  std::vector<std::atomic<int>> per_device_count(kNumDevices);

  auto worker = [this, &devices, &per_device_count, &total_created](int device_index) {
    pti_device_handle_t device = devices[device_index];
    int created = 0;

    for (int i = 0; i < kBuffersPerDevice; i++) {
      if (buffer_manager_->CreateBuffer(1024, device, i) == PTI_SUCCESS) {
        MockBuffer* buf = buffer_manager_->GetCurrentBuffer(device);
        if (buf && buf->GetDeviceHandle() == device) {
          buf->AddMockRecord();
          buffer_manager_->FinalizeCurrentBuffer(device);
          created++;
        }
      }
    }

    per_device_count[device_index] = created;
    total_created += created;
  };

  std::vector<std::thread> threads;
  for (int i = 0; i < kNumDevices; i++) {
    threads.emplace_back(worker, i);
  }

  for (auto& t : threads) {
    t.join();
  }

  size_t final_count = buffer_manager_->GetBufferCount();

  std::cout << "Total created: " << total_created << std::endl;
  std::cout << "Final count: " << final_count << std::endl;

  for (int i = 0; i < kNumDevices; i++) {
    std::cout << "Device " << i << ": " << per_device_count[i] << " buffers" << std::endl;
  }

  // Thread-per-device should work perfectly (no shared state)
  EXPECT_EQ(kNumDevices * kBuffersPerDevice, total_created.load())
      << "FAIL: Some buffers were not created!";

  EXPECT_EQ(total_created.load(), final_count) << "FAIL: Buffer count mismatch!";

  // Each device should have all its buffers
  for (int i = 0; i < kNumDevices; i++) {
    EXPECT_EQ(kBuffersPerDevice, per_device_count[i].load())
        << "FAIL: Device " << i << " missing buffers!";
  }
}

TEST_F(BufferHandlerTest, ConcurrentCreateAndFinalizeShouldNotRace) {
  buffer_manager_->RegisterDevice(device1_);

  std::atomic<bool> stop{false};
  std::atomic<int> create_count{0};
  std::atomic<int> finalize_count{0};
  std::atomic<int> errors{0};

  // Thread 1: Keep creating buffers
  auto creator = [this, &stop, &create_count, &errors]() {
    int id = 0;
    while (!stop.load()) {
      if (buffer_manager_->CreateBuffer(1024, device1_, id++) == PTI_SUCCESS) {
        MockBuffer* buf = buffer_manager_->GetCurrentBuffer(device1_);
        if (buf) {
          buf->AddMockRecord();
          create_count++;
        } else {
          errors++;
        }
      }
      std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
  };

  // Thread 2: Keep finalizing buffers
  auto finalizer = [this, &stop, &finalize_count]() {
    while (!stop.load()) {
      if (buffer_manager_->FinalizeCurrentBuffer(device1_) == PTI_SUCCESS) {
        finalize_count++;
      }
      std::this_thread::sleep_for(std::chrono::microseconds(15));
    }
  };

  std::thread t1(creator);
  std::thread t2(finalizer);

  // Run for 100ms
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  stop = true;

  t1.join();
  t2.join();

  size_t final_count = buffer_manager_->GetBufferCount();

  std::cout << "Created: " << create_count << std::endl;
  std::cout << "Finalized: " << finalize_count << std::endl;
  std::cout << "Final count: " << final_count << std::endl;
  std::cout << "Errors: " << errors << std::endl;

  EXPECT_EQ(0, errors.load()) << "FAIL: Race condition caused errors!";

  // Final count should be reasonable (finalized buffers + maybe 1 current)
  EXPECT_LE(final_count, finalize_count.load() + 1) << "FAIL: More buffers than expected!";
}

TEST_F(BufferHandlerTest, Multithread_ReadersAndWriters_DataIntegrity) {
  // Test concurrent readers (GetBuffer, GetBufferCount) and writers (CreateBuffer, Finalize)
  // Verifies that reads during writes don't crash or see corrupted data

  buffer_manager_->RegisterDevice(device1_);
  buffer_manager_->RegisterDevice(device2_);

  std::atomic<bool> stop_flag{false};
  std::atomic<int> reader_iterations{0};
  std::atomic<int> writer_iterations{0};

  // Writer thread: Creates and finalizes buffers
  std::thread writer([this, &stop_flag, &writer_iterations]() {
    uint64_t buffer_id = 0;
    while (!stop_flag.load()) {
      // Alternate between device1 and device2
      auto device = (buffer_id % 2 == 0) ? device1_ : device2_;

      buffer_manager_->CreateBuffer(1024, device, buffer_id);
      auto* buffer = buffer_manager_->GetCurrentBuffer(device);
      if (buffer) {
        buffer->AddMockRecord();
        buffer_manager_->FinalizeCurrentBuffer(device);
      }

      buffer_id++;
      writer_iterations++;
      std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
  });

  // Reader threads: Constantly read buffer count and access buffers
  std::vector<std::thread> readers;
  for (int i = 0; i < 3; ++i) {
    readers.emplace_back([this, &stop_flag, &reader_iterations]() {
      while (!stop_flag.load()) {
        // Read operations should not crash even during concurrent writes
        size_t count = buffer_manager_->GetBufferCount();

        // Try to access all visible buffers
        for (size_t idx = 0; idx < count; ++idx) {
          auto* buffer = buffer_manager_->GetBuffer(idx);
          if (buffer) {
            // Access buffer data (should not crash)
            volatile void* ptr = buffer->GetRawBuffer();
            (void)ptr;
          }
        }

        reader_iterations++;
        std::this_thread::yield();
      }
    });
  }

  // Let threads run for a short time
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  stop_flag.store(true);

  writer.join();
  for (auto& r : readers) {
    r.join();
  }

  // Verify: No crashes, and both readers and writers made progress
  EXPECT_GT(reader_iterations.load(), 0);
  EXPECT_GT(writer_iterations.load(), 0);

  // Final buffer count should match writer iterations
  EXPECT_EQ(buffer_manager_->GetBufferCount(), writer_iterations.load());
}

TEST_F(BufferHandlerTest, Multithread_FindBufferByRawPointer_DuringFinalization) {
  // Test FindBufferByRawPointer while buffers are being moved from current to finalized

  buffer_manager_->RegisterDevice(device1_);
  buffer_manager_->CreateBuffer(1024, device1_, 0);
  auto* buffer = buffer_manager_->GetCurrentBuffer(device1_);
  buffer->AddMockRecord();
  void* raw_ptr = buffer->GetRawBuffer();

  std::atomic<bool> finalized{false};
  std::atomic<int> lookup_success_count{0};
  std::atomic<int> lookup_attempts{0};

  // Thread 1: Finalizes the buffer (moves it from current to finalized)
  std::thread finalizer([this, &finalized]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    buffer_manager_->FinalizeCurrentBuffer(device1_);
    finalized.store(true);
  });

  // Thread 2: Continuously looks up the buffer by raw pointer
  std::thread lookup([this, raw_ptr, &finalized, &lookup_success_count, &lookup_attempts]() {
    while (!finalized.load()) {
      auto* found = buffer_manager_->FindBufferByRawPointer(raw_ptr);
      lookup_attempts++;
      if (found != nullptr) {
        lookup_success_count++;
      }
      std::this_thread::yield();
    }

    // After finalization, should still find it
    auto* found = buffer_manager_->FindBufferByRawPointer(raw_ptr);
    if (found != nullptr) {
      lookup_success_count++;
    }
    lookup_attempts++;
  });

  finalizer.join();
  lookup.join();

  // Buffer should always be findable (before and after finalization)
  // If any lookup failed, it indicates a race condition
  EXPECT_EQ(lookup_success_count.load(), lookup_attempts.load());
  EXPECT_GT(lookup_attempts.load(), 0);
}

TEST_F(BufferHandlerTest, Multithread_StressTest_AllOperations) {
  // Comprehensive stress test: Multiple threads performing all operations

  buffer_manager_->RegisterDevice(device1_);
  buffer_manager_->RegisterDevice(device2_);
  buffer_manager_->RegisterDevice(device3_);

  std::atomic<bool> stop_flag{false};
  std::vector<std::thread> threads;

  // Thread 1: Creates buffers for device1
  threads.emplace_back([this, &stop_flag]() {
    uint64_t id = 0;
    while (!stop_flag.load()) {
      buffer_manager_->CreateBuffer(1024, device1_, id++);
      auto* buf = buffer_manager_->GetCurrentBuffer(device1_);
      if (buf && (id % 3 == 0)) {
        buf->AddMockRecord();
        buffer_manager_->FinalizeCurrentBuffer(device1_);
      }
      std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
  });

  // Thread 2: Creates buffers for device2
  threads.emplace_back([this, &stop_flag]() {
    uint64_t id = 0;
    while (!stop_flag.load()) {
      buffer_manager_->CreateBuffer(2048, device2_, id++);
      auto* buf = buffer_manager_->GetCurrentBuffer(device2_);
      if (buf && (id % 2 == 0)) {
        buf->AddMockRecord();
        buffer_manager_->FinalizeCurrentBuffer(device2_);
      }
      std::this_thread::sleep_for(std::chrono::microseconds(75));
    }
  });

  // Thread 3: Reads buffer counts and accesses buffers
  threads.emplace_back([this, &stop_flag]() {
    while (!stop_flag.load()) {
      size_t count = buffer_manager_->GetBufferCount();
      for (size_t i = 0; i < (std::min)(count, size_t(5)); ++i) {
        buffer_manager_->GetBuffer(i);
      }
      std::this_thread::yield();
    }
  });

  // Thread 4: Looks up buffers by device
  threads.emplace_back([this, &stop_flag]() {
    std::vector<pti_device_handle_t> devices = {device1_, device2_, device3_};
    while (!stop_flag.load()) {
      for (auto dev : devices) {
        buffer_manager_->GetCurrentBuffer(dev);
      }
      std::this_thread::yield();
    }
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  stop_flag.store(true);

  for (auto& t : threads) {
    t.join();
  }

  size_t final_count = buffer_manager_->GetBufferCount();
  for (size_t i = 0; i < final_count; ++i) {
    buffer_manager_->GetBuffer(i);
  }
}

TEST_F(BufferHandlerTest, Multithread_RegisterFromDifferentThreads_IndexOrderSortedByHandle) {
  // Verifies that device_order_ is sorted by device handle, so GetBuffer() indices
  // are deterministic regardless of which thread registered first.

  constexpr int kNumThreads = 3;
  std::vector<pti_device_handle_t> devices = {device1_, device2_, device3_};
  int buffers_per_device[] = {2, 3, 1};  // device1: 2, device2: 3, device3: 1

  std::atomic<bool> start_flag{false};
  std::atomic<int> ready_count{0};
  std::vector<std::thread> threads;

  // Launch threads with artificial delays to force reverse registration order
  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back([this, &start_flag, &ready_count, device = devices[i],
                          num_buffers = buffers_per_device[i], thread_id = i]() {
      std::this_thread::sleep_for(std::chrono::microseconds((kNumThreads - 1 - thread_id) * 200));

      ready_count++;
      while (!start_flag.load()) {
        std::this_thread::yield();
      }

      buffer_manager_->RegisterDevice(device);

      for (int j = 0; j < num_buffers; ++j) {
        buffer_manager_->CreateBuffer(1024, device, j);
        auto* buf = buffer_manager_->GetCurrentBuffer(device);
        if (buf) {
          buf->AddMockRecord();
          buffer_manager_->FinalizeCurrentBuffer(device);
        }
      }
    });
  }

  while (ready_count.load() < kNumThreads) {
    std::this_thread::yield();
  }
  start_flag.store(true);

  for (auto& t : threads) {
    t.join();
  }

  // Total: 2 + 3 + 1 = 6 buffers
  EXPECT_EQ(buffer_manager_->GetBufferCount(), 6);

  // Expected indices (sorted by device handle):
  //   device1 (0x1000): indices 0, 1
  //   device2 (0x2000): indices 2, 3, 4
  //   device3 (0x3000): index 5

  auto* buffer_at_0 = buffer_manager_->GetBuffer(0);
  ASSERT_NE(buffer_at_0, nullptr);
  EXPECT_EQ(buffer_at_0->GetDeviceHandle(), device1_);

  auto* buffer_at_2 = buffer_manager_->GetBuffer(2);
  ASSERT_NE(buffer_at_2, nullptr);
  EXPECT_EQ(buffer_at_2->GetDeviceHandle(), device2_);

  auto* buffer_at_4 = buffer_manager_->GetBuffer(4);
  ASSERT_NE(buffer_at_4, nullptr);
  EXPECT_EQ(buffer_at_4->GetDeviceHandle(), device2_);
}

TEST_F(BufferHandlerTest, Multithread_ConcurrentRegistration_DeviceOrderConsistent) {
  // Verifies that concurrent device registration produces deterministic device order
  // across multiple runs (sorted by device handle).

  constexpr int kIterations = 10;
  constexpr int kNumThreads = 3;

  std::vector<std::vector<pti_device_handle_t>> observed_orders;

  for (int iter = 0; iter < kIterations; ++iter) {
    auto local_manager = std::make_unique<PtiMetricsScopeBufferHandler<MockBuffer>>();

    std::vector<pti_device_handle_t> devices = {device1_, device2_, device3_};
    int buffers_per_device = 2;

    std::atomic<int> ready_count{0};
    std::atomic<bool> start_flag{false};
    std::vector<std::thread> threads;

    // Register devices concurrently
    for (int i = 0; i < kNumThreads; ++i) {
      threads.emplace_back(
          [&local_manager, &ready_count, &start_flag, device = devices[i], buffers_per_device]() {
            ready_count++;
            while (!start_flag.load()) {
              std::this_thread::yield();
            }

            local_manager->RegisterDevice(device);

            for (int j = 0; j < buffers_per_device; ++j) {
              local_manager->CreateBuffer(1024, device, j);
              auto* buf = local_manager->GetCurrentBuffer(device);
              if (buf) {
                buf->AddMockRecord();
                local_manager->FinalizeCurrentBuffer(device);
              }
            }
          });
    }

    while (ready_count.load() < kNumThreads) {
      std::this_thread::yield();
    }
    start_flag.store(true);

    for (auto& t : threads) {
      t.join();
    }

    std::vector<pti_device_handle_t> device_order;
    size_t total_buffers = local_manager->GetBufferCount();
    EXPECT_EQ(total_buffers, 6);  // 3 devices * 2 buffers each

    pti_device_handle_t last_device = nullptr;
    for (size_t i = 0; i < total_buffers; ++i) {
      auto* buf = local_manager->GetBuffer(i);
      ASSERT_NE(buf, nullptr);
      pti_device_handle_t current_device = buf->GetDeviceHandle();

      if (current_device != last_device) {
        device_order.push_back(current_device);
        last_device = current_device;
      }
    }

    EXPECT_EQ(device_order.size(), 3);
    observed_orders.push_back(device_order);
  }

  // All iterations should produce the same device order
  bool all_orders_match = true;
  const auto& first_order = observed_orders[0];

  for (size_t i = 1; i < observed_orders.size(); ++i) {
    if (observed_orders[i] != first_order) {
      all_orders_match = false;
      break;
    }
  }

  EXPECT_TRUE(all_orders_match) << "Device order inconsistent across iterations. "
                                << "First iteration order: [" << static_cast<void*>(first_order[0])
                                << ", " << static_cast<void*>(first_order[1]) << ", "
                                << static_cast<void*>(first_order[2]) << "]";
}

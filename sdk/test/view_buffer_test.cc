#include "view_buffer.h"

#include <gtest/gtest.h>

#include <array>
#include <cstring>
#include <thread>
#include <vector>

#include "pti/pti_view.h"
#include "utils/test_helpers.h"
#include "view_handler.h"

namespace {

constexpr auto kNumOfMemRecordsInBuffer = 2;
constexpr auto kNumOfKernelRecordsInBuffer = 1;
constexpr auto kNumOfRecordsInBuffer = kNumOfMemRecordsInBuffer + kNumOfKernelRecordsInBuffer;
constexpr auto kSizeOfTestBuffer = kNumOfRecordsInBuffer * SizeOfLargestViewRecord();
constexpr auto kDefaultBufferFillValue = 0;
constexpr const auto* const kDefaultKernelName = "ReallyComplicated_KernelName_SomeOp";

std::size_t FillBuffer(pti::view::utilities::ViewBuffer& a_buffer) {
  std::size_t bytes_inserted = 0;
  auto number_of_memory_records_left = kNumOfMemRecordsInBuffer;
  auto number_of_kernel_records_left = kNumOfKernelRecordsInBuffer;
  auto keep_inserting = true;
  for (auto i = 0; i < kNumOfRecordsInBuffer && keep_inserting; i++) {
    using pti::test::utils::CreateRecord;
    if (number_of_memory_records_left) {
      EXPECT_EQ(keep_inserting, true);
      a_buffer.Insert(CreateRecord<pti_view_record_memory_copy>());
      // check if we can insert a kernel record after
      keep_inserting = !a_buffer.BufferFull<pti_view_record_kernel>();
      bytes_inserted += sizeof(pti_view_record_memory_copy);
      number_of_memory_records_left--;
    }
    if (number_of_kernel_records_left) {
      EXPECT_EQ(keep_inserting, true);
      auto record = CreateRecord<pti_view_record_kernel>();
      record._name = kDefaultKernelName;
      a_buffer.Insert(record);
      // check if we can insert a memory record after
      keep_inserting = !a_buffer.BufferFull<pti_view_record_memory_copy>();
      bytes_inserted += sizeof(pti_view_record_kernel);
      number_of_kernel_records_left--;
    }
  }
  return bytes_inserted;
}

}  // namespace

class ViewBufferFixtureTest : public ::testing::Test {
 protected:
  void SetUp() override {
    standard_buffer_.Refresh(underlying_buffer_.data(), underlying_buffer_.size());

    bytes_inserted_ = FillBuffer(standard_buffer_);
  }

  void TearDown() override {
    std::fill(underlying_buffer_.begin(), underlying_buffer_.end(), kDefaultBufferFillValue);
    bytes_inserted_ = 0;
  }
  std::vector<unsigned char> underlying_buffer_ =
      std::vector<unsigned char>(kSizeOfTestBuffer, kDefaultBufferFillValue);
  pti::view::utilities::ViewBuffer standard_buffer_ = {};
  std::size_t bytes_inserted_ = 0;
};

TEST_F(ViewBufferFixtureTest, MoveConstructor) {
  EXPECT_EQ(standard_buffer_.IsNull(), false);
  EXPECT_EQ(standard_buffer_.GetValidBytes(), bytes_inserted_);
  auto destination_buffer{std::move(standard_buffer_)};
  EXPECT_EQ(destination_buffer.IsNull(), false);
  EXPECT_EQ(destination_buffer.GetValidBytes(), bytes_inserted_);
}

TEST_F(ViewBufferFixtureTest, MoveAssignment) {
  EXPECT_EQ(standard_buffer_.IsNull(), false);
  EXPECT_EQ(standard_buffer_.GetValidBytes(), bytes_inserted_);
  EXPECT_EQ(standard_buffer_.GetBuffer(), underlying_buffer_.data());

  auto plain_buffer = std::vector<unsigned char>(kSizeOfTestBuffer);
  pti::view::utilities::ViewBuffer destination_buffer{plain_buffer.data(), plain_buffer.size(), 0};
  auto valid_bytes = FillBuffer(destination_buffer);
  EXPECT_EQ(destination_buffer.GetValidBytes(), valid_bytes);
  destination_buffer = std::move(standard_buffer_);

  EXPECT_EQ(destination_buffer.IsNull(), false);
  EXPECT_EQ(destination_buffer.GetValidBytes(), bytes_inserted_);
  EXPECT_EQ(destination_buffer.GetBuffer(), underlying_buffer_.data());
}

TEST_F(ViewBufferFixtureTest, QueuePush) {
  pti::view::utilities::ViewBufferQueue buf_queue;
  buf_queue.Push(std::move(standard_buffer_));
  EXPECT_EQ(buf_queue.Size(), static_cast<std::size_t>(1));
}

TEST_F(ViewBufferFixtureTest, QueuePop) {
  pti::view::utilities::ViewBufferQueue buf_queue;
  buf_queue.Push(std::move(standard_buffer_));
  EXPECT_EQ(buf_queue.Size(), static_cast<std::size_t>(1));
  auto consume_buffer = buf_queue.Pop();
  EXPECT_EQ(consume_buffer.IsNull(), false);
  EXPECT_EQ(consume_buffer.GetValidBytes(), bytes_inserted_);
  EXPECT_EQ(consume_buffer.GetBuffer(), underlying_buffer_.data());
}

TEST_F(ViewBufferFixtureTest, QueueMultithreadedPopPush) {
  pti::view::utilities::ViewBufferQueue buf_queue;

  std::thread pop_thread([this, &buf_queue] {
    auto consume_buffer = buf_queue.Pop();
    EXPECT_EQ(buf_queue.Size(), static_cast<std::size_t>(0));
    EXPECT_EQ(consume_buffer.IsNull(), false);
    EXPECT_EQ(consume_buffer.GetValidBytes(), bytes_inserted_);
    EXPECT_EQ(consume_buffer.GetBuffer(), underlying_buffer_.data());
  });

  buf_queue.Push(std::move(standard_buffer_));
  pop_thread.join();
  EXPECT_EQ(buf_queue.Size(), static_cast<std::size_t>(0));
}

TEST_F(ViewBufferFixtureTest, QueueMultithreadedPushPop) {
  constexpr std::size_t kBufferQueueSize = 3UL;
  pti::view::utilities::ViewBufferQueue buf_queue(kBufferQueueSize);

  std::thread push_thread([this, &buf_queue, kBufferQueueSize] {
    buf_queue.Push(pti::view::utilities::ViewBuffer{});
    buf_queue.Push(pti::view::utilities::ViewBuffer{});
    buf_queue.Push(pti::view::utilities::ViewBuffer{});
    buf_queue.Push(std::move(standard_buffer_));
    EXPECT_LE(buf_queue.Size(), kBufferQueueSize);
  });

  // Pop the first buffer
  auto consume_buffer = buf_queue.Pop();
  EXPECT_EQ(consume_buffer.IsNull(), true);
  EXPECT_EQ(consume_buffer.GetValidBytes(), static_cast<std::size_t>(0));
  EXPECT_EQ(consume_buffer.GetBuffer(), nullptr);
  consume_buffer = buf_queue.Pop();
  consume_buffer = buf_queue.Pop();

  // Pop the last buffer
  consume_buffer = buf_queue.Pop();
  push_thread.join();
  EXPECT_EQ(consume_buffer.IsNull(), false);
  EXPECT_EQ(consume_buffer.GetValidBytes(), bytes_inserted_);
  EXPECT_EQ(consume_buffer.GetBuffer(), underlying_buffer_.data());
  EXPECT_EQ(buf_queue.Size(), static_cast<std::size_t>(0));
}

TEST_F(ViewBufferFixtureTest, QueueMultithreadedWaitUntilEmpty) {
  pti::view::utilities::ViewBufferQueue buf_queue;

  std::atomic<bool> stop_consumer = false;

  buf_queue.Push(pti::view::utilities::ViewBuffer{});
  buf_queue.Push(pti::view::utilities::ViewBuffer{});

  std::thread push_thread([this, &buf_queue] {
    buf_queue.Push(pti::view::utilities::ViewBuffer{});
    buf_queue.Push(std::move(standard_buffer_));
  });

  std::thread pop_thread([&buf_queue] {
    buf_queue.Pop();
    buf_queue.Pop();
    buf_queue.Pop();
    buf_queue.Pop();
  });

  buf_queue.WaitUntilEmptyOr(stop_consumer);
  EXPECT_EQ(buf_queue.Size(), static_cast<std::size_t>(0));

  push_thread.join();
  pop_thread.join();
}

TEST_F(ViewBufferFixtureTest, QueueMultithreadedWaitUntilEmptyOr) {
  pti::view::utilities::ViewBufferQueue buf_queue;

  std::atomic<bool> stop_consumer = false;

  buf_queue.Push(pti::view::utilities::ViewBuffer{});

  std::thread push_thread([this, &buf_queue, &stop_consumer] {
    buf_queue.Push(pti::view::utilities::ViewBuffer{});
    stop_consumer = true;
    buf_queue.Push(pti::view::utilities::ViewBuffer{});
    buf_queue.Push(std::move(standard_buffer_));
  });

  buf_queue.WaitUntilEmptyOr(stop_consumer);
  EXPECT_NE(buf_queue.Size(), static_cast<std::size_t>(0));

  push_thread.join();
}

TEST(GuardedUnorderedMapTest, ForEach) {
  using pti::view::utilities::GuardedUnorderedMap;
  GuardedUnorderedMap<int, std::string> table = {};
  table[100] = "hello";
  table[101] = ",";
  table[102] = "world";
  table.ForEach([&](const auto& key, auto& value) {
    switch (key) {
      case 100:
        value = "world";
        break;
      case 101:
        value = ",";
        break;
      case 102:
        value = "hello";
        break;
      default:
        FAIL();
        break;
    }
  });

  EXPECT_EQ(table[100], "world");
  ASSERT_EQ(table[101], ",");
  ASSERT_EQ(table[102], "hello");
}

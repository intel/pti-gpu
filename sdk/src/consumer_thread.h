//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef SRC_CONSUMER_THREAD_H_
#define SRC_CONSUMER_THREAD_H_

/**
 * \internal
 * \file consumer_thread.h
 * \brief Thread that returns buffers to the user via user defined callback.
 *
 * Starts a thread on construction. Operates on queue of moveable callable
 * objects (buffer(s) and callback). This makes it easier to either wait for
 * user to parse buffers with a future or "PushAndForget" and ignore the
 * result.
 *
 */

#include <atomic>
#include <future>
#include <thread>

#include "view_buffer.h"

namespace pti::view {

constexpr auto kDefaultBufferQueueDepth = 50UL;
constexpr auto kBufQueueDepthMult = 2UL;

/**
 * \internal
 * \brief Class that starts a thread that is meant to return buffers to the
 * user.
 *
 * This can accept moveable C++ callable objects; however, it is tuned for
 * ViewBuffers.
 * In C++23, std::move_only_function is introduced. That would be a
 * optimization one could make to this class (along with std::jthread in C++20).
 *
 */
class BufferConsumer {
 public:
  BufferConsumer() {
    const auto threads_supported = std::thread::hardware_concurrency();
    if (threads_supported) {
      queue_.SetBufferDepth(kBufQueueDepthMult * threads_supported);
    }
    consumer_ = std::thread(&BufferConsumer::Run, this);
  }

  BufferConsumer(const BufferConsumer&) = delete;
  BufferConsumer& operator=(const BufferConsumer&) = delete;
  BufferConsumer(BufferConsumer&&) = delete;
  BufferConsumer& operator=(BufferConsumer&&) = delete;

  virtual ~BufferConsumer() {
    Stop();
    if (consumer_.joinable()) {
      consumer_.join();
    }
  }

  /**
   * \internal
   * Add callback that returns user's buffer.
   *
   * \param callable (callable object: functor, lambda, function)
   * \return std::future<void> lets us wait for the result (useful for flush)
   */
  template <typename T>
  inline auto Push(T&& callable) {
    auto delivery = std::packaged_task<void()>(std::move(callable));
    auto delivery_future = delivery.get_future();
    queue_.Push(std::move(delivery));
    return delivery_future;
  }

  /**
   * \internal
   * Add callback that returns user's buffer. Don't care when it returns.
   *
   * \param callable (callable object: functor, lambda, function)
   */
  template <typename T>
  inline void PushAndForget(T&& callable) {
    queue_.Push(std::packaged_task<void()>(std::move(callable)));
  }

  /**
   * \internal
   * Signal to the consumer thread to stop
   *
   * \warning should rarely be used. Stops PTI's global buffer consumer
   */
  inline void Stop() {
    stop_thread_ = true;
    queue_.ResetBufferDepth();
    queue_.Push(std::packaged_task<void()>([] {}));
  }

 private:
  void Run() {
    while (!stop_thread_) {
      auto delivery = queue_.Pop();
      delivery();
    }
  }

  std::atomic<bool> stop_thread_ = false;
  utilities::ViewRecordBufferQueue<std::packaged_task<void()>> queue_{kDefaultBufferQueueDepth};
  std::thread consumer_;
};
}  // namespace pti::view

#endif  // SRC_CONSUMER_THREAD_H_

#include <gtest/gtest.h>

#include <atomic>
#include <list>
#include <thread>
#include <vector>

#include "ze_collector_cb_helpers.h"

class DummySubscriber : public ZeCollectorCBSubscriber {};

TEST(SubscribersCollectionTest, ThreadSafetyAndIteration) {
  SubscribersCollection collection;
  constexpr int num_threads = 8;
  constexpr int subs_per_thread = 100;
  std::vector<std::unique_ptr<DummySubscriber>> subscribers;
  std::vector<_pti_callback_subscriber*> handles(num_threads * subs_per_thread);
  std::atomic<int> ready{0};

  // Prepare subscribers
  for (int i = 0; i < num_threads * subs_per_thread; ++i) {
    subscribers.emplace_back(new DummySubscriber());
  }

  // Add external subscribers in parallel
  std::vector<std::thread> threads;
  for (int t = 0; t < num_threads; ++t) {
    threads.emplace_back([&, t]() {
      ready++;
      while (ready.load() < num_threads) {
      }
      for (int i = 0; i < subs_per_thread; ++i) {
        // Storing handles as will need to remove providing a handle
        handles[t * subs_per_thread + i] =
            collection.AddExternalSubscriber(std::move(subscribers[t * subs_per_thread + i]));
      }
    });
  }
  for (auto& th : threads) th.join();
  ASSERT_EQ(collection.GetSubscriberCount(), num_threads * subs_per_thread);

  // Remove half in parallel
  ready = 0;
  threads.clear();
  for (int t = 0; t < num_threads; ++t) {
    threads.emplace_back([&, t]() {
      ready++;
      while (ready.load() < num_threads) {
      }
      for (int i = 0; i < subs_per_thread / 2; ++i) {
        collection.RemoveExternalSubscriber(handles[t * subs_per_thread + i]);
      }
    });
  }
  for (auto& th : threads) th.join();
  ASSERT_EQ(collection.GetSubscriberCount(), num_threads * subs_per_thread / 2);

  collection.AddInternalSubscriber(std::make_unique<DummySubscriber>());
  collection.AddInternalSubscriber(std::make_unique<DummySubscriber>());

  ASSERT_EQ(collection.GetSubscriberCount(), 2 + num_threads * subs_per_thread / 2);

  // Check that we can iterate over collection as of std::list
  // iterating over remaining subscribers
  size_t count = 0;
  for ([[maybe_unused]] const auto& sub : collection) {
    count++;
  }
  ASSERT_EQ(count, 2 + num_threads * subs_per_thread / 2);
}

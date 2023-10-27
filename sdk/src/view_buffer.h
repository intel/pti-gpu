//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
// LICENSE HERE

#ifndef SRC_VIEW_BUFFER_H_
#define SRC_VIEW_BUFFER_H_

#include <assert.h>

#include <array>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace pti {
namespace view {
namespace utilities {
template <typename U = unsigned char>
struct ViewRecordBuffer {
 public:
  using UnderlyingType = U;
  using SizeType = std::size_t;

  ViewRecordBuffer() = default;

  constexpr explicit ViewRecordBuffer(UnderlyingType* buffer, SizeType size,
                                      SizeType start_position)
      : buf_(buffer), size_(size), pos_(start_position) {}

  ViewRecordBuffer(const ViewRecordBuffer&) = delete;
  ViewRecordBuffer& operator=(const ViewRecordBuffer&) = delete;

  ViewRecordBuffer(ViewRecordBuffer&& other)
      : buf_(std::exchange(other.buf_, nullptr)),
        size_(std::exchange(other.size_, 0)),
        pos_(std::exchange(other.pos_, 0)) {}

  ViewRecordBuffer& operator=(ViewRecordBuffer&& other) noexcept {
    if (this != &other) {
      std::swap(other.buf_, buf_);  // keep ptr to data around
      size_ = std::exchange(other.size_, 0);
      pos_ = std::exchange(other.pos_, 0);
    }
    return *this;
  }
  virtual ~ViewRecordBuffer() = default;

  inline void Refresh(UnderlyingType* buffer, SizeType size) {
    buf_ = buffer;
    pos_ = 0;
    size_ = size;
  }

  // Return the buffer pointer to the inserted record
  template <typename T>
  inline UnderlyingType* Insert(const T& view_record) {
    static_assert(std::is_trivially_copyable<T>::value,
                  "One can only insert trivially copyable types into the "
                  "ViewBuffer (view records)");
    assert(!IsNull());
    assert(FreeBytes() >= sizeof(T));
    auto* inserted_record = buf_ + pos_;
    std::memcpy(inserted_record, &view_record, sizeof(T));
    pos_ += sizeof(T);
    return inserted_record;
  }

  template <typename T>
  inline T* Peek() const {
    return Peek<T>(nullptr, 0);
  }

  template <typename T>
  inline T* Peek(T* element) const {
    return Peek<T>(element, 0);
  }

  template <typename T>
  inline T* Peek(SizeType loc) const {
    return Peek<T>(nullptr, loc);
  }

  /**
   * @brief Peek inside the buffer and view the next element
   *
   * @param element (optional) pointer to current element IN the buffer. If
   * nullptr or unspecified, starts at beginning of buffer
   * @param loc (optional) location of next element. If unspecified or 0,
   * return current element in buffer.
   * @return a pointer to an element inside the buffer, nullptr at end of buffer
   */
  template <typename T>
  inline T* Peek(T* element, SizeType loc) const {
    auto* buffer_window = element ? reinterpret_cast<UnderlyingType*>(element) : buf_;
    auto* peek_element = buffer_window + loc;
    if (peek_element == GetRecordsEnd()) {
      peek_element = nullptr;
    }
    return reinterpret_cast<T*>(peek_element);
  }

  inline bool IsNull() const { return (!buf_ || !size_); }

  constexpr UnderlyingType* GetBuffer() { return buf_; }

  constexpr UnderlyingType* GetBuffer() const { return buf_; }

  constexpr SizeType GetBufferSize() const { return size_; }
  constexpr UnderlyingType* GetBufferEnd() { return buf_ + size_; }

  constexpr SizeType GetValidBytes() const { return pos_; }

  constexpr UnderlyingType* GetRecordsEnd() { return buf_ + pos_; }

  constexpr UnderlyingType* GetRecordsEnd() const { return buf_ + pos_; }

  constexpr SizeType FreeBytes() const {
    assert(size_ - pos_ >= 0);
    return size_ - pos_;
  }

  template <typename T>
  constexpr bool BufferFull() const {
    static_assert(std::is_trivially_copyable<T>::value,
                  "One can only insert trivially copyable types into the "
                  "ViewBuffer (view records)");
    return sizeof(T) >= FreeBytes();
  }

  friend void Swap(ViewRecordBuffer& lhs, ViewRecordBuffer& rhs) { std::swap(lhs, rhs); }

 private:
  UnderlyingType* buf_ = nullptr;
  SizeType size_ = 0;
  SizeType pos_ = 0;
};

template <typename T>
struct ViewRecordBufferQueue {
 public:
  ViewRecordBufferQueue() = default;
  explicit ViewRecordBufferQueue(std::size_t depth) : buffer_depth_(depth) {}
  ViewRecordBufferQueue& operator=(const ViewRecordBufferQueue&) = delete;
  ViewRecordBufferQueue& operator=(ViewRecordBufferQueue&& other) = delete;
  ViewRecordBufferQueue(const ViewRecordBufferQueue&) = delete;
  ViewRecordBufferQueue(ViewRecordBufferQueue&& other) = delete;

  inline void Push(T&& buffer) {
    std::unique_lock<std::mutex> buffer_lock(buffer_queue_mtx_);
    if (buffer_depth_.has_value()) {
      buffer_available_.wait(buffer_lock, [this] { return buffer_queue_.size() < buffer_depth_; });
    }
    buffer_queue_.push(std::move(buffer));
    buffer_lock.unlock();
    buffer_available_.notify_one();
  }

  inline T Pop() {
    std::unique_lock<std::mutex> buffer_lock(buffer_queue_mtx_);
    buffer_available_.wait(buffer_lock, [this] { return !buffer_queue_.empty(); });
    auto buffer = std::move(buffer_queue_.front());
    buffer_queue_.pop();
    buffer_lock.unlock();
    buffer_available_.notify_all();

    return buffer;
  }

  template <typename Condition>
  inline void WaitUntilEmptyOr(const Condition& cond) {
    std::unique_lock<std::mutex> buffer_lock(buffer_queue_mtx_);
    buffer_available_.wait(buffer_lock, [this, &cond] { return buffer_queue_.empty() || cond; });
  }

  inline std::size_t Size() {
    std::lock_guard<std::mutex> buffer_lock(buffer_queue_mtx_);
    return std::size(buffer_queue_);
  }

  inline void ResetBufferDepth() {
    std::lock_guard<std::mutex> buffer_lock(buffer_queue_mtx_);
    buffer_depth_.reset();
  }

  inline void SetBufferDepth(std::size_t depth) {
    std::lock_guard<std::mutex> buffer_lock(buffer_queue_mtx_);
    buffer_depth_ = depth;
  }

  virtual ~ViewRecordBufferQueue() = default;

 private:
  std::queue<T> buffer_queue_;
  mutable std::mutex buffer_queue_mtx_;
  std::condition_variable buffer_available_;
  std::optional<std::size_t> buffer_depth_;
};

// This is not a perfect abstraction.
// User beware. TODO(matthew.schilling@intel.com): Boost? Make more robust?
template <typename KeyT, typename ValueT>
struct ThreadSafeHashTable {
 public:
  ThreadSafeHashTable() = default;
  ThreadSafeHashTable& operator=(const ThreadSafeHashTable&) = delete;
  ThreadSafeHashTable& operator=(ThreadSafeHashTable&& other) = delete;
  ThreadSafeHashTable(const ThreadSafeHashTable&) = delete;
  ThreadSafeHashTable(ThreadSafeHashTable&& other) = delete;
  virtual ~ThreadSafeHashTable() = default;

  inline auto& operator[](const KeyT& key) {
    std::lock_guard<std::mutex> lock_table(hash_table_mtx_);
    return hash_table_[key];
  }

  inline void Erase(const KeyT& key) {
    std::lock_guard<std::mutex> lock_table(hash_table_mtx_);
    hash_table_.erase(key);
  }

  bool Empty() const {
    std::lock_guard<std::mutex> lock_table(hash_table_mtx_);
    return hash_table_.empty();
  }

  inline ValueT* TryFindElement(const KeyT& key) {
    std::lock_guard<std::mutex> lock_table(hash_table_mtx_);
    if (hash_table_.find(key) != hash_table_.end()) {
      return &hash_table_[key];
    }
    return nullptr;
  }

  template <typename Callable>
  inline void ForEach(Callable&& user_function) {
    std::lock_guard<std::mutex> lock_table(hash_table_mtx_);
    auto user_callable = std::forward<Callable>(user_function);
    for (auto&& [key, value] : hash_table_) {
      user_callable(key, value);
    }
  }

 private:
  std::unordered_map<KeyT, ValueT> hash_table_;
  mutable std::mutex hash_table_mtx_;
};

using ViewBuffer = ViewRecordBuffer<unsigned char>;
using ViewBufferQueue = ViewRecordBufferQueue<ViewBuffer>;
template <typename KeyT>
using ViewBufferTable = ThreadSafeHashTable<KeyT, ViewBuffer>;

}  // namespace utilities
}  // namespace view
}  // namespace pti

#endif  // SRC_VIEW_BUFFER_H_

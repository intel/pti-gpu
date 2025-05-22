#ifndef TEST_UTILS_TEST_HELPERS_H_
#define TEST_UTILS_TEST_HELPERS_H_

#include <cstring>
#include <initializer_list>
#include <iostream>
#include <ostream>
#include <type_traits>
#include <vector>

#include "pti/pti_view.h"

// Needs to be in the same namespace as pti_result so leave it outside.
inline std::ostream& operator<<(std::ostream& out, pti_result result_val) {
  out << ptiResultTypeToString(result_val);
  return out;
}

namespace pti::test::utils {

template <typename... T>
constexpr std::size_t ValidateTimestamps(T... args) {
  using TimestampType = std::common_type_t<T...>;
  constexpr auto count = sizeof...(args);
  static_assert(count > 1, "Must provide more than one timestamp to validate");
  std::size_t found_issues = 0;
  TimestampType prev_stamp = 0;
  (
      [&] {
        auto next_stamp = args;
        if (!(prev_stamp <= next_stamp)) {
          found_issues++;
        }
        prev_stamp = next_stamp;
      }(),
      ...);
  return found_issues;
}

constexpr int ValidateNoBigGapBetweenTimestampsNs(uint64_t gap_in_ns,
                                                  std::initializer_list<uint64_t> stamps) {
  if (stamps.size() < 2) {
    return -1;
  }
  int found_issues = 0;
  auto it = stamps.begin();
  auto prev_stamp = *it;
  for (++it; it != stamps.end(); it++) {
    auto next_stamp = *it;
    if (next_stamp > prev_stamp + gap_in_ns) {
      std::cout << "prev_stamp: " << prev_stamp << "\n"
                << "next_stamp: " << next_stamp << std::endl;
      found_issues++;
    }
    prev_stamp = next_stamp;
  }
  return found_issues;
}

inline constexpr auto kDefaultPtiBufferAlignment = std::align_val_t{1};

template <typename T>
[[nodiscard]] inline T* AlignedAlloc(std::size_t size, std::align_val_t align) {
  try {
    return static_cast<T*>(::operator new(size, align));
  } catch (const std::bad_alloc& e) {
    std::cerr << "Alloc failed " << e.what() << '\n';
    return nullptr;
  }
}

template <typename T>
inline void AlignedDealloc(T* buf_ptr, std::align_val_t align) {
  try {
    ::operator delete(buf_ptr, align);
  } catch (...) {
    std::cerr << "DeAlloc failed, abort" << '\n';
    std::abort();
  }
}

template <typename T>
[[nodiscard]] inline T* AlignedAlloc(std::size_t size) {
  return AlignedAlloc<T>(size, kDefaultPtiBufferAlignment);
}

template <typename T>
inline void AlignedDealloc(T* buf_ptr) {
  return AlignedDealloc<T>(buf_ptr, kDefaultPtiBufferAlignment);
}

///////////////////////////////////////////////////////////////////////////////
/// @brief Helper functions for creating "empty" view records.
template <typename T>
inline void Zeroize(T& item) {
  static_assert(std::is_trivially_copyable<T>::value,
                "Can't zeroize an object that's not trivially copyable");
  std::memset(&item, 0, sizeof(T));
}

template <typename T, pti_view_kind E>
inline auto CreateRecord() {
  T record = {};
  Zeroize(record);
  record._view_kind._view_kind = E;
  return record;
}

template <typename T>
inline T CreateRecord() {
  return CreateRecord<T, pti_view_kind::PTI_VIEW_INVALID>();
}

template <pti_view_kind E>
inline auto CreateRecord() {
  return CreateRecord<pti_view_record_kernel, E>();
}

template <>
inline pti_view_record_memory_copy CreateRecord() {
  return CreateRecord<pti_view_record_memory_copy, pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_COPY>();
}

template <>
inline pti_view_record_memory_fill CreateRecord() {
  return CreateRecord<pti_view_record_memory_fill, pti_view_kind::PTI_VIEW_DEVICE_GPU_MEM_FILL>();
}

template <>
inline pti_view_record_kernel CreateRecord() {
  return CreateRecord<pti_view_record_kernel, pti_view_kind::PTI_VIEW_DEVICE_GPU_KERNEL>();
}

template <>
inline pti_view_record_overhead CreateRecord() {
  return CreateRecord<pti_view_record_overhead, pti_view_kind::PTI_VIEW_COLLECTION_OVERHEAD>();
}

template <>
inline pti_view_record_sycl_runtime CreateRecord() {
  return CreateRecord<pti_view_record_sycl_runtime, pti_view_kind::PTI_VIEW_SYCL_RUNTIME_CALLS>();
}

template <>
inline pti_view_record_external_correlation CreateRecord() {
  return CreateRecord<pti_view_record_external_correlation,
                      pti_view_kind::PTI_VIEW_EXTERNAL_CORRELATION>();
}

///////////////////////////////////////////////////////////////////////////////
/// @brief Helper functions for creating buffers based on view record type.
template <typename T, std::size_t N>
struct RecordInserts {
  using Type = T;
  constexpr static std::size_t kNumber = N;
};

template <typename... RecordInsert>
inline std::vector<unsigned char> CreateEmptyBuffer() {
  std::size_t buffer_size = 0;
  ((buffer_size += (RecordInsert::kNumber * sizeof(typename RecordInsert::Type))), ...);
  return std::vector<unsigned char>(buffer_size);
};

template <typename... RecordInsert>
inline std::vector<unsigned char> CreateFullBuffer() {
  auto result_vec = CreateEmptyBuffer<RecordInsert...>();
  std::size_t buffer_pos = 0;
  (
      [&] {
        for (std::size_t i = 0; i < RecordInsert::kNumber; ++i) {
          const auto record = CreateRecord<typename RecordInsert::Type>();
          constexpr auto record_size = sizeof(typename RecordInsert::Type);
          std::memcpy(result_vec.data() + buffer_pos, &record, record_size);
          buffer_pos += record_size;
        }
      }(),
      ...);
  return result_vec;
}
}  // namespace pti::test::utils

#endif  // TEST_UTILS_TEST_HELPERS_H_

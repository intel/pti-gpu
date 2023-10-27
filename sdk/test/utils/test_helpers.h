#ifndef UTILS_TEST_HELPERS_H_
#define UTILS_TEST_HELPERS_H_

#include <cstring>
#include <tuple>
#include <type_traits>
#include <vector>

#include "pti_view.h"

namespace pti::test::utils {

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
  inline constexpr static std::size_t kNumber = N;
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

#endif  // UTILS_TEST_HELPERS_H_

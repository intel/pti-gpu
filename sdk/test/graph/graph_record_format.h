//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef TEST_GRAPH_GRAPH_RECORD_FORMAT_H_
#define TEST_GRAPH_GRAPH_RECORD_FORMAT_H_

#include <fmt/core.h>
#include <fmt/format.h>

#include <string>
#include <type_traits>

#include "pti/pti_view.h"

template <typename T>
inline std::string FormatRecord(const T* record) {
  auto result = fmt::format(
      "Name: {} \nDuration (ns): {}\nStart Time (ns): {}\nEnd Time (ns): {}\nThread ID: {}\n",
      record->_name, record->_end_timestamp - record->_start_timestamp, record->_start_timestamp,
      record->_end_timestamp, record->_thread_id);

  if constexpr (std::is_same_v<T, pti_view_record_kernel> ||
                std::is_same_v<T, pti_view_record_kernel_v2>) {
    result += fmt::format("Submit Time: {}\n", record->_submit_timestamp);
    result += fmt::format("Append Time: {}\n", record->_append_timestamp);
    result += fmt::format("Correlation ID: {}\n", record->_correlation_id);
    result += fmt::format("SYCL Enqueue Begin Time (ns): {}\n", record->_sycl_enqk_begin_timestamp);
    result += fmt::format("SYCL Task Begin ID (ns): {}\n", record->_sycl_task_begin_timestamp);
    result += fmt::format("SYCL Queue ID: {}\n", record->_sycl_queue_id);
    result += fmt::format("Kernel ID: {}\n", record->_kernel_id);
  }

  return result;
}

#endif  // TEST_GRAPH_GRAPH_RECORD_FORMAT_H_

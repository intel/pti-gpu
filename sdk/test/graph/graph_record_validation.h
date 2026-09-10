//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
#ifndef TEST_GRAPH_GRAPH_RECORD_VALIDATION_H_
#define TEST_GRAPH_GRAPH_RECORD_VALIDATION_H_

#include <gtest/gtest.h>

#include <limits>
#include <vector>

#include "graph_record_format.h"

template <typename T>
inline void ValidateView(const T* record) {
  EXPECT_NE(record->_start_timestamp,
            (std::numeric_limits<decltype(record->_start_timestamp)>::min)())
      << "Failing record: " << FormatRecord(record);
  EXPECT_NE(record->_start_timestamp,
            (std::numeric_limits<decltype(record->_start_timestamp)>::max)())
      << "Failing record: " << FormatRecord(record);
  EXPECT_NE(record->_end_timestamp, (std::numeric_limits<decltype(record->_end_timestamp)>::min)())
      << "Failing record: " << FormatRecord(record);
  EXPECT_NE(record->_end_timestamp, (std::numeric_limits<decltype(record->_end_timestamp)>::max)())
      << "Failing record: " << FormatRecord(record);
  EXPECT_LE(record->_start_timestamp, record->_end_timestamp)
      << "Failing record: " << FormatRecord(record);
}

template <typename T>
inline void ValidateViewTimestamps(const std::vector<T*>& records) {
  for (const auto* record : records) {
    ValidateView(record);
  }
}

#endif  // TEST_GRAPH_GRAPH_RECORD_VALIDATION_H_

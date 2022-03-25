//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_ONEPROF_METRIC_QUERY_CACHE_H_
#define PTI_TOOLS_ONEPROF_METRIC_QUERY_CACHE_H_

#include <map>
#include <mutex>
#include <vector>

#include "ze_utils.h"

struct ZeMetricQueryInfo {
  zet_metric_query_pool_handle_t pool;
  ze_context_handle_t context;
};

class MetricQueryCache {
 public:
  MetricQueryCache(
      ze_device_handle_t device,
      zet_metric_group_handle_t group)
      : device_(device), group_(group) {
    PTI_ASSERT(device_ != nullptr);
    PTI_ASSERT(group_ != nullptr);
  }

  ~MetricQueryCache() {
    for (auto& value : query_map_) {
      for (auto query : value.second) {
        auto info = query_info_map_.find(query);
        PTI_ASSERT(info != query_info_map_.end());

        ze_result_t status = ZE_RESULT_SUCCESS;
        status = zetMetricQueryDestroy(query);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
        status = zetMetricQueryPoolDestroy(info->second.pool);
        PTI_ASSERT(status == ZE_RESULT_SUCCESS);
      }
    }
  }

  zet_metric_query_handle_t GetQuery(ze_context_handle_t context) {
    const std::lock_guard<std::mutex> lock(lock_);
    zet_metric_query_handle_t query = nullptr;

    auto result = query_map_.find(context);
    if (result == query_map_.end()) {
      result = query_map_.emplace(
          std::make_pair(
              context,
              std::vector<zet_metric_query_handle_t>())).first;
    }

    if (result->second.empty()) {
      ze_result_t status = ZE_RESULT_SUCCESS;

      zet_metric_query_pool_desc_t pool_desc = {
          ZET_STRUCTURE_TYPE_METRIC_QUERY_POOL_DESC,
          nullptr,
          ZET_METRIC_QUERY_POOL_TYPE_PERFORMANCE,
          1};
      zet_metric_query_pool_handle_t pool = nullptr;
      status = zetMetricQueryPoolCreate(
          context, device_, group_, &pool_desc, &pool);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      status = zetMetricQueryCreate(pool, 0, &query);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);

      PTI_ASSERT(query_info_map_.count(query) == 0);
      query_info_map_[query] = {pool, context};
    } else {
      query = result->second.back();
      result->second.pop_back();
    }

    return query;
  }

  void ResetQuery(zet_metric_query_handle_t query) {
    PTI_ASSERT(query != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);

    auto info = query_info_map_.find(query);
    if (info != query_info_map_.end()) {
      ze_result_t status = zetMetricQueryReset(query);
      PTI_ASSERT(status == ZE_RESULT_SUCCESS);
    }
  }

  void ReleaseQuery(zet_metric_query_handle_t query) {
    PTI_ASSERT(query != nullptr);
    const std::lock_guard<std::mutex> lock(lock_);

    auto info = query_info_map_.find(query);
    if (info == query_info_map_.end()) {
      return;
    }

    auto result = query_map_.find(info->second.context);
    PTI_ASSERT(result != query_map_.end());
    result->second.push_back(query);
  }

 private:
  std::map<
      ze_context_handle_t,
      std::vector<zet_metric_query_handle_t> > query_map_;
  std::map<
      zet_metric_query_handle_t,
      ZeMetricQueryInfo> query_info_map_;
  std::mutex lock_;

  ze_device_handle_t device_ = nullptr;
  zet_metric_group_handle_t group_ = nullptr;
};

#endif // PTI_TOOLS_ONEPROF_METRIC_QUERY_CACHE_H_
//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_LEVELZERO_ZE_GRAPH_STORAGE_H_
#define PTI_LEVELZERO_ZE_GRAPH_STORAGE_H_

#include <level_zero/ze_api.h>

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

#include "overhead_kinds.h"
#include "ze_gpu_command.h"
#include "ze_wrappers.h"

class ZeGraphStorage {
 public:
  explicit ZeGraphStorage(Level0Wrapper* wrapper) : loader_(wrapper) {}

  ZeGraphInfo* GetInfo(ze_executable_graph_handle_t executable_graph_handle) {
    {
      std::shared_lock lock(mutex_);
      auto graph_it = graph_info_map_.find(executable_graph_handle);
      if (graph_it != graph_info_map_.end()) {
        return graph_it->second.get();
      }
    }

    auto graph_info = CreateGraphInfo(executable_graph_handle);

    if (!graph_info) {
      return nullptr;
    }

    std::unique_lock lock(mutex_);
    return graph_info_map_.emplace(executable_graph_handle, std::move(graph_info))
        .first->second.get();
  }

 private:
  std::unique_ptr<ZeGraphInfo> CreateGraphInfo(
      ze_executable_graph_handle_t executable_graph_handle) {
    auto result = ZE_RESULT_SUCCESS;
    ze_graph_handle_t graph_handle = nullptr;
    {
      overhead::ScopedOverheadCollector scoped{zeExecutableGraphGetSourceGraphExt_id};
      result =
          loader_->w_zeExecutableGraphGetSourceGraphExt(executable_graph_handle, &graph_handle);
      if (result != ZE_RESULT_SUCCESS) {
        return nullptr;
      }
    }

    ze_command_list_handle_t command_list_handle = nullptr;
    {
      overhead::ScopedOverheadCollector scoped{zeGraphGetPrimaryCommandListExt_id};
      result = loader_->w_zeGraphGetPrimaryCommandListExt(graph_handle, &command_list_handle);
      if (result != ZE_RESULT_SUCCESS) {
        return nullptr;
      }
    }

    ze_device_handle_t device_handle = nullptr;
    {
      overhead::ScopedOverheadCollector scoped{zeCommandListGetDeviceHandle_id};
      result = loader_->w_zeCommandListGetDeviceHandle(command_list_handle, &device_handle);
      if (result != ZE_RESULT_SUCCESS) {
        return nullptr;
      }
    }

    ze_context_handle_t context_handle = nullptr;
    {
      overhead::ScopedOverheadCollector scoped{zeCommandListGetContextHandle_id};
      result = loader_->w_zeCommandListGetContextHandle(command_list_handle, &context_handle);
      if (result != ZE_RESULT_SUCCESS) {
        return nullptr;
      }
    }

    auto graph_info = std::make_unique<ZeGraphInfo>();
    graph_info->graph = graph_handle;
    graph_info->executable_graph = executable_graph_handle;
    graph_info->primary_command_list = command_list_handle;
    graph_info->device = device_handle;
    graph_info->context = context_handle;
    return graph_info;
  }

  Level0Wrapper* loader_;
  mutable std::shared_mutex mutex_;
  std::unordered_map<ze_executable_graph_handle_t, std::unique_ptr<ZeGraphInfo>> graph_info_map_;
};

#endif  // PTI_LEVELZERO_ZE_GRAPH_STORAGE_H_

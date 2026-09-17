//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#define INTEL_NO_MACRO_BODY
#define INTEL_ITTNOTIFY_API_PRIVATE
#define ITTAPI_CDECL __attribute__((visibility("default")))
#include <ittnotify.h>
#include <ittnotify_config.h>
#include <spdlog/cfg/helpers.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

struct TaskCounts {
  std::atomic<std::uint64_t> begin{0};
  std::atomic<std::uint64_t> end{0};
};

TaskCounts ccl_domain_counts;
TaskCounts trivial_domain_counts;

TaskCounts* GetTaskCounts(const char* domain_name) {
  if (domain_name == nullptr) {
    return nullptr;
  }
  if (std::strcmp(domain_name, "oneCCL::API") == 0) {
    return &ccl_domain_counts;
  }
  if (std::strcmp(domain_name, "trivial::API") == 0) {
    return &trivial_domain_counts;
  }
  return nullptr;
}

void ITTAPI TrivialCollectorTaskBegin([[maybe_unused]] const __itt_domain* domain,
                                      [[maybe_unused]] __itt_id task_id,
                                      [[maybe_unused]] __itt_id parent_id,
                                      [[maybe_unused]] __itt_string_handle* name) {
  if (auto* counts = GetTaskCounts(domain != nullptr ? domain->nameA : nullptr);
      counts != nullptr) {
    counts->begin.fetch_add(1, std::memory_order_relaxed);
  }
  SPDLOG_DEBUG("{}() domain: {}, task: {}", __FUNCTION__,
               domain != nullptr ? domain->nameA : "NULL", name != nullptr ? name->strA : "NULL");
}

void ITTAPI TrivialCollectorTaskEnd([[maybe_unused]] const __itt_domain* domain) {
  if (auto* counts = GetTaskCounts(domain != nullptr ? domain->nameA : nullptr);
      counts != nullptr) {
    counts->end.fetch_add(1, std::memory_order_relaxed);
  }
  SPDLOG_DEBUG("{}() domain: {}", __FUNCTION__, domain != nullptr ? domain->nameA : "NULL");
}

}  // namespace

ITT_EXTERN_C std::uint64_t ITTAPI IttTrivialCollectorGetTaskBeginCount(const char* domain_name) {
  auto* counts = GetTaskCounts(domain_name);
  return counts != nullptr ? counts->begin.load(std::memory_order_relaxed) : 0;
}

ITT_EXTERN_C std::uint64_t ITTAPI IttTrivialCollectorGetTaskEndCount(const char* domain_name) {
  auto* counts = GetTaskCounts(domain_name);
  return counts != nullptr ? counts->end.load(std::memory_order_relaxed) : 0;
}

ITT_EXTERN_C void ITTAPI __itt_api_init(__itt_global* global,
                                        [[maybe_unused]] __itt_group_id init_groups) {
  if (global == nullptr) {
    return;
  }

  if (const char* log_level = std::getenv("PTILOG_LEVEL"); log_level != nullptr) {
    spdlog::cfg::helpers::load_levels(log_level);
  }

  auto* api_list = global->api_list_ptr;
  for (int i = 0; api_list[i].name != nullptr; ++i) {
    void* callback = api_list[i].null_func;
    if (std::strcmp(api_list[i].name, "__itt_task_begin") == 0) {
      callback = reinterpret_cast<void*>(&TrivialCollectorTaskBegin);
    } else if (std::strcmp(api_list[i].name, "__itt_task_end") == 0) {
      callback = reinterpret_cast<void*>(&TrivialCollectorTaskEnd);
    }
    *(api_list[i].func_ptr) = callback;
  }
}
